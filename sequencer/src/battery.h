#ifndef SEQ4H_BATTERY_H
#define SEQ4H_BATTERY_H

/* Battery level from Linux sysfs (/sys/class/power_supply/<name>/). The
 * battery is the supply whose "type" file says "Battery". Reads are small,
 * non-blocking and never fail loudly: a missing or unreadable battery just
 * gives pct = -1. Call battery_read() every few seconds, not every frame. */
typedef struct {
    int pct;          /* 0..100, or -1 = no battery found */
    int charging;     /* status is Charging or Full (charger plugged in) */
    char status[24];  /* raw status text, e.g. "Discharging"; "" if unknown */
} Battery;

/* dir: the power_supply directory; NULL = $SEQ4H_POWER_SUPPLY_DIR if set,
 * else /sys/class/power_supply. Returns b->pct. */
int battery_read(Battery *b, const char *dir);

/* Header text, e.g. "BAT 78%" or "BAT 78%+" while charging; "" if no battery. */
void battery_text(const Battery *b, char *buf, int n);

/* Display level: 0 = normal, 1 = low (below 20%), 2 = critical (below 10%). */
int battery_level(const Battery *b);

#endif
