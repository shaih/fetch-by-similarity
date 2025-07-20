#!/usr/bin/env bash
# ----------------------------------------------------------------------
# Build OpenFHE (CPU backend) at a fixed tag and install to
#   third_party/openfhe
# Reruns only if libopenfhe.a is missing.
#   ./scripts/get-openfhe.sh          # build once, skip if present
#   ./scripts/get-openfhe.sh --force  # wipe & rebuild even if present

set -euo pipefail

# -------- configurable -------------------------------------------------
TAG="v1.3.1"                     # bump only after CI/Docker update
ROOT_DIR="$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )/.." &> /dev/null && pwd )"
SRC_DIR="$ROOT_DIR/third_party/openfhe-src"          # git clone here
INSTALL_DIR="$ROOT_DIR/third_party/openfhe"          # cmake --install here
NPROC=$(nproc 2>/dev/null || sysctl -n hw.ncpu || echo 4)
# ----------------------------------------------------------------------

FORCE=0
[[ ${1:-} == "--force" ]] && FORCE=1

# 0) short-circuit if library already installed and not forcing rebuild
if [[ -d "/usr/local/lib/OpenFHE" && $FORCE -eq 0 ]]; then
    echo "[get-openfhe] Found OpenFHE installed at /usr/local/lib/ (use --force to rebuild)."
    exit 0
fi
if [[ -f "$INSTALL_DIR/lib/libopenfhe.a" && $FORCE -eq 0 ]]; then
    echo "[get-openfhe] Found OpenFHE at $INSTALL_DIR (use --force to rebuild)."
    exit 0
fi

# 1) clone or update repo ------------------------------------------------
mkdir -p "$ROOT_DIR/third_party"
if [[ -d "$SRC_DIR/.git" ]]; then
    echo "[get-openfhe] Updating existing clone in $SRC_DIR"
    git -C "$SRC_DIR" fetch --depth 1 origin "$TAG"
    git -C "$SRC_DIR" checkout -q "$TAG"
else
    echo "[get-openfhe] Cloning OpenFHE $TAG"
    git clone --branch "$TAG" --depth 1 \
        https://github.com/openfheorg/openfhe-development.git "$SRC_DIR"
fi

# 2) wipe previous install if --force -----------------------------------
if [[ $FORCE -eq 1 ]]; then
    echo "[get-openfhe] --force: removing $INSTALL_DIR"
    rm -rf "$INSTALL_DIR"
fi

# 3) configure & build ---------------------------------------------------
echo "[get-openfhe] Configuring CMake…"
cmake -S "$SRC_DIR" -B "$SRC_DIR/build" \
      -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
      -DBUILD_BENCHMARKS=OFF -DWITH_OPENMP=ON \
      -DBUILD_EXTRAS=OFF -DWITH_NTL=OFF \
      -DCMAKE_BUILD_TYPE=Release

echo "[get-openfhe] Building…"
cd "$SRC_DIR/build"
make -j"$NPROC"

echo "[get-openfhe] Installing to $INSTALL_DIR"
make install 

echo "[get-openfhe] Done."