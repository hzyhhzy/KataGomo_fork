#!/usr/bin/env python3

import argparse
import datetime as dt
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import time
from typing import Dict, List, Optional, Tuple


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


def split_csv(values: Optional[List[str]]) -> List[str]:
    if not values:
        return []
    out: List[str] = []
    for item in values:
        for piece in item.split(","):
            piece = piece.strip()
            if piece:
                out.append(piece)
    return out


def parse_positive_int_csv(spec: str, label: str) -> List[int]:
    values: List[int] = []
    for token in split_csv([spec]):
        try:
            num = int(token)
        except ValueError as e:
            raise ValueError(f"Invalid integer in {label}: {token}") from e
        if num <= 0:
            raise ValueError(f"{label} must contain positive integers, got: {num}")
        values.append(num)
    dedup: List[int] = []
    for value in values:
        if value not in dedup:
            dedup.append(value)
    if not dedup:
        raise ValueError(f"{label} cannot be empty")
    return dedup


def parse_nonnegative_int_csv(spec: str, label: str) -> List[int]:
    values: List[int] = []
    for token in split_csv([spec]):
        try:
            num = int(token)
        except ValueError as e:
            raise ValueError(f"Invalid integer in {label}: {token}") from e
        if num < 0:
            raise ValueError(f"{label} must contain non-negative integers, got: {num}")
        values.append(num)
    dedup: List[int] = []
    for value in values:
        if value not in dedup:
            dedup.append(value)
    if not dedup:
        raise ValueError(f"{label} cannot be empty")
    return dedup


def parse_value_with_unit(raw: str) -> Dict[str, object]:
    token = raw.strip()
    match = re.match(r"^([0-9eE+\-.]+)\s*([A-Za-z%/]+)?$", token)
    if not match:
        return {"text": token}
    value = float(match.group(1))
    unit = match.group(2) or ""
    return {"value": value, "unit": unit, "text": token}


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


def case_key(batch_size: int, streams: int) -> str:
    return f"b{batch_size}_s{streams}"


def normalize_key_token(text: str) -> str:
    token = re.sub(r"[^A-Za-z0-9_.-]+", "_", text.strip())
    return token or "plan"


def append_gpu_token_to_path(path: Path, gpu_model_name: str) -> Path:
    gpu_token = f"gpu-{normalize_key_token(gpu_model_name)}"
    stem_normalized = normalize_key_token(path.stem).lower()
    if gpu_token.lower() in stem_normalized:
        return path
    return path.with_name(f"{path.stem}_{gpu_token}{path.suffix}")


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


def case_key_with_plan(
    plan_label: str,
    batch_size: int,
    streams: int,
) -> str:
    return f"pl{normalize_key_token(plan_label)}_{case_key(batch_size, streams)}"


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


def plot_metric_label(metric_name: str) -> str:
    if metric_name == "throughput_qps":
        return "trtexec qps"
    if metric_name == "nn_evals_per_sec":
        return "nnEval/s"
    return metric_name


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
    ax.set_ylabel(plot_metric_label(metric_name))
    ax.set_title(
        f"trtexec benchmark ({plot_metric_label(metric_name)}) | model={model_name} | gpu={gpu_model_name}"
    )
    ax.grid(True, alpha=0.3)
    ax.legend(loc="best")
    fig.tight_layout()

    output_png.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output_png, dpi=160)
    plt.close(fig)
    return True


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


def count_plot_points(
    state: Dict[str, object],
    metric_name: str,
    include_non_ok: bool,
) -> int:
    stream_series = collect_plot_series(
        state=state,
        metric_name=metric_name,
        include_non_ok=include_non_ok,
    )
    return sum(len(points) for points in stream_series.values())


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
        for token in split_csv([spec]):
            item = token.strip()
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

    dedup_labels: Dict[str, int] = {}
    unique: List[Tuple[str, str]] = []
    for label, path in parsed:
        base = label
        suffix = dedup_labels.get(base, 0)
        if suffix == 0 and base not in dedup_labels:
            dedup_labels[base] = 1
            unique.append((base, path))
            continue

        while True:
            suffix += 1
            candidate = f"{base}_{suffix}"
            if candidate not in dedup_labels:
                dedup_labels[base] = suffix
                dedup_labels[candidate] = 1
                unique.append((candidate, path))
                break

    return unique


