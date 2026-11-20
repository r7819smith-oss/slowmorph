#!/usr/bin/env python3
"""Check MIDI that seq4h sent, without any MIDI hardware.

Reads either
  * a capture file, from `seq4h --midi capture:FILE` (live, real timing) or
    `seq4h-render` (offline, ideal timing): "<microseconds> <hex bytes...>"; or
  * the text that `aseqdump` prints (with --aseqdump). aseqdump has no
    timestamps, so only the order, channels and note pairing are checked.

and checks that:
  * the run starts with MIDI Start and ends with Stop (unless --no-clock)
  * notes only use the expected channels (default 1-4: tracks 1-4)
  * every note-on gets a note-off, nothing is left hanging, and each channel
    plays one note at a time (each track is monophonic)
  * Stop is followed by all-notes-off (CC 123) on every channel
  * MIDI clock runs at 24 per beat at the expected tempo, with low jitter
  * every note starts on the 1/16 grid (allowing for swing), within tolerance
  * with --demo: the built-in demo pattern plays as written

Prints a report and exits 1 if any check fails. Python 3 standard library only.

  tools/midicheck.py run.cap --bpm 120 --demo
  tools/midicheck.py run.cap --bpm 140 --swing 6 --tolerance-ms 2
  aseqdump -p seq4h > dump.txt; tools/midicheck.py --aseqdump dump.txt
"""
import argparse
import json
import re
import statistics
import sys

PPQN = 96
TICKS_PER_STEP = 24


def parse_capture(path):
    msgs = []
    with open(path) as f:
        for n, line in enumerate(f, 1):
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split()
            try:
                t = int(parts[0])
                data = [int(b, 16) for b in parts[1:]]
            except ValueError:
                raise SystemExit(f"{path}:{n}: cannot parse {line!r}")
            if data:
                msgs.append((t, data))
    return msgs


ASEQ = [
    (re.compile(r"Note on\s+(\d+), note (\d+), velocity (\d+)"), lambda m: [0x90 | int(m[1]), int(m[2]), int(m[3])]),
    (re.compile(r"Note off\s+(\d+), note (\d+)(?:, velocity (\d+))?"), lambda m: [0x80 | int(m[1]), int(m[2]), int(m[3] or 0)]),
    (re.compile(r"Control change\s+(\d+), controller (\d+), value (\d+)"), lambda m: [0xB0 | int(m[1]), int(m[2]), int(m[3])]),
    (re.compile(r"\bClock\b"), lambda m: [0xF8]),
    (re.compile(r"\bStart\b"), lambda m: [0xFA]),
    (re.compile(r"\bContinue\b"), lambda m: [0xFB]),
    (re.compile(r"\bStop\b"), lambda m: [0xFC]),
]


def parse_aseqdump(path):
    msgs = []
    with open(path) as f:
        for line in f:
            if line.lstrip().startswith(("Source", "Waiting")):
                continue
            for rx, conv in ASEQ:
                m = rx.search(line)
                if m:
                    msgs.append((None, conv(m)))
                    break
    return msgs


def parse_channels(text):
    out = set()
    for part in text.split(","):
        if "-" in part:
            a, b = part.split("-")
            out.update(range(int(a), int(b) + 1))
        elif part:
            out.add(int(part))
    return out


class Report:
    def __init__(self):
        self.results = []
        self.info = {}

    def check(self, name, ok, detail=""):
        self.results.append((name, bool(ok), detail))

    @property
    def ok(self):
        return all(ok for _, ok, _ in self.results)

    def print(self):
        for k, v in self.info.items():
            print(f"  {k}: {v}")
        for name, ok, detail in self.results:
            print(f"{'PASS' if ok else 'FAIL'}  {name}" + (f"  ({detail})" if detail else ""))
        print("OK" if self.ok else "FAILED")


