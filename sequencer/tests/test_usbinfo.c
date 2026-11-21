/* USB device scan against a fake /sys/bus/usb/devices tree. */
#include "../src/usbinfo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static void put(const char *root, const char *dev, const char *file, const char *text) {
    char p[512];
    snprintf(p, sizeof p, "%s/%s", root, dev);
    mkdir(p, 0755);
    snprintf(p, sizeof p, "%s/%s/%s", root, dev, file);
    FILE *f = fopen(p, "w");
    if (!f) { perror(p); exit(1); }
    fputs(text, f);
    fclose(f);
}

static void bind(const char *root, const char *dev, const char *drv) {
    char p[512], t[256];
    snprintf(p, sizeof p, "%s/%s/driver", root, dev);
    snprintf(t, sizeof t, "../../../bus/usb/drivers/%s", drv);
    if (symlink(t, p) != 0) { perror(p); exit(1); }
}

int main(void) {
    char root[] = "/tmp/seq4h-usb-XXXXXX";
    if (!mkdtemp(root)) { perror("mkdtemp"); return 1; }
    UsbDev d[8];
    char line[160];

    CHECK(usb_scan("/nonexistent/usb", d, 8) == 0);   /* no sysfs: nothing, no crash */
    CHECK(usb_scan(root, d, 8) == 0);

    /* Root hub: left out. */
    put(root, "usb1", "idVendor", "1d6b\n");
    put(root, "usb1", "idProduct", "0002\n");
    put(root, "usb1", "bDeviceClass", "09\n");
    CHECK(usb_scan(root, d, 8) == 0);

    /* A cheap class-compliant cable, no driver yet. */
    put(root, "1-1", "idVendor", "1a86\n");
    put(root, "1-1", "idProduct", "752d\n");
    put(root, "1-1", "product", "USB2.0-MIDI\n");
    put(root, "1-1", "bDeviceClass", "00\n");
    put(root, "1-1:1.0", "bInterfaceClass", "01\n");
    put(root, "1-1:1.0", "bInterfaceSubClass", "01\n");
    put(root, "1-1:1.1", "bInterfaceClass", "01\n");
    put(root, "1-1:1.1", "bInterfaceSubClass", "03\n");
    CHECK(usb_scan(root, d, 8) == 1);
    CHECK(!strcmp(d[0].path, "1-1"));
    CHECK(!strcmp(d[0].id, "1a86:752d"));
    CHECK(!strcmp(d[0].name, "USB2.0-MIDI"));
    CHECK(d[0].midi && d[0].audio && !d[0].hub);
    CHECK(d[0].driver[0] == 0);
    usb_describe(&d[0], line, sizeof line);
    CHECK(!strcmp(line, "1-1 1a86:752d USB2.0-MIDI: MIDI, driver NONE"));

    /* The driver binds; the MIDI interface's driver is the one reported. */
    bind(root, "1-1:1.1", "snd-usb-audio");
    usb_scan(root, d, 8);
    CHECK(!strcmp(d[0].driver, "snd-usb-audio"));

    /* Behind a hub: a vendor-specific (not class-compliant) MIDI cable, with a maker name. */
    put(root, "1-1", "bDeviceClass", "09\n");
    put(root, "1-1.2", "idVendor", "0a92\n");
    put(root, "1-1.2", "idProduct", "1010\n");
    put(root, "1-1.2", "manufacturer", "ACME\n");
    put(root, "1-1.2", "product", "MidiLink\n");
    put(root, "1-1.2:1.0", "bInterfaceClass", "ff\n");
    CHECK(usb_scan(root, d, 8) == 2);
    CHECK(!strcmp(d[0].path, "1-1") && d[0].hub);
    CHECK(!strcmp(d[1].path, "1-1.2") && !d[1].midi && !d[1].audio);
    CHECK(!strcmp(d[1].name, "ACME MidiLink"));
    usb_describe(&d[1], line, sizeof line);
    CHECK(!strcmp(line, "1-1.2 0a92:1010 ACME MidiLink: not MIDI"));
    CHECK(usb_scan(root, d, 1) == 1);   /* max is kept */

    char cmd[600];
    snprintf(cmd, sizeof cmd, "rm -rf %s", root);
    if (system(cmd) != 0) {}
    printf("%s: usbinfo\n", fails ? "FAILED" : "ok");
    return fails != 0;
}
