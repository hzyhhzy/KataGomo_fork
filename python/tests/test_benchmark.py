import argparse
import importlib.util
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest import mock


MODULE_PATH = Path(__file__).resolve().parents[1] / "benchmark.py"
SPEC = importlib.util.spec_from_file_location("benchmark_script", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
benchmark = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(benchmark)


def make_args(**overrides):
    defaults = {
        "katago_bin": "/tmp/katago",
        "trtexec_bin": "/tmp/trtexec",
        "config": "/tmp/config.cfg",
        "model": "/tmp/model.onnx",
        "tensorrt_lib": "",
        "max_batch": 4,
        "plan_batch": None,
        "plan_file": None,
        "batch_min": 1,
        "batch_max": 2,
        "stream_min": 1,
        "stream_max": 2,
        "build_count": 1,
        "devices": [0],
        "default_device": 0,
        "shape_template": None,
        "duration_sec": 1.0,
        "warmup_ms": 0,
        "iterations": 1,
        "avg_runs": 1,
        "plan_timeout_sec": 30,
        "trtexec_timeout_sec": 30,
        "gtp_extra_override": [],
        "trtexec_extra_arg": [],
        "output_json": None,
        "home_data_dir_base": None,
        "no_resume": False,
        "rerun_failed": False,
        "stop_on_error": False,
        "plot": False,
        "plot_png": None,
        "plot_metric": "nn_evals_per_sec",
        "plot_include_non_ok": False,
        "smoke": False,
    }
    defaults.update(overrides)
    return argparse.Namespace(**defaults)


class ScalarParsingTests(unittest.TestCase):
    def test_positive_int_parses(self):
        self.assertEqual(benchmark.positive_int("7"), 7)

    def test_positive_int_rejects_zero(self):
        with self.assertRaises(argparse.ArgumentTypeError):
            benchmark.positive_int("0")

    def test_positive_float_rejects_negative(self):
        with self.assertRaises(argparse.ArgumentTypeError):
            benchmark.positive_float("-1.5")

    def test_parse_trtexec_metrics_extracts_values(self):
        text = """
Throughput: 612.5 qps
Total Host Walltime: 1.23 s
Total GPU Compute Time: 0.98 s
GPU Compute Time: min = 1.0 ms, max = 2.0 ms, mean = 1.5 ms, median = 1.4 ms, percentile(90%) = 1.8 ms, percentile(95%) = 1.9 ms, percentile(99%) = 2.0 ms
"""
        metrics = benchmark.parse_trtexec_metrics(text)
        self.assertEqual(metrics["throughput_qps"], 612.5)
        self.assertEqual(metrics["total_host_walltime_s"], 1.23)
        self.assertEqual(metrics["gpu_compute"]["mean"]["value"], 1.5)
        self.assertEqual(metrics["gpu_compute"]["mean"]["unit"], "ms")


class CliParsingTests(unittest.TestCase):
    def make_parser(self):
        return benchmark.build_parser(
            katago_bin_default="/tmp/katago",
            trtexec_bin_default="/tmp/trtexec",
            config_default="/tmp/config.cfg",
            model_default="/tmp/model.onnx",
            tensorrt_lib_default="/tmp/lib",
            device_default=3,
        )

    def test_build_parser_devices_and_plot_flags(self):
        parser = self.make_parser()
        args = parser.parse_args(["--devices", "1", "4", "--no-plot"])
        benchmark.normalize_list_args(args)
        self.assertEqual(args.devices, ["1", "4"])
        self.assertFalse(args.plot)
        self.assertEqual(args.build_count, 1)

    def test_build_parser_devices_supports_comma_list(self):
        parser = self.make_parser()
        args = parser.parse_args(["--devices", "1,4"])
        benchmark.normalize_list_args(args)
        self.assertEqual(args.devices, ["1", "4"])

    def test_build_parser_alias_sets_build_count(self):
        parser = self.make_parser()
        args = parser.parse_args(["--simple-sampling-build-count", "5"])
        self.assertEqual(args.build_count, 5)

    def test_normalize_list_args_splits_other_list_inputs(self):
        args = make_args(
            plan_file=["a.engine,b.engine"],
            shape_template=["x:{batch},y:{batch}"],
            gtp_extra_override=["a=b,c=d"],
            trtexec_extra_arg=["--foo,--bar"],
        )
        benchmark.normalize_list_args(args)
        self.assertEqual(args.plan_file, ["a.engine", "b.engine"])
        self.assertEqual(args.shape_template, ["x:{batch}", "y:{batch}"])
        self.assertEqual(args.gtp_extra_override, ["a=b", "c=d"])
        self.assertEqual(args.trtexec_extra_arg, ["--foo", "--bar"])

    def test_parse_devices_arg_rejects_duplicates(self):
        args = make_args(devices=["0", "0"])
        with self.assertRaisesRegex(ValueError, "duplicates"):
            benchmark.parse_devices_arg(args)


class PathAndPlanTests(unittest.TestCase):
    def test_resolve_home_data_dir_base_uses_output_stem(self):
        with tempfile.TemporaryDirectory() as tmp:
            output_path = Path(tmp) / "result.json"
            base = benchmark.resolve_home_data_dir_base(None, output_path)
            self.assertEqual(base, output_path.with_suffix("") / "home_data")
            self.assertTrue(base.exists())

    def test_parse_plan_file_args_deduplicates_labels(self):
        with tempfile.TemporaryDirectory() as tmp:
            path_a = Path(tmp) / "a.engine"
            path_b = Path(tmp) / "b.engine"
            path_a.write_text("a", encoding="utf-8")
            path_b.write_text("b", encoding="utf-8")
            parsed = benchmark.parse_plan_file_args(
                [f"same={path_a}", f"same={path_b}", str(path_a)]
            )
            self.assertEqual(parsed[0][0], "same")
            self.assertEqual(parsed[1][0], "same_1")
            self.assertEqual(parsed[2][0], "a.engine")

    def test_compute_plan_build_fingerprint_changes_with_overrides(self):
        with tempfile.TemporaryDirectory() as tmp:
            katago = Path(tmp) / "katago"
            config = Path(tmp) / "config.cfg"
            model = Path(tmp) / "model.onnx"
            for path in (katago, config, model):
                path.write_text("x", encoding="utf-8")
            args1 = make_args(
                katago_bin=str(katago),
                config=str(config),
                model=str(model),
                gtp_extra_override=["a=b"],
            )
            args2 = make_args(
                katago_bin=str(katago),
                config=str(config),
                model=str(model),
                gtp_extra_override=["c=d"],
            )
            fp1 = benchmark.compute_plan_build_fingerprint(args1, 7, 0)
            fp2 = benchmark.compute_plan_build_fingerprint(args2, 7, 0)
            self.assertNotEqual(fp1, fp2)


class GroupPreparationTests(unittest.TestCase):
    def test_prepare_standard_groups_respects_cached_plan_and_build_count(self):
        with tempfile.TemporaryDirectory() as tmp:
            cached_plan = Path(tmp) / "cached.engine"
            cached_plan.write_text("plan", encoding="utf-8")
            args = make_args(
                katago_bin=str(Path(tmp) / "katago"),
                config=str(Path(tmp) / "config.cfg"),
                model=str(Path(tmp) / "model.onnx"),
                build_count=2,
                batch_min=1,
                batch_max=1,
                stream_min=1,
                stream_max=2,
            )
            for path in (Path(args.katago_bin), Path(args.config), Path(args.model)):
                path.write_text("x", encoding="utf-8")

            label1 = benchmark.plan_build_label(1, 1)
            key1 = benchmark.plan_state_key(label1, 0)
            fingerprint = benchmark.compute_plan_build_fingerprint(args, 1, 0)
            state = {
                "results": {
                    benchmark.case_key_with_plan(label1, 1, 1, 0): {"status": "ok"},
                },
                "plans": {
                    key1: {
                        "path": str(cached_plan),
                        "build_fingerprint": fingerprint,
                    }
                },
            }

            groups, skipped, total = benchmark.prepare_standard_groups(
                args=args,
                state=state,
                parsed_plan_files=[],
                batch_values=[1],
                stream_values=[1, 2],
                devices=[0],
                home_data_dir_base=Path(tmp) / "home_data",
            )

            self.assertEqual(total, 4)
            self.assertEqual(skipped, 1)
            self.assertEqual(len(groups), 2)
            first = groups[0]
            second = groups[1]
            self.assertEqual(first["plan_label"], label1)
            self.assertEqual(first["plan_path"], str(cached_plan.resolve()))
            self.assertEqual(first["pending_streams"], [2])
            self.assertEqual(second["plan_label"], benchmark.plan_build_label(1, 2))
            self.assertIsNone(second["plan_path"])
            self.assertEqual(second["pending_streams"], [1, 2])

    def test_prepare_standard_groups_plan_file_mode_skips_completed_results(self):
        with tempfile.TemporaryDirectory() as tmp:
            plan = Path(tmp) / "existing.engine"
            plan.write_text("plan", encoding="utf-8")
            state = {
                "results": {
                    benchmark.case_key_with_plan("manual", 1, 1, 0): {"status": "ok"},
                },
                "plans": {},
            }
            args = make_args(build_count=1, batch_min=1, batch_max=1, stream_min=1, stream_max=2)
            groups, skipped, total = benchmark.prepare_standard_groups(
                args=args,
                state=state,
                parsed_plan_files=[("manual", str(plan.resolve()))],
                batch_values=[1],
                stream_values=[1, 2],
                devices=[0],
                home_data_dir_base=Path(tmp) / "home_data",
            )
            self.assertEqual(total, 2)
            self.assertEqual(skipped, 1)
            self.assertEqual(groups[0]["pending_streams"], [2])


class ExecutionTests(unittest.TestCase):
    def test_execute_standard_groups_updates_state_and_counts(self):
        with tempfile.TemporaryDirectory() as tmp:
            output_path = Path(tmp) / "state.json"
            plot_path = Path(tmp) / "plot.png"
            state = {"plans": {}, "results": {}, "meta": {}}
            args = make_args(plot=False, stop_on_error=False)
            groups = [
                {
                    "plan_label": benchmark.plan_build_label(1, 1),
                    "plan_key": benchmark.plan_state_key(benchmark.plan_build_label(1, 1), 0),
                    "plan_path": None,
                    "build_fingerprint": "fp1",
                    "batch_size": 1,
                    "device": 0,
                    "home_data_dir": str(Path(tmp) / "home0"),
                    "pending_streams": [1, 2],
                }
            ]

            def fake_build_plan_once(plan_batch, device, args, env, home_data_dir):
                plan_path = Path(tmp) / f"plan_b{plan_batch}_d{device}.engine"
                plan_path.write_text("plan", encoding="utf-8")
                return {
                    "plan_path": str(plan_path.resolve()),
                    "command": "katago gtp ...",
                    "home_data_dir": str(home_data_dir),
                }

            def fake_benchmark_case(**kwargs):
                streams = kwargs["streams"]
                return {
                    "status": "ok",
                    "error": "",
                    "device": kwargs["device"],
                    "plan_label": kwargs["plan_label"],
                    "batch_size": kwargs["batch_size"],
                    "streams": streams,
                    "plan_path": kwargs["plan_path"],
                    "command": f"trtexec stream={streams}",
                    "metrics": {"throughput_qps": 100.0 + streams},
                    "output_tail": "",
                }

            with mock.patch.object(benchmark, "build_plan_once", side_effect=fake_build_plan_once), \
                 mock.patch.object(benchmark, "probe_plan_input_tensors", return_value={"x": {"opt": [1, 2]}}), \
                 mock.patch.object(benchmark, "benchmark_case", side_effect=fake_benchmark_case), \
                 mock.patch.object(benchmark, "atomic_save_json") as save_json:
                ok_count, err_count, worker_errors = benchmark.execute_standard_groups(
                    args=args,
                    env={},
                    trtexec_bin=Path("/tmp/trtexec"),
                    shape_templates=["x:{batch}x2"],
                    parsed_plan_files=[],
                    devices=[0],
                    groups=groups,
                    state=state,
                    output_path=output_path,
                    plot_path=plot_path,
                    total=2,
                    skipped=0,
                    gpu_model_name="GPU",
                    model_name_for_title="model",
                )

            self.assertEqual(ok_count, 2)
            self.assertEqual(err_count, 0)
            self.assertEqual(worker_errors, [])
            self.assertIn(groups[0]["plan_key"], state["plans"])
            self.assertEqual(len(state["results"]), 2)
            self.assertGreaterEqual(save_json.call_count, 3)

    def test_run_standard_benchmark_mode_uses_existing_plan_file_without_build(self):
        with tempfile.TemporaryDirectory() as tmp:
            plan = Path(tmp) / "manual.engine"
            plan.write_text("plan", encoding="utf-8")
            args = make_args(
                build_count=1,
                batch_min=1,
                batch_max=1,
                stream_min=1,
                stream_max=1,
                output_json=str(Path(tmp) / "report.json"),
                home_data_dir_base=str(Path(tmp) / "home_data"),
                plot=False,
            )
            parsed_plan_files = [("manual", str(plan.resolve()))]

            with mock.patch.object(benchmark, "build_plan_once") as build_plan_once_mock, \
                 mock.patch.object(
                     benchmark,
                     "benchmark_case",
                     return_value={
                         "status": "ok",
                         "error": "",
                         "device": 0,
                         "plan_label": "manual",
                         "batch_size": 1,
                         "streams": 1,
                         "plan_path": str(plan.resolve()),
                         "command": "trtexec ...",
                         "metrics": {"throughput_qps": 123.0},
                         "output_tail": "",
                     },
                 ), \
                 mock.patch.object(benchmark, "probe_plan_input_tensors", return_value={"x": {"opt": [1, 2]}}):
                rc = benchmark.run_standard_benchmark_mode(
                    args=args,
                    katago_bin=Path("/tmp/katago"),
                    trtexec_bin=Path("/tmp/trtexec"),
                    config=Path("/tmp/config.cfg"),
                    model=Path("/tmp/model.onnx"),
                    env={},
                    shape_templates=["x:{batch}x2"],
                    parsed_plan_files=parsed_plan_files,
                    devices=[0],
                    gpu_models={0: "GPU"},
                )

            self.assertEqual(rc, 0)
            build_plan_once_mock.assert_not_called()


if __name__ == "__main__":
    unittest.main()
