#!/bin/sh
# Install or remove the Di1610 driver on macOS. Run as root from the repository or from an
# extracted release archive (both have the binaries in build/).
#   sudo sh macos/install.sh            install filter, PPD, km4usb service and the "Di1610" queue
#   sudo sh macos/install.sh uninstall
set -e
cd "$(dirname "$0")/.."
DEST=/Library/Printers/Di1610
LABEL=io.github.svincio.di1610.km4usb
PLIST=/Library/LaunchDaemons/$LABEL.plist
QUEUE=Di1610

if [ "$1" = uninstall ]; then
    lpadmin -x $QUEUE 2>/dev/null || true
    launchctl bootout system $PLIST 2>/dev/null || true
    rm -rf $DEST $PLIST
    exit 0
fi

for f in build/rastertokm4 build/km4usb; do
    [ -x $f ] || { echo "$f is missing: run make first (or use a release archive)"; exit 1; }
done
install -d -o root -g wheel -m 755 $DEST/Filter $DEST/PPDs $DEST/bin
install -o root -g wheel -m 555 build/rastertokm4 $DEST/Filter/
install -o root -g wheel -m 555 build/km4usb $DEST/bin/
install -o root -g wheel -m 644 ppd/Di1610.ppd $DEST/PPDs/
install -o root -g wheel -m 644 macos/$LABEL.plist $PLIST
xattr -cr $DEST $PLIST          # drop the quarantine flag of archives downloaded with a browser
launchctl bootout system $PLIST 2>/dev/null || true
launchctl bootstrap system $PLIST
lpadmin -p $QUEUE -D "KONICA MINOLTA Di1610" -E -v 'socket://127.0.0.1:9101/?snmp=false' \
        -P $DEST/PPDs/Di1610.ppd -o printer-error-policy=retry-job
echo "Installed. Test with: echo test | lp -d $QUEUE"
