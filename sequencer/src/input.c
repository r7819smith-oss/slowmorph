#include "input.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static const char *BTN_NAMES[B_COUNT] = {
    "up", "down", "left", "right", "a", "b", "x", "y", "l", "r", "l2", "r2", "select", "start", "menu",
    "page", "roll", "undo", "lock", "quit", "prev", "next", "vup", "vdown",
};

static const char *HAT_NAMES[4] = {"up", "right", "down", "left"};

const char *input_btn_name(Btn b) { return b >= 0 && b < B_COUNT ? BTN_NAMES[b] : "none"; }

Btn input_btn_from_name(const char *s) {
    for (int i = 0; i < B_COUNT; i++)
        if (!strcasecmp(s, BTN_NAMES[i])) return (Btn)i;
    return B_NONE;
}

static void set_key(InputMap *m, SDL_Keycode k, Btn b) {
    for (int i = 0; i < m->nkeys; i++)
        if (m->key[i] == k) { m->key_btn[i] = b; return; }
    if (m->nkeys < IN_KEYS) { m->key[m->nkeys] = k; m->key_btn[m->nkeys++] = b; }
}

Btn input_key(const InputMap *m, SDL_Keycode k) {
    for (int i = 0; i < m->nkeys; i++)
        if (m->key[i] == k) return m->key_btn[i];
    return B_NONE;
}

void input_defaults(InputMap *m) {
    memset(m, 0, sizeof *m);
    m->mode = INPUT_AUTO;
    m->axis_threshold = 16000;
    for (int i = 0; i < IN_JOY_BUTTONS; i++) m->joy[i] = B_NONE;
    for (int i = 0; i < IN_JOY_AXES; i++) m->axis[i][0] = m->axis[i][1] = B_NONE;
    for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; i++) m->pad[i] = B_NONE;
    for (int i = 0; i < SDL_CONTROLLER_AXIS_MAX; i++) m->pad_axis[i][0] = m->pad_axis[i][1] = B_NONE;

    /* RG40XX-V raw joystick under Knulli ("Deeplay-keys"): A=3 B=4 Y=5 X=6
     * L1=7 R1=8 Select=9 Start=10 M=11 stick=12 L2=13 R2=14, volume 1/2,
     * D-pad on hat 0. L2/R2 are plain buttons, not trigger axes. Stock
     * firmware numbers the same buttons 3 lower (A=0 ... Start=7). */
    static const struct { int code; Btn b; } JOY[] = {
        {3, B_A}, {4, B_B}, {5, B_Y}, {6, B_X}, {7, B_L}, {8, B_R},
        {9, B_SEL}, {10, B_START}, {11, B_MENU}, {12, B_ROLL}, {13, B_L2}, {14, B_R2},
    };
    for (size_t i = 0; i < sizeof JOY / sizeof JOY[0]; i++) m->joy[JOY[i].code] = JOY[i].b;
    m->hat[0] = B_UP; m->hat[1] = B_RIGHT; m->hat[2] = B_DOWN; m->hat[3] = B_LEFT;
    /* the analog stick, left / right: previous / next step (axis 0 is the
     * stick's left-right on SDL's usual numbering; check with BUTTON CODES) */
    m->axis[0][0] = B_PREV; m->axis[0][1] = B_NEXT;
    m->axis[1][0] = B_VUP; m->axis[1][1] = B_VDOWN;   /* up / down (axis 1, unverified on the RG40XX-V) */

    /* SDL game controller names (used for pads SDL recognises, in auto or controller mode) */
    m->pad[SDL_CONTROLLER_BUTTON_DPAD_UP] = B_UP;
    m->pad[SDL_CONTROLLER_BUTTON_DPAD_DOWN] = B_DOWN;
    m->pad[SDL_CONTROLLER_BUTTON_DPAD_LEFT] = B_LEFT;
    m->pad[SDL_CONTROLLER_BUTTON_DPAD_RIGHT] = B_RIGHT;
    m->pad[SDL_CONTROLLER_BUTTON_A] = B_A;
    m->pad[SDL_CONTROLLER_BUTTON_B] = B_B;
    m->pad[SDL_CONTROLLER_BUTTON_X] = B_X;
    m->pad[SDL_CONTROLLER_BUTTON_Y] = B_Y;
    m->pad[SDL_CONTROLLER_BUTTON_LEFTSHOULDER] = B_L;
    m->pad[SDL_CONTROLLER_BUTTON_RIGHTSHOULDER] = B_R;
    m->pad[SDL_CONTROLLER_BUTTON_BACK] = B_SEL;
    m->pad[SDL_CONTROLLER_BUTTON_START] = B_START;
    m->pad[SDL_CONTROLLER_BUTTON_GUIDE] = B_MENU;
    m->pad_axis[SDL_CONTROLLER_AXIS_LEFTX][0] = B_PREV;
    m->pad_axis[SDL_CONTROLLER_AXIS_LEFTX][1] = B_NEXT;
    m->pad_axis[SDL_CONTROLLER_AXIS_LEFTY][0] = B_VUP;
    m->pad_axis[SDL_CONTROLLER_AXIS_LEFTY][1] = B_VDOWN;
    m->pad[SDL_CONTROLLER_BUTTON_LEFTSTICK] = B_ROLL;   /* stick press: quick roll */
    m->pad_axis[SDL_CONTROLLER_AXIS_TRIGGERLEFT][1] = B_L2;
    m->pad_axis[SDL_CONTROLLER_AXIS_TRIGGERRIGHT][1] = B_R2;

    /* desktop keyboard */
    static const struct { SDL_Keycode k; Btn b; } KEYS[] = {
        {SDLK_UP, B_UP}, {SDLK_DOWN, B_DOWN}, {SDLK_LEFT, B_LEFT}, {SDLK_RIGHT, B_RIGHT},
        {SDLK_z, B_A}, {SDLK_x, B_B}, {SDLK_c, B_X}, {SDLK_v, B_Y},
        {SDLK_q, B_L}, {SDLK_w, B_R}, {SDLK_1, B_L2}, {SDLK_2, B_R2}, {SDLK_TAB, B_SEL}, {SDLK_SPACE, B_START}, {SDLK_m, B_MENU},
        {SDLK_e, B_PAGE}, {SDLK_r, B_ROLL}, {SDLK_u, B_UNDO}, {SDLK_k, B_LOCK}, {SDLK_ESCAPE, B_QUIT},
        {SDLK_COMMA, B_PREV}, {SDLK_PERIOD, B_NEXT}, {SDLK_RIGHTBRACKET, B_VUP}, {SDLK_LEFTBRACKET, B_VDOWN},
    };
    for (size_t i = 0; i < sizeof KEYS / sizeof KEYS[0]; i++) set_key(m, KEYS[i].k, KEYS[i].b);
}

