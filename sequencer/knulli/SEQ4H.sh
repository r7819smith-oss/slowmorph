#!/bin/sh
# Launcher for Knulli's Ports menu. UNTESTED on a device.
# Copy this file and the seq4h binary to /userdata/roms/ports/ (script) and
# /userdata/roms/ports/seq4h/ (binary), then refresh the game list.
# Set SEQ4H_MIDI to part of the MIDI port name, e.g. "FM" (empty = first port).
cd "$(dirname "$0")/seq4h" || exit 1
exec ./seq4h --fullscreen --midi "${SEQ4H_MIDI:-}" --pattern ./seq4h.pat
