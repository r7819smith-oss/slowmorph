#ifndef SEQ4H_BATTERY_H
#define SEQ4H_BATTERY_H

#include <stdio.h>

/* Battery level from Linux sysfs (/sys/class/power_supply/<name>/). The
 * battery is the supply whose "type" file says "Battery". Reads are small,
 * non-blocking and never fail loudly: a missing or unreadable battery just
 * gives pct = -1. Call battery_read() every few seconds, not every frame. */
typedef struct {
    int pct;          /* 0..100, or -1 = no battery found */
    int charging;     /* status is Charging or Full (charger plugged in) */
    char status[24];  /* raw status text, e.g. "Discharging"; "" if unknown */
    int kernel_pct;   /* the kernel fuel gauge's capacity, -1 = none */
    int from_knulli;  /* pct came from Knulli's own reading (/tmp/battery.percent) */
} Battery;

/* dir: the power_supply directory; NULL = $SEQ4H_POWER_SUPPLY_DIR if set,
 * else /sys/class/power_supply. knulli: the file Knulli's batteryplus daemon
 * writes the percentage it shows to; NULL = $SEQ4H_BATTERY_PERCENT_FILE if
 * set, else /tmp/battery.percent. pct prefers Knulli's number, so seq4h
 * agrees with the Knulli menu, and falls back to the kernel's capacity.
 * Returns b->pct. */
int battery_read(Battery *b, const char *dir, const char *knulli);

/* One line per power supply (name, type, status, capacity, voltage) plus
 * Knulli's percentage file, for seq4h.log. */
void battery_log_supplies(FILE *f, const char *dir, const char *knulli);

/* Header text, e.g. "BAT 78%" or "BAT 78%+" while charging; "" if no battery. */
void battery_text(const Battery *b, char *buf, int n);

/* Display level: 0 = normal, 1 = low (below 20%), 2 = critical (below 10%). */
int battery_level(const Battery *b);

#endif
