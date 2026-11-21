#define _GNU_SOURCE
#include "usbinfo.h"

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* First line of a small sysfs file, without the newline. 0 if unreadable. */
static int rd(const char *dir, const char *name, char *buf, size_t n) {
    char p[PATH_MAX + 64];
    snprintf(p, sizeof p, "%s/%s", dir, name);
    buf[0] = 0;
    FILE *f = fopen(p, "r");
    if (!f) return 0;
    int ok = fgets(buf, (int)n, f) != NULL;
    fclose(f);
    buf[strcspn(buf, "\r\n")] = 0;
    return ok;
}

static int hex(const char *dir, const char *name) {
    char b[16];
    return rd(dir, name, b, sizeof b) ? (int)strtol(b, NULL, 16) : -1;
}

static void driver_of(const char *dir, char *out, size_t n) {
    char p[PATH_MAX + 8], t[PATH_MAX];
    snprintf(p, sizeof p, "%s/driver", dir);
    ssize_t k = readlink(p, t, sizeof t - 1);
    out[0] = 0;
    if (k <= 0) return;
    t[k] = 0;
    const char *b = strrchr(t, '/');
    snprintf(out, n, "%.23s", b ? b + 1 : t);
}

static int by_path(const void *a, const void *b) { return strcmp(((const UsbDev *)a)->path, ((const UsbDev *)b)->path); }

int usb_scan(const char *root, UsbDev *out, int max) {
    if (!root) root = getenv("SEQ4H_USB_DIR");
    if (!root || !*root) root = "/sys/bus/usb/devices";
    DIR *d = opendir(root);
    if (!d) return 0;
    int n = 0;
    struct dirent *e;
    while (n < max && (e = readdir(d))) {
        const char *nm = e->d_name;
        /* Devices are "B-P[.P...]"; interfaces have a ':'; root hubs are "usbN". */
        if (nm[0] < '0' || nm[0] > '9' || strchr(nm, ':')) continue;
        char dir[PATH_MAX];
        snprintf(dir, sizeof dir, "%s/%s", root, nm);
        char v[8], p[8];
        if (!rd(dir, "idVendor", v, sizeof v) || !rd(dir, "idProduct", p, sizeof p)) continue;
        UsbDev *u = &out[n];
        memset(u, 0, sizeof *u);
        snprintf(u->path, sizeof u->path, "%.15s", nm);
        snprintf(u->id, sizeof u->id, "%.4s:%.4s", v, p);
        char man[48], prod[48];
        rd(dir, "manufacturer", man, sizeof man);
        rd(dir, "product", prod, sizeof prod);
        if (man[0] && prod[0] && !strcasestr(prod, man)) snprintf(u->name, sizeof u->name, "%.20s %.26s", man, prod);
        else snprintf(u->name, sizeof u->name, "%s", prod[0] ? prod : man[0] ? man : "(NO NAME)");
        u->hub = hex(dir, "bDeviceClass") == 9;
        /* Interfaces: "<name>:<config>.<interface>" next to the device. */
        DIR *d2 = opendir(root);
        struct dirent *f;
        size_t l = strlen(nm);
        while (d2 && (f = readdir(d2))) {
            if (strncmp(f->d_name, nm, l) || f->d_name[l] != ':') continue;
            char idir[PATH_MAX];
            snprintf(idir, sizeof idir, "%s/%s", root, f->d_name);
            int cls = hex(idir, "bInterfaceClass"), sub = hex(idir, "bInterfaceSubClass");
            if (cls != 1) continue;
            char drv[24];
            driver_of(idir, drv, sizeof drv);
            if (sub == 3) {
                if (!u->midi || !u->driver[0]) snprintf(u->driver, sizeof u->driver, "%s", drv);
                u->midi = 1;
            } else if (!u->audio && !u->midi) snprintf(u->driver, sizeof u->driver, "%s", drv);
            u->audio = 1;
        }
        if (d2) closedir(d2);
        n++;
    }
    closedir(d);
    qsort(out, (size_t)n, sizeof *out, by_path);
    return n;
}

void usb_describe(const UsbDev *d, char *buf, size_t n) {
    const char *kind = d->hub ? "hub" : d->midi ? "MIDI" : d->audio ? "audio, no MIDI" : "not MIDI";
    if (d->hub) snprintf(buf, n, "%s %s %s: hub", d->path, d->id, d->name);
    else snprintf(buf, n, "%s %s %s: %s%s%s", d->path, d->id, d->name, kind,
                  d->midi || d->audio ? ", driver " : "", d->midi || d->audio ? (d->driver[0] ? d->driver : "NONE") : "");
}
