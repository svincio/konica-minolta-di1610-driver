# Di1610 driver - macOS build.
# Needs: Xcode Command Line Tools, Homebrew libusb (brew install libusb); python3 for `make test`.
#   make                build into build/
#   make test           byte-exact check against the Windows driver captures
#   sudo make install   install filter, PPD, km4usb service and the "Di1610" print queue
#   sudo make uninstall

JBIG_VER = 2.1
JBIG_URL = https://www.cl.cam.ac.uk/~mgk25/jbigkit/download/jbigkit-$(JBIG_VER).tar.gz
JBIG_SHA = de7106b6bfaf495d6865c7dd7ac6ca1381bd12e0d81405ea81e7f2167263d932
JBIG     = build/jbigkit-$(JBIG_VER)
USB     ?= $(shell brew --prefix libusb 2>/dev/null)

DEST  = /Library/Printers/Di1610
LABEL = io.github.svincio.di1610.km4usb
PLIST = /Library/LaunchDaemons/$(LABEL).plist
QUEUE = Di1610
URI   = socket://127.0.0.1:9101/?snmp=false

all: build/rastertokm4 build/km4usb

$(JBIG)/libjbig/libjbig.a:
	mkdir -p build
	curl -fsSL -o build/jbigkit-$(JBIG_VER).tar.gz $(JBIG_URL)
	echo "$(JBIG_SHA)  build/jbigkit-$(JBIG_VER).tar.gz" | shasum -a 256 -c -
	tar -xzf build/jbigkit-$(JBIG_VER).tar.gz -C build
	$(MAKE) -C $(JBIG)/libjbig libjbig.a

build/rastertokm4: filter/rastertokm4.c $(JBIG)/libjbig/libjbig.a
	cc -O2 -Wall -I$(JBIG)/libjbig -o $@ filter/rastertokm4.c $(JBIG)/libjbig/libjbig.a -lcups

# libusb is linked statically: no runtime dependency on Homebrew.
build/km4usb: macos/km4usb.c
	@test -f "$(USB)/lib/libusb-1.0.a" || { echo "libusb not found: run 'brew install libusb' (and run make without sudo)"; exit 1; }
	mkdir -p build
	cc -O2 -Wall -I$(USB)/include/libusb-1.0 -o $@ macos/km4usb.c $(USB)/lib/libusb-1.0.a \
	   -lobjc -framework IOKit -framework CoreFoundation -framework Security

$(JBIG)/pbmtools/jbgtopbm: $(JBIG)/libjbig/libjbig.a
	$(MAKE) -C $(JBIG)/pbmtools jbgtopbm pbmtojbg

test: build/rastertokm4 $(JBIG)/pbmtools/jbgtopbm
	sh tests/golden.sh

install: build/rastertokm4 build/km4usb
	install -d -o root -g wheel -m 755 $(DEST)/Filter $(DEST)/PPDs $(DEST)/bin
	install -o root -g wheel -m 555 build/rastertokm4 $(DEST)/Filter/
	install -o root -g wheel -m 555 build/km4usb $(DEST)/bin/
	install -o root -g wheel -m 644 ppd/Di1610.ppd $(DEST)/PPDs/
	install -o root -g wheel -m 644 macos/$(LABEL).plist $(PLIST)
	-launchctl bootout system $(PLIST) 2>/dev/null
	launchctl bootstrap system $(PLIST)
	lpadmin -p $(QUEUE) -D "KONICA MINOLTA Di1610" -E -v '$(URI)' -P $(DEST)/PPDs/Di1610.ppd \
	        -o printer-error-policy=retry-job

uninstall:
	-lpadmin -x $(QUEUE)
	-launchctl bootout system $(PLIST)
	rm -rf $(DEST) $(PLIST)

clean:
	rm -rf build

.PHONY: all test install uninstall clean
