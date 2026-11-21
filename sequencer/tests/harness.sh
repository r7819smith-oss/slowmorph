#!/bin/sh
# Hardware-free test harness for seq4h. Run from sequencer/ after `make seq4h
# seq4h-render` (or just `make check`). Everything is written to out/.
#
#   tests/harness.sh              all sections
#   tests/harness.sh render       offline renders: exact timing, channels, soak
#   tests/harness.sh live         the real binary and timing thread, headless,
#                                 MIDI captured to a file
#   tests/harness.sh screenshots  UI screenshots with the dummy video driver
#   tests/harness.sh persist      pattern slots survive a killed process and a
#                                 quit-and-relaunch (the real binary, keys replayed)
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
sections="${*:-render live screenshots persist alsa}"

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
    # The style starting sets, as NEW FROM STYLE builds them: held notes (up to
    # 32 steps), and DRONE's chords overlap on a channel, so --poly pairing.
    ./seq4h-render --style AMBIENT --bars 256 --out "$OUT/midi/render-ambient.cap"
    check render-ambient-256-bars "$OUT/midi/render-ambient.cap" --bpm 72 --max-hold 31 --expect-channels 1-4 --tolerance-ms 0.01
    ./seq4h-render --style DRONE --bars 256 --out "$OUT/midi/render-drone.cap"
    check render-drone-256-bars "$OUT/midi/render-drone.cap" --bpm 66 --poly --max-hold 31 --expect-channels 1-4 --tolerance-ms 0.01
    # Live shuffle (up to 11 ticks, NEVER ON GRID) on hats, and spread offsets
    # drifting with EVO OFFSET: every note on an engine tick, 2-11 ticks off
    # the grid, one note per channel at a time, nothing hung.
    ./seq4h-render --pattern tests/fixtures/shuffle.pat --bars 128 --out "$OUT/midi/render-shuffle.cap"
    check render-shuffle-128-bars "$OUT/midi/render-shuffle.cap" --bpm 120 --offset 11 --min-offset 2 --expect-channels 1-2 --tolerance-ms 0.01
    # Drum tracks: two lanes on one channel at once (so --poly pairing), a
    # techno kit chained over two slots with morphing, evolution and live
    # shuffle, and an ambient kit that re-rolls every 2 bars.
    ./seq4h-render --pattern tests/fixtures/drums.pat --bars 128 --out "$OUT/midi/render-drums.cap"
    check render-drums-128-bars "$OUT/midi/render-drums.cap" --bpm 124 --poly --offset 3 --expect-channels 1,2,4 --tolerance-ms 0.01
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
    # Fake battery: shots named bat17-* see 17% charging; the rest see none.
    rm -rf "$OUT/power"; mkdir -p "$OUT/power/bat17/battery"
    printf 'Battery\n' > "$OUT/power/bat17/battery/type"
    printf '17\n' > "$OUT/power/bat17/battery/capacity"
    printf 'Charging\n' > "$OUT/power/bat17/battery/status"
    # Fake USB for the M > MIDI shots: midi-* see a hub, the FM1 and a cheap
    # cable; midinone-* see nothing. SEQ4H_TEST_PORTS gives the null output
    # two pretend ports.
    rm -rf "$OUT/usb"; mkdir -p "$OUT/usb/none"
    usbdev() {   # dir name vid pid product class [interface-subclass driver]
        mkdir -p "$1/$2"
        printf '%s\n' "$3" > "$1/$2/idVendor"; printf '%s\n' "$4" > "$1/$2/idProduct"
        printf '%s\n' "$5" > "$1/$2/product"; printf '%s\n' "$6" > "$1/$2/bDeviceClass"
        if [ -n "${7:-}" ]; then
            mkdir -p "$1/$2:1.1"
            printf '01\n' > "$1/$2:1.1/bInterfaceClass"; printf '%s\n' "$7" > "$1/$2:1.1/bInterfaceSubClass"
            [ -n "${8:-}" ] && ln -s "../../../bus/usb/drivers/$8" "$1/$2:1.1/driver"
        fi
        return 0
    }
    usbdev "$OUT/usb/midi" 1-1 1a40 0101 "USB 2.0 Hub" 09
    usbdev "$OUT/usb/midi" 1-1.1 1a86 752d "USB2.0-MIDI" 00 03 snd-usb-audio
    usbdev "$OUT/usb/midi" 1-1.2 1209 4f46 "FM-1" 00 03 snd-usb-audio
    # name|keys. Keyboard: arrows = D-pad, Z = A, X = B (back to grid), C = X,
    # V = Y, Q/W = L1/R1 (page), 1/2 = L2/R2 (track), Space = Start.
    # Pages in order: GRID, STEP, TRACK, GENERATE.
    while IFS='|' read -r name keys; do
        [ -z "$name" ] && continue
        usb="$OUT/usb/none"; ports=
        case $name in
        midi-*) usb="$OUT/usb/midi"; ports="USB2.0-MIDI:USB2.0-MIDI MIDI 1,FM-1:FM-1 MIDI 1" ;;
        esac
        if [ -n "$ports" ]; then export SEQ4H_TEST_PORTS="$ports"; else unset SEQ4H_TEST_PORTS; fi
        songs=
        case $name in song-*) songs="$OUT/songs/$name"; rm -rf "$songs"; mkdir -p "$songs" ;; esac
        SEQ4H_USB_DIR="$usb" SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy SEQ4H_POWER_SUPPLY_DIR="$OUT/power/${name%%-*}" SEQ4H_BATTERY_PERCENT_FILE=/nonexistent ./seq4h --midi none --demo \
            --screenshot "$OUT/shots/$name.bmp" ${keys:+--keys "$keys"} ${songs:+--songs "$songs"} >/dev/null 2>&1
        [ -f "$OUT/shots/$name.bmp" ] || note "- **FAIL** $name: no screenshot written"
    done <<'EOF'
