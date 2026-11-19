#!/usr/bin/env python3
"""Check and convert seq4h UI screenshots (BMP from `seq4h --screenshot`).

For each BMP: checks it is 640x480 and not blank (enough distinct colours, some
bright text pixels), writes a PNG next to it, and prints its pixel hash.
With --differ A B: fails if screenshots A and B are identical, which proves a
--keys sequence actually changed the screen.

  tools/shotcheck.py out/shots/*.bmp --differ grid-demo generate-page
Python 3 standard library only.
"""
import argparse
import hashlib
import os
import struct
import sys
import zlib


def read_bmp(path):
    with open(path, "rb") as f:
        data = f.read()
    if data[:2] != b"BM":
        raise ValueError("not a BMP")
    off = struct.unpack_from("<I", data, 10)[0]
    w, h = struct.unpack_from("<ii", data, 18)
    bpp = struct.unpack_from("<H", data, 28)[0]
    if bpp not in (24, 32):
        raise ValueError(f"{bpp}-bit BMP not supported")
    bypp = bpp // 8
    stride = (w * bypp + 3) & ~3
    rows = []
    for y in range(abs(h)):
        src = y if h < 0 else abs(h) - 1 - y      # positive height = bottom-up
        row = data[off + src * stride: off + src * stride + w * bypp]
        rgb = bytearray()
        for x in range(w):
            b, g, r = row[x * bypp: x * bypp + 3]
            rgb += bytes((r, g, b))
        rows.append(bytes(rgb))
    return w, abs(h), rows


def write_png(path, w, h, rows):
    def chunk(tag, body):
        return struct.pack(">I", len(body)) + tag + body + struct.pack(">I", zlib.crc32(tag + body) & 0xFFFFFFFF)
    raw = b"".join(b"\x00" + r for r in rows)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(raw, 9)))
        f.write(chunk(b"IEND", b""))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("bmps", nargs="+")
    ap.add_argument("--size", default="640x480")
    ap.add_argument("--differ", nargs=2, action="append", default=[], metavar=("A", "B"),
                    help="names (file stem) of two shots that must not be identical")
    a = ap.parse_args()
    want_w, want_h = map(int, a.size.split("x"))
    ok, hashes = True, {}
    for p in a.bmps:
        name = os.path.splitext(os.path.basename(p))[0]
        try:
            w, h, rows = read_bmp(p)
        except (OSError, ValueError) as e:
            print(f"FAIL  {name}: {e}")
            ok = False
            continue
        pix = b"".join(rows)
        colours = len({pix[i:i + 3] for i in range(0, len(pix), 3 * 7)})
        bright = sum(1 for i in range(0, len(pix), 3) if pix[i] > 200 and pix[i + 1] > 200)
        problems = []
        if (w, h) != (want_w, want_h):
            problems.append(f"size {w}x{h}, expected {want_w}x{want_h}")
        if colours < 4:
            problems.append(f"only {colours} colours (blank?)")
        if bright < 500:
            problems.append(f"only {bright} bright pixels (no text?)")
        hashes[name] = hashlib.sha256(pix).hexdigest()[:12]
        write_png(os.path.splitext(p)[0] + ".png", w, h, rows)
        print(f"{'FAIL' if problems else 'PASS'}  {name}: {w}x{h}, {colours}+ colours, hash {hashes[name]}"
              + (f"  ({'; '.join(problems)})" if problems else ""))
        ok &= not problems
    for x, y in a.differ:
        if x not in hashes or y not in hashes:
            print(f"FAIL  {x} vs {y}: screenshot missing")
            ok = False
        elif hashes[x] == hashes[y]:
            print(f"FAIL  {x} and {y} are identical (keys had no effect?)")
            ok = False
        else:
            print(f"PASS  {x} differs from {y}")
    print("OK" if ok else "FAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
