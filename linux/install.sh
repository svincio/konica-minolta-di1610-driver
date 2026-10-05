#!/bin/sh
# Install or remove the Di1610 driver on Linux (Debian, Raspberry Pi OS). Needs CUPS
# (apt install cups). Run as root from the repository or from an extracted release archive.
#   sudo sh linux/install.sh            install filter, USB quirk and the "Di1610" queue
#   sudo sh linux/install.sh uninstall
set -e
cd "$(dirname "$0")/.."
FILTER=/usr/lib/cups/filter/rastertokm4
QUIRKS=/usr/share/cups/usb/di1610.usb-quirks
QUEUE=Di1610

if [ "$1" = uninstall ]; then
    lpadmin -x $QUEUE 2>/dev/null || true
    rm -f $FILTER $QUIRKS
    exit 0
fi

[ -x build/rastertokm4 ] || { echo "build/rastertokm4 is missing: run make first (or use a release archive)"; exit 1; }
install -D -m 755 build/rastertokm4 $FILTER
install -D -m 644 linux/di1610.usb-quirks $QUIRKS
uri=$(lpinfo --include-schemes usb -v | awk '/usb:\/\/KONICA/ {print $2; exit}')
[ -n "$uri" ] || { echo "Di1610 not found on USB: turn it on and retry"; exit 1; }
ppd=$(mktemp)
sed 's|/Library/Printers/Di1610/Filter/||' ppd/Di1610.ppd > "$ppd"   # filter is in CUPS' own dir
lpadmin -p $QUEUE -D "KONICA MINOLTA Di1610" -E -v "$uri" -P "$ppd" -o printer-error-policy=retry-job
rm -f "$ppd"
echo "Installed. Test with: echo test | lp -d $QUEUE"
