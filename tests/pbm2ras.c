/* pbm2ras - wrap P4 PBM pages into a CUPS raster stream (test helper for rastertokm4).
 * usage: pbm2ras DPI PAGE_W_PT PAGE_H_PT MEDIA_POSITION file.pbm... > out.ras */
#include <cups/raster.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
    if (argc < 6) { fputs("usage: pbm2ras DPI W_PT H_PT MEDIAPOS file.pbm...\n", stderr); return 1; }
    cups_raster_t *r = cupsRasterOpen(1, CUPS_RASTER_WRITE);
    for (int i = 5; i < argc; i++) {
        FILE *f = fopen(argv[i], "rb");
        unsigned w, h;
        if (!f || fscanf(f, "P4 %u %u", &w, &h) != 2) { fprintf(stderr, "bad pbm %s\n", argv[i]); return 1; }
        fgetc(f);
        cups_page_header2_t hd;
        memset(&hd, 0, sizeof hd);
        hd.HWResolution[0] = hd.HWResolution[1] = atoi(argv[1]);
        hd.PageSize[0] = atoi(argv[2]); hd.PageSize[1] = atoi(argv[3]);
        hd.MediaPosition = atoi(argv[4]);
        hd.cupsWidth = w; hd.cupsHeight = h;
        hd.cupsBitsPerColor = hd.cupsBitsPerPixel = 1;
        hd.cupsBytesPerLine = (w + 7) / 8;
        hd.cupsColorSpace = CUPS_CSPACE_K;
        cupsRasterWriteHeader2(r, &hd);
        unsigned char *row = malloc(hd.cupsBytesPerLine);
        for (unsigned y = 0; y < h; y++) {
            fread(row, 1, hd.cupsBytesPerLine, f);
            cupsRasterWritePixels(r, row, hd.cupsBytesPerLine);
        }
        free(row); fclose(f);
    }
    cupsRasterClose(r);
    return 0;
}
