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

## 3. Find the Best Batch Size

Run:

```bash
python python/benchmark.py
```

The script reads defaults from `env.sh`, generates TensorRT plans, benchmarks different batch sizes and CUDA stream counts, and writes results under `benchmark/`.

If you want PNG plots, install `matplotlib` first. Without it, the script can still run, but plotting will be skipped.

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