/* "N+" / "N-" or "name+" / "name-"; returns the sign index (1 = +, 0 = -) or -1. */
static int split_sign(char *s) {
    size_t n = strlen(s);
    if (n < 2) return -1;
    char c = s[n - 1];
    if (c != '+' && c != '-') return -1;
    s[n - 1] = 0;
    return c == '+';
}

static int parse_int(const char *s, int lo, int hi, int *out) {
    char *end;
    long v = strtol(s, &end, 10);
    if (!*s || *end || v < lo || v > hi) return -1;
    *out = (int)v;
    return 0;
}

int input_parse_line(InputMap *m, const char *line) {
    char buf[256];
    snprintf(buf, sizeof buf, "%s", line);
    char *hash = strchr(buf, '#');
    if (hash) *hash = 0;
    char *src = strtok(buf, " \t\r\n");
    if (!src) return 0;                                    /* blank or comment */
    char *code = strtok(NULL, " \t\r\n");
    char *target = strtok(NULL, " \t\r\n");
    if (!code || strtok(NULL, " \t\r\n")) return -1;

    if (!strcasecmp(src, "mode")) {
        if (target) return -1;
        if (!strcasecmp(code, "auto")) m->mode = INPUT_AUTO;
        else if (!strcasecmp(code, "joystick")) m->mode = INPUT_JOYSTICK;
        else if (!strcasecmp(code, "controller")) m->mode = INPUT_CONTROLLER;
        else return -1;
        return 0;
    }
    if (!strcasecmp(src, "threshold")) return target ? -1 : parse_int(code, 1, 32767, &m->axis_threshold);

    if (!target) return -1;
    Btn b = input_btn_from_name(target);
    if (b == B_NONE && strcasecmp(target, "none")) return -1;

    int n;
    if (!strcasecmp(src, "joy")) {
        if (parse_int(code, 0, IN_JOY_BUTTONS - 1, &n)) return -1;
        m->joy[n] = b;
    } else if (!strcasecmp(src, "hat")) {
        for (n = 0; n < 4 && strcasecmp(code, HAT_NAMES[n]); n++) {}
        if (n == 4) return -1;
        m->hat[n] = b;
    } else if (!strcasecmp(src, "axis")) {
        int sign = split_sign(code);
        if (sign < 0 || parse_int(code, 0, IN_JOY_AXES - 1, &n)) return -1;
        m->axis[n][sign] = b;
    } else if (!strcasecmp(src, "pad")) {
        SDL_GameControllerButton pb = SDL_GameControllerGetButtonFromString(code);
        if (pb != SDL_CONTROLLER_BUTTON_INVALID) { m->pad[pb] = b; return 0; }
        int sign = split_sign(code);
        SDL_GameControllerAxis ax = SDL_GameControllerGetAxisFromString(code);
        if (ax == SDL_CONTROLLER_AXIS_INVALID) return -1;
        if (sign < 0) {                                    /* bare trigger name means its pressed direction */
            if (ax != SDL_CONTROLLER_AXIS_TRIGGERLEFT && ax != SDL_CONTROLLER_AXIS_TRIGGERRIGHT) return -1;
            sign = 1;
        }
        m->pad_axis[ax][sign] = b;
    } else if (!strcasecmp(src, "key")) {
        for (char *p = code; *p; p++) if (*p == '_') *p = ' ';   /* "Left_Shift" for names with spaces */
        SDL_Keycode k = SDL_GetKeyFromName(code);
        if (k == SDLK_UNKNOWN) return -1;
        set_key(m, k, b);
    } else {
        return -1;
    }
    return 0;
}

