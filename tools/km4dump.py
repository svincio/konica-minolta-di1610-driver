#!/usr/bin/env python3
"""Segment a Di1610 (KM4) print-to-file dump into runs/blocks. Usage: km4dump.py FILE..."""
import sys

def segments(d):
    i, out = 0, []
    while i < len(d):
        for pat in (b'\xff\x05', b'\x05\xff', b'\x00\x00'):
            j = i
            while d[j:j+2] == pat: j += 2
            if j - i >= 32:
                out.append((i, j - i, 'run ' + pat.hex())); i = j; break
        else:
            j = i + 1
            while j < len(d) and not any(d[j:j+32] == p * 16 for p in (b'\xff\x05', b'\x05\xff', b'\x00\x00')): j += 1
            out.append((i, j - i, 'data')); i = j
    return out

for f in sys.argv[1:]:
    d = open(f, 'rb').read()
    print(f'=== {f} ({len(d)} bytes)')
    for off, n, kind in segments(d):
        print(f'  0x{off:06x} {n:7d}  {kind}')
        if kind == 'data':
            for k in range(0, min(n, 0x60), 16):
                print('      ' + d[off+k:off+min(k+16, n)].hex(' '))
            if n > 0x60: print('      ... ' + d[off+n-8:off+n].hex(' '))
