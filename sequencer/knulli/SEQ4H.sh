#!/bin/sh
# Launcher for Knulli's Ports menu. Works on a Knulli RG40XX-V (2026-11-19).
# Copy this file to /userdata/roms/ports/ and the seq4h binary plus
# seq4h.input to /userdata/roms/ports/seq4h/, then refresh the game list.
# Set SEQ4H_MIDI to part of the MIDI port name, e.g. "FM" (empty = first port).
# Output and errors go to /userdata/roms/ports/seq4h/seq4h.log.
# Set SEQ4H_SHOW_INPUT=1 (or start SHOW-BUTTONS.sh) to show each button's code
# on screen and in the log. M > BUTTON CODES does the same while running.
cd "$(dirname "$0")/seq4h" || exit 1
exec >>seq4h.log 2>&1
echo "=== $(date) seq4h start"

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
[ "${SEQ4H_SHOW_INPUT:-0}" = 1 ] && SHOW=--show-input
exec ./seq4h --fullscreen --midi "${SEQ4H_MIDI:-}" --pattern ./seq4h.pat $SHOW
