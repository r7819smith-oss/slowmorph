#!/bin/sh
# Reproducible aarch64 build of seq4h for Knulli (RG40XX-V), packaged as a zip.
#
# Needs only Docker. Run from anywhere:
#   sequencer/knulli/build.sh
# Output:
#   sequencer/dist/seq4h          aarch64 binary
#   sequencer/dist/seq4h-knulli-aarch64.zip   unzip into /userdata/roms/ports/
#     (SEQ4H.sh, USB-MIDI-CHECK.sh, seq4h/seq4h, seq4h/seq4h.input,
#     seq4h/README.md; the zip keeps
#     the execute bits)
#
# SEQ4H_IMAGE=name   use an image you already built instead of building one.
# SEQ4H_IN_CONTAINER=1   skip Docker and use this machine's tools (used inside
#   the container, or on an Ubuntu 20.04 box with the Dockerfile's packages).
set -eu

here=$(cd "$(dirname "$0")" && pwd)
seq=$(dirname "$here")

if [ "${SEQ4H_IN_CONTAINER:-0}" != 1 ]; then
    image=${SEQ4H_IMAGE:-}
    if [ -z "$image" ]; then
        image=seq4h-knulli-build
        docker build -t "$image" "$here"
    fi
    exec docker run --rm -u "$(id -u):$(id -g)" -e SEQ4H_IN_CONTAINER=1 \
        -v "$seq:/src" -w /src "$image" sh knulli/build.sh
fi

cd "$seq"
: "${CROSS_PKG:=env PKG_CONFIG_LIBDIR=/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig pkg-config}"

# 1. Unit tests on the build machine (the engine is portable C).
make clean PKG="$CROSS_PKG"
make test PKG="$CROSS_PKG"

# 2. Cross-build. -mcpu matches the H700's Cortex-A53 cores.
make clean PKG="$CROSS_PKG"
make seq4h CC=aarch64-linux-gnu-gcc PKG="$CROSS_PKG" \
    CFLAGS="-O2 -mcpu=cortex-a53 -ffile-prefix-map=$seq=."
aarch64-linux-gnu-strip seq4h

# 3. Check what the binary needs from the device.
file seq4h
file seq4h | grep -q 'ARM aarch64' || { echo "not an aarch64 binary" >&2; exit 1; }
echo "Shared libraries needed:"
aarch64-linux-gnu-readelf -d seq4h | sed -n 's/.*(NEEDED).*\[\(.*\)\]/  \1/p'
aarch64-linux-gnu-readelf -d seq4h | grep -q 'libasound.so.2' \
    || { echo "built without ALSA MIDI output" >&2; exit 1; }
maxglibc=$(aarch64-linux-gnu-objdump -T seq4h | grep -o 'GLIBC_[0-9.]*' \
    | sort -t. -k1,1 -k2,2n -k3,3n -u | tail -1)
echo "Newest glibc symbol: $maxglibc"

# 4. Smoke test under qemu: unit tests built for aarch64, then the real binary
#    draws a frame with SDL's dummy video driver and the arm64 SDL library.
if command -v qemu-aarch64-static >/dev/null 2>&1; then
    aarch64-linux-gnu-gcc -O2 -mcpu=cortex-a53 -o tests/test_engine.arm64 \
        tests/test_engine.c src/engine.c src/gen.c
    qemu-aarch64-static tests/test_engine.arm64
    rm -f tests/test_engine.arm64 smoke.bmp
    SDL_VIDEODRIVER=dummy qemu-aarch64-static ./seq4h --midi none --demo \
        --screenshot smoke.bmp --keys "Up,Right,z"
    test -s smoke.bmp || { echo "smoke test drew nothing" >&2; exit 1; }
    rm -f smoke.bmp seq4h.pat
    echo "aarch64 smoke test passed"
else
    echo "qemu-aarch64-static not found: skipping aarch64 smoke test"
fi

# 5. Package in the layout the Ports launcher expects.
rm -rf dist
mkdir -p dist/pkg/seq4h
cp seq4h dist/seq4h
cp seq4h dist/pkg/seq4h/seq4h
cp knulli/SEQ4H.sh knulli/USB-MIDI-CHECK.sh dist/pkg/
cp README.md dist/pkg/seq4h/README.md
cp knulli/seq4h.input dist/pkg/seq4h/seq4h.input
chmod 755 dist/pkg/*.sh dist/pkg/seq4h/seq4h
# Fixed timestamps so the zip is byte-for-byte reproducible.
find dist/pkg -exec touch -h -d '2000-01-01 00:00:00' {} +
(cd dist/pkg && TZ=UTC zip -X -r -q ../seq4h-knulli-aarch64.zip SEQ4H.sh USB-MIDI-CHECK.sh seq4h)
rm -rf dist/pkg
make clean PKG="$CROSS_PKG" >/dev/null
( cd dist && sha256sum seq4h seq4h-knulli-aarch64.zip )
