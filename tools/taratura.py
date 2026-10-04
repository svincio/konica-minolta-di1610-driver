#!/usr/bin/env python3
"""A4 tone-calibration page (PDF): 11-step gray wedge, smooth ramp, a photo, text.
usage: taratura.py PHOTO OUT.pdf"""
import sys
from PIL import Image, ImageDraw, ImageFont

DPI = 150
W, H = round(210 / 25.4 * DPI), round(297 / 25.4 * DPI)
mm = lambda v: round(v / 25.4 * DPI)
page = Image.new('L', (W, H), 255)
d = ImageDraw.Draw(page)
try:
    font = ImageFont.truetype('/System/Library/Fonts/Helvetica.ttc', mm(4))
except OSError:
    font = ImageFont.load_default()

d.text((mm(15), mm(12)), 'Di1610 taratura - scrivi a penna: dither / gamma', fill=0, font=font)
# 11-step wedge: coverage 0..100 %
x0, y0, s = mm(15), mm(25), mm(16)
for i in range(11):
    c = i * 10
    d.rectangle([x0 + i * s, y0, x0 + (i + 1) * s - mm(1), y0 + s], fill=round(255 * (1 - c / 100)))
    d.text((x0 + i * s + mm(3), y0 + s + mm(2)), f'{c}%', fill=0, font=font)
# smooth ramp white -> black
ry = y0 + s + mm(12)
for x in range(mm(176)):
    d.line([x0 + x, ry, x0 + x, ry + mm(12)], fill=round(255 * (1 - x / mm(176))))
# photo
photo = Image.open(sys.argv[1]).convert('L')
photo.thumbnail((mm(176), mm(150)))
page.paste(photo, (x0, ry + mm(20)))
# text sample, must stay solid black
d.text((x0, mm(270)), 'Testo nero pieno: AaBbCc 0123456789 - deve restare nitido', fill=0, font=font)
page.save(sys.argv[2], resolution=DPI)
