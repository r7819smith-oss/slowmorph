#ifndef SEQ4H_UI_H
#define SEQ4H_UI_H

#include "input.h"
#include "player.h"

typedef struct {
    int fullscreen;
    const char *screenshot;   /* if set: render a few frames, save BMP, exit */
    const char *keys;         /* testing: comma-separated SDL key names to replay; "+name" = press only, "-name" = release only */
    const InputMap *input;    /* button mapping; NULL = built-in defaults */
    int show_input;           /* show the last raw input on screen and print it to stderr */
    int verbose;              /* print each input device as it is opened */
    const char *autosave;     /* pattern file to keep saved while running (NULL = none); see autosave() */
} UiOptions;

/* Runs the interface until the user quits. Returns 0 on success. */
int ui_run(Player *p, const UiOptions *opt);

#endif
