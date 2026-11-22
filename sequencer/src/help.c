#include "help.h"

#include <string.h>

typedef struct { const char *page; const char *const *lines; } HelpPage;

#define END NULL

static const char *const EVERYWHERE[] = {
    "#EVERYWHERE",
    "L1 / R1|PREVIOUS / NEXT PAGE (GRID, STEP, TRACK, GENERATE)",
    "L2 / R2|PREVIOUS / NEXT TRACK",
    "M (TAP)|MAIN MENU; M OR B AGAIN CLOSES IT",
    "HOLD M|JUMP PANEL: D-PAD PICKS A PAGE, LET GO OF M TO GO THERE",
    "START|PLAY / STOP",
    "B|BACK TO THE GRID",
    "STICK LEFT / RIGHT|PREVIOUS / NEXT STEP",
    "STICK UP / DOWN|VELOCITY (LISTS: THE ROW'S VALUE)",
    "STICK PRESS|ROLL THE TRACK",
    "SELECT + A|ROLL THE TRACK",
    "SELECT + X|UNDO (MANY STEPS: M > UNDO HISTORY)",
    "SELECT + B|REDO",
    "SELECT + Y|LOCK / UNLOCK THE WHOLE TRACK",
    "SELECT + STICK UP/DN|TEMPO",
    "SELECT + START|SAVE AND QUIT",
    END
};

static const char *const GRID[] = {
    "#GRID: ALL TRACKS",
    "D-PAD|MOVE: UP/DOWN TRACK (PAST T4: T5-T8), LEFT/RIGHT STEP",
    "A|STEP ON / OFF (DRUM TRACK: OPENS THE DRUM GRID)",
    "X|EDIT THE STEP ON THE STEP PAGE",
    "Y|LOCK / UNLOCK THE STEP (ROLL AND EVOLVE LEAVE IT)",
    "SELECT|PIANO ROLL (NOTE TRACK) OR DRUM GRID (DRUM TRACK)",
    "LEFT FROM STEP 1|ONTO THE TRACK BOX (T1, T2 ...)",
    "#ON THE TRACK BOX",
    "A / B|MUTE / SOLO THE TRACK",
    "X|ROLL THE TRACK (HOLD X: UNDO)",
    "Y|LOCK THE WHOLE TRACK",
    "SELECT|ALL STEPS: CHANGE EVERY STEP AT ONCE",
    "UP / DOWN, RIGHT|OTHER TRACK (T1-T8), BACK TO STEP 1",
    END
};

static const char *const STEP[] = {
    "#STEP: ONE STEP OF THE TRACK",
    "UP / DOWN|PICK A ROW",
    "LEFT / RIGHT|CHANGE IT (HOLD Y: BIGGER STEPS, NOTE BY OCTAVE)",
    "HOLD X + LEFT/RIGHT|PREVIOUS / NEXT STEP, SAME ROW (OR THE STICK)",
    "A|STEP ON / OFF (DRUM STEP: THE LANE'S HIT)",
    "B|BACK TO THE GRID (OR THE PIANO ROLL IF YOU CAME FROM IT)",
    END
};

static const char *const TRACK[] = {
    "#TRACK: SETTINGS OF THE TRACK",
    "UP / DOWN|PICK A ROW",
    "LEFT / RIGHT|CHANGE IT (HOLD Y: BIGGER STEPS)",
    "ROWS|TYPE, CHANNEL, LENGTH, MUTE, SHUFFLE, BPM, SWING, VOICES",
    "|STEP RATE (1/32 TO 1/1, TRIPLETS), REALIGN ALL (A)",
    END
};

static const char *const GENERATE[] = {
    "#GENERATE: HOW THE TRACK IS MADE AND EVOLVES",
    "UP / DOWN|PICK A ROW",
    "LEFT / RIGHT|CHANGE IT (HOLD Y: BIGGER STEPS)",
    "A|ROLL, OR THE ROW'S ACTION (ANCHOR, TRANSFORM, SLOT, SNAP)",
    "X|UNDO THE LAST EDIT",
    "Locked steps and tracks never change on a roll or evolve.",
    END
};

static const char *const ROLL[] = {
    "#PIANO ROLL: ONE NOTE TRACK",
    "D-PAD|MOVE: LEFT/RIGHT STEP, UP/DOWN PITCH",
    "A|PUT A NOTE HERE / TAKE IT OFF (POLY: ADD TO THE CHORD)",
    "HOLD Y + LEFT/RIGHT|NOTE LENGTH (AND THE LENGTH OF NEW NOTES)",
    "HOLD Y + UP/DOWN|MOVE THE NOTE BY A SCALE DEGREE",
    "Y (TAP)|LOCK / UNLOCK THE STEP",
    "HOLD X + LEFT/RIGHT|OFFSET: EARLIER / LATER",
    "HOLD X + UP/DOWN|VELOCITY",
    "B OR SELECT|BACK TO THE GRID",
    END
};

static const char *const DRUMS[] = {
    "#DRUM GRID: THE 8 LANES OF A DRUM TRACK",
    "D-PAD|MOVE: UP/DOWN LANE, LEFT/RIGHT STEP",
    "A|HIT ON / OFF",
    "HOLD X + UP/DOWN|VELOCITY",
    "HOLD X + LEFT/RIGHT|CHANCE",
    "Y|LOCK / UNLOCK THE LANE",
    "LEFT FROM STEP 1|LANE NAME: A MUTE, B SOLO, X HEAR, Y LOCK",
    "B OR SELECT|BACK TO THE GRID",
    END
};

