# syntax=docker/dockerfile:1
#
# TrimUI Brick (tg5040, aarch64) mupen64plus builder image.
# Runs as the host arch (amd64/arm64) and emits aarch64 binaries via
# CROSS_COMPILE - no qemu involved.
#
# 1. GCC 16.2.0 cross toolchain (baked in from toolchain/*.tar.gz)
# 2. Brick sysroot (Linaro glibc 2.25 + TrimUI SDK usr + SDL2 GE8300),
#    downloaded at build time, cached in a BuildKit cache mount.

FROM ubuntu:26.04

# Host deps: the compiler itself needs glibc >= 2.38 and binutils >= 2.44
# (it emits .aeabi_subsection), plus libmpc/libmpfr/libgmp for cc1. Its
# binutils entries are symlinks to /usr/bin/aarch64-linux-gnu-*.
RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates curl tar gzip make patch git file \
        libmpc-dev libmpfr-dev libgmp-dev \
        binutils-aarch64-linux-gnu \
    && rm -rf /var/lib/apt/lists/*

# 1) Toolchain (baked in)
COPY toolchain/gcc16-aarch64-linux-gnu-cross.tar.gz /opt/downloads/
RUN mkdir -p /opt/gcc16-aarch64 \
    && tar -xzf /opt/downloads/gcc16-aarch64-linux-gnu-cross.tar.gz \
           -C /opt/gcc16-aarch64 --strip-components=1 \
    && rm -f /opt/downloads/gcc16-aarch64-linux-gnu-cross.tar.gz

# 2) Sysroot (downloaded, cached). /opt/seeded holds a host-side cache copy
#    (sysroot/cache/ in the repo) to skip the network when present.
COPY docker/fetch-sysroot.sh /opt/fetch-sysroot.sh
COPY sysroot/cache/ /opt/seeded/

RUN --mount=type=cache,id=brick-sysroot-downloads,target=/opt/downloads \
    /opt/fetch-sysroot.sh /opt/downloads /opt/seeded

# Installed at the compiler's baked-in --with-sysroot path so no --sysroot
# flag is needed at build time.
RUN --mount=type=cache,id=brick-sysroot-downloads,target=/opt/downloads \
    mkdir -p /usr/aarch64-linux-gnu \
    && tar -xzf /opt/downloads/aarch64-linux-gnu-7.5.0-linaro.tgz \
           -C /usr/aarch64-linux-gnu --strip-components=3 \
           aarch64-linux-gnu-7.5.0-linaro/aarch64-linux-gnu/libc \
    && tar -xzf /opt/downloads/SDK_usr_tg5040_a133p.tgz \
           -C /usr/aarch64-linux-gnu usr \
    && rm -f /opt/downloads/*.tgz

# 3) Component build script
COPY docker/build-all.sh /opt/build-all.sh
RUN chmod +x /opt/build-all.sh /opt/fetch-sysroot.sh \
    && sed -i 's/\r$//' /opt/build-all.sh /opt/fetch-sysroot.sh

ENV CROSS_COMPILE=/opt/gcc16-aarch64/bin/aarch64-linux-gnu- \
    SYSROOT=/usr/aarch64-linux-gnu \
    PATH=/opt/gcc16-aarch64/bin:$PATH

WORKDIR /build
