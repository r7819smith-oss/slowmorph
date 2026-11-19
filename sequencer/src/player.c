#define _GNU_SOURCE
#include "player.h"

#include <sched.h>
#include <time.h>

static void send_cb(void *ctx, const uint8_t *msg, int len) {
    Player *p = ctx;
    if (p->midi) p->midi->send(p->midi, msg, len);
}

void player_init(Player *p, Midi *midi) {
    p->midi = midi;
    pthread_mutex_init(&p->mu, NULL);
    atomic_init(&p->run, 0);
    seq_init(&p->seq, send_cb, p);
}

void player_lock(Player *p) { pthread_mutex_lock(&p->mu); }
void player_unlock(Player *p) { pthread_mutex_unlock(&p->mu); }

static void add_ns(struct timespec *t, long ns) {
    t->tv_nsec += ns;
    while (t->tv_nsec >= 1000000000L) { t->tv_nsec -= 1000000000L; t->tv_sec++; }
}

static long ns_diff(const struct timespec *a, const struct timespec *b) {   /* a - b */
    return (a->tv_sec - b->tv_sec) * 1000000000L + (a->tv_nsec - b->tv_nsec);
}

static void *run(void *arg) {
    Player *p = arg;
    /* Best effort: real-time priority lowers jitter; silently ignored if not permitted. */
    struct sched_param sp = {.sched_priority = 10};
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);

    struct timespec next, now;
    clock_gettime(CLOCK_MONOTONIC, &next);
    while (atomic_load(&p->run)) {
        player_lock(p);
        int bpm = p->seq.bpm;
        seq_tick(&p->seq);
        player_unlock(p);

        add_ns(&next, (long)(60000000000LL / ((long long)bpm * SEQ_PPQN)));
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (ns_diff(&now, &next) > 50000000L) next = now;   /* fell behind (e.g. suspended): don't burst */
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
    }
    return NULL;
}

int player_begin(Player *p) {
    atomic_store(&p->run, 1);
    return pthread_create(&p->thread, NULL, run, p);
}

void player_end(Player *p) {
    if (atomic_exchange(&p->run, 0)) pthread_join(p->thread, NULL);
    player_lock(p);
    seq_stop(&p->seq);
    player_unlock(p);
    if (p->midi) { p->midi->close(p->midi); p->midi = NULL; }
}
