# OGX-Mini 2026 - ARM cross-compile dev image

ARG PICO_SDK_VERSION=2.1.0
ARG ARM_TOOLCHAIN_VERSION=13.2.Rel1

FROM debian:trixie-slim AS base

RUN rm -f /etc/apt/apt.conf.d/docker-clean \
    && echo 'Binary::apt::APT::Keep-Downloaded-Packages "true";' \
        > /etc/apt/apt.conf.d/keep-cache

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    apt-get update && apt-get install -y --no-install-recommends \
        git cmake ninja-build \
        gcc g++ \
        ca-certificates

FROM alpine/git AS pico-sdk-builder

ARG PICO_SDK_VERSION
RUN git clone --depth 1 --branch ${PICO_SDK_VERSION} \
        https://github.com/raspberrypi/pico-sdk.git /build/pico-sdk \
    && cd /build/pico-sdk \
    && git submodule update --init --depth 1

FROM base AS picotool-builder

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    apt-get update && apt-get install -y --no-install-recommends \
        libusb-1.0-0-dev pkg-config

COPY --from=pico-sdk-builder /build/pico-sdk /build/pico-sdk

ARG PICO_SDK_VERSION
RUN git clone --depth 1 --branch ${PICO_SDK_VERSION} \
        https://github.com/raspberrypi/picotool.git /build/picotool \
    && cmake -G Ninja \
        -S /build/picotool \
        -B /build/picotool/build \
        -DPICO_SDK_PATH=/build/pico-sdk \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/opt/picotool \
    && cmake --build /build/picotool/build \
    && cmake --install /build/picotool/build \
    && strip /opt/picotool/bin/picotool

FROM alpine/curl AS arm-toolchain-builder

ARG ARM_TOOLCHAIN_VERSION
# Set by BuildKit to the host platform unless --platform overrides it.
ARG TARGETARCH
RUN apk add --no-cache xz tar \
    && case "${TARGETARCH}" in \
         amd64) HOST_ARCH=x86_64 ;; \
         arm64) HOST_ARCH=aarch64 ;; \
         *) echo "Unsupported host arch: ${TARGETARCH}" >&2; exit 1 ;; \
       esac \
    && ARM_VER_LOWER=$(echo "${ARM_TOOLCHAIN_VERSION}" | tr '[:upper:]' '[:lower:]') \
    && ARM_TOOLCHAIN_DIR="arm-gnu-toolchain-${ARM_VER_LOWER}-${HOST_ARCH}-arm-none-eabi" \
    && curl -fL \
        "https://developer.arm.com/-/media/Files/downloads/gnu/${ARM_VER_LOWER}/binrel/${ARM_TOOLCHAIN_DIR}.tar.xz" \
        -o /tmp/arm-toolchain.tar.xz \
    && mkdir -p /opt/arm-toolchain \
    && tar -xJf /tmp/arm-toolchain.tar.xz -C /opt/arm-toolchain --strip-components=1 \
    && rm /tmp/arm-toolchain.tar.xz

FROM base

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    apt-get update && apt-get install -y --no-install-recommends \
        python3 python3-pycryptodome \
        libusb-1.0-0

# /usr/local puts picotoolConfig.cmake on cmake's default search path so
# find_package(picotool) resolves without extra hints (no network fetch).
COPY --from=picotool-builder /opt/picotool /usr/local

COPY --from=pico-sdk-builder /build/pico-sdk /opt/pico-sdk

COPY Firmware/external/patches/ /tmp/patches/
RUN cd /opt/pico-sdk \
    && find /tmp/patches -name "pico_sdk_*.diff" | sort | xargs git apply --ignore-whitespace \
    && rm -rf /tmp/patches

COPY --from=arm-toolchain-builder /opt/arm-toolchain /opt/arm-toolchain

ENV PICO_SDK_PATH=/opt/pico-sdk \
    PATH="/opt/arm-toolchain/bin:${PATH}"
WORKDIR /repo

COPY scripts/docker/container-build.sh /entrypoint.sh
ENTRYPOINT ["/entrypoint.sh"]
