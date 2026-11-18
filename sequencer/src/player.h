#ifndef SEQ4H_PLAYER_H
#define SEQ4H_PLAYER_H

#include <pthread.h>
#include <stdatomic.h>

#include "engine.h"
#include "midi.h"

/* Owns the Seq and a timing thread that calls seq_tick() at 96 PPQN.
 * All access to `seq` from other threads must hold the lock. */
typedef struct {
    Seq seq;
    Midi *midi;
    pthread_mutex_t mu;
    pthread_t thread;
    atomic_int run;
} Player;

void player_init(Player *p, Midi *midi);
int  player_begin(Player *p);     /* start timing thread; 0 on success */
void player_end(Player *p);       /* stop thread, silence, close MIDI */
void player_lock(Player *p);
void player_unlock(Player *p);

#endif
