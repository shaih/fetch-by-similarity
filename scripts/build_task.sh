#!/usr/bin/env bash
# ----------------------------------------------------------------------
# Usage: ./scripts/build_task.sh <task>/submission
# Compiles preprocess/compute/postprocess inside that folder.

# The build environment for this reference code assume that OpenFHE
# version >=  1.3 is already installed and can be found by cmake

set -euo pipefail
ROOT="$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )/.." &> /dev/null && pwd )"
TASK_DIR="$1"
BUILD="$TASK_DIR/build"

cmake -S "$TASK_DIR" -B "$BUILD"
cd "$TASK_DIR/build"
make -j
