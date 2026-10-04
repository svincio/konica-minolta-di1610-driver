/*
 * rastertokm4 - CUPS filter for the KONICA MINOLTA (Minolta) Dialta Di1610, host-based "KM4" GDI protocol.
 * Reverse-engineered from the Windows Vista driver; stream verified byte-identical
 * against print-to-file captures (see docs/PROTOCOL.md, tests/golden.sh).
 *
 * Input: CUPS raster, 8-bit gray (W/SW) or 1-bit K. Output: KM4 job on stdout.
 * Gray is halftoned with an ordered matrix after a tone curve (the laser has heavy dot gain).
 * Job options (lp -o): km4-dither=bayer|cluster, km4-gamma=<float, 1 = linear, >1 lighter>.
 * Pure black and white are never altered, so text stays solid.
 * usage: rastertokm4 job user title copies options [file]
 *
 * Copyright (C) 2026 Simone Vincenzi. License: GPL-2.0-or-later (see LICENSE).
 */
#include <cups/cups.h>
#include <cups/raster.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "jbig.h"

/* ponytail: calibrated on one unit with tools/taratura.py: cluster + gamma 1.8 keeps the
 * whole 0-100% wedge distinguishable (bayer/linear: black from 50%, heavy banding). */
#define DEFAULT_GAMMA 1.8
#define DEFAULT_DITHER "cluster"

static const unsigned char bayer[8][8] = {   /* dispersed dot */
    {0, 32, 8, 40, 2, 34, 10, 42},  {48, 16, 56, 24, 50, 18, 58, 26},
    {12, 44, 4, 36, 14, 46, 6, 38}, {60, 28, 52, 20, 62, 30, 54, 22},
    {3, 35, 11, 43, 1, 33, 9, 41},  {51, 19, 59, 27, 49, 17, 57, 25},
    {15, 47, 7, 39, 13, 45, 5, 37}, {63, 31, 55, 23, 61, 29, 53, 21}};
static const unsigned char cluster[8][8] = { /* clustered dot, 45 degrees, ~106 lpi at 600 dpi */
    {24, 10, 12, 26, 35, 47, 49, 37}, {8, 0, 2, 14, 45, 59, 61, 51},
    {22, 6, 4, 16, 43, 57, 63, 53},   {30, 20, 18, 28, 33, 41, 55, 39},
    {34, 46, 48, 36, 25, 11, 13, 27}, {44, 58, 60, 50, 9, 1, 3, 15},
    {42, 56, 62, 52, 23, 7, 5, 17},   {32, 40, 54, 38, 31, 21, 19, 29}};

typedef struct { unsigned char *p; size_t n, cap; } buf_t;

static void buf_add(unsigned char *p, size_t n, void *v)
{
    buf_t *b = v;
    if (b->n + n > b->cap) {
        b->cap = (b->n + n) * 2;
        if (!(b->p = realloc(b->p, b->cap))) { fputs("ERROR: out of memory\n", stderr); exit(1); }
    }
    memcpy(b->p + b->n, p, n);
    b->n += n;
}

static void put(const void *p, size_t n) { fwrite(p, 1, n, stdout); }
static void rep(const char *pat, size_t plen, int times) { while (times--) put(pat, plen); }
static void job_mark(void) { rep("\x55", 1, 6); rep("\xff", 1, 16); rep("\x55", 1, 42); }

