#!/usr/bin/env python3

import argparse
from concurrent.futures import ThreadPoolExecutor
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import threading
import time
from queue import Queue
from typing import Any, Dict, List, Optional, Tuple


PLAN_PATH_RE = re.compile(
    r"(?:Saved new plan cache to|Using existing plan cache at)\s+(.+)$",
    re.MULTILINE,
)
THROUGHPUT_RE = re.compile(r"Throughput:\s*([0-9eE+\-.]+)\s*qps")
TOTAL_HOST_RE = re.compile(r"Total Host Walltime:\s*([0-9eE+\-.]+)\s*s")
TOTAL_GPU_RE = re.compile(r"Total GPU Compute Time:\s*([0-9eE+\-.]+)\s*s")
SUMMARY_RE = {
    "latency": re.compile(
        r"Latency:\s*min = ([^,]+), max = ([^,]+), mean = ([^,]+), median = ([^,]+), "
        r"percentile\(90%\) = ([^,]+), percentile\(95%\) = ([^,]+), percentile\(99%\) = ([^\n,]+)"
    ),
    "enqueue": re.compile(
        r"Enqueue Time:\s*min = ([^,]+), max = ([^,]+), mean = ([^,]+), median = ([^,]+), "
        r"percentile\(90%\) = ([^,]+), percentile\(95%\) = ([^,]+), percentile\(99%\) = ([^\n,]+)"
    ),
    "h2d": re.compile(
        r"H2D Latency:\s*min = ([^,]+), max = ([^,]+), mean = ([^,]+), median = ([^,]+), "
        r"percentile\(90%\) = ([^,]+), percentile\(95%\) = ([^,]+), percentile\(99%\) = ([^\n,]+)"
    ),
    "gpu_compute": re.compile(
        r"GPU Compute Time:\s*min = ([^,]+), max = ([^,]+), mean = ([^,]+), median = ([^,]+), "
        r"percentile\(90%\) = ([^,]+), percentile\(95%\) = ([^,]+), percentile\(99%\) = ([^\n,]+)"
    ),
    "d2h": re.compile(
        r"D2H Latency:\s*min = ([^,]+), max = ([^,]+), mean = ([^,]+), median = ([^,]+), "
        r"percentile\(90%\) = ([^,]+), percentile\(95%\) = ([^,]+), percentile\(99%\) = ([^\n,]+)"
    ),
}
PLAN_INPUT_RE = re.compile(
    r"Model input\s+(.+?)\s+\(profile\s+[0-9]+\):\s+min=([^,]+),\s*opt=([^,]+),\s*max=([^\n\r]+)"
)


def now_iso() -> str:
    return dt.datetime.now(dt.timezone.utc).isoformat()


def join_cmd(cmd: List[str]) -> str:
    return shlex.join(cmd)


def detect_default_path(candidates: List[str]) -> str:
    for candidate in candidates:
        if Path(candidate).exists():
            return str(Path(candidate).resolve())
    return candidates[0]


def resolve_path_with_base(raw: str, base_dir: Path) -> str:
    expanded = os.path.expanduser(os.path.expandvars(raw.strip()))
    path = Path(expanded)
    if not path.is_absolute():
        path = (base_dir / path).resolve()
    return str(path)


def load_env_sh_defaults(env_sh_path: Path) -> Dict[str, str]:
    if not env_sh_path.exists():
        return {}

    keys = [
        "TENSORRT_ROOT",
        "KATAGO_BIN_PATH",
        "KATAGO_MODEL_PATH",
        "KATAGO_CONFIG_PATH",
        "TRT_DEVICE_ID",
    ]
    path_keys = {
        "TENSORRT_ROOT",
        "KATAGO_BIN_PATH",
        "KATAGO_MODEL_PATH",
        "KATAGO_CONFIG_PATH",
    }
    var_expr = " ".join(f'"${{{key}-}}"' for key in keys)
    shell_cmd = f"source {shlex.quote(str(env_sh_path))} >/dev/null 2>&1 && printf '%s\\n' {var_expr}"
    proc = subprocess.run(
        ["bash", "-lc", shell_cmd],
        text=True,
        capture_output=True,
        check=False,
    )
    if proc.returncode != 0:
        return {}

    lines = (proc.stdout or "").splitlines()
    out: Dict[str, str] = {}
    base_dir = env_sh_path.parent
    for idx, key in enumerate(keys):
        if idx >= len(lines):
            break
        value = lines[idx].strip()
        if not value:
            continue
        if key in path_keys:
            out[key] = resolve_path_with_base(value, base_dir)
        else:
            out[key] = value
    return out


def nonnegative_int(raw: str) -> int:
    try:
        value = int(raw)
    except ValueError as e:
        raise argparse.ArgumentTypeError(f"invalid integer: {raw}") from e
    if value < 0:
        raise argparse.ArgumentTypeError(f"value must be non-negative: {raw}")
    return value


def positive_int(raw: str) -> int:
    value = nonnegative_int(raw)
    if value <= 0:
        raise argparse.ArgumentTypeError(f"value must be positive: {raw}")
    return value


def positive_float(raw: str) -> float:
    try:
        value = float(raw)
    except ValueError as e:
        raise argparse.ArgumentTypeError(f"invalid number: {raw}") from e
    if value <= 0:
        raise argparse.ArgumentTypeError(f"value must be positive: {raw}")
    return value


def parse_value_with_unit(raw: str) -> Dict[str, object]:
    token = raw.strip()
    match = re.match(r"^([0-9eE+\-.]+)\s*([A-Za-z%/]+)?$", token)
    if not match:
        return {"text": token}
    value = float(match.group(1))
    unit = match.group(2) or ""
    return {"value": value, "unit": unit, "text": token}


def split_list_arg_values(values: Optional[List[str]]) -> List[str]:
    if not values:
        return []
    out: List[str] = []
    for value in values:
        for piece in str(value).split(","):
            piece = piece.strip()
            if piece:
                out.append(piece)
    return out


def normalize_list_args(args: argparse.Namespace) -> None:
    for name in ("plan_file", "devices", "shape_template", "gtp_extra_override", "trtexec_extra_arg"):
        setattr(args, name, split_list_arg_values(getattr(args, name, None)))


