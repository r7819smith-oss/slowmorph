#ifndef SEQ4H_INPUT_H
#define SEQ4H_INPUT_H

/* Button mapping: turns keyboard, raw joystick and SDL game controller input
 * into seq4h's buttons. Defaults match the RG40XX-V under Knulli (raw
 * joystick) and a desktop keyboard; a text file can change any of it.
 * The file format is described in README.md ("Button mapping"). */

#include <SDL.h>
#include <stdio.h>

typedef enum {
    B_UP, B_DOWN, B_LEFT, B_RIGHT, B_A, B_B, B_X, B_Y, B_L, B_R, B_L2, B_R2, B_SEL, B_START, B_MENU,
    /* actions a button can be mapped to directly (shortcuts, e.g. for a keyboard) */
    B_PAGE, B_ROLL, B_UNDO, B_LOCK, B_QUIT,
    B_PREV, B_NEXT,   /* previous / next step (the stick, left / right), on every page */
    B_COUNT,
    B_NONE = -1
} Btn;

typedef enum { INPUT_AUTO, INPUT_JOYSTICK, INPUT_CONTROLLER } InputMode;

#define IN_JOY_BUTTONS 32
#define IN_JOY_AXES 16
#define IN_KEYS 64

typedef struct {
    InputMode mode;
    int axis_threshold;                       /* 1..32767; an axis past this counts as pressed */
    Btn joy[IN_JOY_BUTTONS];                  /* raw joystick button index -> button */
    Btn hat[4];                               /* hat up, right, down, left (any hat) */
    Btn axis[IN_JOY_AXES][2];                 /* raw axis, [0] = negative, [1] = positive */
    Btn pad[SDL_CONTROLLER_BUTTON_MAX];       /* SDL game controller button -> button */
    Btn pad_axis[SDL_CONTROLLER_AXIS_MAX][2]; /* SDL game controller axis (triggers are positive only) */
    int nkeys;
    SDL_Keycode key[IN_KEYS];
    Btn key_btn[IN_KEYS];
} InputMap;

/* Fills in the built-in defaults. */
void input_defaults(InputMap *m);

/* Reads mapping lines from f on top of what is already in m. Returns the
 * number of lines it could not understand; each is reported on err (if set)
 * with name for context. */
int input_load(InputMap *m, FILE *f, const char *name, FILE *err);

/* Writes m in the file format, so it can be used as a starting point. */
void input_save(const InputMap *m, FILE *f);

/* Applies one line, e.g. "joy 10 page". Returns 0 on success. */
int input_parse_line(InputMap *m, const char *line);

Btn input_key(const InputMap *m, SDL_Keycode k);
const char *input_btn_name(Btn b);
Btn input_btn_from_name(const char *s);

#endif
