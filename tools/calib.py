#!/usr/bin/env python3
"""Calibration pages: 50% Bayer gray band + cm ruler on the left. Finds how many cm of
'every-pixel' content the Di1610 decodes in real time. Writes calib_* KM4 jobs in cwd."""
import os, subprocess, sys
sys.path.insert(0, os.path.dirname(__file__))
import km4gen

def page(dpi, band_frac, path):
    m = dpi * 8 // 100
    w, h = round(210 / 25.4 * dpi) - 2 * m, round(297 / 25.4 * dpi) - 2 * m
    bpl, cm = (w + 7) // 8, dpi / 2.54
    bm = bytearray(bpl * h)
    def px(x, y): bm[y * bpl + (x >> 3)] |= 0x80 >> (x & 7)
    # ruler: tick every cm (from the sheet's top edge), long tick every 5 cm
    for c in range(1, 30):
        y0 = int(c * cm) - m
        if not 0 <= y0 < h - 4: continue
        for y in range(y0, y0 + max(2, dpi // 150)):
            for x in range(int(cm * (1.2 if c % 5 == 0 else 0.6))): px(x, y)
    # 50% ordered-dither band: 2 cm from the left, 3 cm to 28 cm from the top
    x0, x1 = int(2 * cm), int(2 * cm + (w - 2 * cm) * band_frac)
    pat = [[1 if km4gen_bayer[y][x] < 32 else 0 for x in range(8)] for y in range(8)]
    for y in range(int(3 * cm) - m, int(28 * cm) - m):
        for x in range(x0, x1):
            if pat[y & 7][x & 7]: px(x, y)
    pbm = path + '.pbm'
    open(pbm, 'wb').write(b'P4\n%d %d\n' % (w, h) + bm)
    subprocess.run([sys.executable, km4gen.__file__, path, '--dpi', str(dpi), pbm], check=True)
    os.remove(pbm)
    print(f'{path}: {os.path.getsize(path) // 1024} KiB')

km4gen_bayer = [[0, 32, 8, 40, 2, 34, 10, 42], [48, 16, 56, 24, 50, 18, 58, 26],
                [12, 44, 4, 36, 14, 46, 6, 38], [60, 28, 52, 20, 62, 30, 54, 22],
                [3, 35, 11, 43, 1, 33, 9, 41], [51, 19, 59, 27, 49, 17, 57, 25],
                [15, 47, 7, 39, 13, 45, 5, 37], [63, 31, 55, 23, 61, 29, 53, 21]]

page(600, 1.0, 'calib_600_piena')
page(600, 0.5, 'calib_600_meta')
page(300, 1.0, 'calib_300_piena')
