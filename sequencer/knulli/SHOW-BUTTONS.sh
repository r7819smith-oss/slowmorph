#!/bin/sh
# Ports menu entry: starts seq4h with each button's code shown on screen,
# top right (e.g. "JOY 10 = START"). Everything else works as normal.
# Same as M > BUTTON CODES, for when the M button itself does not work.
# Copy next to SEQ4H.sh in /userdata/roms/ports/.
SEQ4H_SHOW_INPUT=1 exec "$(dirname "$0")/SEQ4H.sh"
