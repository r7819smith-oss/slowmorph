#!/bin/sh
# Reproducible aarch64 build of Genvolve (built as seq4h, its old name) for
# Knulli (RG40XX-V), packaged as a zip.
#
# Needs only Docker. Run from anywhere:
#   sequencer/knulli/build.sh
# Output:
#   sequencer/dist/genvolve       aarch64 binary
#   sequencer/dist/genvolve-knulli-aarch64.zip   unzip into /userdata/roms/ports/
#     (GENVOLVE.sh, SHOW-BUTTONS.sh, USB-MIDI-CHECK.sh, genvolve/genvolve,
#     genvolve/seq4h.input, genvolve/README.md; the zip keeps the execute bits)
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

# 1. Engine unit tests on the build machine (portable C, no SDL). The input
#    tests need SDL, which is only installed for arm64: they run under qemu below.
make clean PKG="$CROSS_PKG"
make tests/test_engine tests/test_gen tests/test_battery tests/test_usbinfo PKG="$CROSS_PKG"
./tests/test_engine
./tests/test_gen
./tests/test_battery
./tests/test_usbinfo

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
        tests/test_engine.c src/engine.c src/gen.c src/drum.c
    qemu-aarch64-static tests/test_engine.arm64
    aarch64-linux-gnu-gcc -O2 -mcpu=cortex-a53 -o tests/test_gen.arm64 \
        tests/test_gen.c src/engine.c src/gen.c src/drum.c
    qemu-aarch64-static tests/test_gen.arm64
    aarch64-linux-gnu-gcc -O2 -mcpu=cortex-a53 $($CROSS_PKG --cflags sdl2) -o tests/test_input.arm64 \
        tests/test_input.c src/input.c $($CROSS_PKG --libs sdl2)
    qemu-aarch64-static tests/test_input.arm64
    rm -f tests/test_engine.arm64 tests/test_gen.arm64 tests/test_input.arm64 smoke.bmp
    SDL_VIDEODRIVER=dummy qemu-aarch64-static ./seq4h --midi none --demo \
        --screenshot smoke.bmp --keys "Up,Right,z"
    test -s smoke.bmp || { echo "smoke test drew nothing" >&2; exit 1; }
    rm -f smoke.bmp seq4h.pat
    echo "aarch64 smoke test passed"
else
    echo "qemu-aarch64-static not found: skipping aarch64 smoke test"
fi

# 5. Package in the layout the Ports launcher expects. The source and make
#    target keep the old name seq4h; the installed app is genvolve.
rm -rf dist
mkdir -p dist/pkg/genvolve
cp seq4h dist/genvolve
cp seq4h dist/pkg/genvolve/genvolve
cp knulli/GENVOLVE.sh knulli/SHOW-BUTTONS.sh knulli/USB-MIDI-CHECK.sh dist/pkg/
cp README.md dist/pkg/genvolve/README.md
cp knulli/seq4h.input dist/pkg/genvolve/seq4h.input
chmod 755 dist/pkg/*.sh dist/pkg/genvolve/genvolve
# Fixed timestamps so the zip is byte-for-byte reproducible.
find dist/pkg -exec touch -h -d '2000-01-01 00:00:00' {} +
(cd dist/pkg && TZ=UTC zip -X -r -q ../genvolve-knulli-aarch64.zip GENVOLVE.sh SHOW-BUTTONS.sh USB-MIDI-CHECK.sh genvolve)
rm -rf dist/pkg
make clean PKG="$CROSS_PKG" >/dev/null
( cd dist && sha256sum genvolve genvolve-knulli-aarch64.zip )
