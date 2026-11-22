#include "history.h"

#include <stdio.h>
#include <string.h>

void hist_clear(History *h) { h->head = h->nundo = h->nredo = 0; h->last_key = h->last_ms = 0; }

void hist_capture(const Seq *s, HistSnap *out) {
    memset(out, 0, sizeof *out);   /* padding too, so hist_changed can compare bytes */
    for (int t = 0; t < SEQ_TRACKS; t++) {
        const SeqTrack *tr = &s->track[t];
        HistTrack *o = &out->t[t];
        memcpy(o->step, tr->step, sizeof o->step);
        o->drum = tr->drum;
        o->drum_step_lock = tr->drum_step_lock;
        o->len = tr->len;
        o->chord = tr->chord;
        o->poly = tr->poly;
    }
}

int hist_changed(const HistSnap *a, const HistSnap *b) {
    int m = 0;
    for (int t = 0; t < SEQ_TRACKS; t++)
        if (memcmp(&a->t[t], &b->t[t], sizeof a->t[t])) m |= 1 << t;
    return m;
}

void hist_restore(Seq *s, const HistSnap *snap) {
    for (int t = 0; t < SEQ_TRACKS; t++) {
        SeqTrack *tr = &s->track[t];
        const HistTrack *o = &snap->t[t];
        memcpy(tr->step, o->step, sizeof tr->step);
        tr->drum = o->drum;
        tr->drum_step_lock = o->drum_step_lock;
        tr->len = o->len < 1 ? 1 : o->len > SEQ_STEPS ? SEQ_STEPS : o->len;
        tr->chord = o->chord;
        if (tr->poly != o->poly) seq_track_poly(s, t, o->poly);   /* lets the sounding notes go */
    }
}

static HistSnap *undo_at(History *h, int k) { return &h->undo[(h->head + h->nundo - 1 - k + HIST_DEPTH) % HIST_DEPTH]; }

const HistSnap *hist_undo_entry(const History *h, int k) {
    if (k < 0 || k >= h->nundo) return NULL;
    return &h->undo[(h->head + h->nundo - 1 - k + HIST_DEPTH) % HIST_DEPTH];
}

const HistSnap *hist_redo_entry(const History *h, int k) {
    if (k < 0 || k >= h->nredo) return NULL;
    return &h->redo[h->nredo - 1 - k];
}

static void push_undo(History *h, const HistSnap *snap) {
    if (h->nundo == HIST_DEPTH) { h->head = (h->head + 1) % HIST_DEPTH; h->nundo--; }   /* drop the oldest */
    h->undo[(h->head + h->nundo) % HIST_DEPTH] = *snap;
    h->nundo++;
}

void hist_push(History *h, const HistSnap *before, const char *what, uint32_t key, uint32_t now_ms) {
    h->nredo = 0;
    if (key && key == h->last_key && h->nundo && now_ms - h->last_ms < HIST_MERGE_MS) { h->last_ms = now_ms; return; }
    push_undo(h, before);
    snprintf(undo_at(h, 0)->what, sizeof before->what, "%s", what ? what : "EDIT");
    h->last_key = key;
    h->last_ms = now_ms;
}

int hist_undo(History *h, Seq *s, char what[HIST_WHAT + 1]) {
    if (!h->nundo) return 0;
    HistSnap *e = undo_at(h, 0), now;
    hist_capture(s, &now);
    memcpy(now.what, e->what, sizeof now.what);   /* redo does the same edit again */
    if (what) memcpy(what, e->what, HIST_WHAT + 1);
    hist_restore(s, e);
    h->nundo--;
    if (h->nredo < HIST_DEPTH) h->redo[h->nredo++] = now;
    h->last_key = 0;
    return 1;
}

int hist_redo(History *h, Seq *s, char what[HIST_WHAT + 1]) {
    if (!h->nredo) return 0;
    HistSnap *e = &h->redo[h->nredo - 1], now;
    hist_capture(s, &now);
    memcpy(now.what, e->what, sizeof now.what);
    if (what) memcpy(what, e->what, HIST_WHAT + 1);
    hist_restore(s, e);
    h->nredo--;
    push_undo(h, &now);
    h->last_key = 0;
    return 1;
}
