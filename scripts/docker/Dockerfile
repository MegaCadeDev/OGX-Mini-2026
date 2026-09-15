# OGX-Mini 2026 - ARM cross-compile dev image

ARG PICO_SDK_VERSION=2.1.0
ARG ARM_TOOLCHAIN_VERSION=13.2.Rel1

FROM debian:trixie-slim AS pico-sdk-builder

RUN apt-get update && apt-get install -y --no-install-recommends \
        git ca-certificates \
    && rm -rf /var/lib/apt/lists/*

ARG PICO_SDK_VERSION
RUN git clone --depth 1 --branch ${PICO_SDK_VERSION} \
        https://github.com/raspberrypi/pico-sdk.git /build/pico-sdk \
    && cd /build/pico-sdk \
    && git submodule update --init --depth 1

FROM debian:trixie-slim AS picotool-builder

RUN apt-get update && apt-get install -y --no-install-recommends \
        git cmake ninja-build g++ \
        libusb-1.0-0-dev pkg-config \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

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

FROM debian:trixie-slim AS arm-toolchain-builder

ARG ARM_TOOLCHAIN_VERSION
RUN apt-get update && apt-get install -y --no-install-recommends \
        curl xz-utils ca-certificates \
    && rm -rf /var/lib/apt/lists/* \
    && ARM_VER_LOWER=$(echo "${ARM_TOOLCHAIN_VERSION}" | tr '[:upper:]' '[:lower:]') \
    && ARM_TOOLCHAIN_DIR="arm-gnu-toolchain-${ARM_VER_LOWER}-x86_64-arm-none-eabi" \
    && curl -fL \
        "https://developer.arm.com/-/media/Files/downloads/gnu/${ARM_VER_LOWER}/binrel/${ARM_TOOLCHAIN_DIR}.tar.xz" \
        -o /tmp/arm-toolchain.tar.xz \
    && mkdir -p /opt/arm-toolchain \
    && tar -xJf /tmp/arm-toolchain.tar.xz -C /opt/arm-toolchain --strip-components=1 \
    && rm /tmp/arm-toolchain.tar.xz

FROM debian:trixie-slim

# gcc/g++ needed for pioasm (ExternalProject, host build).
RUN apt-get update && apt-get install -y --no-install-recommends \
        git cmake ninja-build \
        gcc g++ \
        python3 python3-pycryptodome \
        libusb-1.0-0 \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# /usr/local puts picotoolConfig.cmake on cmake's default search path so
# find_package(picotool) resolves without extra hints. Source kept at
# /opt/picotool-src so PICOTOOL_FETCH_FROM_GIT_PATH avoids network fetches.
COPY --from=picotool-builder /opt/picotool /usr/local
COPY --from=picotool-builder /build/picotool /opt/picotool-src

COPY --from=pico-sdk-builder /build/pico-sdk /opt/pico-sdk

# Separate layer so patch updates don't bust the SDK copy above.
COPY Firmware/external/patches/pico_sdk_hids_host.diff /tmp/pico_sdk_hids_host.diff
RUN cd /opt/pico-sdk \
    && git apply --ignore-whitespace /tmp/pico_sdk_hids_host.diff \
    && rm /tmp/pico_sdk_hids_host.diff

COPY --from=arm-toolchain-builder /opt/arm-toolchain /opt/arm-toolchain

# Static config last so changes here don't bust the cache above
ENV PICO_SDK_PATH=/opt/pico-sdk \
    PICOTOOL_FETCH_FROM_GIT_PATH=/opt/picotool-src
ENV PATH="/opt/arm-toolchain/bin:${PATH}"
WORKDIR /repo
