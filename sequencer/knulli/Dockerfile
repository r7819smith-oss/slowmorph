# Cross-build environment for seq4h on Knulli (aarch64).
#
# Ubuntu 20.04 is used on purpose: its glibc is 2.31, older than any Knulli
# release, so the binary only asks the device for symbols it has. SDL2 and
# ALSA are linked dynamically against the device's own libraries
# (libSDL2-2.0.so.0, libasound.so.2), so Knulli's own SDL video backend is used.
FROM ubuntu:20.04

# amd64 packages come from archive/security.ubuntu.com, arm64 from
# ports.ubuntu.com (multiarch). qemu-user-static runs the arm64 build as a
# smoke test.
RUN sed -i 's/^deb /deb [arch=amd64] /' /etc/apt/sources.list \
 && for s in focal focal-updates focal-security; do \
      echo "deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports $s main universe" \
        >> /etc/apt/sources.list; \
    done \
 && dpkg --add-architecture arm64 \
 && apt-get update \
 && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
      build-essential gcc-aarch64-linux-gnu libc6-dev-arm64-cross \
      binutils-aarch64-linux-gnu pkg-config make file zip qemu-user-static \
      libsdl2-dev:arm64 libasound2-dev:arm64 \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /src
