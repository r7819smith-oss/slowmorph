#ifndef SEQ4H_HELP_H
#define SEQ4H_HELP_H

/* On-screen help (M > HELP, or HELP in the hold-M jump panel): the controls
 * of one page, then the ones that work everywhere. Each line is
 * "BUTTON|WHAT IT DOES"; a line starting with '#' is a heading, and a line
 * without '|' is a note. Page names are the UI's (PAGE_NAMES in ui.c). */

/* The lines for the page called `page` (its own controls, then EVERYWHERE).
 * Returns how many were written to out (at most max). */
int help_lines(const char *page, const char **out, int max);

#endif