grid-demo|
grid-cursor-moved|Down,Down,Right,Right,Right
grid-step-toggled|Right,z
grid-step-locked|Right,v
step-page|Right,c
step-note-up|c,Down,Down,Right,Right
step-offset|c,Down,Down,Down,Down,Down,Down,Left,Left,Left,Left,Left
grid-hold-offset|c,Down,Down,Down,Down,Down,Right,Right,Down,Right,Right,Right,Right,Right,x,Right,Right,Right,Right,c,Left,Left,Left,Left,Left,Left,Left,Left,x,Right,Right,Right,Right,c,Up,Right,Right,Right,Right,Right,Right,x
track-page|w,w
track-shuffle|w,w,Down,Down,Down,Down,Down,Right,Right,Right,Right,Right,Right,Down,Right
generate-spread|w,w,w,Down,Down,Down,Down,Down,Down,Down,Down,Down,Down,Down,Down,Down,Down,Down,Down,Down,Down,Down,Down,Down,Right,Right,Right,Right,Right,Right,Right,Right
grid-after-spread|w,w,w,Down,Down,Down,Down,Down,Down,Down,Down,Down,Down,Down,Down,Down,Down,Down,Down,Down,Down,Down,Down,Down,Right,Right,Right,Right,Right,Right,Right,Right,z,x
track-length-12|w,w,Down,Down,Down,Left,Left,Left,Left
track2-muted|2,w,w,Down,Down,Down,Down,Right
generate-page|w,w,w
generate-euclid|w,w,w,Down,Right
grid-after-euclid-roll|w,w,w,Down,Right,z,x
grid-label-cursor|Left
grid-label-muted|Left,z
grid-label-solo|Down,Left,x,Down,x
grid-label-back|Left,Right
track-page-solo|Left,x,Down,x,w,w,Down,Down,Down,Down
bat17-grid|
menu-style-blank|m,Down,Right,Right,Right
grid-blank|m,Down,Right,Right,Right,Down,z,z
menu-style-drone|m,Down,Right,Right,Right,Right
grid-drone|m,Down,Right,Right,Right,Down,z,z,m,Up,Right,Down,z,z
generate-drone|m,Down,Right,Right,Right,Down,z,z,m,Up,Right,Down,z,z,w,w,w
generate-drone-chord|m,Down,Right,Right,Right,Down,z,z,m,Up,Right,Down,z,z,w,w,w,Down,Down,Down,Down,Down,Down,Down,Down,Down,Down,Down,Down,Down,Down,Down,Down,Down,Down,Down,Down,Down,Down,Down,Down,Down,Down,Down,Down,Down,Down,Down
slots-page|m,Down,Down,Down,z
slots-saved|m,Down,Down,Down,z,c,Down,c,c,c
slots-rename|m,Down,Down,Down,z,c,v,Up,Up,Up,Up,Right,Up,Up
slots-named|m,Down,Down,Down,z,c,v,Down,Down,Down,Down,Right,Down,z
slots-copy-panel|m,Down,Down,Down,z,c,Tab,Right
roll-page|Tab
roll-edited|Tab,Up,Up,z,+v,Right,Right,Right,-v,+c,Up,Up,Up,Right,Right,-c
roll-track2|Tab,2
track-type-drums|w,w,Down,Right
grid-drums|w,w,Down,Right,w,z,x
step-drums|w,w,Down,Right,w,z,x,w
drums-page|w,w,Down,Right,w,z,x,Tab
drums-edited|w,w,Down,Right,w,z,x,Tab,Down,z,Right,Right,+c,Down,Down,Left,-c,v
drums-label|w,w,Down,Right,w,z,x,Tab,Left
drums-lane-mute|w,w,Down,Right,w,z,x,Tab,Left,z
drums-lane-solo|w,w,Down,Right,w,z,x,Tab,Left,Down,Down,x
drum-map|m,Down,Down,Down,Down,Down,z
drum-map-rename|m,Down,Down,Down,Down,Down,z,Down,Right,Right,c,Up
drum-map-next-sound|m,Down,Down,Down,Down,Down,z,Down,+v,Right,Right,-v
drum-map-preset|m,Down,Down,Down,Down,Down,z,Up,Right
step-next-x|c,Down,Down,+c,Right,Right,-c
step-next-stick|c,Down,Down,period,period,period
roll-new-length|Tab,Right,+v,Right,Right,Right,-v,Up,Up,z
midi-page|m,Down,Down,Down,Down,Down,Down,z
midi-cable-row|m,Down,Down,Down,Down,Down,Down,z,Down
midi-cable-chosen|m,Down,Down,Down,Down,Down,Down,z,Down,z
midinone-page|m,Down,Down,Down,Down,Down,Down,z
menu-song-save|m,Down,Down,Down,Down
song-page|m,Down,Down,Down,Down,z
song-saved|m,Down,Down,Down,Down,z,c
song-load-confirm|m,Down,Down,Down,Down,z,c,z
song-loaded|m,Down,Down,Down,Down,z,c,z,z
song-rename|m,Down,Down,Down,Down,z,c,v,Up,Right,Up
song-named|m,Down,Down,Down,Down,z,c,v,Up,Right,Up,z
song-panel|m,Down,Down,Down,Down,z,c,Tab
EOF
    unset SEQ4H_TEST_PORTS
    "$PY" tools/shotcheck.py "$OUT"/shots/*.bmp \
        --differ grid-demo grid-cursor-moved --differ grid-demo grid-step-toggled \
        --differ grid-demo grid-step-locked --differ grid-demo step-page \
        --differ step-page step-note-up --differ step-page step-offset \
        --differ grid-demo grid-hold-offset --differ grid-demo track-page \
        --differ track-page track-shuffle --differ generate-page generate-spread --differ grid-demo grid-after-spread \
        --differ track-page track-length-12 --differ track-page track2-muted \
        --differ grid-demo generate-page --differ generate-page generate-euclid \
        --differ grid-demo grid-after-euclid-roll --differ grid-demo bat17-grid \
        --differ grid-demo grid-label-cursor --differ grid-label-cursor grid-label-muted \
        --differ grid-label-cursor grid-label-solo --differ grid-label-cursor grid-label-back \
        --differ track-page track-page-solo --differ grid-demo grid-blank --differ grid-blank grid-drone --differ grid-demo grid-drone --differ menu-style-blank menu-style-drone \
        --differ grid-drone generate-drone --differ generate-drone generate-drone-chord \
        \
        --differ grid-demo slots-page --differ slots-page slots-saved --differ slots-saved slots-rename \
        --differ slots-rename slots-named --differ slots-saved slots-copy-panel \
        --differ grid-demo roll-page --differ roll-page roll-edited --differ roll-page roll-track2 \
        --differ midi-page midi-cable-row --differ midi-cable-row midi-cable-chosen --differ midi-page midinone-page \
        --differ track-page track-type-drums --differ grid-demo grid-drums --differ step-page step-drums \
        --differ grid-drums drums-page --differ drums-page drums-edited --differ drums-page drums-label --differ drums-label drums-lane-mute --differ drums-label drums-lane-solo \
        --differ grid-demo drum-map --differ drum-map drum-map-rename \
        --differ drum-map drum-map-next-sound --differ drum-map drum-map-preset \
        --differ slots-page song-page --differ song-page song-saved --differ song-saved song-load-confirm \
        --differ song-load-confirm song-loaded --differ song-saved song-rename --differ song-rename song-named \
        --differ song-saved song-panel \
        --differ step-page step-next-x --differ step-next-x step-next-stick --differ roll-page roll-new-length \
        > "$OUT/shots/shotcheck.txt" 2>&1
    rc=$?
    cat "$OUT/shots/shotcheck.txt"
    result "$(ls "$OUT"/shots/*.png 2>/dev/null | wc -l) screenshots are 640x480, not blank, and each key sequence changed the screen" $rc
    note ""
    ;;
persist)
    note "### Saving (out/persist/)"
    P="$OUT/persist"
    rm -rf "$P"; mkdir -p "$P"
    # Generate page (w,w,w), down to SAVE TO SLOT, store slots 1-8 on track 1,
    # then slot 8 on track 2 (R2 = 2): nine stored slots.
    keys="w,w,w"
    i=0; while [ $i -lt 24 ]; do keys="$keys,Down"; i=$((i + 1)); done
    keys="$keys,z"
    i=0; while [ $i -lt 7 ]; do keys="$keys,Right,z"; i=$((i + 1)); done
    keys="$keys,2,z"
    # Then M > SLOTS on track 2 (opens on slot 8), Y rename, up turns the S
    # of "SLOT 8" into T, A keeps it: saved as "sn 1 7 TLOT_8".
    keys="$keys,m,Down,Down,Down,z,v,Up,z"
    # Killed with SIGKILL (as a system exit hotkey or a flat battery would):
    # nothing runs at exit, so only the autosave two seconds after the edits
    # can have written the file.
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ./seq4h --midi none --demo --pattern "$P/seq4h.pat" --keys "$keys" 2> "$P/run1.log" &
    pid=$!
    sleep 5
    kill -9 $pid 2>/dev/null; wait $pid 2>/dev/null
    cat "$P/run1.log"
    n=$(grep -c '^sl ' "$P/seq4h.pat" 2>/dev/null || echo 0)
    [ "$n" = 9 ] && [ ! -e "$P/seq4h.pat.tmp" ]
    result "autosave kept all 9 stored slots when the process was killed (found $n)" $?
    grep -q '^sn 1 7 TLOT_8$' "$P/seq4h.pat"
    result "a slot renamed on the SLOTS page was autosaved" $?
    # Relaunch, quit straight away (Escape): every slot loads, and the file
    # written at quit is byte-for-byte the one loaded.
    cp "$P/seq4h.pat" "$P/before.pat"
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout 20 ./seq4h --midi none --pattern "$P/seq4h.pat" --keys Escape 2> "$P/run2.log"
    rc=$?
    cat "$P/run2.log"
    [ $rc = 0 ] && grep -q 'loaded .*(9 stored slots)' "$P/run2.log" && cmp -s "$P/before.pat" "$P/seq4h.pat"
    result "relaunch loads all 9 slots and the slot name, and quit saves the same file" $?
    # M > MIDI: picking the second pretend port keeps it in seq4h.midi next to
    # the pattern, and the next start (no --midi name, as SEQ4H.sh runs it)
    # asks for that port.
    SEQ4H_TEST_PORTS="USB2.0-MIDI:USB2.0-MIDI MIDI 1,FM-1:FM-1 MIDI 1" SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
        timeout 20 ./seq4h --midi none --pattern "$P/seq4h.pat" --keys "m,Down,Down,Down,Down,Down,Down,z,Down,Down,z,Escape" 2> "$P/run3.log"
    [ "$(cat "$P/seq4h.midi" 2>/dev/null)" = "FM-1:FM-1 MIDI 1" ]
    result "a port picked on M > MIDI is kept in seq4h.midi" $?
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout 20 ./seq4h --midi "" --pattern "$P/seq4h.pat" --keys Escape 2> "$P/run4.log"
    cat "$P/run4.log"
    grep -q 'MIDI output from .*seq4h.midi: FM-1:FM-1 MIDI 1' "$P/run4.log"
    result "the next start uses the kept port" $?
    # M > SONG SAVE: X saves the session as song 1, Y renames it (up turns the
    # S of "SONG 1" into T, A keeps it), down + X saves song 2. Both files sit
    # next to seq4h.pat, written safely (no .tmp left).
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout 20 ./seq4h --midi none --pattern "$P/seq4h.pat" \
        --keys "m,Down,Down,Down,Down,z,c,v,Up,z,Down,c,Escape" 2> "$P/run5.log"
    [ -f "$P/seq4h-song1.pat" ] && [ -f "$P/seq4h-song2.pat" ] && [ ! -e "$P/seq4h-song1.pat.tmp" ] \
        && grep -q '^name TONG_1$' "$P/seq4h-song1.pat" && ! grep -q '^name ' "$P/seq4h-song2.pat" \
        && [ "$(grep -c '^sl ' "$P/seq4h-song1.pat")" = 9 ]
    result "SONG SAVE writes seq4h-song1.pat (named, with all 9 pattern slots) and seq4h-song2.pat" $?
    # A song file is a pattern file: it loads as seq4h.pat with --pattern.
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout 20 ./seq4h --midi none --pattern "$P/seq4h-song1.pat" --songs "$P/none" --keys Escape 2> "$P/run6.log"
    grep -q 'loaded .*(9 stored slots)' "$P/run6.log"
    result "a song file also loads as a pattern file" $?
    # Load song 1 into a session started at 211 BPM: A, A again. The session
    # saved at quit has the song's tempo and slots again.
    cp "$P/seq4h-song1.pat" "$P/song1-before.pat"
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout 20 ./seq4h --midi none --demo --bpm 211 --pattern "$P/seq4h.pat" \
        --keys "m,Down,Down,Down,Down,z,z,z,Escape" 2> "$P/run7.log"
    [ "$(grep '^bpm ' "$P/seq4h.pat")" = "$(grep '^bpm ' "$P/seq4h-song1.pat")" ] && ! grep -q '^bpm 211$' "$P/seq4h.pat" \
        && [ "$(grep -c '^sl ' "$P/seq4h.pat")" = 9 ] && cmp -s "$P/song1-before.pat" "$P/seq4h-song1.pat"
    result "loading song 1 brings back its tempo and slots, and leaves the song file as it was" $?
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
