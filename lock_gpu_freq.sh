#!/usr/bin/env bash
set -euo pipefail

# Lock or reset GPU clocks for benchmark stability.
# Usage:
#   ./lock_gpu_freq.sh on
#   ./lock_gpu_freq.sh off
#   ./lock_gpu_freq.sh list

GPU_ID="${GPU_ID:-0}"
GPU_CLOCK_MHZ="${GPU_CLOCK_MHZ:-2550}"
MEM_CLOCK_MHZ="${MEM_CLOCK_MHZ:-14801}"

usage() {
  cat <<USAGE
Usage: $(basename "$0") <on|off|list>

Commands:
  on    Enable persistence mode and lock GPU/memory clocks
  off   Reset GPU/memory clocks to driver defaults
  list  List supported memory/graphics clock combinations for target GPU

Env overrides:
  GPU_ID         GPU index (default: ${GPU_ID})
  GPU_CLOCK_MHZ  Locked graphics clock MHz (default: ${GPU_CLOCK_MHZ})
  MEM_CLOCK_MHZ  Locked memory clock MHz (default: ${MEM_CLOCK_MHZ})
USAGE
}

if [[ $# -ne 1 ]]; then
  usage >&2
  exit 1
fi

if ! command -v nvidia-smi >/dev/null 2>&1; then
  echo "nvidia-smi not found" >&2
  exit 1
fi

case "$1" in
  list)
    nvidia-smi -i "${GPU_ID}" -q -d SUPPORTED_CLOCKS
    ;;
  on)
    sudo nvidia-smi -i "${GPU_ID}" -pm 1
    sudo nvidia-smi -i "${GPU_ID}" --lock-memory-clocks="${MEM_CLOCK_MHZ},${MEM_CLOCK_MHZ}"
    sudo nvidia-smi -i "${GPU_ID}" --lock-gpu-clocks="${GPU_CLOCK_MHZ},${GPU_CLOCK_MHZ}"
    nvidia-smi --query-gpu=index,clocks.current.graphics,clocks.current.memory,pstate,power.draw,temperature.gpu --format=csv,noheader
    ;;
  off)
    sudo nvidia-smi -i "${GPU_ID}" --reset-memory-clocks
    sudo nvidia-smi -i "${GPU_ID}" --reset-gpu-clocks
    nvidia-smi --query-gpu=index,clocks.current.graphics,clocks.current.memory,pstate,power.draw,temperature.gpu --format=csv,noheader
    ;;
  -h|--help)
    usage
    ;;
  *)
    usage >&2
    exit 1
    ;;
esac
