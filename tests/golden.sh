#!/bin/sh
# Golden test: rastertokm4 must reproduce the Windows driver captures byte-for-byte.
# The captures in tests/captures were made with the official Windows driver (print to FILE:).
# Their pages are decoded, wrapped into CUPS raster and fed to the filter. Run via `make test`.
set -e
cd "$(dirname "$0")/.."
B=build P=build/pages C=tests/captures F=build/rastertokm4 rc=0
cc -O2 -o $B/pbm2ras tests/pbm2ras.c -lcups
python3 tools/km4pages.py $C/* > /dev/null
t() { # name dpi wpt hpt mediapos pages...
  n=$1; shift; $B/pbm2ras "$@" > $B/$n.ras
  $F 1 u t 1 '' $B/$n.ras > $B/$n.out 2>/dev/null
  if cmp -s $C/$n $B/$n.out; then echo "OK    $n"; else echo "FAIL  $n"; rc=1; fi
}
t printX        600 595 842 0 $P/printX_p1.pbm
t X_tray1       600 595 842 1 $P/X_tray1_p1.pbm
t X_bypass      600 595 842 6 $P/X_bypass_p1.pbm
t X_300dpi      300 595 842 0 $P/X_300dpi_p1.pbm
t X_2pagine     600 595 842 0 $P/X_2pagine_p1.pbm $P/X_2pagine_p2.pbm $P/X_2pagine_p3.pbm
t X_layout_2in1 600 595 842 0 $P/X_layout_2in1_p1.pbm $P/X_layout_2in1_p2.pbm
t pieno_A4      600 595 842 0 $P/pieno_A4_p1.pbm
# X_Letter is kept as protocol reference only: the filter always sends A4.
exit $rc
