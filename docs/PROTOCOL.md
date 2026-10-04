# The Di1610 "KM4" print protocol

The KONICA MINOLTA (Minolta) Dialta Di1610 is a host-based (GDI) printer: it understands
neither PCL nor PostScript. The Windows driver (`KM4*.dll`, version 10.3.2.0, 2007) renders
each page on the host and sends it as a JBIG1 bitmap inside a simple framing.

Everything below was reverse-engineered from "print to FILE:" captures of the official
Windows driver (see `tests/captures/`). `tools/km4gen.py` and `filter/rastertokm4.c`
regenerate **all** captures byte-for-byte (`make test`), so no field is guessed except
where marked *constant* (identical in every capture, meaning unknown).

## Job layout

```
ff 05 × 512, 05 ff × 512          preamble, 2048 bytes
job mark                          64 bytes
page block × N
job mark                          64 bytes
```

**Job mark**: `55`×6, `ff`×16, `55`×42 (64 bytes). It is the same at the start and at the end of the job.

## Page block

```
page header                       64 bytes
JBIG1 BIE + ff 02                 BIE followed by one extra empty SDE (SDNORM)
00 …                              zero padding up to N×1024 − 2 bytes (counted from the BIE start)
ff 05 ff 05 NN NN                 NN NN = N, number of 1 KiB blocks, big-endian
05 ff × 512, ff 05 × 510          filler, 2044 bytes
```

N is the smallest number of 1 KiB blocks such that `len(BIE + ff02) + 2 ≤ N × 1024`.

## Page header (64 bytes)

| Offset | Value | Meaning |
|---|---|---|
| 0–5 | `01 05 25 00 00 00` | constant |
| 6–21 | `ff` × 16 | constant |
| 22–28 | `01 01 02 05 00 0f 03` | constant |
| 29 | `02` / `00` | resolution: 600 dpi / 300 dpi |
| 30 | `02` / `0c` | paper: A4 / Letter |
| 31 | `00` / `01` / `06` | paper source: auto / tray 1 / bypass |
| 32 | `00` | constant |
| 33 | `01`, `02`, … | copies (the printer repeats the page) |
| 34 | `01`, `02`, … | page number |
| 35–37 | `00` | constant |
| 38–39 | LE16 | image width in pixels |
| 40–41 | LE16 | image height in pixels |
| 42–46 | `00` | constant |
| 47 | `03` | constant |
| 48–49 | `00 01` | constant |
| 50–63 | `00` | constant |

The image covers the printable area: the sheet minus 0.08" (≈2 mm) per side, i.e.
`round(sheet_inches × dpi) − 2 × (dpi × 0.08)`:

| Paper | 600 dpi | 300 dpi |
|---|---|---|
| A4 | 4865 × 6920 | 2432 × 3460 |
| Letter | 5004 × 6504 | 2502 × 3252 (derived, not captured) |

Only A4 and Letter were captured. Note that on the author's unit Letter jobs end in a
paper error (also from Windows), so the filter always sends A4.

## JBIG1 parameters

Standard JBIG1 (ITU-T T.82) bi-level image entity, 1 = black:

* DL = D = 0, P = 1 (single layer, single plane, sequential)
* L0 = image height (the whole page is one stripe)
* MX = MY = 0 (no adaptive-template moves)
* order = 3 (`ILEAVE | SMID`), options = 0x48 (`LRLTWO | TPBON`)

jbigkit 2.1 with `pbmtojbg -q -o 3 -p 72 -m 0 -s <height>` (or the equivalent
`jbg_enc_*` calls in the filter) produces output identical to the Windows encoder.

## Transport

USB printer class, interface 1 (VID 0x0686, PID 0x400D on the tested unit; interface 0 is the
vendor-specific scanner), bulk OUT endpoint 0x01. The device ID is
`MFG:KONICA MINOLTA;CMD:GDI;MDL:Di1610;CLS:PRINTER;`.

**The back channel must not be polled.** The Di1610 answers bulk-IN reads with STALL. The
macOS CUPS `usb` backend reads every 250 ms, and data still in flight while the sheet is
moving gets corrupted: pages larger than a few tens of KiB print fine for a few cm, then
turn into noise, and sometimes the firmware stops with `(01854) FMNEU.C`. Writing only,
as `macos/km4usb.c` does, prints everything correctly. Pages up to 650 KiB of JBIG data
have been printed this way. On Linux the stock CUPS `usb` backend
works once the `unidir` quirk (`linux/di1610.usb-quirks`) turns its back-channel reads off.

After a corrupted job the printer must be power-cycled before it accepts new jobs.

The Windows "language monitor" (`KM4MON.dll`) is a PJL monitor (`@PJL USTATUS …`). The printer
does not need it: raw jobs without any PJL print fine.

## Rendering notes

The engine has heavy dot gain. With a dispersed 8×8 Bayer screen, 50 % gray already prints
black and horizontal banding is strong. A clustered-dot 8×8 screen (45°, ≈106 lpi at 600 dpi)
with a gamma of 1.8 keeps the whole 0–100 % wedge distinguishable. The Windows driver also
lightens images: a solid black bitmap became 69 % coverage. Pure black and pure white are
left untouched, so text stays solid.
