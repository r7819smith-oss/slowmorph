#!/bin/sh
# Genvolve launcher for Knulli's Ports menu (RG40XX-V). Genvolve was called
# seq4h up to November 2026.
# Copy this file to /userdata/roms/ports/ and the genvolve folder (the genvolve
# binary plus seq4h.input) to /userdata/roms/ports/genvolve/, then refresh the
# game list. Delete the old SEQ4H.sh so the Ports menu shows only Genvolve.
#
# First start after the rename: if the old seq4h folder is next to this file,
# your saved work (seq4h.pat, seq4h.midi, seq4h-song1..8.pat) is copied into
# genvolve/. Files already in genvolve/ are never overwritten, and the old
# folder is left as it was, as a backup. The saved files keep their seq4h
# names and format, so older builds can still read them.
#
# Set GENVOLVE_MIDI to part of the MIDI port name, e.g. "FM" (empty = the port
# picked on M > MIDI OUTPUT, kept in genvolve/seq4h.midi; none picked = first
# port). Output and errors go to /userdata/roms/ports/genvolve/genvolve.log.
# Set GENVOLVE_SHOW_INPUT=1 (or start SHOW-BUTTONS.sh) to show each button's
# code on screen and in the log. M > BUTTON CODES does the same while running.
here="$(cd "$(dirname "$0")" && pwd)"
cd "$here/genvolve" || exit 1
exec >>genvolve.log 2>&1
echo "=== $(date) genvolve start"

# One-time copy of saved work from the old seq4h folder.
if [ -d "$here/seq4h" ]; then
    for f in "$here"/seq4h/seq4h.pat "$here"/seq4h/seq4h.midi "$here"/seq4h/seq4h-song*.pat; do
        [ -f "$f" ] || continue
        b=$(basename "$f")
        if [ ! -e "$b" ]; then
            cp -p "$f" "$b" && echo "copied old $f to genvolve/$b"
        fi
    done
fi

# If the kernel lacks USB MIDI support, matching modules built for this exact
# kernel can be placed in ./modules/ (see NOTES.md). Load them if present.
if [ -d modules ] && ls modules/*.ko >/dev/null 2>&1; then
    for m in snd-rawmidi snd-seq-device snd-seq snd-seq-midi-event snd-seq-midi \
             snd-hwdep snd-usbmidi-lib snd-usb-audio; do
        [ -f "modules/$m.ko" ] && insmod "modules/$m.ko" 2>/dev/null
    done
fi
modprobe -q snd-seq-midi 2>/dev/null   # creates sequencer ports for USB MIDI, if built
modprobe -q snd-usb-audio 2>/dev/null

SHOW=
[ "${GENVOLVE_SHOW_INPUT:-${SEQ4H_SHOW_INPUT:-0}}" = 1 ] && SHOW=--show-input
exec ./genvolve --fullscreen --midi "${GENVOLVE_MIDI:-${SEQ4H_MIDI:-}}" --pattern ./seq4h.pat $SHOW
