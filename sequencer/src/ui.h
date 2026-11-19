#ifndef SEQ4H_UI_H
#define SEQ4H_UI_H

#include "player.h"

typedef struct {
    int fullscreen;
    const char *screenshot;   /* if set: render a few frames, save BMP, exit */
    const char *keys;         /* testing: comma-separated SDL key names to replay; "+name" = press only, "-name" = release only */
    const char *midi_desc;    /* shown in the header */
} UiOptions;

/* Runs the interface until the user quits. Returns 0 on success. */
int ui_run(Player *p, const UiOptions *opt);

#endif
