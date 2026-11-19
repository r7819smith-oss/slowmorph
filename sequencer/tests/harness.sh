#!/bin/sh
# Hardware-free test harness for seq4h. Run from sequencer/ after `make seq4h
# seq4h-render` (or just `make check`). Everything is written to out/.
#
#   tests/harness.sh              all sections
#   tests/harness.sh render       offline renders: exact timing, channels, soak
#   tests/harness.sh live         the real binary and timing thread, headless,
#                                 MIDI captured to a file
#   tests/harness.sh screenshots  UI screenshots with the dummy video driver
#   tests/harness.sh alsa         real ALSA sequencer output into aseqdump
#                                 (skipped if /dev/snd/seq or aseqdump is missing)
#
# LIVE_TOL_MS: timing error allowed for 99% of events in live runs (default 3 ms;
# shared CI machines are noisy). LIVE_MAX_MS: worst single event (default 20 ms).
# On the handheld, aim for LIVE_TOL_MS=2 LIVE_MAX_MS=8.
set -u
cd "$(dirname "$0")/.." || exit 1
OUT=out
mkdir -p "$OUT/midi" "$OUT/shots"
SUMMARY="$OUT/summary.md"
LIVE_TOL_MS="${LIVE_TOL_MS:-3}"
LIVE_MAX_MS="${LIVE_MAX_MS:-20}"
PY="${PYTHON:-python3}"
fails=0
sections="${*:-render live screenshots alsa}"

note() { printf '%s\n' "$*" | tee -a "$SUMMARY"; }
result() {   # name status
    if [ "$2" = 0 ]; then note "- PASS $1"; else note "- **FAIL** $1"; fails=$((fails + 1)); fi
}
# check NAME FILE midicheck-args...
check() {
    name=$1; file=$2; shift 2
    "$PY" tools/midicheck.py "$file" --json "$OUT/midi/$name.json" "$@" > "$OUT/midi/$name.txt" 2>&1
    rc=$?
    cat "$OUT/midi/$name.txt"
    timing=$(grep -E 'note timing error|clock interval jitter' "$OUT/midi/$name.txt" | sed 's/^ *//' | tr '\n' ';')
    result "$name ${timing:+($timing)}" $rc
}

: > "$SUMMARY"
note "## seq4h hardware-free harness"
note ""

for sec in $sections; do
case $sec in
render)
    note "### Offline renders (exact timing)"
    ./seq4h-render --demo --bars 8 --out "$OUT/midi/render-demo.cap"
    check render-demo "$OUT/midi/render-demo.cap" --bpm 120 --demo --expect-channels 1-4 --tolerance-ms 0.01
    ./seq4h-render --demo --bars 8 --bpm 90 --swing 6 --out "$OUT/midi/render-swing.cap"
    check render-swing-90bpm "$OUT/midi/render-swing.cap" --bpm 90 --swing 6 --demo --tolerance-ms 0.01
    ./seq4h-render --demo --bars 4 --channel 5 --out "$OUT/midi/render-sel.cap"
    check render-one-channel-5 "$OUT/midi/render-sel.cap" --poly --channels 5 --expect-channels 5 --tolerance-ms 0.01
    ./seq4h-render --demo --bars 4 --no-clock --out "$OUT/midi/render-noclock.cap"
    check render-no-clock "$OUT/midi/render-noclock.cap" --no-clock --tolerance-ms 0.01
    # Soak: generators, evolution every bar, polymeter, swing, probability.
    ./seq4h-render --pattern tests/fixtures/evolve.pat --bars 256 --out "$OUT/midi/render-evolve.cap"
    check render-evolve-256-bars "$OUT/midi/render-evolve.cap" --bpm 140 --swing 6 --expect-channels 1-4 --tolerance-ms 0.01
    ./seq4h-render --pattern tests/fixtures/evolve.pat --bars 256 --out "$OUT/midi/render-evolve-2.cap"
    cmp -s "$OUT/midi/render-evolve.cap" "$OUT/midi/render-evolve-2.cap"
    result "same pattern renders identically twice" $?
    "$PY" tests/test_midicheck.py "$OUT/midi/render-demo.cap"
    result "midicheck catches broken MIDI (wrong channel, hung note, late note, slow clock...)" $?
    note ""
    ;;
