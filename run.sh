#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=./env.sh
source "${SCRIPT_DIR}/env.sh"

# Tunable defaults
DEFAULT_NUM_SEARCH_THREADS=21
DEFAULT_NN_MAX_BATCHSIZE=7
DEFAULT_TRT_CUDA_STREAMS=2
DEFAULT_TRT_DEVICE_ID=0
DEFAULT_TRT_USE_CUDA_GRAPH=true
# Benchmark params defaults
DEFAULT_BENCH_VISITS=10000

MODE="gtp"
for arg in "$@"; do
  case "${arg}" in
    --benchmark)
      MODE="benchmark"
      ;;
    --gtp)
      MODE="gtp"
      ;;
  esac
done

NN_MAX_BATCHSIZE="${DEFAULT_NN_MAX_BATCHSIZE}"
NUM_SEARCH_THREADS="${DEFAULT_NUM_SEARCH_THREADS}"
TRT_CUDA_STREAMS="${DEFAULT_TRT_CUDA_STREAMS}"
BENCH_VISITS="${DEFAULT_BENCH_VISITS}"
TRT_USE_CUDA_GRAPH="${DEFAULT_TRT_USE_CUDA_GRAPH}"

TRT_DEVICE_ID_RAW="${TRT_DEVICE_ID:-${DEFAULT_TRT_DEVICE_ID}}"
KATAGO_BIN="${KATAGO_BIN_PATH}"
KATAGO_MODEL="${KATAGO_MODEL_PATH}"
KATAGO_CONFIG="${KATAGO_CONFIG_PATH}"

print_usage() {
  cat <<EOF
Usage: $(basename "$0") [--gtp|--benchmark] [options]

Modes:
  --gtp                        Run katago gtp (default)
  --benchmark                  Run katago benchmark

Options:
  --katago-bin PATH            Path to katago executable (default: ${KATAGO_BIN})
  --model PATH                 Path to model file (default: ${KATAGO_MODEL})
  --config PATH                Path to config file (default: ${KATAGO_CONFIG})
  --search-threads N           Number of search threads (default depends on mode)
  --batch-size N               NN max/fixed batch size (default depends on mode)
  --cuda-streams N             NN server threads per GPU (default depends on mode)
  --trt-device-id VAL          Device id list, e.g. "0" or "0,1" (default: ${TRT_DEVICE_ID_RAW})
  --trt-use-cuda-graph             Set trtUseCudaGraph=true
  --no-trt-use-cuda-graph          Set trtUseCudaGraph=false
  --visits N                   Benchmark visits (benchmark mode only)
  -h, --help                   Show this help message

Device assignment:
  Read from --trt-device-id (or fallback to built-in default).
  Supported forms: "0" or "0,1".
EOF
}

require_value() {
  local opt_name="$1"
  local opt_value="${2:-}"
  if [[ -z "${opt_value}" ]]; then
    echo "Missing value for ${opt_name}" >&2
    print_usage >&2
    exit 1
  fi
}

parse_args() {
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --gtp)
        MODE="gtp"
        shift
        ;;
      --benchmark)
        MODE="benchmark"
        shift
        ;;
      --katago-bin)
        require_value "$1" "${2:-}"
        KATAGO_BIN="$2"
        shift 2
        ;;
      --model)
        require_value "$1" "${2:-}"
        KATAGO_MODEL="$2"
        shift 2
        ;;
      --config)
        require_value "$1" "${2:-}"
        KATAGO_CONFIG="$2"
        shift 2
        ;;
      --search-threads)
        require_value "$1" "${2:-}"
        NUM_SEARCH_THREADS="$2"
        shift 2
        ;;
      --batch-size)
        require_value "$1" "${2:-}"
        NN_MAX_BATCHSIZE="$2"
        shift 2
        ;;
      --cuda-streams)
        require_value "$1" "${2:-}"
        TRT_CUDA_STREAMS="$2"
        shift 2
        ;;
      --trt-device-id)
        require_value "$1" "${2:-}"
        TRT_DEVICE_ID_RAW="$2"
        shift 2
        ;;
      --trt-use-cuda-graph)
        TRT_USE_CUDA_GRAPH=true
        shift
        ;;
      --no-trt-use-cuda-graph)
        TRT_USE_CUDA_GRAPH=false
        shift
        ;;
      --visits)
        require_value "$1" "${2:-}"
        BENCH_VISITS="$2"
        shift 2
        ;;
      -h|--help)
        print_usage
        exit 0
        ;;
      *)
        echo "Unknown argument: $1" >&2
        print_usage >&2
        exit 1
        ;;
    esac
  done
}

