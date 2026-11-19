/* Battery reader against a fake /sys/class/power_supply tree. */
#include "../src/battery.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int fails;
static const char *NOK = "/nonexistent/battery.percent";
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static void put(const char *root, const char *supply, const char *file, const char *text) {
    char p[512];
    snprintf(p, sizeof p, "%s/%s", root, supply);
    mkdir(p, 0755);
    snprintf(p, sizeof p, "%s/%s/%s", root, supply, file);
    FILE *f = fopen(p, "w");
    if (!f) { perror(p); exit(1); }
    fputs(text, f);
    fclose(f);
}

int main(void) {
    char root[] = "/tmp/seq4h-battery-XXXXXX";
    if (!mkdtemp(root)) { perror("mkdtemp"); return 1; }
    Battery b;
    char buf[32];

    CHECK(battery_read(&b, "/nonexistent/power_supply", NOK) == -1);   /* no sysfs: no battery, no crash */
    battery_text(&b, buf, sizeof buf);
    CHECK(buf[0] == 0);
    CHECK(battery_read(&b, root, NOK) == -1);                          /* empty directory */

    put(root, "ac", "type", "Mains\n");                           /* the charger is not the battery */
    put(root, "ac", "capacity", "5\n");
    CHECK(battery_read(&b, root, NOK) == -1);

    put(root, "battery", "type", "Battery\n");
    put(root, "battery", "capacity", "78\n");
    put(root, "battery", "status", "Discharging\n");
    CHECK(battery_read(&b, root, NOK) == 78);
    CHECK(!b.charging && !strcmp(b.status, "Discharging"));
    battery_text(&b, buf, sizeof buf);
    CHECK(!strcmp(buf, "BAT 78%"));
    CHECK(battery_level(&b) == 0);

    put(root, "battery", "status", "Charging\n");
    put(root, "battery", "capacity", "19\n");
    battery_read(&b, root, NOK);
    battery_text(&b, buf, sizeof buf);
    CHECK(!strcmp(buf, "BAT 19%+"));
    CHECK(battery_level(&b) == 1);

    put(root, "battery", "capacity", "9\n");
    battery_read(&b, root, NOK);
    CHECK(battery_level(&b) == 2);
    put(root, "battery", "capacity", "10\n");
    battery_read(&b, root, NOK);
    CHECK(battery_level(&b) == 1);

    put(root, "battery", "capacity", "250\n");                    /* out of range is clamped */
    CHECK(battery_read(&b, root, NOK) == 100);
    put(root, "battery", "status", "Full\n");
    battery_read(&b, root, NOK);
    CHECK(b.charging);

    put(root, "battery", "capacity", "");                         /* unreadable capacity: treated as missing */
    CHECK(battery_read(&b, root, NOK) == -1);

    /* Knulli's own number wins over the kernel gauge (Roy's RG40XX-V: kernel 8%, Knulli 38%). */
    put(root, "battery", "capacity", "8\n");
    put(root, "battery", "status", "Discharging\n");
    put(root, "knulli", "battery.percent", "38\n");
    char kf[600];
    snprintf(kf, sizeof kf, "%s/knulli/battery.percent", root);
    CHECK(battery_read(&b, root, kf) == 38);
    CHECK(b.from_knulli && b.kernel_pct == 8 && !b.charging);
    put(root, "knulli", "battery.percent", "");                   /* empty file: kernel value */
    CHECK(battery_read(&b, root, kf) == 8);
    put(root, "knulli", "battery.percent", "55%");
    CHECK(battery_read(&b, "/nonexistent/power_supply", kf) == 55);   /* Knulli file alone is enough */
    FILE *lf = tmpfile();
    battery_log_supplies(lf, root, kf);
    rewind(lf);
    char all[2048] = "";
    size_t got = fread(all, 1, sizeof all - 1, lf);
    all[got] = 0;
    fclose(lf);
    CHECK(strstr(all, "power supply battery: type=Battery status=Discharging capacity=8"));
    CHECK(strstr(all, "power supply ac: type=Mains"));
    CHECK(strstr(all, "Knulli battery") && strstr(all, ": 55%"));

    char cmd[600];
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", root);
    if (system(cmd)) {}
    if (fails) return 1;
    puts("battery tests passed");
    return 0;
}
