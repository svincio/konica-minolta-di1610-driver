# Di1610 driver - macOS and Linux build.
# macOS needs: Xcode Command Line Tools, Homebrew libusb (brew install libusb).
# Linux (Debian, Raspberry Pi OS) needs: apt install cups libcups2-dev.
# python3 for `make test`.
#   make                build into build/
#   make test           byte-exact check against the Windows driver captures
#   sudo make install   install filter, PPD, transport and the "Di1610" print queue
#   sudo make uninstall
# Distribution packaging (Linux): make SYSTEM_JBIG=1 && make test SYSTEM_JBIG=1 \
#   && make install-pkg DESTDIR=... (system libjbig-dev + jbigkit-bin, no download, no print queue)

JBIG_VER = 2.1
JBIG_URL = https://www.cl.cam.ac.uk/~mgk25/jbigkit/download/jbigkit-$(JBIG_VER).tar.gz
JBIG_SHA = de7106b6bfaf495d6865c7dd7ac6ca1381bd12e0d81405ea81e7f2167263d932
JBIG     = build/jbigkit-$(JBIG_VER)

ifdef SYSTEM_JBIG
# Use the distribution's jbigkit (libjbig-dev, jbigkit-bin): nothing is downloaded.
JBIG_CFLAGS =
JBIG_LIBS   = -ljbig
JBIG_DEP    =
JBGTOPBM    = $(shell command -v jbgtopbm)
else
JBIG_CFLAGS = -I$(JBIG)/libjbig
JBIG_LIBS   = $(JBIG)/libjbig/libjbig.a
JBIG_DEP    = $(JBIG)/libjbig/libjbig.a
JBGTOPBM    = $(JBIG)/pbmtools/jbgtopbm
endif

ifeq ($(shell uname),Linux)
# Linux: the stock CUPS usb backend, with the back channel turned off by the "unidir" quirk.
OS = linux
all: build/rastertokm4
else
OS = macos
USB ?= $(shell brew --prefix libusb 2>/dev/null)
all: build/rastertokm4 build/km4usb

# libusb is linked statically: no runtime dependency on Homebrew.
build/km4usb: macos/km4usb.c
	@test -f "$(USB)/lib/libusb-1.0.a" || { echo "libusb not found: run 'brew install libusb' (and run make without sudo)"; exit 1; }
	mkdir -p build
	cc -O2 -Wall -I$(USB)/include/libusb-1.0 -o $@ macos/km4usb.c $(USB)/lib/libusb-1.0.a \
	   -lobjc -framework IOKit -framework CoreFoundation -framework Security
endif

# The install logic lives in <os>/install.sh, also used by the prebuilt release archives.
install: all
	sh $(OS)/install.sh

uninstall:
	sh $(OS)/install.sh uninstall

$(JBIG)/libjbig/libjbig.a:
	mkdir -p build
	curl -fsSL -o build/jbigkit-$(JBIG_VER).tar.gz $(JBIG_URL)
	echo "$(JBIG_SHA)  build/jbigkit-$(JBIG_VER).tar.gz" | shasum -a 256 -c -
	tar -xzf build/jbigkit-$(JBIG_VER).tar.gz -C build
	$(MAKE) -C $(JBIG)/libjbig libjbig.a

build/rastertokm4: filter/rastertokm4.c $(JBIG_DEP)
	mkdir -p build
	$(CC) $(CFLAGS) $(CPPFLAGS) -O2 -Wall $(JBIG_CFLAGS) -o $@ filter/rastertokm4.c $(JBIG_LIBS) $(LDFLAGS) -lcups -lm


$(JBIG)/pbmtools/jbgtopbm: $(JBIG)/libjbig/libjbig.a
	$(MAKE) -C $(JBIG)/pbmtools jbgtopbm pbmtojbg

ifndef SYSTEM_JBIG
test: $(JBIG)/pbmtools/jbgtopbm
endif
test: build/rastertokm4
	JBGTOPBM=$(JBGTOPBM) sh tests/golden.sh

# Linux packaging: files only, laid out for CUPS (no print queue: CUPS matches the PPD by IEEE 1284 id).
CUPS_SERVERBIN ?= $(shell cups-config --serverbin 2>/dev/null || echo /usr/lib/cups)
CUPS_DATADIR   ?= $(shell cups-config --datadir 2>/dev/null || echo /usr/share/cups)
DOCDIR         ?= /usr/share/doc/konica-minolta-di1610-driver
install-pkg: build/rastertokm4
	install -D -m 755 build/rastertokm4 $(DESTDIR)$(CUPS_SERVERBIN)/filter/rastertokm4
	install -D -m 644 linux/di1610.usb-quirks $(DESTDIR)$(CUPS_DATADIR)/usb/di1610.usb-quirks
	mkdir -p $(DESTDIR)$(CUPS_DATADIR)/model
	sed 's|/Library/Printers/Di1610/Filter/||' ppd/Di1610.ppd | gzip -9n > $(DESTDIR)$(CUPS_DATADIR)/model/Di1610.ppd.gz
	install -D -m 644 docs/PROTOCOL.md $(DESTDIR)$(DOCDIR)/PROTOCOL.md

clean:
	rm -rf build

.PHONY: all test install-pkg install uninstall clean
