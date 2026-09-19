#!/usr/bin/env bash
# Runs inside the build container; called by docker-build.sh.
set -e
cd /repo

git config --global --add safe.directory /repo

echo "==> Initialising submodules"
git submodule update --init --recursive Firmware/external/bluepad32
git submodule update --init \
    Firmware/external/tinyusb \
    Firmware/external/Pico-PIO-USB \
    Firmware/external/libfixmath
echo

# Copy to tmpfs first - bash reading a script off a Windows bind mount mid-execution
# can hit ENODATA errors when it blocks on interactive reads.
cp /repo/scripts/build.sh /tmp/build.sh

_build_output=$(mktemp)
_cleanup() {
    [ -f /tmp/build_log.txt ] && cp /tmp/build_log.txt /output/build_log.txt
    rm -f "$_build_output"
}
trap _cleanup EXIT

touch /tmp/build_start
OGXM_REPO_ROOT=/repo bash /tmp/build.sh 2>&1 | tee "$_build_output"
_build_exit=${PIPESTATUS[0]}
[ "$_build_exit" -eq 2 ] && exit 0
[ "$_build_exit" -ne 0 ] && exit "$_build_exit"

BUILD_DIR=$(grep "^Output directory:" "$_build_output" | tail -1 | cut -d' ' -f3-)

echo "==> Copying firmware"

while IFS= read -r line; do
    case "$line" in
        OGXM_BOARD:*)        BOARD="${line#*=}" ;;
        CMAKE_BUILD_TYPE:*)  TYPE="${line#*=}" ;;
        OGXM_FIXED_DRIVER:*) FIXED="${line#*=}" ;;
    esac
done < <(grep -E "^(OGXM_BOARD|CMAKE_BUILD_TYPE|OGXM_FIXED_DRIVER):" "$BUILD_DIR/CMakeCache.txt")

DBG=""
[ "$TYPE" = "Debug" ] && DBG="-Debug"
SUFFIX="${FIXED:-Multi}"

_find_fw() {
    find "$BUILD_DIR" -maxdepth 1 "$@" \
        \( -name "*-${BOARD}${DBG}-${SUFFIX}.uf2" \
        -o -name "*-${BOARD}${DBG}-${SUFFIX}.elf" \)
}

mapfile -t FIRMWARE < <(_find_fw -newer /tmp/build_start)
if [ ${#FIRMWARE[@]} -eq 0 ]; then
    mapfile -t FIRMWARE < <(_find_fw)
    [ ${#FIRMWARE[@]} -gt 0 ] && echo "  (build fully cached -- using firmware from volume)"
fi

for f in "${FIRMWARE[@]}"; do
    cp "$f" /output/
    printf "  %s\n" "$(basename "$f")"
done
