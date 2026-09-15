#!/usr/bin/env bash
# Docker-based build -- no local toolchain required. Output: scripts/build/

set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
OUTPUT_DIR="$REPO_ROOT/scripts/build"

IMAGE_NAME="ogx-mini-2026"
BUILD_VOLUME="ogx-mini-2026-build"

if ! command -v docker &>/dev/null; then
    echo "Error: 'docker' not found in PATH."
    echo "Install Docker Desktop (Windows/macOS) or Docker Engine (Linux/macOS) and try again."
    exit 1
fi

if ! docker info &>/dev/null; then
    echo "Error: Docker daemon is not running. Start Docker Desktop and try again."
    exit 1
fi

DOCKERFILE="$SCRIPT_DIR/Dockerfile"
PATCH_FILE="$REPO_ROOT/Firmware/external/patches/pico_sdk_hids_host.diff"
_hash_files() {
    if command -v sha256sum &>/dev/null; then sha256sum "$@"; else shasum -a 256 "$@"; fi
}
IMAGE_HASH=$(_hash_files "$DOCKERFILE" "$PATCH_FILE" | _hash_files | cut -d' ' -f1)
PICO_SDK_VERSION=$(grep -m1 '^set(PICOSDK_VERSION_TAG' "$REPO_ROOT/Firmware/RP2040/CMakeLists.txt" | grep -o '"[^"]*"' | tr -d '"')
[ -z "$PICO_SDK_VERSION" ] && { echo "Error: could not read PICOSDK_VERSION_TAG from CMakeLists.txt"; exit 1; }
read -r EXISTING_HASH EXISTING_SDK <<< "$(docker image inspect "$IMAGE_NAME" \
    --format '{{ index .Config.Labels "image_hash" }} {{ index .Config.Labels "pico_sdk_version" }}' \
    2>/dev/null || true)"

if [ "$IMAGE_HASH" != "$EXISTING_HASH" ] || [ "$PICO_SDK_VERSION" != "$EXISTING_SDK" ]; then
    echo "Building Docker image '$IMAGE_NAME'..."
    echo
    docker build \
        --label "image_hash=$IMAGE_HASH" \
        --label "pico_sdk_version=$PICO_SDK_VERSION" \
        --build-arg "PICO_SDK_VERSION=$PICO_SDK_VERSION" \
        -t "$IMAGE_NAME" -f "$DOCKERFILE" "$REPO_ROOT"
    echo
else
    echo "Docker image '$IMAGE_NAME' is up to date, skipping build."
    echo
fi

docker volume create "$BUILD_VOLUME" &>/dev/null || true
mkdir -p "$OUTPUT_DIR" 2>/dev/null || true

TTY_FLAG=""
[ -t 0 ] && TTY_FLAG="-t"
# MSYS_NO_PATHCONV=1: prevents Git Bash from converting /repo and /output to Windows paths
# shellcheck disable=SC2086
MSYS_NO_PATHCONV=1 docker run --rm -i $TTY_FLAG \
    -v "$REPO_ROOT:/repo" \
    -v "$BUILD_VOLUME:/build" \
    -v "$OUTPUT_DIR:/output" \
    -e OGXM_BUILD_DIR=/build \
    -w /repo \
    "$IMAGE_NAME" \
    bash scripts/docker/container-build.sh

echo
echo "Firmware files in: $OUTPUT_DIR"
