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

static const char *supply_dir(const char *dir) {
    if (!dir) dir = getenv("SEQ4H_POWER_SUPPLY_DIR");
    return dir && *dir ? dir : "/sys/class/power_supply";
}

static const char *knulli_file(const char *knulli) {
    if (!knulli) knulli = getenv("SEQ4H_BATTERY_PERCENT_FILE");
    return knulli && *knulli ? knulli : "/tmp/battery.percent";
}

/* A percentage like "38", "38%" or "38\n"; -1 if none. */
static int parse_pct(const char *buf) {
    char *end;
    long v = strtol(buf, &end, 10);
    if (end == buf) return -1;
    return v < 0 ? 0 : v > 100 ? 100 : (int)v;
}

int battery_read(Battery *b, const char *dir, const char *knulli) {
    char path[512], buf[32];
    b->pct = b->kernel_pct = -1;
    b->charging = b->from_knulli = 0;
    b->status[0] = 0;
    dir = supply_dir(dir);
    DIR *d = opendir(dir);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d))) {
            if (e->d_name[0] == '.') continue;
            snprintf(path, sizeof path, "%s/%s/type", dir, e->d_name);
            if (read_small(path, buf, sizeof buf) < 0 || strcmp(buf, "Battery")) continue;
            snprintf(path, sizeof path, "%s/%s/capacity", dir, e->d_name);
            if (read_small(path, buf, sizeof buf) <= 0 || (b->kernel_pct = parse_pct(buf)) < 0) continue;
            snprintf(path, sizeof path, "%s/%s/status", dir, e->d_name);
            if (read_small(path, b->status, sizeof b->status) < 0) b->status[0] = 0;
            b->charging = !strcmp(b->status, "Charging") || !strcmp(b->status, "Full");
            break;
        }
        closedir(d);
    }
    b->pct = b->kernel_pct;
    if (read_small(knulli_file(knulli), buf, sizeof buf) > 0) {
        int k = parse_pct(buf);
        if (k >= 0) { b->pct = k; b->from_knulli = 1; }
    }
    return b->pct;
}

void battery_log_supplies(FILE *f, const char *dir, const char *knulli) {
    char path[512], v[5][32];
    static const char *A[5] = {"type", "status", "capacity", "voltage_now", "online"};
    dir = supply_dir(dir);
    DIR *d = opendir(dir);
    if (!d) fprintf(f, "seq4h: power supplies: %s not readable\n", dir);
    else {
        struct dirent *e;
        int n = 0;
        while ((e = readdir(d))) {
            if (e->d_name[0] == '.') continue;
            for (int i = 0; i < 5; i++) {
                snprintf(path, sizeof path, "%s/%s/%s", dir, e->d_name, A[i]);
                if (read_small(path, v[i], sizeof v[i]) < 0) snprintf(v[i], sizeof v[i], "-");
            }
            fprintf(f, "seq4h: power supply %s: type=%s status=%s capacity=%s voltage_now=%s online=%s\n",
                    e->d_name, v[0], v[1], v[2], v[3], v[4]);
            n++;
        }
        closedir(d);
        if (!n) fprintf(f, "seq4h: power supplies: none in %s\n", dir);
    }
    const char *k = knulli_file(knulli);
    if (read_small(k, v[0], sizeof v[0]) < 0) snprintf(v[0], sizeof v[0], "(missing)");
    fprintf(f, "seq4h: Knulli battery %s: %s\n", k, v[0]);
    fflush(f);
}

void battery_text(const Battery *b, char *buf, int n) {
    if (b->pct < 0) { if (n > 0) buf[0] = 0; return; }
    snprintf(buf, (size_t)n, "BAT %d%%%s", b->pct, b->charging ? "+" : "");
}

int battery_level(const Battery *b) {
    if (b->pct < 0) return 0;
    return b->pct < 10 ? 2 : b->pct < 20 ? 1 : 0;
}
