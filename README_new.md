# KataGomo TensorRT Workflow

This repository includes a small set of helper scripts for building, benchmarking, and running the TensorRT backend.

## 1. Prepare `env.sh`

Copy `env_sample.sh` to `env.sh` and fill in the local paths:

```bash
cp env_sample.sh env.sh
```

Edit at least these variables in `env.sh`:
- `TENSORRT_ROOT`
- `KATAGO_BIN_PATH`
- `KATAGO_MODEL_PATH`
- `KATAGO_CONFIG_PATH`

`env.sh` is shared by all new helper scripts:
- `build.sh`
- `run.sh`
- `python/benchmark.py`

## 2. Build

Run the build script from the repository root:

```bash
./build.sh
```

This script compiles the TensorRT backend and deploys the built `katago` binary to `KATAGO_BIN_PATH`.

## 3. `python/benchmark.py`

`python/benchmark.py` is the main TensorRT benchmarking helper.

It combines plan generation and plan benchmarking in one workflow:
- read default paths from `env.sh`
- build TensorRT plans through `katago gtp`
- benchmark those plans through `trtexec`
- sweep batch size and CUDA stream count
- optionally rebuild the same batch multiple times with `--build-count`
- isolate `trtcache` output with `homeDataDir`
- write structured JSON results under `benchmark/`
- optionally emit a PNG plot

### 3.1. Basic usage

Run the default sweep:

```bash
python python/benchmark.py
```

This reads:
- `TENSORRT_ROOT`
- `KATAGO_BIN_PATH`
- `KATAGO_MODEL_PATH`
- `KATAGO_CONFIG_PATH`

from `env.sh`.

If `matplotlib` is installed, the script also writes a PNG plot. Otherwise plotting is skipped and the benchmark still completes.

### 3.2. What it actually does

For each plan candidate, the script:

1. runs `katago gtp`
2. injects an override config containing:
   - `nnMaxBatchSize=<plan batch>`
   - `numSearchThreads=1`
   - `numNNServerThreadsPerModel=1`
   - `trtDeviceToUseThread0=<device>`
   - `homeDataDir=<isolated directory>`
3. parses the generated plan path from KataGo output
4. loads that plan with `trtexec`
5. runs `trtexec --useCudaGraph`
6. records throughput and latency metrics

This is intentional: the benchmark uses KataGo's real TensorRT build path, not a separate synthetic builder.

### 3.3. Key options

#### Device selection

Use one or more GPUs:

```bash
--devices 0 1
```

Comma-separated form is also accepted:

```bash
--devices 0,1
```

All selected devices must be the same GPU model.

#### Rebuilding the same batch multiple times

Use `--build-count` to compile multiple independent engines for the same batch size:

```bash
--build-count 64
```

`build_count=1` is the ordinary case. Larger values are useful for simple sampling when you want to keep rebuilding the same batch and compare the resulting plans.

#### Restricting the sweep

Benchmark only batch 7 and stream 2:

```bash
--max-batch 7 --batch-min 7 --batch-max 7 --stream-min 2 --stream-max 2
```

#### Isolating TensorRT cache output

Use:

```bash
--home-data-dir-base benchmark/home_data_runs
```

This makes each build write to an isolated tree such as:

```text
benchmark/home_data_runs/device3/batch7_build0001/trtcache/...
```

instead of sharing `~/.katago/trtcache`.

This is especially important when:
- using multiple GPUs
- using `--build-count > 1`
- comparing independently built plans
- archiving or inspecting generated TRT plans

#### Plotting

Plotting is enabled by default if `matplotlib` is available.

Disable it with:

```bash
--no-plot
```

Choose the plotted metric with:

```bash
--plot-metric nn_evals_per_sec
```

or:

```bash
--plot-metric throughput_qps
```

### 3.4. Typical commands

Find a good batch/stream setting on one GPU:

```bash
python python/benchmark.py --devices 0
```

Rebuild batch 7 sixty-four times on two GPUs:

```bash
python python/benchmark.py \
  --devices 3,4 \
  --build-count 64 \
  --max-batch 7 \
  --batch-min 7 \
  --batch-max 7 \
  --stream-min 2 \
  --stream-max 2 \
  --home-data-dir-base benchmark/home_data_runs
```

Benchmark existing plan files without rebuilding:

```bash
python python/benchmark.py \
  --plan-file best=/path/to/best.engine other=/path/to/other.engine \
  --batch-min 7 \
  --batch-max 7 \
  --stream-min 2 \
  --stream-max 2
```

Add KataGo override config:

```bash
python python/benchmark.py \
  --gtp-extra-override trtUseCudaGraph=true,logSearchInfo=false
```

Add extra `trtexec` flags:

```bash
python python/benchmark.py \
  --trtexec-extra-arg --dumpProfile,--separateProfileRun
```

List-valued arguments accept both whitespace and comma-separated forms. For example, these are equivalent:

```bash
--devices 0 1
```

```bash
--devices 0,1
```

The same rule applies to:
- `--plan-file`
- `--shape-template`
- `--gtp-extra-override`
- `--trtexec-extra-arg`

### 3.5. Output layout

By default, JSON output goes under `benchmark/` with a filename that includes:
- model name
- selected device set
- build count
- GPU model

Example:

```text
benchmark/trtexec_benchmark_b18tf.onnx_devs-3-4_build64_gpu-NVIDIA_GeForce_RTX_4090_D.json
```

The JSON contains:
- `meta`: run configuration and resolved paths
- `plans`: plan metadata and build fingerprints
- `results`: one entry per `(plan_label, device, batch, stream)`

If plotting succeeds, the plot path is also recorded in `meta`.

### 3.6. Resume behavior

If an existing `--output-json` is reused, the script can resume.

It skips a case when:
- a previous result exists with `status=ok`
- or `status=error` and `--rerun-failed` is not set

For plan reuse, the script also checks a build fingerprint derived from:
- the `katago` binary
- config file
- model file
- plan batch
- device
- `--gtp-extra-override`

So changing TRT-relevant inputs will not silently reuse stale plans.

### 3.7. Smoke test

Quick sanity check:

```bash
python python/benchmark.py --smoke
```

This forces:
- batch 1
- stream 1
- short runtime

and is useful for checking that paths and binaries are wired correctly.

### 3.8. Unit tests

The Python-side control logic for `benchmark.py` has unit tests in:

```text
python/tests/test_benchmark.py
```

Run them with:

```bash
python3 -m unittest discover -s python/tests -p 'test_*.py' -v
```

These tests do not require a GPU. They mock `katago` and `trtexec` and validate argument parsing, state handling, group generation, and benchmark orchestration.

## 4. Run Benchmark or GTP

Run a benchmark once:

```bash
./run.sh --benchmark
```

Run GTP mode:

```bash
./run.sh
```

You can inspect all optional runtime overrides with:

```bash
./run.sh --help
```
