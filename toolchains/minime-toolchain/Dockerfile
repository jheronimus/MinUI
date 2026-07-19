# Minime toolchain — Debian-based aarch64 cross-compilation environment.
# Installs the ARM64 cross-compiler, SDL2 development libraries,
# and other dependencies needed for MinUI's shared components.
FROM debian:bookworm-slim

RUN dpkg --add-architecture arm64 && \
    apt-get update && \
    apt-get install -y --no-install-recommends \
        ca-certificates \
        crossbuild-essential-arm64 \
        libsdl2-dev:arm64 \
        libsdl2-image-dev:arm64 \
        libsdl2-ttf-dev:arm64 \
        libdbus-1-dev:arm64 \
        liblz4-dev:arm64 \
        libpng-dev:arm64 \
        zlib1g-dev:arm64 \
        make \
        git \
        curl \
        && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

ENV CROSS_COMPILE=aarch64-linux-gnu-
ENV PLATFORM=minime
ENV UNION_PLATFORM=minime
ENV PREFIX=/usr/aarch64-linux-gnu

WORKDIR /root/workspace
