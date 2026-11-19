#!/bin/sh
# Launcher for Knulli's Ports menu. UNTESTED on a device.
# Copy this file to /userdata/roms/ports/ and the seq4h binary plus
# seq4h.input to /userdata/roms/ports/seq4h/, then refresh the game list.
# Set SEQ4H_MIDI to part of the MIDI port name, e.g. "FM" (empty = first port).
# Set SEQ4H_SHOW_INPUT=1 to show each button's code on screen; the codes and
# any errors are written to seq4h/seq4h.log.
cd "$(dirname "$0")/seq4h" || exit 1
SHOW=
[ "${SEQ4H_SHOW_INPUT:-0}" = 1 ] && SHOW=--show-input
exec ./seq4h --fullscreen --midi "${SEQ4H_MIDI:-}" --pattern ./seq4h.pat $SHOW 2>seq4h.log