static const char *const ALLSTEPS[] = {
    "#ALL STEPS: EVERY STEP OF THE TRACK AT ONCE",
    "UP / DOWN|PICK A ROW",
    "LEFT / RIGHT|MOVE EVERY STEP'S VALUE (HOLD Y: BIG)",
    "A|EVERY STEP TO THE AVERAGE (STEP LOCKS: LOCK ALL / CLEAR)",
    "B OR SELECT|BACK TO THE TRACK BOX",
    END
};

static const char *const MENU[] = {
    "#MAIN MENU",
    "UP / DOWN|PICK A ROW",
    "LEFT / RIGHT|CHANGE IT (T ROWS: HOLD Y = NEXT STYLE)",
    "A|OPEN / DO THE ROW (NEW FROM STYLE: A TWICE)",
    "M OR B|CLOSE THE MENU",
    END
};

static const char *const SLOTS[] = {
    "#PATTERN SAVE: 8 SLOTS PER TRACK",
    "UP / DOWN|PICK A SLOT",
    "L2 / R2|OTHER TRACK",
    "A|LOAD THE SLOT (UNDO WITH SELECT + X)",
    "X|SAVE THE PATTERN HERE (X AGAIN IF THE SLOT IS FULL)",
    "Y|RENAME: UP/DOWN LETTER, LEFT/RIGHT MOVE, A KEEP, B CANCEL",
    "SELECT|COPY / CLEAR PANEL",
    "B|BACK TO THE MENU",
    END
};

static const char *const SONGS[] = {
    "#SONG SAVE: WHOLE SESSIONS",
    "UP / DOWN|PICK A SONG",
    "A|LOAD (A AGAIN TO CONFIRM)",
    "X|SAVE THE WHOLE SESSION HERE",
    "Y|RENAME",
    "SELECT|COPY / CLEAR / UNDO LOAD",
    "B|BACK TO THE MENU",
    "Loading a song clears the undo history (UNDO LOAD still works).",
    END
};

static const char *const DRUMMAP[] = {
    "#DRUM MAP: THE NOTE EACH DRUM LANE SENDS",
    "UP / DOWN|PICK A LANE",
    "LEFT / RIGHT|CHANGE ITS NOTE (HEARD)",
    "HOLD Y + LEFT/RIGHT|NEXT FELUCCA SOUND",
    "A|HEAR IT",
    "X|RENAME THE LANE",
    "B|BACK TO THE MENU",
    END
};

static const char *const MIDI[] = {
    "#MIDI OUTPUT",
    "UP / DOWN|PICK A PORT",
    "A|USE IT",
    "B|BACK TO THE MENU",
    END
};

static const char *const DEVICE[] = {
    "#DEVICE + CLOCK",
    "UP / DOWN|PICK A ROW",
    "LEFT / RIGHT|CHANGE IT",
    "A|APPLY THE PRESET (A TWICE)",
    "B|BACK TO THE MENU",
    END
};

static const char *const MOD[] = {
    "#MODULATION: LFO, CC DEST AND MIDI IN",
    "L1 / R1|OTHER MODULATION PAGE",
    "UP / DOWN|PICK A ROW",
    "LEFT / RIGHT|CHANGE IT",
    "A|THE ROW'S ACTION (LEARN, SEND ONCE ...)",
    "B|BACK TO THE MENU",
    END
};

static const char *const HISTORY[] = {
    "#UNDO HISTORY",
    "UP / DOWN|PICK A POINT IN THE LIST",
    "A|GO BACK (OR FORWARD) TO IT",
    "B|BACK",
    "Newest edits at the top. Redo entries sit above NOW.",
    "Undo keeps the last 32 pattern edits: notes, lanes, length.",
    END
};

static const char *const RECORD[] = {
    "#LIVE + RECORD",
    "UP / DOWN|PICK A ROW",
    "LEFT / RIGHT|CHANGE IT (COUNT-IN, CLICK, SONG LENGTH, STOP, SONG POSITION)",
    "A|SET MARK NOW / PUT MARK BACK",
    "START|PLAY (WITH THE COUNT-IN); AGAIN: STOP (AT THE BAR END IF SET)",
    "B|BACK TO THE MENU",
    "The header shows BAR n.n / LENGTH while playing.",
    END
};

static const HelpPage PAGES[] = {
    {"GRID", GRID}, {"STEP", STEP}, {"TRACK", TRACK}, {"GENERATE", GENERATE}, {"ROLL", ROLL},
    {"DRUMS", DRUMS}, {"ALL STEPS", ALLSTEPS}, {"MENU", MENU}, {"PATTERN SAVE", SLOTS},
    {"SONG SAVE", SONGS}, {"DRUM MAP", DRUMMAP}, {"MIDI", MIDI}, {"DEVICE", DEVICE},
    {"LFO", MOD}, {"CC DEST", MOD}, {"MIDI IN", MOD}, {"UNDO HISTORY", HISTORY}, {"RECORD", RECORD},
};

int help_lines(const char *page, const char **out, int max) {
    int n = 0;
    for (unsigned k = 0; k < sizeof PAGES / sizeof PAGES[0]; k++)
        if (page && !strcmp(PAGES[k].page, page)) {
            for (const char *const *l = PAGES[k].lines; *l && n < max; l++) out[n++] = *l;
            if (n < max) out[n++] = "";
            break;
        }
    for (const char *const *l = EVERYWHERE; *l && n < max; l++) out[n++] = *l;
    return n;
}