def parse_trtexec_metrics(text: str) -> Dict[str, object]:
    metrics: Dict[str, object] = {}
    throughput = THROUGHPUT_RE.search(text)
    if throughput:
        metrics["throughput_qps"] = float(throughput.group(1))

    total_host = TOTAL_HOST_RE.search(text)
    if total_host:
        metrics["total_host_walltime_s"] = float(total_host.group(1))

    total_gpu = TOTAL_GPU_RE.search(text)
    if total_gpu:
        metrics["total_gpu_compute_s"] = float(total_gpu.group(1))

    for name, regex in SUMMARY_RE.items():
        match = regex.search(text)
        if not match:
            continue
        metrics[name] = {
            "min": parse_value_with_unit(match.group(1)),
            "max": parse_value_with_unit(match.group(2)),
            "mean": parse_value_with_unit(match.group(3)),
            "median": parse_value_with_unit(match.group(4)),
            "p90": parse_value_with_unit(match.group(5)),
            "p95": parse_value_with_unit(match.group(6)),
            "p99": parse_value_with_unit(match.group(7)),
        }
    return metrics


def parse_dims_token(raw: str) -> List[int]:
    token = raw.strip()
    if token == "" or token.lower() == "scalar":
        return []
    dims: List[int] = []
    for piece in token.split("x"):
        part = piece.strip()
        if part == "":
            raise ValueError(f"Invalid shape token: {raw}")
        try:
            dims.append(int(part))
        except ValueError as e:
            raise ValueError(f"Invalid shape token: {raw}") from e
    return dims


def dims_to_shape_spec(dims: List[int]) -> str:
    if not dims:
        return "scalar"
    return "x".join(str(v) for v in dims)


def probe_plan_input_tensors(
    trtexec_bin: Path,
    plan_path: str,
    env: Dict[str, str],
    timeout_sec: int,
    device: int,
) -> Dict[str, Dict[str, List[int]]]:
    cmd = [
        str(trtexec_bin.resolve()),
        f"--loadEngine={plan_path}",
        "--dumpOptimizationProfile",
        "--skipInference",
        f"--device={device}",
    ]
    rc, text = run_command(cmd, env=env, timeout_sec=timeout_sec)
    if rc != 0:
        raise RuntimeError(
            f"trtexec input-shape probe failed (exit={rc})\n{join_cmd(cmd)}\n--- output tail ---\n{output_tail(text)}"
        )

    tensors: Dict[str, Dict[str, List[int]]] = {}
    for match in PLAN_INPUT_RE.finditer(text):
        tensor_name = match.group(1).strip()
        min_dims = parse_dims_token(match.group(2))
        opt_dims = parse_dims_token(match.group(3))
        max_dims = parse_dims_token(match.group(4))
        tensors[tensor_name] = {
            "min": min_dims,
            "opt": opt_dims,
            "max": max_dims,
        }

    if not tensors:
        raise RuntimeError(
            "Failed to parse input shapes from trtexec output.\n"
            f"Command: {join_cmd(cmd)}\n--- output tail ---\n{output_tail(text)}"
        )
    return tensors


def build_shapes_for_batch(
    plan_tensors: Dict[str, Dict[str, List[int]]],
    batch_size: int,
) -> str:
    specs: List[str] = []
    for tensor_name, tensor_spec in plan_tensors.items():
        dims = list(tensor_spec.get("opt", []))
        if dims:
            dims[0] = batch_size
        specs.append(f"{tensor_name}:{dims_to_shape_spec(dims)}")
    if not specs:
        raise RuntimeError("Cannot construct --shapes: no input tensors found in plan")
    return ",".join(specs)


def output_tail(text: str, lines: int = 100) -> str:
    rows = text.splitlines()
    if len(rows) <= lines:
        return text
    return "\n".join(rows[-lines:])


