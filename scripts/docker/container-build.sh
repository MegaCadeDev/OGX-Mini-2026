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
trap '[ -f /tmp/build_log.txt ] && cp /tmp/build_log.txt /output/build_log.txt' EXIT
touch /tmp/build_start
OGXM_REPO_ROOT=/repo bash /tmp/build.sh

echo "==> Copying firmware"
BOARD=$(grep -m1 "^OGXM_BOARD:"        "$OGXM_BUILD_DIR/CMakeCache.txt" | cut -d= -f2)
TYPE=$( grep -m1 "^CMAKE_BUILD_TYPE:"  "$OGXM_BUILD_DIR/CMakeCache.txt" | cut -d= -f2)
FIXED=$(grep -m1 "^OGXM_FIXED_DRIVER:" "$OGXM_BUILD_DIR/CMakeCache.txt" | cut -d= -f2)
DBG=""; [ "$TYPE" = "Debug" ] && DBG="-Debug"
SUFFIX="${FIXED:-Multi}"
while IFS= read -r f; do
    cp "$f" /output/
    printf "  %s\n" "$(basename "$f")"
done < <(find "$OGXM_BUILD_DIR" -maxdepth 1 -newer /tmp/build_start \
    \( -name "*-${BOARD}${DBG}-${SUFFIX}.uf2" -o -name "*-${BOARD}${DBG}-${SUFFIX}.elf" \))
