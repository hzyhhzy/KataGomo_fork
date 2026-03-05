#!/usr/bin/env bash
set -euo pipefail

# Build script for KataGomo with TensorRT backend (Gomoku version)
# This script handles compilation and deployment to /opt/katago

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=./env.sh
source "${SCRIPT_DIR}/env.sh"

# Configuration
BUILD_DIR="${SCRIPT_DIR}/cpp/build"
KATAGO_BIN="${KATAGO_BIN_PATH}"
DEPLOY_DIR="$(dirname "${KATAGO_BIN}")"
NUM_JOBS="${NUM_JOBS:-$(nproc)}"

BASE_RELEASE_FLAGS="-O3 -DNDEBUG -march=native -mtune=native -fomit-frame-pointer"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Functions
print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
    exit 1
}

print_usage() {
    cat <<EOF
Usage: $(basename "$0") [--help]

Options:
  --help               Show this help message
EOF
}

parse_args() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --help|-h)
                print_usage
                exit 0
                ;;
            *)
                print_error "Unknown argument: $1 (use --help for usage)"
                ;;
        esac
    done
}

configure_and_build() {
    local build_dir="$1"
    local c_flags_release="$2"
    local cxx_flags_release="$3"
    rm -rf "${build_dir}"
    mkdir -p "${build_dir}"
    print_info "Configuring CMake in ${build_dir}"
    cmake -S "${SCRIPT_DIR}/cpp" -B "${build_dir}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON \
        -DCMAKE_C_FLAGS_RELEASE="${c_flags_release}" \
        -DCMAKE_CXX_FLAGS_RELEASE="${cxx_flags_release}" \
        -DUSE_BACKEND=TENSORRT \
        -DUSE_AVX2=1 \
        -DTENSORRT_INCLUDE_DIR="${TENSORRT_ROOT}/include" \
        -DTENSORRT_LIBRARY="${TENSORRT_ROOT}/lib/libnvinfer.so" \
        -DTENSORRT_ONNX_LIBRARY="${TENSORRT_ROOT}/lib/libnvonnxparser.so"
    if [ $? -ne 0 ]; then
        print_error "CMake configuration failed for ${build_dir}"
    fi

    print_info "Compiling KataGomo in ${build_dir} with ${NUM_JOBS} jobs"
    cmake --build "${build_dir}" --parallel "${NUM_JOBS}"
    if [ $? -ne 0 ]; then
        print_error "Compilation failed for ${build_dir}"
    fi
}

# Parse CLI args
parse_args "$@"

# Check if we're in the right directory
if [ ! -d "${SCRIPT_DIR}/cpp" ]; then
    print_error "This script must be run from the root directory of the KataGomo repository"
fi

# Main build process
main() {
    print_info "Starting KataGomo build process"

    print_info "Preparing build environment"
    mkdir -p "${BUILD_DIR}"
    configure_and_build "${BUILD_DIR}" "${BASE_RELEASE_FLAGS}" "${BASE_RELEASE_FLAGS}"

    # Deploy
    print_info "Deploying to ${DEPLOY_DIR}"
    mkdir -p "${DEPLOY_DIR}"
    rm -f "${KATAGO_BIN}"
    cp "${BUILD_DIR}/katago" "${KATAGO_BIN}"
    chmod +x "${KATAGO_BIN}"

    # Verify installation
    print_info "Verifying installation"
    "${KATAGO_BIN}" version

    if [ $? -ne 0 ]; then
        print_error "Verification failed"
    fi

    print_info "Build and deployment completed successfully!"
}

# Run the build
main