def ensure_plan(
    plan_batch: int,
    args: argparse.Namespace,
    state: Dict[str, object],
    output_path: Path,
    env: Dict[str, str],
) -> str:
    plans = state["plans"]
    existing_entry = plans.get(str(plan_batch))
    if isinstance(existing_entry, dict):
        cached_path = existing_entry.get("path", "")
        if cached_path and Path(cached_path).exists():
            return str(Path(cached_path).resolve())

    print(f"\n[plan] ensure batch={plan_batch}")
    override_parts = [
        f"nnMaxBatchSize={plan_batch}",
        "numSearchThreads=1",
        "numNNServerThreadsPerModel=1",
        f"trtDeviceToUseThread0={args.device}",
        "ponderingEnabled=false",
        "logSearchInfo=false",
        "logAllGTPCommunication=false",
        "logToStderr=true",
    ]
    override_parts.extend(split_csv(args.gtp_extra_override))
    override_str = ",".join(override_parts)

    cmd = [
        args.katago_bin,
        "gtp",
        "-config",
        args.config,
        "-model",
        args.model,
        "-override-config",
        override_str,
    ]
    retcode, text = run_command(cmd, env=env, timeout_sec=args.plan_timeout_sec, input_text="quit\n")
    if retcode != 0:
        raise RuntimeError(
            f"katago gtp failed for batch={plan_batch} (exit={retcode})\n{join_cmd(cmd)}\n--- output tail ---\n{output_tail(text)}"
        )

    matches = PLAN_PATH_RE.findall(text)
    if not matches:
        raise RuntimeError(
            "Failed to parse plan path from katago output.\n"
            f"Command: {join_cmd(cmd)}\n--- output tail ---\n{output_tail(text)}"
        )
    plan_path = matches[-1].strip()
    plan_file = Path(plan_path).expanduser()
    if not plan_file.exists():
        raise RuntimeError(f"Parsed plan path does not exist: {plan_file}")

    plans[str(plan_batch)] = {
        "path": str(plan_file.resolve()),
        "updated_at": now_iso(),
        "source": "katago-gtp",
    }
    atomic_save_json(output_path, state)
    return str(plan_file.resolve())


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
        device_default = parse_nonnegative_int_csv(device_default_raw, "TRT_DEVICE_ID")[0]
    except ValueError:
        device_default = 0

    parser = argparse.ArgumentParser(
        description="Generate single-profile TensorRT plans via katago, benchmark with trtexec, then plot.",
    )
    parser.add_argument(
        "--katago-bin",
        default=katago_bin_default,
    )
    parser.add_argument(
        "--trtexec-bin",
        default=trtexec_bin_default,
    )
    parser.add_argument(
        "--config",
        default=config_default,
    )
    parser.add_argument(
        "--model",
        default=model_default,
    )
    parser.add_argument("--output-json", default=None)
    parser.add_argument("--tensorrt-lib", default=tensorrt_lib_default)
    parser.add_argument("--max-batch", type=int, default=32)
    parser.add_argument("--plan-batch", type=int, default=None, help=argparse.SUPPRESS)
    parser.add_argument(
        "--plan-file",
        action="append",
        default=[],
        help="Use existing plan file(s), format: /path/to/plan or label=/path/to/plan. Can be passed multiple times.",
    )
    parser.add_argument("--batch-min", type=int, default=1)
    parser.add_argument("--batch-max", type=int, default=None)
    parser.add_argument("--stream-min", type=int, default=1)
    parser.add_argument("--stream-max", type=int, default=4)
    parser.add_argument(
        "--shape-template",
        action="append",
        default=None,
        help=(
            "Manual override for input shape template(s) with {batch}, e.g. "
            "InputSpatial:{batch}x22x19x19. If omitted, probe input names/shapes from plan via trtexec."
        ),
    )
    parser.add_argument("--duration-sec", type=float, default=3.0)
    parser.add_argument("--warmup-ms", type=int, default=200)
    parser.add_argument("--iterations", type=int, default=20)
    parser.add_argument("--avg-runs", type=int, default=10)
    parser.add_argument("--device", type=int, default=device_default)
    parser.add_argument("--plan-timeout-sec", type=int, default=1800)
    parser.add_argument("--trtexec-timeout-sec", type=int, default=600)
    parser.add_argument("--gtp-extra-override", action="append", default=[])
    parser.add_argument("--trtexec-extra-arg", action="append", default=[])
    parser.add_argument("--no-resume", action="store_true")
    parser.add_argument("--rerun-failed", action="store_true")
    parser.add_argument("--stop-on-error", action="store_true")
    parser.add_argument("--no-plot", action="store_true")
    parser.add_argument("--plot-png", default=None)
    parser.add_argument(
        "--plot-metric",
        default="nn_evals_per_sec",
        choices=["nn_evals_per_sec", "throughput_qps"],
    )
    parser.add_argument("--plot-include-non-ok", action="store_true")
    parser.add_argument("--smoke", action="store_true", help="Quick smoke mode")
    args = parser.parse_args()

    if args.plan_batch is not None:
        args.max_batch = args.plan_batch

    if args.smoke:
        args.max_batch = 1
        args.batch_min = 1
        args.batch_max = 1
        args.stream_min = 1
        args.stream_max = 1
        args.duration_sec = min(args.duration_sec, 1.0)
        args.warmup_ms = 0
        args.iterations = min(args.iterations, 20)
        args.avg_runs = min(args.avg_runs, 10)

    if args.max_batch <= 0:
        raise ValueError("max-batch must be positive")
    if args.batch_min <= 0:
        raise ValueError("batch-min must be positive")
    if args.stream_min <= 0 or args.stream_max < args.stream_min:
        raise ValueError("Invalid stream range")
    if args.batch_max is not None and args.batch_max <= 0:
        raise ValueError("batch-max must be positive")
    if args.batch_max is not None and args.batch_max < args.batch_min:
        raise ValueError("batch-max must be >= batch-min")
    if args.batch_min > args.max_batch:
        raise ValueError("batch-min cannot exceed max-batch")

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

    shape_templates = split_csv(args.shape_template)

    stream_values = list(range(args.stream_min, args.stream_max + 1))
    batch_max = args.max_batch if args.batch_max is None else min(args.max_batch, args.batch_max)
    if args.batch_min > batch_max:
        raise ValueError("No batch to benchmark after applying batch range and max-batch")
    batch_values = list(range(args.batch_min, batch_max + 1))
    env = build_env(args.tensorrt_lib)
    model_basename = Path(args.model).name
    model_name_for_title = Path(args.model).stem
    gpu_model_name = detect_gpu_model(args.device)
    default_stem = (
        "trtexec_benchmark_"
        + normalize_key_token(model_basename)
        + "_gpu-"
        + normalize_key_token(gpu_model_name)
    )

    if args.output_json:
        output_path = Path(args.output_json)
    else:
        output_path = Path("benchmark") / f"{default_stem}.json"
    output_path = append_gpu_token_to_path(output_path, gpu_model_name)

    if args.plot_png:
        plot_path = Path(args.plot_png)
    else:
        plot_path = output_path.with_suffix(".png")
    plot_path = append_gpu_token_to_path(plot_path, gpu_model_name)

    # For default output naming, keep a stable filename and overwrite previous runs.
    use_default_output_name = args.output_json is None
    if use_default_output_name:
        output_path.unlink(missing_ok=True)
        plot_path.unlink(missing_ok=True)

    meta = {
        "created_at": now_iso(),
        "cmdline": sys.argv,
        "katago_bin": str(katago_bin.resolve()) if katago_bin.exists() else str(katago_bin),
        "trtexec_bin": str(trtexec_bin.resolve()),
        "config": str(config.resolve()) if config.exists() else str(config),
        "model": str(model.resolve()) if model.exists() else str(model),
        "max_batch": args.max_batch,
        "batch_range": [args.batch_min, batch_max],
        "stream_range": [args.stream_min, args.stream_max],
        "plan_mode": "single-plan-per-batch",
        "shape_mode": "manual-template" if shape_templates else "trtexec-probe",
        "shape_templates": shape_templates,
        "model_filename": model_basename,
        "gpu_model": gpu_model_name,
        "plan_files": [{"label": label, "path": path} for label, path in parsed_plan_files],
    }
    state = load_or_init_state(
        output_path,
        resume=(not args.no_resume and not use_default_output_name),
        meta=meta,
    )
    state["meta"]["shape_mode"] = "manual-template" if shape_templates else "trtexec-probe"
    state["meta"]["shape_templates"] = shape_templates
    state["meta"]["last_run_at"] = now_iso()
    atomic_save_json(output_path, state)

    probed_plan_inputs: Dict[str, Dict[str, Dict[str, List[int]]]] = {}
    if use_existing_plan_files:
        total = len(parsed_plan_files) * len(batch_values) * len(stream_values)
    else:
        total = len(batch_values) * len(stream_values)

    skipped = 0
    if use_existing_plan_files:
        for plan_label, _plan_path in parsed_plan_files:
            for batch_size in batch_values:
                for streams in stream_values:
                    existing = state["results"].get(case_key_with_plan(plan_label, batch_size, streams))
                    if should_skip_result(existing, args.rerun_failed):
                        skipped += 1
    else:
        for batch_size in batch_values:
            plan_label = f"batch{batch_size}"
            for streams in stream_values:
                existing = state["results"].get(case_key_with_plan(plan_label, batch_size, streams))
                if should_skip_result(existing, args.rerun_failed):
                    skipped += 1

    done = skipped
    render_progress(done, total, f"resume skip={skipped}")

    ok_count = 0
    err_count = 0

    def run_single_case(plan_label: str, plan_path: str, batch_size: int, streams: int) -> bool:
        nonlocal done, ok_count, err_count

        key = case_key_with_plan(plan_label, batch_size, streams)
        existing = state["results"].get(key)
        if should_skip_result(existing, args.rerun_failed):
            if existing.get("status") == "ok":
                ok_count += 1
            elif existing.get("status") == "error":
                err_count += 1
            return False

        if shape_templates:
            shapes = ",".join(t.replace("{batch}", str(batch_size)) for t in shape_templates)
        else:
            plan_inputs = probed_plan_inputs.get(plan_path)
            if not plan_inputs:
                raise RuntimeError(f"Missing probed input info for plan: {plan_path}")
            shapes = build_shapes_for_batch(plan_inputs, batch_size)

        cmd = [
            str(trtexec_bin.resolve()),
            f"--loadEngine={plan_path}",
            f"--shapes={shapes}",
            f"--duration={args.duration_sec}",
            f"--warmUp={args.warmup_ms}",
            f"--iterations={args.iterations}",
            f"--avgRuns={args.avg_runs}",
            f"--infStreams={streams}",
            f"--device={args.device}",
            "--useCudaGraph",
        ]
        cmd.extend(split_csv(args.trtexec_extra_arg))

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
            passed = rc == 0 and "&&&& PASSED" in raw
            if not passed:
                status = "error"
                error_text = f"trtexec failed or did not report PASSED (exit={rc})"
        except Exception as e:
            status = "error"
            error_text = str(e)

        elapsed = time.time() - started
        result = {
            "status": status,
            "error": error_text,
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
            "elapsed_sec": elapsed,
            "metrics": metrics,
            "output_tail": output_tail(raw, lines=120),
        }
        state["results"][key] = result
        atomic_save_json(output_path, state)
        if not args.no_plot:
            current_plot_point_count = count_plot_points(
                state=state,
                metric_name=args.plot_metric,
                include_non_ok=args.plot_include_non_ok,
            )
            # Refresh plot immediately once any drawable point exists.
            if current_plot_point_count > 0:
                plotted = draw_plot(
                    state=state,
                    output_png=plot_path,
                    metric_name=args.plot_metric,
                    include_non_ok=args.plot_include_non_ok,
                    gpu_model_name=gpu_model_name,
                    model_name=model_name_for_title,
                )
                if plotted:
                    state["meta"]["plot_metric"] = args.plot_metric
                    state["meta"]["plot_png"] = str(plot_path.resolve())
                    atomic_save_json(output_path, state)

        done += 1
        if status == "ok":
            ok_count += 1
        else:
            err_count += 1
        render_progress(
            done,
            total,
            (
                f"plan={normalize_key_token(plan_label)} "
                f"batch={batch_size} s={streams} {status}"
            ),
        )

        if status == "error":
            print("\n[error] benchmark case failed, stop immediately", file=sys.stderr)
            print(f"[error] case={key}", file=sys.stderr)
            print(f"[error] command={join_cmd(cmd)}", file=sys.stderr)
            if error_text:
                print(f"[error] reason={error_text}", file=sys.stderr)
            if raw:
                print("[error] trtexec output tail:", file=sys.stderr)
                print(output_tail(raw, lines=120), file=sys.stderr)
            return True
        return False

    stop_requested = False
    if use_existing_plan_files:
        for plan_label, plan_path in parsed_plan_files:
            if not shape_templates and plan_path not in probed_plan_inputs:
                print(f"\n[probe] trtexec inspect inputs: {plan_path}")
                probed_plan_inputs[plan_path] = probe_plan_input_tensors(
                    trtexec_bin=trtexec_bin,
                    plan_path=plan_path,
                    env=env,
                    timeout_sec=args.trtexec_timeout_sec,
                    device=args.device,
                )
            for batch_size in batch_values:
                for streams in stream_values:
                    if run_single_case(plan_label, plan_path, batch_size, streams):
                        stop_requested = True
                        break
                if stop_requested:
                    break
            if stop_requested:
                break
    else:
        for batch_size in batch_values:
            plan_label = f"batch{batch_size}"
            plan_path = ensure_plan(batch_size, args, state, output_path, env)
            if not shape_templates and plan_path not in probed_plan_inputs:
                print(f"\n[probe] trtexec inspect inputs: {plan_path}")
                probed_plan_inputs[plan_path] = probe_plan_input_tensors(
                    trtexec_bin=trtexec_bin,
                    plan_path=plan_path,
                    env=env,
                    timeout_sec=args.trtexec_timeout_sec,
                    device=args.device,
                )
            for streams in stream_values:
                if run_single_case(plan_label, plan_path, batch_size, streams):
                    stop_requested = True
                    break
            if stop_requested:
                break

    if not args.no_plot and done == skipped:
        plotted = draw_plot(
            state=state,
            output_png=plot_path,
            metric_name=args.plot_metric,
            include_non_ok=args.plot_include_non_ok,
            gpu_model_name=gpu_model_name,
            model_name=model_name_for_title,
        )
        state["meta"]["plot_metric"] = args.plot_metric
        state["meta"]["plot_png"] = str(plot_path.resolve()) if plotted else ""
        atomic_save_json(output_path, state)

    print()
    print(
        f"Done. total={total} skipped={skipped} ok={ok_count} error={err_count} output={output_path.resolve()}"
    )
    return 0 if err_count == 0 else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as e:
        print(f"[error] {e}", file=sys.stderr)
        sys.exit(1)
