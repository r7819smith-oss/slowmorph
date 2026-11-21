#include "../src/input.h"

#include <stdio.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); return 1; } } while (0)

int main(void) {
    InputMap m;
    input_defaults(&m);

    /* RG40XX-V raw joystick defaults under Knulli, L2/R2 are buttons 13/14 */
    CHECK(m.mode == INPUT_AUTO);
    CHECK(m.joy[3] == B_A && m.joy[4] == B_B && m.joy[5] == B_Y && m.joy[6] == B_X);
    CHECK(m.joy[7] == B_L && m.joy[8] == B_R && m.joy[9] == B_SEL && m.joy[10] == B_START);
    CHECK(m.joy[13] == B_L2 && m.joy[14] == B_R2 && m.joy[0] == B_NONE && m.joy[12] == B_ROLL);
    CHECK(m.axis[1][0] == B_VUP && m.axis[1][1] == B_VDOWN && input_btn_from_name("vdown") == B_VDOWN);
    CHECK(m.hat[0] == B_UP && m.hat[3] == B_LEFT);
    CHECK(m.pad_axis[SDL_CONTROLLER_AXIS_TRIGGERRIGHT][1] == B_R2);
    CHECK(m.joy[11] == B_MENU && input_key(&m, SDLK_m) == B_MENU && input_btn_from_name("menu") == B_MENU);
    CHECK(input_key(&m, SDLK_1) == B_L2 && input_btn_from_name("r2") == B_R2);
    CHECK(input_key(&m, SDLK_z) == B_A && input_key(&m, SDLK_ESCAPE) == B_QUIT);

    /* each source */
    CHECK(input_parse_line(&m, "mode joystick") == 0 && m.mode == INPUT_JOYSTICK);
    CHECK(input_parse_line(&m, "joy 0 b   # swap") == 0 && m.joy[0] == B_B);
    CHECK(input_parse_line(&m, "JOY 14 Quit") == 0 && m.joy[14] == B_QUIT);
    CHECK(input_parse_line(&m, "joy 10 none") == 0 && m.joy[10] == B_NONE);
    CHECK(input_parse_line(&m, "hat down up") == 0 && m.hat[2] == B_UP);
    CHECK(input_parse_line(&m, "axis 1- up") == 0 && m.axis[1][0] == B_UP);
    CHECK(input_parse_line(&m, "axis 1+ down") == 0 && m.axis[1][1] == B_DOWN);
    CHECK(input_parse_line(&m, "pad a b") == 0 && m.pad[SDL_CONTROLLER_BUTTON_A] == B_B);
    CHECK(input_parse_line(&m, "pad lefttrigger undo") == 0 && m.pad_axis[SDL_CONTROLLER_AXIS_TRIGGERLEFT][1] == B_UNDO);
    CHECK(input_parse_line(&m, "pad leftx- left") == 0 && m.pad_axis[SDL_CONTROLLER_AXIS_LEFTX][0] == B_LEFT);
    CHECK(input_parse_line(&m, "key Left_Shift lock") == 0 && input_key(&m, SDLK_LSHIFT) == B_LOCK);
    CHECK(input_parse_line(&m, "key z none") == 0 && input_key(&m, SDLK_z) == B_NONE);
    CHECK(input_parse_line(&m, "threshold 8000") == 0 && m.axis_threshold == 8000);
    CHECK(input_parse_line(&m, "   # only a comment") == 0);
    CHECK(input_parse_line(&m, "") == 0);

    /* rejected lines leave the map alone */
    CHECK(input_parse_line(&m, "joy 99 a") != 0);
    CHECK(input_parse_line(&m, "joy 1 jump") != 0);
    CHECK(input_parse_line(&m, "joy 1") != 0);
    CHECK(input_parse_line(&m, "axis 1 up") != 0);
    CHECK(input_parse_line(&m, "pad leftx up") != 0);
    CHECK(input_parse_line(&m, "hat middle a") != 0);
    CHECK(input_parse_line(&m, "mode sideways") != 0);
    CHECK(input_parse_line(&m, "joy 1 a extra") != 0);
    CHECK(m.joy[0] == B_B && m.mode == INPUT_JOYSTICK);

    /* save then load gives the same map */
    FILE *f = tmpfile();
    input_save(&m, f);
    rewind(f);
    InputMap n;
    input_defaults(&n);
    for (int i = 0; i < IN_JOY_BUTTONS; i++) n.joy[i] = B_NONE;   /* prove the file sets them */
    CHECK(input_load(&n, f, "tmp", stderr) == 0);
    fclose(f);
    CHECK(n.mode == m.mode && n.axis_threshold == m.axis_threshold);
    CHECK(!memcmp(n.joy, m.joy, sizeof m.joy) && !memcmp(n.hat, m.hat, sizeof m.hat));
    CHECK(!memcmp(n.axis, m.axis, sizeof m.axis) && !memcmp(n.pad, m.pad, sizeof m.pad));
    CHECK(!memcmp(n.pad_axis, m.pad_axis, sizeof m.pad_axis));
    CHECK(input_key(&n, SDLK_LSHIFT) == B_LOCK && input_key(&n, SDLK_SPACE) == B_START);

    /* the shipped Knulli file parses cleanly */
    f = fopen("knulli/seq4h.input", "r");
    CHECK(f);
    input_defaults(&n);
    CHECK(input_load(&n, f, "knulli/seq4h.input", stderr) == 0);
    fclose(f);
    CHECK(n.mode == INPUT_JOYSTICK && n.joy[13] == B_L2 && n.joy[14] == B_R2 && n.joy[11] == B_MENU);
    CHECK(n.joy[7] == B_L && n.joy[10] == B_START && n.joy[3] == B_A && n.joy[0] == B_NONE);

    puts("input tests passed");
    return 0;
}