live)
    note "### Live runs (timing thread, captured to file, 99% within ${LIVE_TOL_MS} ms, all within ${LIVE_MAX_MS} ms)"
    ./seq4h --midi capture:"$OUT/midi/live-demo.cap" --demo --headless 10
    check live-demo-120bpm "$OUT/midi/live-demo.cap" --bpm 120 --demo --duration 10 --expect-channels 1-4 --tolerance-ms "$LIVE_TOL_MS" --max-ms "$LIVE_MAX_MS"
    ./seq4h --midi capture:"$OUT/midi/live-fast.cap" --demo --bpm 300 --headless 5
    check live-demo-300bpm "$OUT/midi/live-fast.cap" --bpm 300 --demo --duration 5 --tolerance-ms "$LIVE_TOL_MS" --max-ms "$LIVE_MAX_MS"
    ./seq4h --midi capture:"$OUT/midi/live-evolve.cap" --pattern tests/fixtures/evolve.pat --headless 10
    check live-evolve-140bpm "$OUT/midi/live-evolve.cap" --bpm 140 --swing 6 --duration 10 --tolerance-ms "$LIVE_TOL_MS" --max-ms "$LIVE_MAX_MS"
    note ""
    ;;
screenshots)
    note "### Screenshots (out/shots/*.png)"
    rm -f "$OUT"/shots/*.bmp "$OUT"/shots/*.png
    # name|keys. Keyboard: arrows = D-pad, Z = A, X = B (back to grid), C = X,
    # V = Y, Q/W = L1/R1 (page), 1/2 = L2/R2 (track), Space = Start.
    # Pages in order: GRID, STEP, TRACK, GENERATE.
    while IFS='|' read -r name keys; do
        [ -z "$name" ] && continue
        SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ./seq4h --midi none --demo \
            --screenshot "$OUT/shots/$name.bmp" ${keys:+--keys "$keys"} >/dev/null 2>&1
        [ -f "$OUT/shots/$name.bmp" ] || note "- **FAIL** $name: no screenshot written"
    done <<'EOF'
grid-demo|
grid-cursor-moved|Down,Down,Right,Right,Right
grid-step-toggled|Right,z
grid-step-locked|Right,v
step-page|Right,c
step-note-up|c,Down,Down,Right,Right
track-page|w,w
track-length-12|w,w,Down,Down,Left,Left,Left,Left
track2-muted|2,w,w,Down,Down,Down,Right
generate-page|w,w,w
generate-euclid|w,w,w,Down,Right
grid-after-euclid-roll|w,w,w,Down,Right,z,x
EOF
    "$PY" tools/shotcheck.py "$OUT"/shots/*.bmp \
        --differ grid-demo grid-cursor-moved --differ grid-demo grid-step-toggled \
        --differ grid-demo grid-step-locked --differ grid-demo step-page \
        --differ step-page step-note-up --differ grid-demo track-page \
        --differ track-page track-length-12 --differ track-page track2-muted \
        --differ grid-demo generate-page --differ generate-page generate-euclid \
        --differ grid-demo grid-after-euclid-roll \
        > "$OUT/shots/shotcheck.txt" 2>&1
    rc=$?
    cat "$OUT/shots/shotcheck.txt"
    result "$(ls "$OUT"/shots/*.png 2>/dev/null | wc -l) screenshots are 640x480, not blank, and each key sequence changed the screen" $rc
    note ""
    ;;
alsa)
    note "### ALSA sequencer loopback"
    if [ ! -e /dev/snd/seq ] || ! command -v aseqdump >/dev/null 2>&1; then
        note "- SKIP no /dev/snd/seq or no aseqdump (try: sudo modprobe snd-seq; apt install alsa-utils)"
    else
        aseqdump > "$OUT/midi/alsa-aseqdump.txt" 2>&1 &
        dump=$!
        sleep 1
        ./seq4h --midi list > "$OUT/midi/alsa-ports.txt" 2>&1
        cat "$OUT/midi/alsa-ports.txt"
        ./seq4h --midi aseqdump --demo --headless 4 2> "$OUT/midi/alsa-seq4h.txt"
        rc=$?
        sleep 0.5
        kill $dump 2>/dev/null; wait $dump 2>/dev/null
        if grep -q "running without MIDI output" "$OUT/midi/alsa-seq4h.txt"; then
            cat "$OUT/midi/alsa-seq4h.txt"
            result "seq4h found and connected to the aseqdump port" 1
        else
            result "seq4h found and connected to the aseqdump port" $rc
            check alsa-aseqdump "$OUT/midi/alsa-aseqdump.txt" --aseqdump --expect-channels 1-4
        fi
    fi
    note ""
    ;;
*) echo "unknown section: $sec"; exit 2 ;;
esac
done

if [ "$fails" = 0 ]; then note "**All harness checks passed.**"; else note "**$fails harness check(s) failed.**"; fi
exit $((fails > 0))