def atomic_save_json(path: Path, data: Dict[str, object]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(path.suffix + ".tmp")
    with tmp.open("w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=2, sort_keys=True)
        f.write("\n")
    tmp.replace(path)


def load_or_init_state(path: Path, resume: bool, meta: Dict[str, object]) -> Dict[str, object]:
    if resume and path.exists():
        with path.open("r", encoding="utf-8") as f:
            state = json.load(f)
        state.setdefault("meta", {})
        state.setdefault("plans", {})
        state.setdefault("results", {})
        state["meta"]["last_resume_at"] = now_iso()
        return state
    return {"meta": meta, "plans": {}, "results": {}}


def build_env(tensorrt_lib: str) -> Dict[str, str]:
    env = os.environ.copy()
    if tensorrt_lib:
        existing = env.get("LD_LIBRARY_PATH", "")
        if existing:
            env["LD_LIBRARY_PATH"] = f"{tensorrt_lib}:{existing}"
        else:
            env["LD_LIBRARY_PATH"] = tensorrt_lib
    return env


def run_command(
    cmd: List[str],
    env: Dict[str, str],
    timeout_sec: int,
    input_text: Optional[str] = None,
) -> Tuple[int, str]:
    try:
        proc = subprocess.run(
            cmd,
            input=input_text,
            text=True,
            capture_output=True,
            env=env,
            timeout=timeout_sec,
            check=False,
        )
    except subprocess.TimeoutExpired as e:
        partial = (e.stdout or "") + ("\n" + e.stderr if e.stderr else "")
        raise RuntimeError(
            f"Command timeout after {timeout_sec}s\n{join_cmd(cmd)}\n--- output tail ---\n{output_tail(partial)}"
        ) from e
    combined = (proc.stdout or "") + ("\n" + proc.stderr if proc.stderr else "")
    return proc.returncode, combined


def render_progress(done: int, total: int, status: str) -> None:
    width = 36
    ratio = 1.0 if total == 0 else done / total
    filled = int(width * ratio)
    bar = "#" * filled + "-" * (width - filled)
    line = f"\r[{bar}] {done}/{total} {ratio * 100:6.2f}% {status[:100]}"
    sys.stdout.write(line)
    sys.stdout.flush()


def normalize_key_token(text: str) -> str:
    token = re.sub(r"[^A-Za-z0-9_.-]+", "_", text.strip())
    return token or "plan"


def append_gpu_token_to_path(path: Path, gpu_model_name: str) -> Path:
    gpu_token = f"gpu-{normalize_key_token(gpu_model_name)}"
    stem_normalized = normalize_key_token(path.stem).lower()
    if gpu_token.lower() in stem_normalized:
        return path
    return path.with_name(f"{path.stem}_{gpu_token}{path.suffix}")


def device_set_token(devices: List[int]) -> str:
    return "devs-" + "-".join(str(device) for device in devices)


def detect_gpu_model(device: int) -> str:
    cmd = ["nvidia-smi", "--query-gpu=name", "--format=csv,noheader", "-i", str(device)]
    try:
        proc = subprocess.run(
            cmd,
            text=True,
            capture_output=True,
            check=False,
        )
    except FileNotFoundError:
        return f"gpu{device}"
    if proc.returncode != 0:
        return f"gpu{device}"
    lines = [line.strip() for line in (proc.stdout or "").splitlines() if line.strip()]
    if not lines:
        return f"gpu{device}"
    return lines[0]


def build_shapes_from_templates(shape_templates: List[str], batch_size: int) -> str:
    return ",".join(template.replace("{batch}", str(batch_size)) for template in shape_templates)


def default_benchmark_output_path(
    model_basename: str,
    gpu_model_name: str,
    devices: List[int],
    build_count: int,
) -> Path:
    stem = (
        "trtexec_benchmark_"
        + normalize_key_token(model_basename)
        + "_"
        + device_set_token(devices)
        + f"_build{build_count}"
        + "_gpu-"
        + normalize_key_token(gpu_model_name)
    )
    return Path("benchmark") / f"{stem}.json"


def resolve_output_json_path(
    output_json: Optional[str],
    default_output_path: Path,
    gpu_model_name: str,
) -> Path:
    path = Path(output_json) if output_json else default_output_path
    return append_gpu_token_to_path(path, gpu_model_name)


def resolve_plot_path(
    plot_png: Optional[str],
    output_path: Path,
    gpu_model_name: str,
) -> Path:
    path = Path(plot_png) if plot_png else output_path.with_suffix(".png")
    return append_gpu_token_to_path(path, gpu_model_name)


def resolve_home_data_dir_base(
    home_data_dir_base_raw: Optional[str],
    output_path: Path,
) -> Path:
    path = Path(home_data_dir_base_raw).resolve() if home_data_dir_base_raw else output_path.with_suffix("") / "home_data"
    path.mkdir(parents=True, exist_ok=True)
    return path


def parse_devices_arg(args: argparse.Namespace) -> List[int]:
    raw_devices = list(args.devices or [str(args.default_device)])
    devices = [nonnegative_int(raw) for raw in raw_devices]
    if len(set(devices)) != len(devices):
        raise ValueError(
            "--devices contains duplicates. Logical multi-worker on one physical GPU is not implemented; "
            "use distinct device ids such as --devices 0,1."
        )
    return devices


def ensure_same_gpu_model(devices: List[int]) -> Dict[int, str]:
    gpu_models = {device: detect_gpu_model(device) for device in devices}
    unique_models = sorted(set(gpu_models.values()))
    if len(unique_models) > 1:
        raise ValueError(f"All devices must be the same GPU model, got {gpu_models}")
    return gpu_models


def home_data_dir_for_label(home_data_dir_base: Path, device: int, label: str) -> Path:
    return home_data_dir_base / f"device{device}" / label


def plan_state_key(plan_label: str, device: int) -> str:
    return f"{normalize_key_token(plan_label)}_dev{device}"


def plan_build_label(batch_size: int, build_index: int) -> str:
    return f"batch{batch_size}_build{build_index:04d}"


def collect_pending_streams(
    *,
    state: Dict[str, Any],
    plan_label: str,
    batch_size: int,
    device: int,
    stream_values: List[int],
    rerun_failed: bool,
) -> Tuple[List[int], int]:
    pending_streams: List[int] = []
    skipped = 0
    for streams in stream_values:
        existing = state["results"].get(case_key_with_plan(plan_label, batch_size, streams, device))
        if should_skip_result(existing, rerun_failed):
            skipped += 1
        else:
            pending_streams.append(streams)
    return pending_streams, skipped


def path_identity(path: str) -> Dict[str, object]:
    resolved = Path(path).resolve()
    stat = resolved.stat()
    return {
        "path": str(resolved),
        "size": int(stat.st_size),
        "mtime_ns": int(stat.st_mtime_ns),
    }


def compute_plan_build_fingerprint(
    args: argparse.Namespace,
    plan_batch: int,
    device: int,
) -> str:
    payload = {
        "katago_bin": path_identity(args.katago_bin),
        "config": path_identity(args.config),
        "model": path_identity(args.model),
        "plan_batch": int(plan_batch),
        "device": int(device),
        "gtp_extra_override": list(args.gtp_extra_override),
    }
    encoded = json.dumps(payload, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()


def build_plan_override_config(
    args: argparse.Namespace,
    plan_batch: int,
    device: int,
    home_data_dir: Path,
) -> str:
    override_parts = list(args.gtp_extra_override)
    override_parts.extend(
        [
            f"nnMaxBatchSize={plan_batch}",
            "numSearchThreads=1",
            "numNNServerThreadsPerModel=1",
            f"trtDeviceToUseThread0={device}",
            f"homeDataDir={home_data_dir.resolve()}",
            "ponderingEnabled=false",
            "logSearchInfo=false",
            "logAllGTPCommunication=false",
            "logToStderr=true",
        ]
    )
    return ",".join(override_parts)


def build_plan_once(
    plan_batch: int,
    device: int,
    args: argparse.Namespace,
    env: Dict[str, str],
    home_data_dir: Path,
) -> Dict[str, Any]:
    home_data_dir.mkdir(parents=True, exist_ok=True)
    cmd = [
        args.katago_bin,
        "gtp",
        "-config",
        args.config,
        "-model",
        args.model,
        "-override-config",
        build_plan_override_config(args, plan_batch, device, home_data_dir),
    ]
    retcode, text = run_command(cmd, env=env, timeout_sec=args.plan_timeout_sec, input_text="quit\n")
    if retcode != 0:
        raise RuntimeError(
            f"katago gtp failed for batch={plan_batch} device={device} (exit={retcode})\n"
            f"{join_cmd(cmd)}\n--- output tail ---\n{output_tail(text)}"
        )

    matches = PLAN_PATH_RE.findall(text)
    if not matches:
        raise RuntimeError(
            "Failed to parse plan path from katago output.\n"
            f"Command: {join_cmd(cmd)}\n--- output tail ---\n{output_tail(text)}"
        )
    plan_path = Path(matches[-1].strip()).expanduser()
    if not plan_path.exists():
        raise RuntimeError(f"Parsed plan path does not exist: {plan_path}")

    return {
        "plan_path": str(plan_path.resolve()),
        "command": join_cmd(cmd),
        "output_text": text,
        "output_tail": output_tail(text),
        "home_data_dir": str(home_data_dir.resolve()),
    }


def benchmark_case(
    *,
    trtexec_bin: Path,
    env: Dict[str, str],
    args: argparse.Namespace,
    device: int,
    plan_label: str,
    plan_path: str,
    batch_size: int,
    streams: int,
    shapes: str,
) -> Dict[str, Any]:
    cmd = [
        str(trtexec_bin.resolve()),
        f"--loadEngine={plan_path}",
        f"--shapes={shapes}",
        f"--duration={args.duration_sec}",
        f"--warmUp={args.warmup_ms}",
        f"--iterations={args.iterations}",
        f"--avgRuns={args.avg_runs}",
        f"--infStreams={streams}",
        f"--device={device}",
        "--useCudaGraph",
    ]
    cmd.extend(args.trtexec_extra_arg)

    started = time.time()
    started_at = now_iso()
    status = "ok"
    error_text = ""
    rc = -1
    raw = ""
    metrics: Dict[str, object] = {}
    try:
        rc, raw = run_command(cmd, env=env, timeout_sec=args.trtexec_timeout_sec)
        metrics = parse_trtexec_metrics(raw)
        if rc != 0 or "&&&& PASSED" not in raw:
            status = "error"
            error_text = f"trtexec failed or did not report PASSED (exit={rc})"
    except Exception as e:
        status = "error"
        error_text = str(e)

    return {
        "status": status,
        "error": error_text,
        "device": device,
        "plan_label": plan_label,
        "max_batch": args.max_batch,
        "batch_size": batch_size,
        "streams": streams,
        "plan_path": plan_path,
        "shapes": shapes,
        "command": join_cmd(cmd),
        "return_code": rc,
        "started_at": started_at,
        "ended_at": now_iso(),
        "elapsed_sec": time.time() - started,
        "metrics": metrics,
        "output_tail": output_tail(raw, lines=120),
    }


def case_key_with_plan(
    plan_label: str,
    batch_size: int,
    streams: int,
    device: int,
) -> str:
    return f"pl{normalize_key_token(plan_label)}_d{device}_b{batch_size}_s{streams}"


def extract_plot_value(metric_name: str, result: Dict[str, object]) -> Optional[float]:
    metrics = result.get("metrics", {})
    if not isinstance(metrics, dict):
        return None
    throughput = metrics.get("throughput_qps")
    if not isinstance(throughput, (int, float)):
        return None

    if metric_name == "throughput_qps":
        return float(throughput)
    if metric_name == "nn_evals_per_sec":
        batch_size = result.get("batch_size")
        if not isinstance(batch_size, int):
            return None
        return float(throughput) * float(batch_size)
    raise ValueError(f"Unsupported plot metric: {metric_name}")


def draw_plot(
    state: Dict[str, object],
    output_png: Path,
    metric_name: str,
    include_non_ok: bool,
    gpu_model_name: str,
    model_name: str,
) -> bool:
    try:
        import matplotlib

        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except Exception as e:
        print(f"[warn] Skip plotting: matplotlib unavailable ({e})")
        return False

    if metric_name == "throughput_qps":
        metric_label = "trtexec qps"
    elif metric_name == "nn_evals_per_sec":
        metric_label = "nnEval/s"
    else:
        metric_label = metric_name

    stream_series = collect_plot_series(
        state=state,
        metric_name=metric_name,
        include_non_ok=include_non_ok,
    )

    if not stream_series:
        print("[warn] Skip plotting: no usable benchmark points")
        return False

    fig, ax = plt.subplots(figsize=(11, 6))
    # Use a monotonic sequential palette so stream id maps to color in a logical order.
    color_map = plt.get_cmap("viridis")
    sorted_streams = sorted(stream_series.keys())
    min_stream = sorted_streams[0]
    max_stream = sorted_streams[-1]
    any_point = False
    for stream in sorted_streams:
        points = stream_series[stream]
        xs = sorted(points.keys())
        ys = [points[x] for x in xs]
        if not xs:
            continue
        any_point = True
        if max_stream == min_stream:
            color_pos = 0.5
        else:
            color_pos = (stream - min_stream) / float(max_stream - min_stream)
        ax.plot(
            xs,
            ys,
            marker="o",
            linewidth=2.0,
            markersize=4.5,
            color=color_map(color_pos),
            label=f"stream={stream}",
        )

    if not any_point:
        print("[warn] Skip plotting: no usable benchmark points")
        plt.close(fig)
        return False

    ax.set_xlabel("batch size")
    ax.set_ylabel(metric_label)
    ax.set_title(
        f"trtexec benchmark ({metric_label}) | model={model_name} | gpu={gpu_model_name}"
    )
    ax.grid(True, alpha=0.3)
    ax.legend(loc="best")
    fig.tight_layout()

    output_png.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output_png, dpi=160)
    plt.close(fig)
    return True


def maybe_refresh_plot(
    *,
    state: Dict[str, object],
    output_path: Path,
    output_png: Path,
    metric_name: str,
    include_non_ok: bool,
    gpu_model_name: str,
    model_name: str,
) -> None:
    plotted = draw_plot(
        state=state,
        output_png=output_png,
        metric_name=metric_name,
        include_non_ok=include_non_ok,
        gpu_model_name=gpu_model_name,
        model_name=model_name,
    )
    if plotted:
        state["meta"]["plot_metric"] = metric_name
        state["meta"]["plot_png"] = str(output_png.resolve())
        atomic_save_json(output_path, state)


def collect_plot_series(
    state: Dict[str, object],
    metric_name: str,
    include_non_ok: bool,
) -> Dict[int, Dict[int, float]]:
    results = state.get("results", {})
    if not isinstance(results, dict):
        return {}

    stream_series: Dict[int, Dict[int, float]] = {}
    for result in results.values():
        if not isinstance(result, dict):
            continue
        if not include_non_ok and result.get("status") != "ok":
            continue
        stream = result.get("streams")
        batch_size = result.get("batch_size")
        if not isinstance(stream, int) or not isinstance(batch_size, int):
            continue
        value = extract_plot_value(metric_name, result)
        if value is None:
            continue
        stream_series.setdefault(stream, {})[batch_size] = value
    return stream_series


def should_skip_result(existing: Optional[Dict[str, object]], rerun_failed: bool) -> bool:
    if not existing:
        return False
    status = existing.get("status")
    if status == "ok":
        return True
    if status == "error" and not rerun_failed:
        return True
    return False


def parse_plan_file_args(specs: Optional[List[str]]) -> List[Tuple[str, str]]:
    parsed: List[Tuple[str, str]] = []
    if not specs:
        return parsed

    for spec in specs:
        item = spec.strip()
        if not item:
            continue

        if "=" in item:
            label, raw_path = item.split("=", 1)
            label = label.strip()
            raw_path = raw_path.strip()
            if not raw_path:
                raise ValueError(f"Invalid --plan-file entry: {item}")
            if not label:
                label = Path(raw_path).name
        else:
            raw_path = item
            label = Path(raw_path).name

        plan_path = Path(raw_path).expanduser()
        if not plan_path.exists():
            raise FileNotFoundError(f"plan path not found: {plan_path}")
        parsed.append((label, str(plan_path.resolve())))

    next_suffix: Dict[str, int] = {}
    used_labels = set()
    unique: List[Tuple[str, str]] = []
    for label, path in parsed:
        if label not in used_labels:
            used_labels.add(label)
            next_suffix[label] = 1
            unique.append((label, path))
            continue

        suffix = next_suffix.get(label, 1)
        while True:
            candidate = f"{label}_{suffix}"
            suffix += 1
            if candidate not in used_labels:
                used_labels.add(candidate)
                next_suffix[label] = suffix
                unique.append((candidate, path))
                break

    return unique


def build_parser(
    *,
    katago_bin_default: str,
    trtexec_bin_default: str,
    config_default: str,
    model_default: str,
    tensorrt_lib_default: str,
    device_default: int,
) -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Generate single-profile TensorRT plans via katago, benchmark with trtexec, then plot.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    input_group = parser.add_argument_group("Inputs")
    sweep_group = parser.add_argument_group("Benchmark sweep")
    override_group = parser.add_argument_group("Overrides")
    output_group = parser.add_argument_group("Output and resume")
    plot_group = parser.add_argument_group("Plotting")

    input_group.add_argument(
        "--katago-bin",
        default=katago_bin_default,
    )
    input_group.add_argument(
        "--trtexec-bin",
        default=trtexec_bin_default,
    )
    input_group.add_argument(
        "--config",
        default=config_default,
    )
    input_group.add_argument(
        "--model",
        default=model_default,
    )
    input_group.add_argument("--tensorrt-lib", default=tensorrt_lib_default)

    sweep_group.add_argument("--max-batch", type=positive_int, default=32)
    sweep_group.add_argument("--plan-batch", type=positive_int, default=None, help=argparse.SUPPRESS)
    sweep_group.add_argument(
        "--plan-file",
        action="extend",
        nargs="+",
        default=None,
        help="Use existing plan file(s), format: /path/to/plan or label=/path/to/plan. Can be passed multiple times.",
    )
    sweep_group.add_argument("--batch-min", type=positive_int, default=1)
    sweep_group.add_argument("--batch-max", type=positive_int, default=None)
    sweep_group.add_argument("--stream-min", type=positive_int, default=1)
    sweep_group.add_argument("--stream-max", type=positive_int, default=4)
    sweep_group.add_argument(
        "--build-count",
        type=positive_int,
        default=argparse.SUPPRESS,
        help="Number of independently built plans per batch size.",
    )
    sweep_group.add_argument(
        "--simple-sampling-build-count",
        dest="build_count",
        type=positive_int,
        default=argparse.SUPPRESS,
        help=argparse.SUPPRESS,
    )
    sweep_group.add_argument(
        "--device",
        dest="devices",
        type=str,
        action="append",
        default=None,
        help="Use a single GPU device id. Can be repeated.",
    )
    sweep_group.add_argument(
        "--devices",
        dest="devices",
        type=str,
        nargs="+",
        action="extend",
        default=None,
        help="Use multiple GPU device ids.",
    )

    override_group.add_argument(
        "--shape-template",
        action="extend",
        nargs="+",
        default=None,
        help=(
            "Manual override for input shape template(s) with {batch}, e.g. "
            "InputSpatial:{batch}x22x19x19. If omitted, probe input names/shapes from plan via trtexec."
        ),
    )
    override_group.add_argument("--duration-sec", type=positive_float, default=3.0)
    override_group.add_argument("--warmup-ms", type=nonnegative_int, default=200)
    override_group.add_argument("--iterations", type=positive_int, default=20)
    override_group.add_argument("--avg-runs", type=positive_int, default=10)
    override_group.add_argument("--plan-timeout-sec", type=positive_int, default=1800)
    override_group.add_argument("--trtexec-timeout-sec", type=positive_int, default=600)
    override_group.add_argument("--gtp-extra-override", action="extend", nargs="+", default=[])
    override_group.add_argument("--trtexec-extra-arg", action="extend", nargs="+", default=[])

    output_group.add_argument("--output-json", default=None)
    output_group.add_argument(
        "--home-data-dir-base",
        default=None,
        help="Base directory for isolated KataGo homeDataDir trees used by benchmark.py.",
    )
    output_group.add_argument("--no-resume", action="store_true")
    output_group.add_argument("--rerun-failed", action="store_true")
    output_group.add_argument("--stop-on-error", action="store_true")

    plot_group.add_argument("--plot", action=argparse.BooleanOptionalAction, default=True)
    plot_group.add_argument("--plot-png", default=None)
    plot_group.add_argument(
        "--plot-metric",
        default="nn_evals_per_sec",
        choices=["nn_evals_per_sec", "throughput_qps"],
    )
    plot_group.add_argument(
        "--plot-include-non-ok",
        action=argparse.BooleanOptionalAction,
        default=False,
    )
    parser.add_argument("--smoke", action="store_true", help="Quick smoke mode")
    parser.set_defaults(default_device=device_default, build_count=1)
    return parser


def apply_smoke_overrides(args: argparse.Namespace) -> None:
    args.max_batch = 1
    args.batch_min = 1
    args.batch_max = 1
    args.stream_min = 1
    args.stream_max = 1
    args.duration_sec = min(args.duration_sec, 1.0)
    args.warmup_ms = 0
    args.iterations = min(args.iterations, 20)
    args.avg_runs = min(args.avg_runs, 10)


def validate_args(args: argparse.Namespace) -> None:
    if args.stream_max < args.stream_min:
        raise ValueError("Invalid stream range")
    if args.batch_max is not None and args.batch_max < args.batch_min:
        raise ValueError("batch-max must be >= batch-min")
    if args.batch_min > args.max_batch:
        raise ValueError("batch-min cannot exceed max-batch")
    if args.plan_file and args.build_count != 1:
        raise ValueError("--build-count cannot be used with --plan-file")


def build_standard_state_meta(
    *,
    args: argparse.Namespace,
    katago_bin: Path,
    trtexec_bin: Path,
    config: Path,
    model: Path,
    batch_max: int,
    shape_templates: List[str],
    model_basename: str,
    gpu_model_name: str,
    devices: List[int],
    gpu_models: Dict[int, str],
    home_data_dir_base: Path,
    parsed_plan_files: List[Tuple[str, str]],
) -> Dict[str, Any]:
    return {
        "created_at": now_iso(),
        "cmdline": sys.argv,
        "katago_bin": str(katago_bin.resolve()) if katago_bin.exists() else str(katago_bin),
        "trtexec_bin": str(trtexec_bin.resolve()),
        "config": str(config.resolve()) if config.exists() else str(config),
        "model": str(model.resolve()) if model.exists() else str(model),
        "max_batch": args.max_batch,
        "build_count": args.build_count,
        "batch_range": [args.batch_min, batch_max],
        "stream_range": [args.stream_min, args.stream_max],
        "shape_mode": "manual-template" if shape_templates else "trtexec-probe",
        "shape_templates": shape_templates,
        "model_filename": model_basename,
        "gpu_model": gpu_model_name,
        "devices": devices,
        "gpu_models": gpu_models,
        "home_data_dir_base": str(home_data_dir_base.resolve()),
        "plan_files": [{"label": label, "path": path} for label, path in parsed_plan_files],
    }


def build_standard_group_candidates(
    *,
    args: argparse.Namespace,
    state: Dict[str, Any],
    parsed_plan_files: List[Tuple[str, str]],
    batch_values: List[int],
    devices: List[int],
    home_data_dir_base: Path,
) -> List[Dict[str, Any]]:
    candidates: List[Dict[str, Any]] = []
    group_index = 0
    use_existing_plan_files = len(parsed_plan_files) > 0

    if use_existing_plan_files:
        for plan_label, plan_path in parsed_plan_files:
            for batch_size in batch_values:
                device = devices[group_index % len(devices)]
                candidates.append(
                    {
                        "plan_label": plan_label,
                        "plan_path": plan_path,
                        "batch_size": batch_size,
                        "device": device,
                    }
                )
                group_index += 1
    else:
        plans = state.setdefault("plans", {})
        if not isinstance(plans, dict):
            state["plans"] = {}
            plans = state["plans"]
        for batch_size in batch_values:
            for build_index in range(1, args.build_count + 1):
                device = devices[group_index % len(devices)]
                plan_label = plan_build_label(batch_size, build_index)
                plan_key = plan_state_key(plan_label, device)
                build_fingerprint = compute_plan_build_fingerprint(args, batch_size, device)
                existing_plan = plans.get(plan_key)
                plan_path = None
                if isinstance(existing_plan, dict):
                    cached_path = existing_plan.get("path")
                    cached_fingerprint = existing_plan.get("build_fingerprint")
                    if (
                        isinstance(cached_path, str)
                        and Path(cached_path).exists()
                        and cached_fingerprint == build_fingerprint
                    ):
                        plan_path = str(Path(cached_path).resolve())
                candidates.append(
                    {
                        "plan_label": plan_label,
                        "plan_key": plan_key,
                        "plan_path": plan_path,
                        "build_fingerprint": build_fingerprint,
                        "batch_size": batch_size,
                        "build_index": build_index,
                        "device": device,
                        "home_data_dir": str(home_data_dir_for_label(home_data_dir_base, device, plan_label).resolve()),
                    }
                )
                group_index += 1
    return candidates


def prepare_standard_groups(
    *,
    args: argparse.Namespace,
    state: Dict[str, Any],
    parsed_plan_files: List[Tuple[str, str]],
    batch_values: List[int],
    stream_values: List[int],
    devices: List[int],
    home_data_dir_base: Path,
) -> Tuple[List[Dict[str, Any]], int, int]:
    candidates = build_standard_group_candidates(
        args=args,
        state=state,
        parsed_plan_files=parsed_plan_files,
        batch_values=batch_values,
        devices=devices,
        home_data_dir_base=home_data_dir_base,
    )

    groups: List[Dict[str, Any]] = []
    skipped = 0
    for candidate in candidates:
        pending_streams, skipped_here = collect_pending_streams(
            state=state,
            plan_label=str(candidate["plan_label"]),
            batch_size=int(candidate["batch_size"]),
            device=int(candidate["device"]),
            stream_values=stream_values,
            rerun_failed=args.rerun_failed,
        )
        skipped += skipped_here
        if pending_streams:
            group = dict(candidate)
            group["pending_streams"] = pending_streams
            groups.append(group)

    total = len(candidates) * len(stream_values)
    return groups, skipped, total


def log_benchmark_case_error(key: str, result: Dict[str, Any]) -> None:
    print("\n[error] benchmark case failed, stop immediately", file=sys.stderr)
    print(f"[error] case={key}", file=sys.stderr)
    print(f"[error] command={result['command']}", file=sys.stderr)
    if result["error"]:
        print(f"[error] reason={result['error']}", file=sys.stderr)
    if result["output_tail"]:
        print("[error] trtexec output tail:", file=sys.stderr)
        print(result["output_tail"], file=sys.stderr)


def execute_standard_groups(
    *,
    args: argparse.Namespace,
    env: Dict[str, str],
    trtexec_bin: Path,
    shape_templates: List[str],
    parsed_plan_files: List[Tuple[str, str]],
    devices: List[int],
    groups: List[Dict[str, Any]],
    state: Dict[str, Any],
    output_path: Path,
    plot_path: Path,
    total: int,
    skipped: int,
    gpu_model_name: str,
    model_name_for_title: str,
) -> Tuple[int, int, List[str]]:
    groups_by_device: Dict[int, List[Dict[str, Any]]] = {device: [] for device in devices}
    for group in groups:
        groups_by_device[group["device"]].append(group)

    event_queue: Queue[Dict[str, Any]] = Queue()
    stop_event = threading.Event()
    active_devices = [device for device, device_groups in groups_by_device.items() if device_groups]
    use_existing_plan_files = len(parsed_plan_files) > 0

    def worker(device: int, device_groups: List[Dict[str, Any]]) -> None:
        try:
            for group in device_groups:
                if stop_event.is_set():
                    break

                plan_label = str(group["plan_label"])
                batch_size = int(group["batch_size"])
                plan_path = group.get("plan_path")
                if not isinstance(plan_path, str) or not Path(plan_path).exists():
                    if use_existing_plan_files:
                        raise RuntimeError(f"Missing existing plan file for {plan_label} batch={batch_size}")
                    build_info = build_plan_once(
                        plan_batch=batch_size,
                        device=device,
                        args=args,
                        env=env,
                        home_data_dir=Path(str(group["home_data_dir"])),
                    )
                    plan_path = str(build_info["plan_path"])
                    event_queue.put(
                        {
                            "type": "plan",
                            "plan_key": str(group["plan_key"]),
                            "plan_entry": {
                                "path": plan_path,
                                "updated_at": now_iso(),
                                "source": "katago-gtp",
                                "device": device,
                                "build_fingerprint": str(group["build_fingerprint"]),
                                "home_data_dir": str(group["home_data_dir"]),
                                "build_command": str(build_info["command"]),
                            },
                        }
                    )

                plan_inputs = None
                if not shape_templates:
                    plan_inputs = probe_plan_input_tensors(
                        trtexec_bin=trtexec_bin,
                        plan_path=plan_path,
                        env=env,
                        timeout_sec=args.trtexec_timeout_sec,
                        device=device,
                    )
                for streams in group["pending_streams"]:
                    if stop_event.is_set():
                        break
                    shapes = (
                        build_shapes_from_templates(shape_templates, batch_size)
                        if shape_templates else
                        build_shapes_for_batch(plan_inputs or {}, batch_size)
                    )
                    result = benchmark_case(
                        trtexec_bin=trtexec_bin,
                        env=env,
                        args=args,
                        device=device,
                        plan_label=plan_label,
                        plan_path=plan_path,
                        batch_size=batch_size,
                        streams=int(streams),
                        shapes=shapes,
                    )
                    event_queue.put(
                        {
                            "type": "result",
                            "key": case_key_with_plan(plan_label, batch_size, int(streams), device),
                            "result": result,
                        }
                    )
                    if result["status"] != "ok":
                        stop_event.set()
                        break
        except Exception as e:
            stop_event.set()
            event_queue.put({"type": "worker_exception", "device": device, "error": str(e)})
        finally:
            event_queue.put({"type": "worker_done", "device": device})

    done = skipped
    ok_count = 0
    err_count = 0
    worker_errors: List[str] = []
    finished_workers = 0
    with ThreadPoolExecutor(max_workers=max(1, len(active_devices))) as executor:
        futures = [
            executor.submit(worker, device, device_groups)
            for device, device_groups in groups_by_device.items()
            if device_groups
        ]
        while finished_workers < len(active_devices):
            event = event_queue.get()
            event_type = event["type"]
            if event_type == "plan":
                state["plans"][str(event["plan_key"])] = event["plan_entry"]
                atomic_save_json(output_path, state)
            elif event_type == "result":
                result = event["result"]
                key = str(event["key"])
                state["results"][key] = result
                atomic_save_json(output_path, state)
                if args.plot and (result["status"] == "ok" or args.plot_include_non_ok):
                    maybe_refresh_plot(
                        state=state,
                        output_path=output_path,
                        output_png=plot_path,
                        metric_name=args.plot_metric,
                        include_non_ok=args.plot_include_non_ok,
                        gpu_model_name=gpu_model_name,
                        model_name=model_name_for_title,
                    )

                done += 1
                if result["status"] == "ok":
                    ok_count += 1
                else:
                    err_count += 1
                    log_benchmark_case_error(key, result)
                render_progress(
                    done,
                    total,
                    (
                        f"plan={normalize_key_token(str(result['plan_label']))} "
                        f"dev={result['device']} "
                        f"batch={result['batch_size']} s={result['streams']} {result['status']}"
                    ),
                )
            elif event_type == "worker_exception":
                worker_errors.append(f"device {event['device']}: {event['error']}")
            elif event_type == "worker_done":
                finished_workers += 1
        for future in futures:
            future.result()

    if args.plot and done == skipped:
        maybe_refresh_plot(
            state=state,
            output_path=output_path,
            output_png=plot_path,
            metric_name=args.plot_metric,
            include_non_ok=args.plot_include_non_ok,
            gpu_model_name=gpu_model_name,
            model_name=model_name_for_title,
        )
    return ok_count, err_count, worker_errors


def run_standard_benchmark_mode(
    *,
    args: argparse.Namespace,
    katago_bin: Path,
    trtexec_bin: Path,
    config: Path,
    model: Path,
    env: Dict[str, str],
    shape_templates: List[str],
    parsed_plan_files: List[Tuple[str, str]],
    devices: List[int],
    gpu_models: Dict[int, str],
) -> int:
    gpu_model_name = next(iter(gpu_models.values()))
    stream_values = list(range(args.stream_min, args.stream_max + 1))
    batch_max = args.max_batch if args.batch_max is None else min(args.max_batch, args.batch_max)
    if args.batch_min > batch_max:
        raise ValueError("No batch to benchmark after applying batch range and max-batch")
    batch_values = list(range(args.batch_min, batch_max + 1))

    model_basename = Path(args.model).name
    model_name_for_title = Path(args.model).stem
    output_path = resolve_output_json_path(
        args.output_json,
        default_benchmark_output_path(model_basename, gpu_model_name, devices, args.build_count),
        gpu_model_name,
    )
    plot_path = resolve_plot_path(args.plot_png, output_path, gpu_model_name)

    use_default_output_name = args.output_json is None
    if use_default_output_name:
        output_path.unlink(missing_ok=True)
        plot_path.unlink(missing_ok=True)

    home_data_dir_base = resolve_home_data_dir_base(args.home_data_dir_base, output_path)
    meta = build_standard_state_meta(
        args=args,
        katago_bin=katago_bin,
        trtexec_bin=trtexec_bin,
        config=config,
        model=model,
        batch_max=batch_max,
        shape_templates=shape_templates,
        model_basename=model_basename,
        gpu_model_name=gpu_model_name,
        devices=devices,
        gpu_models=gpu_models,
        home_data_dir_base=home_data_dir_base,
        parsed_plan_files=parsed_plan_files,
    )
    state = load_or_init_state(
        output_path,
        resume=(not args.no_resume and not use_default_output_name),
        meta=meta,
    )
    state["meta"].update(
        {
            "shape_mode": "manual-template" if shape_templates else "trtexec-probe",
            "shape_templates": shape_templates,
            "last_run_at": now_iso(),
        }
    )
    atomic_save_json(output_path, state)

    groups, skipped, total = prepare_standard_groups(
        args=args,
        state=state,
        parsed_plan_files=parsed_plan_files,
        batch_values=batch_values,
        stream_values=stream_values,
        devices=devices,
        home_data_dir_base=home_data_dir_base,
    )
    render_progress(skipped, total, f"resume skip={skipped}")

    ok_count, err_count, worker_errors = execute_standard_groups(
        args=args,
        env=env,
        trtexec_bin=trtexec_bin,
        shape_templates=shape_templates,
        parsed_plan_files=parsed_plan_files,
        devices=devices,
        groups=groups,
        state=state,
        output_path=output_path,
        plot_path=plot_path,
        total=total,
        skipped=skipped,
        gpu_model_name=gpu_model_name,
        model_name_for_title=model_name_for_title,
    )

    print()
    print(
        f"Done. build_count={args.build_count} total={total} skipped={skipped} ok={ok_count} error={err_count} "
        f"output={output_path.resolve()}"
    )
    if worker_errors:
        for row in worker_errors:
            print(f"[error] {row}", file=sys.stderr)
    return 0 if err_count == 0 and not worker_errors else 1


def main() -> int:
    script_dir = Path(__file__).resolve().parent
    env_sh_path = script_dir.parent / "env.sh"
    env_defaults = load_env_sh_defaults(env_sh_path)

    tensorrt_root_default = env_defaults.get("TENSORRT_ROOT", "/opt/tensorrt")
    katago_bin_default = env_defaults.get(
        "KATAGO_BIN_PATH",
        detect_default_path(["build/katago", "/opt/katago/katago"]),
    )
    trtexec_bin_default = detect_default_path(
        [str(Path(tensorrt_root_default) / "bin" / "trtexec"), "/opt/tensorrt/bin/trtexec"]
    )
    config_default = env_defaults.get(
        "KATAGO_CONFIG_PATH",
        detect_default_path(
            ["/opt/katago/config/gtp_example.cfg", "/opt/katago/configs/gtp_example.cfg", "cpp/tests/data/configs/analysis_example.cfg"]
        ),
    )
    model_default = env_defaults.get(
        "KATAGO_MODEL_PATH",
        detect_default_path(["/opt/katago/weights/b18tf.onnx", "/opt/katago/weight/b18tf.onnx"]),
    )
    tensorrt_lib_default = str(Path(tensorrt_root_default) / "lib")
    device_default_raw = env_defaults.get("TRT_DEVICE_ID", "0")
    try:
        device_default = nonnegative_int(device_default_raw)
    except argparse.ArgumentTypeError:
        device_default = 0

    parser = build_parser(
        katago_bin_default=katago_bin_default,
        trtexec_bin_default=trtexec_bin_default,
        config_default=config_default,
        model_default=model_default,
        tensorrt_lib_default=tensorrt_lib_default,
        device_default=device_default,
    )
    args = parser.parse_args()
    normalize_list_args(args)

    if args.plan_batch is not None:
        args.max_batch = args.plan_batch

    if args.smoke:
        apply_smoke_overrides(args)

    validate_args(args)

    parsed_plan_files = parse_plan_file_args(args.plan_file)
    use_existing_plan_files = len(parsed_plan_files) > 0

    katago_bin = Path(args.katago_bin)
    trtexec_bin = Path(args.trtexec_bin)
    config = Path(args.config)
    model = Path(args.model)
    if not trtexec_bin.exists():
        raise FileNotFoundError(f"trtexec path not found: {trtexec_bin}")
    if not use_existing_plan_files:
        for path_obj, label in [(katago_bin, "katago"), (config, "config"), (model, "model")]:
            if not path_obj.exists():
                raise FileNotFoundError(f"{label} path not found: {path_obj}")

    shape_templates = list(args.shape_template or [])
    devices = parse_devices_arg(args)
    gpu_models = ensure_same_gpu_model(devices)

    return run_standard_benchmark_mode(
        args=args,
        katago_bin=katago_bin,
        trtexec_bin=trtexec_bin,
        config=config,
        model=model,
        env=build_env(args.tensorrt_lib),
        shape_templates=shape_templates,
        parsed_plan_files=parsed_plan_files,
        devices=devices,
        gpu_models=gpu_models,
    )


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as e:
        print(f"[error] {e}", file=sys.stderr)
        sys.exit(1)
