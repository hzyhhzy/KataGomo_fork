#!/usr/bin/env bash

# Unified environment variables for local KataGo scripts.
# Edit values directly in this file as needed.

# 1) TensorRT location (used for both tools and runtime libraries)
export TENSORRT_ROOT="/path/to/tensorrt"
export LD_LIBRARY_PATH="${TENSORRT_ROOT}/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

# 2) KataGo binary location
export KATAGO_BIN_PATH="/path/to/katago"

# 3) Model location
export KATAGO_MODEL_PATH="/path/to/model.bin.gz/or/model.onnx/"

# 4) Config location
export KATAGO_CONFIG_PATH="/path/to/gtp.cfg"
