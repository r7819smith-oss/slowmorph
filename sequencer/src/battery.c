#include "battery.h"

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Reads one short sysfs file into buf, trailing newline removed. O_NONBLOCK so
 * an odd file can't hang the UI. Returns length, or -1. */
static int read_small(const char *path, char *buf, int n) {
    int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) return -1;
    ssize_t k = read(fd, buf, (size_t)n - 1);
    close(fd);
    if (k < 0) return -1;
    buf[k] = 0;
    while (k > 0 && (buf[k - 1] == '\n' || buf[k - 1] == ' ' || buf[k - 1] == '\r')) buf[--k] = 0;
    return (int)k;
}

int battery_read(Battery *b, const char *dir) {
    char path[512], buf[32];
    b->pct = -1;
    b->charging = 0;
    b->status[0] = 0;
    if (!dir) dir = getenv("SEQ4H_POWER_SUPPLY_DIR");
    if (!dir || !*dir) dir = "/sys/class/power_supply";
    DIR *d = opendir(dir);
    if (!d) return -1;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        snprintf(path, sizeof path, "%s/%s/type", dir, e->d_name);
        if (read_small(path, buf, sizeof buf) < 0 || strcmp(buf, "Battery")) continue;
        snprintf(path, sizeof path, "%s/%s/capacity", dir, e->d_name);
        if (read_small(path, buf, sizeof buf) <= 0) continue;
        char *end;
        long v = strtol(buf, &end, 10);
        if (end == buf) continue;
        b->pct = v < 0 ? 0 : v > 100 ? 100 : (int)v;
        snprintf(path, sizeof path, "%s/%s/status", dir, e->d_name);
        if (read_small(path, b->status, sizeof b->status) < 0) b->status[0] = 0;
        b->charging = !strcmp(b->status, "Charging") || !strcmp(b->status, "Full");
        break;
    }
    closedir(d);
    return b->pct;
}

void battery_text(const Battery *b, char *buf, int n) {
    if (b->pct < 0) { if (n > 0) buf[0] = 0; return; }
    snprintf(buf, (size_t)n, "BAT %d%%%s", b->pct, b->charging ? "+" : "");
}

int battery_level(const Battery *b) {
    if (b->pct < 0) return 0;
    return b->pct < 10 ? 2 : b->pct < 20 ? 1 : 0;
}
