#!/bin/sh
# Build a release archive: the sources (git HEAD) plus the prebuilt binaries in build/.
# usage: tools/package.sh VERSION PLATFORM      -> dist/konica-minolta-di1610-driver-VERSION-PLATFORM.tar.gz
set -e
cd "$(dirname "$0")/.."
N=konica-minolta-di1610-driver-$1-$2
rm -rf "dist/$N"
mkdir -p "dist/$N/build"
git archive HEAD | tar -x -C "dist/$N"
for f in build/rastertokm4 build/km4usb; do
    if [ -f $f ]; then cp $f "dist/$N/build/"; fi
done
tar -czf "dist/$N.tar.gz" -C dist "$N"
echo "dist/$N.tar.gz"