int main(int argc, char *argv[])
{
    if (argc < 6 || argc > 7) {
        fputs("ERROR: rastertokm4 job user title copies options [file]\n", stderr);
        return 1;
    }
    int fd = argc == 7 ? open(argv[6], O_RDONLY) : 0;
    if (fd < 0) { perror("ERROR: unable to open raster file"); return 1; }

    /* tone curve + halftone matrix from job options */
    cups_option_t *opts = NULL;
    int nopts = cupsParseOptions(argv[5], 0, &opts);
    const char *o = cupsGetOption("km4-gamma", nopts, opts);
    double gamma = o ? atof(o) : DEFAULT_GAMMA;
    if (gamma < 0.2 || gamma > 5) gamma = DEFAULT_GAMMA;
    o = cupsGetOption("km4-dither", nopts, opts);
    const unsigned char (*mat)[8] = !strcmp(o ? o : DEFAULT_DITHER, "cluster") ? cluster : bayer;
    unsigned char lut[256];
    for (int i = 0; i < 256; i++) lut[i] = (unsigned char)lround(255 * pow(i / 255.0, 1 / gamma));
    fprintf(stderr, "DEBUG: km4 gamma %.2f, dither %s\n", gamma, mat == cluster ? "cluster" : "bayer");
    cupsFreeOptions(nopts, opts);

    cups_raster_t *ras = cupsRasterOpen(fd, CUPS_RASTER_READ);
    cups_page_header2_t h;
    buf_t bie = {0};
    int npage = 0;

    rep("\xff\x05", 2, 512); rep("\x05\xff", 2, 512);
    job_mark();

    while (cupsRasterReadHeader2(ras, &h)) {
        npage++;
        unsigned dpi = h.HWResolution[1];
        int gray8 = h.cupsBitsPerColor == 8 && (h.cupsColorSpace == CUPS_CSPACE_W || h.cupsColorSpace == CUPS_CSPACE_SW);
        int k1 = h.cupsBitsPerColor == 1 && h.cupsColorSpace == CUPS_CSPACE_K;
        if (!(gray8 || k1) || (dpi != 300 && dpi != 600) || h.HWResolution[0] != dpi) {
            fprintf(stderr, "ERROR: unsupported raster: %u bpc, colorspace %u, %ux%u dpi\n",
                    h.cupsBitsPerColor, h.cupsColorSpace, h.HWResolution[0], h.HWResolution[1]);
            return 1;
        }
        /* A4 only: the printer errors out on other sizes (Letter = code 0x0c, see docs/PROTOCOL.md). */
        if (abs((int)h.PageSize[0] - 595) > 2 || abs((int)h.PageSize[1] - 842) > 2)
            fprintf(stderr, "WARNING: page size %ux%u pt is not A4, printing on A4\n", h.PageSize[0], h.PageSize[1]);

        /* Printable area = sheet minus 0.08" per side (matches all Windows captures). */
        unsigned m = dpi * 8 / 100;
        unsigned w = (unsigned)lround(210 / 25.4 * dpi) - 2 * m, ht = (unsigned)lround(297 / 25.4 * dpi) - 2 * m;
        unsigned bpl = (w + 7) / 8;
        int ox = ((int)h.cupsWidth - (int)w) / 2, oy = ((int)h.cupsHeight - (int)ht) / 2;
        unsigned char *row = malloc(h.cupsBytesPerLine), *bm = calloc((size_t)bpl * ht, 1);
        if (!row || !bm) { fputs("ERROR: out of memory\n", stderr); return 1; }

        /* raster (full or imageable area) -> printable-area bitmap, centred crop/pad, halftoned */
        for (unsigned y = 0; y < h.cupsHeight; y++) {
            if (cupsRasterReadPixels(ras, row, h.cupsBytesPerLine) != h.cupsBytesPerLine) break;
            int ty = (int)y - oy;
            if (ty < 0 || ty >= (int)ht) continue;
            unsigned char *dst = bm + (size_t)ty * bpl;
            for (unsigned tx = 0; tx < w; tx++) {
                int sx = (int)tx + ox;
                if (sx < 0 || sx >= (int)h.cupsWidth) continue;
                int black = gray8 ? lut[row[sx]] < mat[ty & 7][tx & 7] * 4u + 2
                                  : (row[sx >> 3] & (0x80 >> (sx & 7))) != 0;
                if (black) dst[tx >> 3] |= 0x80 >> (tx & 7);
            }
        }
        free(row);

        /* JBIG1 BIE: sequential, 1 stripe, ILEAVE|SMID, LRLTWO|TPBON, no AT moves (as Windows) */
        unsigned char *planes[1] = {bm};
        struct jbg_enc_state s;
        bie.n = 0;
        jbg_enc_init(&s, w, ht, 1, planes, buf_add, &bie);
        jbg_enc_layers(&s, 0);
        jbg_enc_options(&s, JBG_ILEAVE | JBG_SMID, JBG_LRLTWO | JBG_TPBON, ht, 0, -1);
        jbg_enc_out(&s);
        jbg_enc_free(&s);
        free(bm);

        /* 64-byte page header */
        unsigned char hdr[64] = {0x01, 0x05, 0x25};
        memset(hdr + 6, 0xff, 16);
        memcpy(hdr + 22, "\x01\x01\x02\x05\x00\x0f\x03", 7);
        hdr[29] = dpi == 600 ? 0x02 : 0x00;
        hdr[30] = 0x02;                      /* A4 */
        hdr[31] = h.MediaPosition & 0xff;   /* 0 auto, 1 tray 1, 6 bypass */
        hdr[33] = 1;                         /* copies: done by CUPS (cupsManualCopies) */
        hdr[34] = npage & 0xff;
        hdr[38] = w & 0xff; hdr[39] = w >> 8;
        hdr[40] = ht & 0xff; hdr[41] = ht >> 8;
        hdr[47] = 0x03;
        hdr[49] = 0x01;
        put(hdr, sizeof hdr);

        /* BIE + empty SDE, padded to N KiB blocks (last 2 bytes ff05), block count, filler */
        put(bie.p, bie.n);
        put("\xff\x02", 2);
        size_t len = bie.n + 2, n = (len + 2 + 1023) / 1024;
        for (size_t i = len; i < n * 1024 - 2; i++) put("", 1);
        unsigned char tail[6] = {0xff, 0x05, 0xff, 0x05, (n >> 8) & 0xff, n & 0xff};
        put(tail, 6);
        rep("\x05\xff", 2, 512); rep("\xff\x05", 2, 510);

        fprintf(stderr, "PAGE: %d 1\n", npage);
    }

    job_mark();
    fflush(stdout);
    cupsRasterClose(ras);
    if (fd) close(fd);
    free(bie.p);
    return npage ? 0 : 1;
}
