/* km4usb - send a print job to the Di1610 over USB, write-only.
 * The macOS CUPS usb backend polls the back channel every 250 ms; the Di1610 answers with
 * STALLs and corrupts data that is still arriving while the sheet moves (big pages print as
 * garbage). This tool never reads. Run by launchd (inetd mode) behind socket://127.0.0.1:9101,
 * job on stdin; or by hand: km4usb FILE.
 * If the printer is not on the bus it waits up to 5 minutes, so jobs are not lost.
 *
 * Copyright (C) 2026 Simone Vincenzi. License: GPL-2.0-or-later (see LICENSE). */
#include <libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

static libusb_device_handle *open_printer(libusb_context *ctx, int *ifn, int *ep)
{
    libusb_device **list;
    libusb_device_handle *h = NULL;
    ssize_t nd = libusb_get_device_list(ctx, &list);
    for (ssize_t i = 0; i < nd && !h; i++) {
        struct libusb_device_descriptor dd;
        struct libusb_config_descriptor *cfg;
        libusb_get_device_descriptor(list[i], &dd);
        if (dd.idVendor != 0x0686 || libusb_get_active_config_descriptor(list[i], &cfg)) continue;
        *ifn = -1;
        for (int k = 0; k < cfg->bNumInterfaces && *ifn < 0; k++) {
            const struct libusb_interface_descriptor *id = &cfg->interface[k].altsetting[0];
            if (id->bInterfaceClass != 7) continue;          /* printer class */
            for (int e = 0; e < id->bNumEndpoints; e++) {
                const struct libusb_endpoint_descriptor *ed = &id->endpoint[e];
                if ((ed->bmAttributes & 3) == LIBUSB_TRANSFER_TYPE_BULK && !(ed->bEndpointAddress & 0x80))
                    *ifn = id->bInterfaceNumber, *ep = ed->bEndpointAddress;
            }
        }
        libusb_free_config_descriptor(cfg);
        if (*ifn >= 0 && libusb_open(list[i], &h) == 0 && libusb_claim_interface(h, *ifn)) {
            libusb_close(h);
            h = NULL;
        }
    }
    libusb_free_device_list(list, 1);
    return h;
}

int main(int argc, char *argv[])
{
    int in = argc > 1 ? open(argv[1], O_RDONLY) : 0;
    if (in < 0) { perror(argv[1]); return 1; }

    libusb_context *ctx;
    libusb_device_handle *h = NULL;
    int ifn, ep;
    libusb_init(&ctx);
    for (int tries = 0; !(h = open_printer(ctx, &ifn, &ep)); tries++) {
        if (tries == 60) { fputs("km4usb: Di1610 not found, giving up\n", stderr); return 1; }
        sleep(5);
    }

    unsigned char buf[4096];
    ssize_t n;
    long total = 0;
    while ((n = read(in, buf, sizeof buf)) > 0) {
        for (ssize_t off = 0; off < n;) {
            int done = 0, r = libusb_bulk_transfer(h, ep, buf + off, (int)(n - off), &done, 60000);
            off += done; total += done;
            if (r && r != LIBUSB_ERROR_TIMEOUT) {   /* timeout = printer busy: keep going */
                fprintf(stderr, "km4usb: write error after %ld bytes: %s\n", total, libusb_error_name(r));
                return 1;
            }
        }
    }
    fprintf(stderr, "km4usb: sent %ld bytes\n", total);
    libusb_release_interface(h, ifn);
    libusb_close(h);
    libusb_exit(ctx);
    return 0;
}
