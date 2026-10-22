# Getting the driver into Linux distributions

Prerequisite for everything: tag a release that contains `make install-pkg` (v1.1.0), then
`makepkg -g` / `uscan` will find the tarball.

Tested here (Debian 13 arm64 container): `make SYSTEM_JBIG=1 && make test SYSTEM_JBIG=1`,
`dpkg-buildpackage` OK, `lintian --pedantic` clean except the placeholder ITP bug number,
`cupstestppd` PASS, and `lpinfo --device-id` / `lpinfo -m` list the PPD.

## 1. Arch (AUR) - immediate, no review
`packaging/arch/PKGBUILD`: after the tag, `makepkg -g` for the sha256, test with `makepkg -s`,
then push to `ssh://aur@aur.archlinux.org/konica-minolta-di1610-driver.git` (needs an AUR account).

## 2. Debian (-> Ubuntu, Mint, Raspberry Pi OS)
`packaging/debian/` is a complete source package (`printer-driver-di1610`). Steps:
1. File an ITP bug against `wnpp` (reportbug wnpp: "ITP: konica-minolta-di1610-driver -- CUPS driver
   for KONICA MINOLTA Dialta Di1610") and put its number in debian/changelog.
2. Copy `packaging/debian` to the root of the release tarball, build with `sbuild`/`dpkg-buildpackage`.
3. Upload to mentors.debian.net and file an RFS (sponsorship request) on debian-mentors; a DD uploads it.
   The Debian Printing team (debian-printing@lists.debian.org) is the natural sponsor.
Alternative with no packaging work on your side: open an RFP bug and wait for a volunteer.

## 3. Fedora
Needs a spec file and a package review request on Bugzilla (needs a FAS account and a sponsor
for the first package). Not prepared yet: do it after Debian/Arch.

## 4. OpenPrinting (printer catalog)
OpenPrinting/foomatic-db holds the XML entries behind the "Printer Compatibility" database.
`packaging/openprinting/` contains the driver and printer entries, modelled on `brlaser`.
Pull request: put them in `db/source/driver/di1610.xml` and `db/source/printer/KONICA_MINOLTA-Di1610.xml`
(needs a fork of OpenPrinting/foomatic-db, done by you or on your OK).
