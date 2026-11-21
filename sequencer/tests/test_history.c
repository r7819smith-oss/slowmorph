/* Undo history: many steps back, redo, merging repeated edits, the depth limit. */
#include "../src/engine.h"
#include "../src/gen.h"
#include "../src/history.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void nosend(void *ctx, const uint8_t *m, int n) { (void)ctx; (void)m; (void)n; }

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); return 1; } } while (0)

static History h;
static Seq s;

/* One edit as the UI makes it: capture, change, push if anything changed. */
static void edit(int t, int i, const char *what, uint32_t key, uint32_t ms) {
    HistSnap pre, post;
    hist_capture(&s, &pre);
    seq_step_toggle(&s, t, i);
    hist_capture(&s, &post);
    if (hist_changed(&pre, &post)) hist_push(&h, &pre, what, key, ms);
}

static int test_undo_redo(void) {
    seq_init(&s, nosend, NULL);
    style_apply(&s, STYLE_AMBIENT, 0);
    hist_clear(&h);
    HistSnap start, mid, now;
    hist_capture(&s, &start);
    edit(0, 1, "A", 0, 0);
    edit(1, 2, "B", 0, 10);
    hist_capture(&s, &mid);
    edit(2, 3, "C", 0, 20);
    CHECK(h.nundo == 3 && h.nredo == 0);
    CHECK(!strcmp(hist_undo_entry(&h, 0)->what, "C") && !strcmp(hist_undo_entry(&h, 2)->what, "A"));
    char w[HIST_WHAT + 1];
    CHECK(hist_undo(&h, &s, w) && !strcmp(w, "C"));
    hist_capture(&s, &now);
    CHECK(!hist_changed(&now, &mid));
    CHECK(hist_undo(&h, &s, w) && hist_undo(&h, &s, w) && !strcmp(w, "A"));
    hist_capture(&s, &now);
    CHECK(!hist_changed(&now, &start));
    CHECK(!hist_undo(&h, &s, w));
    CHECK(h.nredo == 3 && !strcmp(hist_redo_entry(&h, 0)->what, "A"));
    CHECK(hist_redo(&h, &s, w) && !strcmp(w, "A") && hist_redo(&h, &s, w) && !strcmp(w, "B"));
    hist_capture(&s, &now);
    CHECK(!hist_changed(&now, &mid));
    edit(3, 4, "D", 0, 30);   /* a new edit drops the redo list */
    CHECK(h.nredo == 0 && h.nundo == 3);
    return 0;
}

static int test_merge_and_depth(void) {
    seq_init(&s, nosend, NULL);
    hist_clear(&h);
    edit(0, 0, "X", 7, 0);
    edit(0, 1, "X", 7, 500);
    edit(0, 2, "X", 7, 1000);    /* same key, each within the merge time: one entry */
    CHECK(h.nundo == 1);
    edit(0, 3, "X", 7, 5000);    /* too late: a new one */
    CHECK(h.nundo == 2);
    hist_undo(&h, &s, NULL);
    hist_undo(&h, &s, NULL);
    for (int i = 0; i < 4; i++) CHECK(!s.track[0].step[i].on);
    hist_clear(&h);
    for (int k = 0; k < HIST_DEPTH + 5; k++) { char w[8]; snprintf(w, sizeof w, "E%d", k); edit(1, k % SEQ_STEPS, w, 0, (uint32_t)k); }
    CHECK(h.nundo == HIST_DEPTH);
    char w[8];
    snprintf(w, sizeof w, "E%d", 5);
    CHECK(!strcmp(hist_undo_entry(&h, HIST_DEPTH - 1)->what, w));   /* the oldest five went */
    int n = 0;
    while (hist_undo(&h, &s, NULL)) n++;
    CHECK(n == HIST_DEPTH && h.nredo == HIST_DEPTH);
    return 0;
}

/* Length, drum lanes and POLY come back too. */
static int test_fields(void) {
    seq_init(&s, nosend, NULL);
    hist_clear(&h);
    HistSnap pre, post;
    s.track[2].type = TRACK_DRUMS;
    hist_capture(&s, &pre);
    s.track[2].drum.vel[3][5] = 99;
    s.track[1].len = 7;
    seq_track_poly(&s, 0, 1);
    hist_capture(&s, &post);
    CHECK(hist_changed(&pre, &post) == 7);
    hist_push(&h, &pre, "MANY", 0, 0);
    CHECK(hist_undo(&h, &s, NULL));
    CHECK(s.track[2].drum.vel[3][5] == 0 && s.track[1].len == pre.t[1].len && !s.track[0].poly);
    CHECK(hist_redo(&h, &s, NULL));
    CHECK(s.track[2].drum.vel[3][5] == 99 && s.track[1].len == 7 && s.track[0].poly);
    return 0;
}

int main(void) {
    if (test_undo_redo() || test_merge_and_depth() || test_fields()) return 1;
    printf("test_history: all passed\n");
    return 0;
}