def pct(values, p):
    if not values:
        return 0.0
    s = sorted(values)
    return s[min(len(s) - 1, int(round(p / 100 * (len(s) - 1))))]


def analyse(msgs, a):
    r = Report()
    timed = all(t is not None for t, _ in msgs)
    allowed = parse_channels(a.channels)
    notes_on = [(t, d) for t, d in msgs if d[0] & 0xF0 == 0x90 and len(d) == 3 and d[2] > 0]
    r.info["messages"] = len(msgs)
    r.info["note-ons"] = len(notes_on)
    r.check("some notes were sent", notes_on, f"{len(notes_on)} note-ons")

    # Start / Stop framing
    starts = [i for i, (_, d) in enumerate(msgs) if d[0] == 0xFA]
    stops = [i for i, (_, d) in enumerate(msgs) if d[0] == 0xFC]
    if not a.no_clock:
        r.check("MIDI Start comes first", starts and starts[0] == 0, f"{len(starts)} Start")
        r.check("MIDI Stop comes last", stops and stops[-1] == len(msgs) - 1, f"{len(stops)} Stop")
    else:
        clocks = sum(1 for _, d in msgs if d[0] in (0xF8, 0xFA, 0xFC))
        r.check("no clock/start/stop with --no-clock", clocks == 0, f"{clocks} found")

    # Channels
    used = sorted({(d[0] & 0x0F) + 1 for _, d in notes_on})
    r.info["note channels (1-16)"] = used
    r.check(f"notes only on channels {a.channels}", set(used) <= allowed,
            "unexpected: " + str(sorted(set(used) - allowed)) if set(used) - allowed else "")
    if a.expect_channels:
        need = parse_channels(a.expect_channels)
        r.check(f"notes on every channel in {a.expect_channels}", need <= set(used),
                "missing: " + str(sorted(need - set(used))) if need - set(used) else "")

    # Note pairing, and monophony per channel (each track is monophonic)
    gates = []
    if a.poly:
        check_pairing_shared(r, msgs, timed, gates)
    else:
        check_pairing_mono(r, msgs, timed, gates)

    # All notes off after Stop
    if stops:
        after = {(d[0] & 0x0F) + 1 for _, d in msgs[stops[-1] - 2 * 16:] if d[0] & 0xF0 == 0xB0 and d[1] == 123}
        r.check("all-notes-off (CC 123) on each channel at Stop", set(used) <= after,
                "missing: " + str(sorted(set(used) - after)) if set(used) - after else "")

    if not timed:
        r.info["timing"] = "not checked (no timestamps)"
        return r

    # Clock
    step_us = 60e6 / a.bpm / 4
    clock_t = [t for t, d in msgs if d[0] == 0xF8]
    t0 = clock_t[0] if clock_t else (msgs[starts[0]][0] if starts else notes_on[0][0])
    tol = a.tolerance_ms * 1000
    tmax = (a.max_ms if a.max_ms else 4 * a.tolerance_ms) * 1000
    if not a.no_clock and len(clock_t) > 2:
        iv = [b - x for x, b in zip(clock_t, clock_t[1:])]
        expect = 60e6 / a.bpm / 24
        mean = statistics.mean(iv)
        bpm_seen = 60e6 / mean / 24
        jit = [abs(v - expect) for v in iv]
        # Lateness: where each clock lands against an ideal clock started at
        # t0. The timing thread uses absolute deadlines, so one late tick does
        # not push the following ones back; a steady climb would mean drift.
        drift = [abs(t - (t0 + k * expect)) for k, t in enumerate(clock_t)]
        r.info["tempo from clock"] = f"{bpm_seen:.3f} BPM (expected {a.bpm})"
        r.info["clock interval jitter"] = f"p50 {pct(jit, 50) / 1000:.3f} ms, p99 {pct(jit, 99) / 1000:.3f} ms, max {max(jit) / 1000:.3f} ms"
        r.info["clock error vs ideal grid"] = f"p50 {pct(drift, 50) / 1000:.3f} ms, p99 {pct(drift, 99) / 1000:.3f} ms, max {max(drift) / 1000:.3f} ms over {len(clock_t)} clocks"
        r.check("clock tempo within 0.5% of expected", abs(bpm_seen - a.bpm) / a.bpm < 0.005, f"{bpm_seen:.2f} BPM")
        r.check(f"clock on time: 99% within {a.tolerance_ms} ms, all within {tmax / 1000:g} ms",
                pct(drift, 99) <= tol and max(drift) <= tmax,
                f"p99 {pct(drift, 99) / 1000:.3f} ms, max {max(drift) / 1000:.3f} ms")
        if a.duration:
            want = a.duration * a.bpm * 24 / 60
            r.check("clock count matches run length (within 3%)", abs(len(clock_t) - want) <= want * 0.03 + 2,
                    f"{len(clock_t)} clocks, expected about {want:.0f}")

    # Onsets on the grid, allowing swing on odd steps
    swing_us = a.swing * step_us / TICKS_PER_STEP
    # One late tick delays every note on it, so count each onset time once.
    errs, worst, seen_t = [], (0, None), set()
    for t, d in notes_on:
        if t // 500 in seen_t:
            continue
        seen_t.add(t // 500)
        rel = t - t0
        k = round(rel / step_us)
        cands = [k - 1, k, k + 1]
        # With --offset N a note may sit up to N ticks either side of its grid
        # line, but still on an engine tick; --min-offset keeps it off the line.
        tick_us = step_us / TICKS_PER_STEP
        e = 0 if a.offset and rel <= tol else None   # an early first step plays at Start
        for j in cands:
            dt = rel - (j * step_us + (swing_us if j % 2 else 0))
            if abs(dt) <= a.offset * tick_us + tol:
                ej = abs(dt - round(dt / tick_us) * tick_us)
                if a.min_offset and round(abs(dt) / tick_us) < a.min_offset:
                    ej = max(ej, (a.min_offset - abs(dt) / tick_us) * tick_us)
            else:
                ej = abs(dt) - a.offset * tick_us
            e = ej if e is None else min(e, ej)
        if a.offset and rel <= tol:
            e = 0
        errs.append(e)
        if e > worst[0]:
            worst = (e, f"ch{(d[0] & 15) + 1} note {d[1]} at {rel / 1000:.3f} ms")
    r.info["note timing error"] = f"p50 {pct(errs, 50) / 1000:.3f} ms, p99 {pct(errs, 99) / 1000:.3f} ms, max {max(errs) / 1000:.3f} ms" if errs else "n/a"
    r.check(f"notes start on the 1/16 grid: 99% within {a.tolerance_ms} ms, all within {tmax / 1000:g} ms",
            errs and pct(errs, 99) <= tol and max(errs) <= tmax, "worst: " + worst[1] if worst[1] else "")
    if gates:
        r.info["gate lengths"] = f"min {min(gates) / 1000:.1f} ms, max {max(gates) / 1000:.1f} ms"
        limit = (a.max_hold + 1) * step_us
        what = "one step" if not a.max_hold else f"{a.max_hold + 1} steps (held notes)"
        r.check(f"gates are positive and no longer than {what}", min(gates) > 0 and max(gates) <= limit + tol)

    if a.demo:
        demo_checks(r, notes_on, t0, msgs[-1][0], step_us, swing_us)
    return r


def check_pairing_mono(r, msgs, timed, gates):
    sounding = {}                   # channel -> (note, start time, index)
    problems = []
    for i, (t, d) in enumerate(msgs):
        st, ch = d[0] & 0xF0, d[0] & 0x0F
        if st == 0x90 and d[2] > 0:
            if ch in sounding:
                problems.append(f"ch{ch + 1} note {d[1]} started while {sounding[ch][0]} still sounding (msg {i})")
            sounding[ch] = (d[1], t, i)
        elif st == 0x80 or (st == 0x90 and d[2] == 0):
            if ch in sounding and sounding[ch][0] == d[1]:
                if timed:
                    gates.append(t - sounding[ch][1])
                del sounding[ch]
            else:
                problems.append(f"ch{ch + 1} note-off {d[1]} with no matching note-on (msg {i})")
    for ch, (n, _, i) in sounding.items():
        problems.append(f"ch{ch + 1} note {n} never released (from msg {i})")
    r.check("every note-on has a matching note-off; one note per channel at a time",
            not problems, "; ".join(problems[:3]) + (f" ... {len(problems)} in all" if len(problems) > 3 else ""))


def check_pairing_shared(r, msgs, timed, gates):
    """Shared channel: pair note-offs with the oldest matching note-on. Two
    tracks playing the same note overlap, and the first note-off cuts both on
    the synth; that is reported, not failed, as it follows from sharing."""
    held = {}                       # (channel, note) -> [start times]
    problems, overlaps = [], 0
    for i, (t, d) in enumerate(msgs):
        st, key = d[0] & 0xF0, (d[0] & 0x0F, d[1] if len(d) > 1 else 0)
        if st == 0x90 and d[2] > 0:
            overlaps += bool(held.get(key))
            held.setdefault(key, []).append(t)
        elif st == 0x80 or (st == 0x90 and d[2] == 0):
            if held.get(key):
                t_on = held[key].pop(0)
                if timed:
                    gates.append(t - t_on)
            else:
                problems.append(f"ch{key[0] + 1} note-off {key[1]} with no matching note-on (msg {i})")
    for (ch, n), ts in held.items():
        if ts:
            problems.append(f"ch{ch + 1} note {n} never released")
    r.info["same-note overlaps on a shared channel"] = overlaps
    r.check("every note-on has a matching note-off (shared channel)", not problems, "; ".join(problems[:3]))


def demo_checks(r, notes_on, t0, end_t, step_us, swing_us):
    """The demo pattern from seq_load_demo() in src/engine.c."""
    want = {
        1: {0: 36, 4: 36, 8: 36, 12: 36},              # kick
        2: {2: 54, 6: 54, 10: 54, 14: 54},              # hats
        3: {0: 36, 3: 36, 6: 39, 10: 34, 12: 36},       # bass
        4: {0: 67, 4: 70, 7: 72, 11: 70, 14: 74},       # lead, 80% probability
    }
    seen = {ch: {} for ch in want}
    wrong = []
    for t, d in notes_on:
        ch = (d[0] & 15) + 1
        rel = t - t0
        k = round((rel - (swing_us if round(rel / step_us) % 2 else 0)) / step_us)
        bar, step = divmod(k, 16)
        if ch not in want or want[ch].get(step) != d[1]:
            wrong.append(f"ch{ch} step {step} note {d[1]}")
            continue
        seen[ch].setdefault(bar, set()).add(step)
    r.check("demo: only the demo's notes, on the demo's steps", not wrong, ", ".join(wrong[:4]))
    # Only whole bars count: a run that stops mid-bar leaves the last one short.
    bars = int((end_t - t0) // (16 * step_us))
    full = all(set(seen[ch].get(b, ())) == set(want[ch]) for ch in (1, 2, 3) for b in range(bars))
    r.check("demo: tracks 1-3 play every step in every bar", bars > 0 and full, f"{bars} bars")
    lead = sum(len(s) for b, s in seen[4].items() if b < bars)
    total = 5 * bars
    r.check("demo: track 4 (80% probability) plays 55-100% of its steps",
            total == 0 or 0.55 <= lead / total <= 1.0, f"{lead}/{total}")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("file")
    ap.add_argument("--aseqdump", action="store_true", help="input is aseqdump text, not a capture file")
    ap.add_argument("--bpm", type=float, default=120)
    ap.add_argument("--swing", type=int, default=0, help="swing in ticks (0-12), as in the pattern")
    ap.add_argument("--channels", default="1-4", help="channels notes may use, e.g. 1-4 or 5")
    ap.add_argument("--expect-channels", default="", help="channels that must each get notes")
    ap.add_argument("--offset", type=int, default=0, help="notes may be up to this many ticks (1/24 step) off the grid")
    ap.add_argument("--min-offset", type=int, default=0, help="...and at least this many ticks off it (NEVER ON GRID)")
    ap.add_argument("--tolerance-ms", type=float, default=2.0, help="timing error allowed for 99%% of events")
    ap.add_argument("--max-ms", type=float, default=0, help="largest timing error allowed (default 4x tolerance)")
    ap.add_argument("--poly", action="store_true",
                    help="several tracks share a channel (--channel N): allow overlapping notes per channel")
    ap.add_argument("--max-hold", type=int, default=0,
                    help="notes may be held over this many extra steps (the pattern uses HOLD)")
    ap.add_argument("--duration", type=float, default=0, help="run length in seconds, to check the clock count")
    ap.add_argument("--no-clock", action="store_true", help="the run used --no-clock")
    ap.add_argument("--demo", action="store_true", help="also check the built-in demo pattern")
    ap.add_argument("--json", help="also write the report as JSON to this file")
    a = ap.parse_args()

    msgs = parse_aseqdump(a.file) if a.aseqdump else parse_capture(a.file)
    if not msgs:
        print(f"{a.file}: no MIDI messages")
        return 1
    print(f"== {a.file}")
    segments, stray = split_runs(msgs, a.no_clock)
    reports = []
    if len(segments) > 1 or stray is not None:
        idle = sum(1 for _, d in msgs if d[0] == 0xF8) - sum(sum(1 for _, d in s if d[0] == 0xF8) for s in segments)
        print(f"  {len(segments)} Start..Stop run(s); {idle} clocks while stopped (seq4h is clock master)")
        r = Report()
        r.check("only MIDI clock while stopped (no notes outside Start..Stop)", not stray, stray or "")
        r.print()
        reports.append(r)
    for k, seg in enumerate(segments):
        if len(segments) > 1:
            print(f"-- run {k + 1} of {len(segments)}")
        r = analyse(seg, a)
        r.print()
        reports.append(r)
    ok = all(r.ok for r in reports)
    if a.json:
        with open(a.json, "w") as f:
            json.dump({"file": a.file, "ok": ok, "runs": [
                {"info": r.info, "checks": [{"name": n, "ok": o, "detail": d} for n, o, d in r.results]}
                for r in reports]}, f, indent=2)
    if len(reports) > 1:
        print("ALL OK" if ok else "SOME CHECKS FAILED")
    return 0 if ok else 1


def split_runs(msgs, no_clock):
    """Split into Start..Stop runs. seq4h sends clock while stopped, and a
    session may be started and stopped many times. Returns (runs, problem),
    where problem describes anything but clock found outside a run (None when
    the capture is exactly one run, as from --headless or seq4h-render)."""
    if no_clock or not any(d[0] == 0xFA for _, d in msgs):
        return [msgs], None
    runs, cur, stray = [], None, []
    for i, (t, d) in enumerate(msgs):
        if d[0] == 0xFA:
            if cur is not None:
                runs.append(cur)      # Start without Stop: close the open run
            cur = [(t, d)]
        elif cur is not None:
            cur.append((t, d))
            if d[0] == 0xFC:
                runs.append(cur)
                cur = None
        elif d[0] != 0xF8:
            stray.append(f"msg {i}: " + " ".join(f"{x:02X}" for x in d))
    if cur is not None:
        runs.append(cur)
    single = len(runs) == 1 and len(runs[0]) == len(msgs)
    return runs, None if single else ("; ".join(stray[:3]) if stray else "")


if __name__ == "__main__":
    sys.exit(main())
