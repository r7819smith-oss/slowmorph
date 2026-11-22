/* Modulation (M > MODULATION): LFO shapes and rates, CC output limits, saving,
 * the MIDI input parser, the monitor and learn. */
#include "../src/engine.h"
#include "../src/mod.h"
#include "../src/song.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); return 1; } } while (0)

/* Every CC sent, with the tick it went out on. */
typedef struct { int n; uint8_t m[20000][3]; uint32_t at[20000]; uint32_t now; int notes; } Log;
static void rec(void *ctx, const uint8_t *msg, int len) {
    Log *l = ctx;
    if (len == 3 && (msg[0] & 0xF0) == 0xB0 && msg[1] < 120 && l->n < 20000) {
        memcpy(l->m[l->n], msg, 3);
        l->at[l->n++] = l->now;
    }
    if ((msg[0] & 0xF0) == 0x90) l->notes++;
}

static Log lg;
static void run(Seq *s, int ticks) { for (int i = 0; i < ticks; i++) { lg.now++; seq_tick(s); } }

static int near(double a, double b, double e) { return a - b < e && b - a < e; }

int main(void) {
    static Seq s;
    /* ---- shapes ---- */
    CHECK(near(mod_wave_at(WAVE_SINE, 0, 0, 0), 0, 0.01));
    CHECK(near(mod_wave_at(WAVE_SINE, 0.25, 0, 0), 0.5, 0.01));
    CHECK(near(mod_wave_at(WAVE_SINE, 0.5, 0, 0), 1, 0.01));
    CHECK(near(mod_wave_at(WAVE_SINE, 0.75, 0, 0), 0.5, 0.01));
    for (int i = 0; i < 1000; i++) {   /* sine stays close to 0.5 - 0.5 cos: within one CC step of 127 */
        double p = i / 1000.0, v = mod_wave_at(WAVE_SINE, p, 0, 0);
        CHECK(v >= -0.001 && v <= 1.001);
        double t = p < 0.5 ? p : 1 - p;           /* symmetric about the top */
        CHECK(near(v, mod_wave_at(WAVE_SINE, 1 - p, 0, 0), 0.004));
        (void)t;
    }
    CHECK(near(mod_wave_at(WAVE_TRI, 0.25, 0, 0), 0.5, 1e-9) && near(mod_wave_at(WAVE_TRI, 0.5, 0, 0), 1, 1e-9));
    CHECK(near(mod_wave_at(WAVE_RAMP_UP, 0.3, 0, 0), 0.3, 1e-9) && near(mod_wave_at(WAVE_RAMP_DOWN, 0.3, 0, 0), 0.7, 1e-9));
    CHECK(mod_wave_at(WAVE_SQUARE, 0.2, 0, 0) == 0 && mod_wave_at(WAVE_SQUARE, 0.7, 0, 0) == 1);
    CHECK(near(mod_wave_at(WAVE_SH, 0.1, 0, 65535), 1, 1e-9) && near(mod_wave_at(WAVE_SH, 0.9, 0, 65535), 1, 1e-9));
    CHECK(near(mod_wave_at(WAVE_SMOOTH, 0, 0, 65535), 0, 1e-9) && near(mod_wave_at(WAVE_SMOOTH, 0.5, 0, 65535), 0.5, 1e-9));

    /* ---- names ---- */
    char b[32];
    mod_rate_name(1, 2, b, sizeof b); CHECK(!strcmp(b, "1/4"));
    mod_rate_name(1, 4, b, sizeof b); CHECK(!strcmp(b, "1 BAR"));
    mod_rate_name(1, 9, b, sizeof b); CHECK(!strcmp(b, "8 BARS"));
    mod_rate_name(0, 0, b, sizeof b); CHECK(!strcmp(b, "0.1 S"));
    mod_rate_name(0, 15, b, sizeof b); CHECK(!strcmp(b, "30 S"));
    mod_rate_name(0, 18, b, sizeof b); CHECK(!strcmp(b, "1.5 MIN"));
    mod_rate_name(0, mod_rate_count(0) - 1, b, sizeof b); CHECK(!strcmp(b, "10 MIN"));
    CHECK(!strcmp(mod_felucca_cc(74), "CUTOFF") && !strcmp(mod_felucca_cc(10), "PAN") && !mod_felucca_cc(20));
    CHECK(mod_cc_reserved(0) && mod_cc_reserved(123) && !mod_cc_reserved(74));

    /* ---- defaults: modulation on, every destination off, nothing sent ---- */
    seq_init(&s, rec, &lg);
    CHECK(s.mod.on);
    for (int d = 0; d < MOD_DESTS; d++) CHECK(!s.mod.dest[d].on);
    run(&s, 2000);
    CHECK(lg.n == 0);

    /* ---- one destination: 8-bar sine on CC 74, 40..90, on T1 ---- */
    ModDest *x = &s.mod.dest[0];
    x->on = 1;
    s.mod.lfo[0].rate = 4;            /* 1 bar = 384 ticks */
    seq_start(&s);
    lg.n = 0;
    run(&s, 384 * 4);
    CHECK(lg.n > 20);
    int lo = 127, hi = 0;
    for (int i = 0; i < lg.n; i++) {
        CHECK(lg.m[i][0] == 0xB0 && lg.m[i][1] == 74);   /* T1 = channel 1 */
        if (lg.m[i][2] < lo) lo = lg.m[i][2];
        if (lg.m[i][2] > hi) hi = lg.m[i][2];
        if (i) CHECK(lg.m[i][2] != lg.m[i - 1][2]);       /* never the same value twice in a row */
        if (i) CHECK(lg.at[i] - lg.at[i - 1] >= 4);       /* at 120 BPM: at least 20 ms (3.84 ticks, rounded up) apart */
    }
    CHECK(lo == 40 && hi == 90);                          /* reaches both limits, never past them */
    /* At most one CC per 20 ms: 4 bars at 120 BPM = 8 s. */
    CHECK(lg.n <= 8000 / 20 + 1);
    /* restarts in phase: right after Start the value is LOW (sine starts low) */
    seq_stop(&s);
    seq_start(&s);
    run(&s, 1);
    CHECK(mod_value(&s.mod, 0) == 40);

    /* invert: the same LFO, top value is LOW */
    x->inv = 1;
    CHECK(mod_value(&s.mod, 0) == 90);
    x->inv = 0;

    /* LOW = HIGH holds one value; LOW is pushed by HIGH and back */
    mod_set_lo(&s.mod, 0, 100); CHECK(x->lo == 100 && x->hi == 100);
    mod_set_hi(&s.mod, 0, 20); CHECK(x->lo == 20 && x->hi == 20);
    mod_set_lo(&s.mod, 0, -5); mod_set_hi(&s.mod, 0, 300); CHECK(x->lo == 0 && x->hi == 127);
    mod_set_lo(&s.mod, 0, 30); mod_set_hi(&s.mod, 0, 60);
    mod_shift(&s.mod, 0, 100); CHECK(x->lo == 97 && x->hi == 127);   /* width 30 kept, clamped at 127 */
    mod_shift(&s.mod, 0, -200); CHECK(x->lo == 0 && x->hi == 30);

    /* paused LFO: nothing more is sent */
    s.mod.lfo[0].hold = 1;
    run(&s, 50);
    lg.n = 0;
    run(&s, 384);
    CHECK(lg.n == 0);
    s.mod.lfo[0].hold = 0;
    /* WHEN STOPPED: PAUSES */
    s.mod.lfo[0].stop_hold = 1;
    seq_stop(&s);
    run(&s, 50);
    lg.n = 0;
    run(&s, 384);
    CHECK(lg.n == 0);
    /* KEEPS MOVING while stopped (the default) */
    s.mod.lfo[0].stop_hold = 0;
    run(&s, 384);
    CHECK(lg.n > 5);
    /* ALL MOD off: silent */
    s.mod.on = 0;
    lg.n = 0;
    run(&s, 384);
    CHECK(lg.n == 0);
    s.mod.on = 1;

    /* destination off: silent */
    x->on = 0;
    lg.n = 0;
    run(&s, 384);
    CHECK(lg.n == 0);

    /* ---- free rate does not follow the BPM: 2 s cycle at 60 and 240 BPM ---- */
    seq_init(&s, rec, &lg);
    s.mod.lfo[1] = (ModLfo){.wave = WAVE_RAMP_UP, .sync = 0, .rate = 7};   /* 2 s */
    CHECK(mod_free_tenths(7) == 20);
    s.bpm = 60;
    run(&s, 96 * 1);    /* 1 s */
    CHECK(near(mod_lfo_out(&s.mod, 1), 0.5, 0.02));
    s.bpm = 240;
    run(&s, 96 * 2);    /* 0.5 s at 240 BPM */
    CHECK(near(mod_lfo_out(&s.mod, 1), 0.75, 0.02));
    /* sync rate follows bars: 1/4 = 96 ticks, whatever the BPM */
    s.mod.lfo[2] = (ModLfo){.wave = WAVE_RAMP_UP, .sync = 1, .rate = 2};
    seq_start(&s);
    run(&s, 48);
    CHECK(near(mod_lfo_out(&s.mod, 2), 0.5, 0.02));
    /* start phase 50% */
    s.mod.lfo[2].phase = 50;
    seq_stop(&s);
    seq_start(&s);
    run(&s, 1);
    CHECK(near(mod_lfo_out(&s.mod, 2), 0.5, 0.02));

    /* ---- T1-T4 sends on each track channel once; 16 destinations stay within 4 CCs a tick ---- */
    seq_init(&s, rec, &lg);
    s.track[1].channel = 0;   /* T2 shares channel 1 with T1 */
    uint8_t ch[MOD_TRACKS] = {0, 0, 2, 3, 4, 5, 6, 3}, c[MOD_TRACKS];
    s.mod.dest[0].to = TO_ALL;
    CHECK(mod_channels(&s.mod, 0, ch, c) == 3);
    s.mod.dest[0].to = TO_ALL2;   /* T5-T8: channels 5, 6, 7 and T8 on 4 */
    CHECK(mod_channels(&s.mod, 0, ch, c) == 4 && c[0] == 4 && c[3] == 3);
    s.mod.dest[0].to = TO_ALL8;   /* every track's channel once */
    CHECK(mod_channels(&s.mod, 0, ch, c) == 6);
    s.mod.dest[0].to = TO_T1 + 6;
    CHECK(mod_channels(&s.mod, 0, ch, c) == 1 && c[0] == 6);
    {   /* T1-T8 is 8 channels, more than MOD_PER_TICK: it still goes out */
        static Seq q;
        seq_init(&q, rec, &lg);
        q.mod.on = 1;
        q.mod.dest[0] = (ModDest){1, 3, TO_ALL8, 30, 0, 127, 0, -1, 0};
        q.mod.lfo[3] = (ModLfo){.wave = WAVE_SQUARE, .sync = 1, .rate = 0};
        lg.n = 0;
        seq_start(&q);
        for (int i = 0; i < 96; i++) seq_tick(&q);
        int chs8 = 0;
        for (int i = 0; i < lg.n; i++) if ((lg.m[i][0] & 0xF0) == 0xB0 && lg.m[i][1] == 30) chs8 |= 1 << (lg.m[i][0] & 15);
        CHECK(chs8 == 0xFF);
    }
    s.mod.dest[0].to = TO_CH1 + 9;
    CHECK(mod_channels(&s.mod, 0, ch, c) == 1 && c[0] == 9);
    for (int d = 0; d < MOD_DESTS; d++) {
        s.mod.dest[d] = (ModDest){1, 3, TO_ALL, (uint8_t)(20 + d), 0, 127, 0, -1, 0};
    }
    s.mod.lfo[3] = (ModLfo){.wave = WAVE_SQUARE, .sync = 1, .rate = 0};   /* jumps every 1/32 */
    lg.n = 0;
    uint32_t start = lg.now;
    run(&s, 384);
    CHECK(lg.n > 0);
    int per_tick[400] = {0};
    for (int i = 0; i < lg.n; i++) per_tick[lg.at[i] - start - 1]++;
    for (int i = 0; i < 384; i++) CHECK(per_tick[i] <= MOD_PER_TICK);
    for (int d = 0; d < MOD_DESTS; d++) {   /* every destination gets its turn */
        int seen = 0;
        for (int i = 0; i < lg.n; i++) seen |= lg.m[i][1] == 20 + d;
        CHECK(seen);
    }
    CHECK(mod_find(&s.mod, 2, 25, ch, -1) == 5 && mod_find(&s.mod, 2, 25, ch, 5) == -1 && mod_find(&s.mod, 9, 25, ch, -1) == -1);

    /* SEND ONCE goes out whether or not the destination is on */
    s.mod.dest[4].on = 0;
    s.mod.dest[4].to = TO_T1 + 2;
    lg.n = 0;
    CHECK(seq_mod_send_once(&s, 4) == 1 && lg.n == 1 && lg.m[0][0] == 0xB2 && lg.m[0][1] == 24);

    /* ---- save and load, and songs carry it ---- */
    seq_init(&s, rec, &lg);
    s.mod.on = 0;
    s.mod.lfo[2] = (ModLfo){.wave = WAVE_SMOOTH, .sync = 0, .rate = 20, .phase = 35, .hold = 1, .stop_hold = 1};
    s.mod.dest[11] = (ModDest){1, 2, TO_CH1 + 12, 94, 17, 99, 1, -1, 0};
    char dir[] = "/tmp/seq4h-mod-XXXXXX", path[256];
    CHECK(mkdtemp(dir));
    snprintf(path, sizeof path, "%s/m.pat", dir);
    CHECK(seq_save_file(&s, path) == 0);
    static Seq t;
    seq_init(&t, NULL, NULL);
    FILE *f = fopen(path, "r");
    CHECK(f && seq_load(&t, f) == 0);
    fclose(f);
    CHECK(t.mod.on == 0);
    CHECK(t.mod.lfo[2].wave == WAVE_SMOOTH && !t.mod.lfo[2].sync && t.mod.lfo[2].rate == 20 && t.mod.lfo[2].phase == 35 && t.mod.lfo[2].hold && t.mod.lfo[2].stop_hold);
    ModDest *y = &t.mod.dest[11];
    CHECK(y->on && y->lfo == 2 && y->to == TO_CH1 + 12 && y->cc == 94 && y->lo == 17 && y->hi == 99 && y->inv);
    {   /* SEND TO in the file keeps the 4-track numbers: 0-3 T1-T4, 4 T1-T4 ALL, 5-20 CH 1-16; then 21-24 T5-T8, 25, 26 */
        static const struct { int mem, file; } TO[] = {{TO_T1 + 2, 2}, {TO_ALL, 4}, {TO_CH1, 5}, {TO_CH1 + 15, 20},
                                                      {TO_T1 + 4, 21}, {TO_T1 + 7, 24}, {TO_ALL2, 25}, {TO_ALL8, 26}};
        for (unsigned k = 0; k < sizeof TO / sizeof TO[0]; k++) {
            char line[64], want[16];
            Mod mm;
            mod_init(&mm);
            mm.dest[0].to = (uint8_t)TO[k].mem;
            char buf[4096] = {0};
            FILE *mf = fmemopen(buf, sizeof buf, "w");
            mod_save(&mm, mf);
            fclose(mf);
            snprintf(want, sizeof want, "md 0 %d %d %d ", mm.dest[0].on, mm.dest[0].lfo, TO[k].file);
            CHECK(strstr(buf, want) != NULL);
            snprintf(line, sizeof line, "md 0 1 0 %d 74 0 127 0\n", TO[k].file);
            mod_init(&mm);
            CHECK(mod_load_line(&mm, line) == 1 && mm.dest[0].to == TO[k].mem);
        }
    }
    snprintf(path, sizeof path, "%s/song.pat", dir);
    CHECK(song_save_file(&s, "MOD", path) == 0);
    seq_init(&t, NULL, NULL);
    CHECK(song_read_file(path, &t, NULL) == 0 && t.mod.dest[11].cc == 94 && t.mod.lfo[2].phase == 35);
    /* bad values are clamped */
    seq_init(&t, NULL, NULL);
    CHECK(mod_load_line(&t.mod, "md 3 1 9 99 200 90 10 0\n") == 1);
    CHECK(t.mod.dest[3].lfo == 3 && t.mod.dest[3].to == TO_COUNT - 1 && t.mod.dest[3].cc == 119 && t.mod.dest[3].lo == 90 && t.mod.dest[3].hi == 90);
    CHECK(mod_load_line(&t.mod, "lf 9 1 1 1 1 1 1\n") == 1 && mod_load_line(&t.mod, "lfo 1\n") == 0 && mod_load_line(&t.mod, "s 0 0 1 60 100 50 100 0\n") == 0);
    /* an old file without mod lines keeps the defaults */
    snprintf(path, sizeof path, "%s/old.pat", dir);
    f = fopen(path, "w");
    fprintf(f, "seq4h 1\nbpm 90\n");
    fclose(f);
    seq_init(&t, NULL, NULL);
    f = fopen(path, "r");
    CHECK(seq_load(&t, f) == 0);
    fclose(f);
    CHECK(t.mod.on && !t.mod.dest[0].on && t.mod.dest[0].cc == 74);

    /* ---- MIDI input parser ---- */
    MidiParse p = {0};
    uint8_t out[3];
    const uint8_t in[] = {0x90, 60, 100, 62, 0, /* running status: a second note on (vel 0) */
                          0xB0, 74, 0xF8, 64,   /* clock in the middle of a CC */
                          0xF0, 1, 2, 3, 0xF7,  /* SysEx skipped */
                          0xC3, 5, 0xE0, 0, 64, 0xFA};
    int got[8][4], n = 0;
    for (unsigned i = 0; i < sizeof in; i++) {
        int len = midi_parse_byte(&p, in[i], out);
        if (len) { got[n][0] = len; got[n][1] = out[0]; got[n][2] = out[1]; got[n][3] = out[2]; n++; }
    }
    CHECK(n == 7);
    CHECK(got[0][0] == 3 && got[0][1] == 0x90 && got[0][2] == 60 && got[0][3] == 100);
    CHECK(got[1][0] == 3 && got[1][1] == 0x90 && got[1][2] == 62 && got[1][3] == 0);
    CHECK(got[2][0] == 1 && got[2][1] == 0xF8);
    CHECK(got[3][0] == 3 && got[3][1] == 0xB0 && got[3][2] == 74 && got[3][3] == 64);
    CHECK(got[4][0] == 2 && got[4][1] == 0xC3 && got[4][2] == 5);
    CHECK(got[5][0] == 3 && got[5][1] == 0xE0 && got[5][3] == 64);
    CHECK(got[6][0] == 1 && got[6][1] == 0xFA);

    /* ---- monitor ---- */
    MidiMon mon;
    mon_clear(&mon);
    uint8_t clk = 0xF8, cc[3] = {0xB1, 1, 64}, note[3] = {0x90, 60, 0};
    for (int i = 0; i < 30; i++) mon_add(&mon, &clk, 1, 0);
    CHECK(mon.clocks == 30 && mon.nlog == 0);   /* clock is counted, not listed */
    mon_add(&mon, note, 3, 0);
    mon_add(&mon, cc, 3, 1);
    uint8_t m3[3];
    int echo;
    CHECK(mon_get(&mon, 0, m3, &echo) == 3 && m3[0] == 0xB1 && echo == 1);
    CHECK(mon_get(&mon, 1, m3, &echo) == 3 && m3[0] == 0x90 && echo == 0);
    CHECK(mon_get(&mon, 2, m3, &echo) == 0);
    for (int i = 0; i < 40; i++) mon_add(&mon, cc, 3, 0);
    CHECK(mon.nlog == MON_LINES && mon.ccs == 41);
    char txt[48];
    mon_text(cc, 3, txt, sizeof txt); CHECK(!strcmp(txt, "CH 2  CC 1 = 64"));
    mon_text(note, 3, txt, sizeof txt); CHECK(!strcmp(txt, "CH 1  NOTE OFF C4"));

    /* ---- learn ---- */
    seq_init(&s, rec, &lg);
    uint8_t chs[MOD_TRACKS] = {0, 1, 2, 3, 4, 5, 6, 7};
    uint8_t l1[3] = {0xB1, 21, 10}, l2[3] = {0xB9, 33, 0}, bad[3] = {0xB0, 123, 0};
    CHECK(mod_learn(&s.mod, 5, l1, 3, chs) && s.mod.dest[5].cc == 21 && s.mod.dest[5].to == TO_T1 + 1);   /* channel 2 = T2 */
    CHECK(mod_learn(&s.mod, 5, l2, 3, chs) && s.mod.dest[5].cc == 33 && s.mod.dest[5].to == TO_CH1 + 9);  /* no track on 10 */
    CHECK(!mod_learn(&s.mod, 5, bad, 3, chs) && !mod_learn(&s.mod, 5, note, 3, chs) && s.mod.dest[5].cc == 33);

    /* ---- the pattern still plays the same with modulation running ---- */
    static Seq a, z;
    static Log la;
    seq_init(&a, rec, &la);
    seq_load_demo(&a);
    z = a;
    z.mod.dest[0].on = z.mod.dest[1].on = 1;
    seq_start(&a);
    seq_start(&z);
    la.notes = 0;
    for (int i = 0; i < 384 * 8; i++) seq_tick(&a);
    int notes_a = la.notes;
    la.notes = 0;
    for (int i = 0; i < 384 * 8; i++) seq_tick(&z);
    CHECK(la.notes == notes_a && notes_a > 0);

    printf("mod tests passed\n");
    return 0;
}
