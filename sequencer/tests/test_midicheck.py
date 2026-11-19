#!/usr/bin/env python3
"""Proves tools/midicheck.py catches broken MIDI: feeds it a good capture,
then copies with one fault each, and expects only the good one to pass.

  tests/test_midicheck.py GOOD.cap      (GOOD.cap from seq4h-render --demo)
"""
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
CHECK = os.path.join(HERE, "..", "tools", "midicheck.py")


def load(path):
    out = []
    for line in open(path):
        if line.startswith("#") or not line.strip():
            continue
        t, *b = line.split()
        out.append([int(t), [int(x, 16) for x in b]])
    return out


def save(msgs, path):
    with open(path, "w") as f:
        for t, b in msgs:
            f.write(f"{t} " + " ".join(f"{x:02X}" for x in b) + "\n")


def first(msgs, pred):
    return next(i for i, (_, b) in enumerate(msgs) if pred(b))


def faults(good):
    def wrong_channel(m):           # a track-1 note sent on channel 7
        i = first(m, lambda b: b[0] == 0x90)
        j = first(m[i:], lambda b: b[0] == 0x80) + i
        m[i][1][0] = m[j][1][0] = 0x96

    def hanging_note(m):            # first note-off lost
        del m[first(m, lambda b: b[0] & 0xF0 == 0x80)]

    def late_note(m):               # one note 5 ms late
        m[first(m, lambda b: b[0] == 0x92)][0] += 5000

    def slow_clock(m):              # whole run 1% slow
        for x in m:
            x[0] = int(x[0] * 1.01)

    def missing_stop(m):
        del m[first(m, lambda b: b[0] == 0xFC)]

    def overlap(m):                 # a second note on channel 1 while one sounds
        i = first(m, lambda b: b[0] == 0x90)
        m.insert(i + 1, [m[i][0], [0x90, 50, 100]])
        m.insert(i + 2, [m[i][0] + 1000, [0x80, 50, 0]])

    def not_demo(m):                # a note the demo does not have
        m[first(m, lambda b: b[0] == 0x93)][1][1] = 61
        j = first(m, lambda b: b[0] == 0x83)
        m[j][1][1] = 61

    return [wrong_channel, hanging_note, late_note, slow_clock, missing_stop, overlap, not_demo]


def run(path):
    r = subprocess.run([sys.executable, CHECK, path, "--demo", "--tolerance-ms", "1"],
                       capture_output=True, text=True)
    return r.returncode, r.stdout


def main():
    good = load(sys.argv[1])
    ok = True
    with tempfile.TemporaryDirectory() as d:
        p = os.path.join(d, "good.cap")
        save(good, p)
        rc, out = run(p)
        print(f"{'PASS' if rc == 0 else 'FAIL'}  unchanged capture passes")
        if rc:
            print(out)
        ok &= rc == 0
        for f in faults(good):
            m = [[t, list(b)] for t, b in good]
            f(m)
            p = os.path.join(d, f.__name__ + ".cap")
            save(m, p)
            rc, out = run(p)
            fails = [ln for ln in out.splitlines() if ln.startswith("FAIL")]
            print(f"{'PASS' if rc else 'FAIL'}  {f.__name__} is caught" + (f": {fails[0][6:]}" if fails else ""))
            ok &= rc != 0
    print("OK" if ok else "FAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
