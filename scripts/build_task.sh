#!/usr/bin/env bash
# ----------------------------------------------------------------------
# Usage: ./scripts/build_task.sh <task>/submission
# Compiles preprocess/compute/postprocess inside that folder.
set -euo pipefail
ROOT="$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )/.." &> /dev/null && pwd )"
TASK_DIR="$1"
BUILD="$TASK_DIR/build"
#NPROC=$(nproc 2>/dev/null || sysctl -n hw.ncpu || echo 4)

# "$ROOT/scripts/get-openfhe.sh" >/dev/null
cmake -S "$TASK_DIR" -B "$BUILD" \
      -DOpenFHE_DIR="$ROOT/third_party/openfhe/lib"
cd "$TASK_DIR/build"
make -j
