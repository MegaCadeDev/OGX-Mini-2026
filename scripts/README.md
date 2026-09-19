# scripts/

Build helpers for OGX-Mini firmware.

## Files

| Path                        | Purpose                                                        |
| --------------------------- | -------------------------------------------------------------- |
| `build.sh`                  | Interactive build script (Linux / macOS / Windows POSIX shell) |
| `build.ps1`                 | Interactive build script (Windows PowerShell)                  |
| `docker/docker-build.sh`    | Docker-based build -- no local toolchain required (see below)  |
| `docker/container-build.sh` | Runs inside the container; called by `docker-build.sh`         |
| `docker/Dockerfile`         | Multi-stage image: pico-sdk, picotool, ARM toolchain, runtime  |
| `build/`                    | Output directory -- `.uf2` and `.elf` files land here          |

## Native build

Requires: `git`, `python3`, `cmake`, `ninja`, `arm-none-eabi-gcc` on PATH.

```bash
# Linux / macOS / Windows (Git Bash, MSYS2, WSL)
./scripts/build.sh
```

```powershell
# Windows PowerShell
.\scripts\build.ps1
```

See [Building_From_Source.md](../Firmware/RP2040/docs/Building_From_Source.md) for full setup instructions.

## Docker build (no local toolchain required)

Requires: **Docker Desktop** (Windows / macOS) or **Docker Engine** (Linux). No ARM toolchain, CMake, Ninja, or pico-sdk install needed.

From the repository root (any POSIX shell -- Git Bash, MSYS2, or WSL on Windows):

```bash
./scripts/docker/docker-build.sh
```

What it does:

1. Builds (or reuses) the `ogx-mini-2026` image. The image is rebuilt automatically if the Dockerfile, the pico-sdk patch file, or the SDK version tag in `CMakeLists.txt` change.
2. Prompts for board, output mode, and build type -- same as `build.sh`.
3. Runs the build inside the container using a named Docker volume for incremental object files.
4. Copies only the `.uf2` (and `.elf` for debug) produced by the current run into `scripts/build/` on the host.

Flash the resulting `.uf2` via BOOTSEL as usual.

Only a plain `git clone` (no `--recursive`) is needed before the first run. Submodules are initialized automatically inside the container.

### First run

The first run builds the Docker image, which may take a few minutes to download the toolchain and pico-sdk. Subsequent runs reuse the cached image and the incremental build volume.

### Windows notes

- Run from a POSIX shell (Git Bash, MSYS2, or WSL) -- not PowerShell or CMD.
- Docker Desktop must be running before invoking the script.
