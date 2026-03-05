#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=./env.sh
source "${SCRIPT_DIR}/env.sh"

"${KATAGO_BIN_PATH}" benchmark \
  -model "${KATAGO_MODEL_PATH}" \
  -config "${KATAGO_CONFIG_PATH}" \
  "$@"