if [[ $# -gt 0 ]]; then
  parse_args "$@"
fi

if ! [[ "${TRT_CUDA_STREAMS}" =~ ^[1-9][0-9]*$ ]]; then
  echo "TRT_CUDA_STREAMS must be a positive integer, got: ${TRT_CUDA_STREAMS}" >&2
  exit 1
fi

if ! [[ "${NUM_SEARCH_THREADS}" =~ ^[1-9][0-9]*$ ]]; then
  echo "NUM_SEARCH_THREADS must be a positive integer, got: ${NUM_SEARCH_THREADS}" >&2
  exit 1
fi

if ! [[ "${NN_MAX_BATCHSIZE}" =~ ^[1-9][0-9]*$ ]]; then
  echo "NN_MAX_BATCHSIZE must be a positive integer, got: ${NN_MAX_BATCHSIZE}" >&2
  exit 1
fi

if [[ "${MODE}" == "benchmark" ]] && ! [[ "${BENCH_VISITS}" =~ ^[1-9][0-9]*$ ]]; then
  echo "BENCH_VISITS must be a positive integer, got: ${BENCH_VISITS}" >&2
  exit 1
fi

declare -a TRT_DEVICE_ID_LIST=()
IFS=',' read -r -a RAW_TRT_DEVICE_IDS <<< "${TRT_DEVICE_ID_RAW}"
for raw_id in "${RAW_TRT_DEVICE_IDS[@]}"; do
  id="${raw_id//[[:space:]]/}"
  if [[ -z "${id}" ]]; then
    echo "TRT_DEVICE_ID contains an empty item: ${TRT_DEVICE_ID_RAW}" >&2
    exit 1
  fi
  if ! [[ "${id}" =~ ^[0-9]+$ ]]; then
    echo "TRT_DEVICE_ID must be a non-negative integer or comma-separated list, got: ${TRT_DEVICE_ID_RAW}" >&2
    exit 1
  fi
  TRT_DEVICE_ID_LIST+=("${id}")
done

if [[ "${#TRT_DEVICE_ID_LIST[@]}" -le 0 ]]; then
  echo "TRT_DEVICE_ID must contain at least one device id" >&2
  exit 1
fi

if [[ "${TRT_USE_CUDA_GRAPH}" != "true" && "${TRT_USE_CUDA_GRAPH}" != "false" ]]; then
  echo "TRT_USE_CUDA_GRAPH must be true/false, got: ${TRT_USE_CUDA_GRAPH}" >&2
  exit 1
fi

NUM_NN_SERVER_THREADS=$(( TRT_CUDA_STREAMS * ${#TRT_DEVICE_ID_LIST[@]} ))
OVERRIDE_CONFIG="numNNServerThreadsPerModel=${NUM_NN_SERVER_THREADS}"
for ((thread_idx=0; thread_idx<NUM_NN_SERVER_THREADS; thread_idx++)); do
  device_list_idx=$(( thread_idx / TRT_CUDA_STREAMS ))
  mapped_device_id="${TRT_DEVICE_ID_LIST[${device_list_idx}]}"
  OVERRIDE_CONFIG+=",trtDeviceToUseThread${thread_idx}=${mapped_device_id}"
done

OVERRIDE_CONFIG+=",numSearchThreads=${NUM_SEARCH_THREADS}"
OVERRIDE_CONFIG+=",trtUseCudaGraph=${TRT_USE_CUDA_GRAPH}"

if [[ "${MODE}" == "benchmark" ]]; then
  OVERRIDE_CONFIG+=",useEvalCache=true"

  "${KATAGO_BIN}" benchmark \
    -model "${KATAGO_MODEL}" \
    -config "${KATAGO_CONFIG}" \
    -v "${BENCH_VISITS}" \
    -t "${NUM_SEARCH_THREADS}" \
    -fixed-batch-size "${NN_MAX_BATCHSIZE}" \
    -override-config "${OVERRIDE_CONFIG}"
else
  OVERRIDE_CONFIG+=",analysisPVLen=99"
  OVERRIDE_CONFIG+=",useEvalCache=true"
  OVERRIDE_CONFIG+=",nnMaxBatchSize=${NN_MAX_BATCHSIZE}"

  "${KATAGO_BIN}" gtp \
    -model "${KATAGO_MODEL}" \
    -config "${KATAGO_CONFIG}" \
    -override-config "${OVERRIDE_CONFIG}"
fi
