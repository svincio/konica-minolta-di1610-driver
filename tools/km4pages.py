#!/usr/bin/env python3
"""Split KM4 (Di1610) jobs into pages: print each 64-byte header and decode its JBIG image
to build/pages/<file>_p<N>.pbm (+ .png on macOS). Needs `make tools` (jbigkit's jbgtopbm).
usage: km4pages.py FILE..."""
import os, shutil, subprocess, sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
JBG = os.path.join(ROOT, 'build/jbigkit-2.1/pbmtools/jbgtopbm')
OUT = os.path.join(ROOT, 'build/pages')
HDR = bytes.fromhex('010525000000') + b'\xff' * 12      # start of every page header

os.makedirs(OUT, exist_ok=True)
for f in sys.argv[1:]:
    d = open(f, 'rb').read()
    p, n = d.find(HDR), 0
    while p >= 0:
        n += 1
        h, bie = d[p:p + 64], p + 64
        end = d.find(b'\xff\x02\xff\x02', bie) + 4      # BIE ends with SDNORM + extra empty SDE
        nxt = d.find(b'\xff\x05', end)                   # first byte after the zero padding
        print(f'{f:16s} p{n} @0x{p:06x} hdr[22:48]={h[22:48].hex(" ")}  bie={end - bie:6d}  '
              f'bie+pad={nxt - bie:6d}  next={d[nxt:nxt + 6].hex(" ")}')
        base = os.path.join(OUT, f'{os.path.basename(f)}_p{n}')
        open(base + '.jbg', 'wb').write(d[bie:end - 2])
        r = subprocess.run([JBG, base + '.jbg', base + '.pbm'], capture_output=True, text=True)
        if r.returncode:
            print('   decode error:', r.stderr.strip())
        elif shutil.which('sips'):
            subprocess.run(['sips', '-Z', '600', '-s', 'format', 'png', base + '.pbm', '--out', base + '.png'],
                           capture_output=True)
        p = d.find(HDR, end)