int input_load(InputMap *m, FILE *f, const char *name, FILE *err) {
    char line[256];
    int bad = 0, no = 0;
    while (fgets(line, sizeof line, f)) {
        no++;
        if (input_parse_line(m, line) != 0) {
            bad++;
            if (err) {
                line[strcspn(line, "\r\n")] = 0;
                fprintf(err, "seq4h: %s:%d: cannot read \"%s\"\n", name, no, line);
            }
        }
    }
    return bad;
}

static void key_name(SDL_Keycode k, char *out, size_t n) {
    snprintf(out, n, "%s", SDL_GetKeyName(k));
    for (char *p = out; *p; p++) if (*p == ' ') *p = '_';
}

void input_save(const InputMap *m, FILE *f) {
    static const char *MODES[3] = {"auto", "joystick", "controller"};
    fprintf(f, "# seq4h button map. Lines: SOURCE CODE BUTTON; later lines win.\n"
               "# buttons: up down left right a b x y l r l2 r2 select start menu page roll undo lock quit none\n");
    fprintf(f, "mode %s\nthreshold %d\n", MODES[m->mode], m->axis_threshold);
    for (int i = 0; i < IN_JOY_BUTTONS; i++)
        if (m->joy[i] != B_NONE) fprintf(f, "joy %d %s\n", i, input_btn_name(m->joy[i]));
    for (int i = 0; i < 4; i++)
        if (m->hat[i] != B_NONE) fprintf(f, "hat %s %s\n", HAT_NAMES[i], input_btn_name(m->hat[i]));
    for (int i = 0; i < IN_JOY_AXES; i++)
        for (int s = 0; s < 2; s++)
            if (m->axis[i][s] != B_NONE) fprintf(f, "axis %d%c %s\n", i, s ? '+' : '-', input_btn_name(m->axis[i][s]));
    for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; i++)
        if (m->pad[i] != B_NONE)
            fprintf(f, "pad %s %s\n", SDL_GameControllerGetStringForButton((SDL_GameControllerButton)i), input_btn_name(m->pad[i]));
    for (int i = 0; i < SDL_CONTROLLER_AXIS_MAX; i++)
        for (int s = 0; s < 2; s++)
            if (m->pad_axis[i][s] != B_NONE)
                fprintf(f, "pad %s%c %s\n", SDL_GameControllerGetStringForAxis((SDL_GameControllerAxis)i), s ? '+' : '-',
                        input_btn_name(m->pad_axis[i][s]));
    for (int i = 0; i < m->nkeys; i++)
        if (m->key_btn[i] != B_NONE) {
            char kn[64];
            key_name(m->key[i], kn, sizeof kn);
            fprintf(f, "key %s %s\n", kn, input_btn_name(m->key_btn[i]));
        }
}
