/*
  Copyright (C) 1999-2002 Ricardo Ueda Karpischek

  This is free software; you can redistribute it and/or modify
  it under the terms of the version 2 of the GNU General Public
  License as published by the Free Software Foundation.

  This software is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this software; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
  USA.
*/

/*

event.c: GUI initialization and event handler

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <fcntl.h>
#include <stdarg.h>
#include <webkit/webkit.h>
#include <assert.h>
#include "common.h"
#include "gui.h"
#include "docView.h"
#ifndef PI
#define PI M_PI
#endif

/* Event masks */
#define ev1 ButtonPressMask|ButtonReleaseMask|KeyPressMask
#define ev2 ExposureMask|PointerMotionMask|PointerMotionMask
#define ev3 StructureNotifyMask|FocusChangeMask
/*   #define ev3 StructureNotifyMask */
#define evmasks (ev1|ev2|ev3)

/* #define evmasks ((1L<<25)-1) */


/*

Menu availability flags.

    CMA_PAGE     .. there exists a page loaded
    CMA_PATTERNS .. the bookfont is nonempty
    CMA_IDLE     .. the OCR thread is idle
    CMA_CL       .. requires closures computed
    CMA_ZONE     .. at least one zone is defined
    CMA_ABSENT   .. unimplemented feature
    CMA_NCL      .. requires closures not computed

*/
#define CMA_PAGE     1
#define CMA_PATTERNS 2
#define CMA_IDLE     4
#define CMA_CL       8
#define CMA_ZONE     16
#define CMA_ABSENT   32
#define CMA_NCL      64

GtkWidget *menubar, *sbStatus,
    *nbViews, *nbPage, *nbPatterns, *nbTune, *wPageList, *wDocView;

GtkListStore *pageStore;
GtkTreeIter *pageIters = NULL;

static gboolean gui_ready = FALSE;

static GtkTextBuffer *tbOutput = NULL;

/*

Menu help message and failure warning.

*/
char *mhelp;

/*

Do we have the focus?

*/
int focused = 1;

/* (devel)

Redraw flags
------------

The redraw flags inform the function redraw about which portions
of the application window must be redraw. The precise meaning of
each flag depends on the implementation of the redraw function,
that can be analysed directly on the source code.

    redraw_button .. one specific button or all buttons
    redraw_bg     .. redraw background
    redraw_grid   .. the grid on fatbits windows
    redraw_stline .. the status line
    redraw_dw     .. all visible windows
    redraw_inp    .. all text input fields
    redraw_tab    .. tabs and their labels
    redraw_zone   .. rectangle that defines the zone
    redraw_menu   .. menu bar and currently open menu
    redraw_j1     .. redraw junction 1 (page tab)
    redraw_j2     .. redraw junction 2 (page tab)
    redraw_pbar   .. progress bar
    redraw_map    .. alphabet map
    redraw_flea   .. the flea

An individual button may be redraw to reflect a status change
(on/off). The junction 1 is the junction of the top and middle
windows on the page tab, and the junction 2 is the junction of
the middle and bottom window on the page tab. The correspondig
flags are used when resizing some window on the page tab.

If redraw_menu is 2, the menu is entirely redrawn. If redraw_menu is
1, then the draw_menu function will redraw only the last selected item
and the newly selected item, except if the menu is being drawn by the
first time.

The progress bar is displayed on the bottom of the window to
reflect the progress of some slow operation. By now, the
progress bar is unused.

*/

/*

cfont is the display buffer. It contains a matrix of pixels to
be displayed on the windows PAGE_FATBITS and PATTERN.

*/
char cfont[4 * FS * 4 * FS];

/* buffer of messages */
char mb[MMB + 1], mba[MMB + 1];

/* font used to display text messages, its height and width */


/*

The windows.

doc  .. the scanned page, reduced mode
fb   .. the scanned page, fat bits mode
f    .. font editor
outp .. the OCR output
sb   .. the symbol browser
dl   .. the list of pages
sl   .. the list of patterns

*/
dwdesc dw[TOP_DW + 1];

/*

Restricted clip window for the PAGE window.

*/
int PHR, PVR, PDM, PDT;

/* active window and active mode for each tab */
int CDW = -1;
int PAGE_CM = PAGE, PATTERN_CM = PATTERN, TUNE_CM = TUNE;

/* Xcolors */

/*

Mode for display matches.

This variable selects how the matches of symbols and patterns
from the bookfont are displayed. When 0, the GUI will use black
to the symbol pixels and gray to the pattern skeleton
pixels. When 1, swap these colors.

*/
//int dmmode=1;

/* the event */
int have_s_ev;

/* Context */
char *cschema;
int mclip = 0;

/*

The window and its buffer.

*/

/* default name for each tab */
char *tabl[] = { "page", "patterns", "tune", "history" };

/*

Currently active tab. Possible values are PAGE, PATTERN or
TUNE. These names are just the names of some windows from
these tabs (one for each) re-used to identify each tab.

*/
int tab;

/*

Zoomed pixel size. ZPS must be an odd integer, and may be changed
through the -z command-line flag (please check the currently
limits imposed on ZPS value at the getopt loop).

*/
int ZPS = 1;

/* current color */
int COLOR = BLACK;

/* (devel)

The zones
---------

Clara OCR allows to create "zones". Zones are usually used to identify
one text block in the page. For instance, a page containing two text
columns should use one zone to limit each column. The zone limits are
given by the "limits" array. The top left is (limits[0],limits[1]) as
presented by the figure:

    +---------------------------+
    | (0,1)       (6,7)         |
    |  +-----------+            |
    |  |this is a  |            |
    |  |text block |            |
    |  |identifyed |            |
    |  |by a       |            |
    |  |rectangular|            |
    |  |zone.      |            |
    |  +-----------+            |
    | (2,3)       (4,5)         |
    |                           |
    +---------------------------+

Multiple zones are supported simultaneously, and each one is handled
separately when building words and lines and generating the
output. The limits of the second zone are limists[8..15], and so
on. Also, non-rectangular zones are supported, in order to cover
nonrectangular (skewed) text blocks.

*/
int zones = 0;
int limits[LIMITS_SIZE];
int curr_corner;

/*

To measure the duration of a mouse click.

*/
struct timeval tclick;

/*

These variables are documented below (search the service
enter_wait).

*/
int waiting_key = 0, key_pressed;
char inp_str[MMB + 1];
int inp_str_ps;

/*

This is the flea path.

*/
// TODO: This was unsigned originally. Why?
short fp[2 * sizeof(short) * FS * FS];
float fsl[3 * FS * FS], fpp[FS * FS];
int topfp, fpsz, curr_fp = 0, last_fp = -1;

/*

Spyhole stuff.

*/
int sh_tries;


static void rebuild_page_contents();
/*

Create a new menu.

The initial number of items is m and the menu title is tt. If a==1,
the menu is added to the menu bar. If a==2, it must be activated by
some special action, like clicking the mouse button 3 somewhere.

*/
#if 0
cmdesc *addmenu(int a, char *tt, int m) {
        /* enlarge CM */
        if (++TOP_CM >= CM_SZ) {
                int a = 0;

                /*
                   The pointer to the currently active menu (if any) must
                   be recomputed accordingly to the new dynamic CM address.
                 */
                if (cmenu != NULL)
                        a = cmenu - CM;
                CM = c_realloc(CM, sizeof(cmdesc) * (CM_SZ += 30), NULL);
                if (cmenu != NULL)
                        cmenu = CM + a;
        }

        /* Create the menu */
        CM[TOP_CM].a = a;
        strncpy(CM[TOP_CM].tt, tt, MAX_MT);
        CM[TOP_CM].tt[MAX_MT] = 0;
        if (m <= 0)
                m = 5;
        CM[TOP_CM].m = m;
        CM[TOP_CM].n = 0;
        CM[TOP_CM].l = c_realloc(NULL, m * (MAX_MT + 1) + m, NULL);
        CM[TOP_CM].t = c_realloc(NULL, m, NULL);
        CM[TOP_CM].h = c_realloc(NULL, m * sizeof(char *), NULL);
        CM[TOP_CM].f = c_realloc(NULL, m * sizeof(int), NULL);
        CM[TOP_CM].c = -1;
        return (CM + TOP_CM);
}

/*

Add item to menu m, and returns the item identifier.

The item label is t, its type is p and the flag g ("group")
informs if a separator (an horizontal line) must be drawn between
this item and the one just above it. The flag d ("dismiss")
informs if the menu must be dismissed when this item is selected.

The parameter h is the short help message to be presented by the
GUI. The parameter f are the activation flags.

The types are: CM_TA (performs an action when selected), CM_TB
(on/off) and CM_TR (radio item).

*/
int additem(cmdesc *m, char *t, int p, int g, int d, char *h, int f) {
        int id;

        /* maximum number of items reached: enlarge */
        if (m->n >= m->m) {
                int o;

                o = m->m;
                m->l = c_realloc(m->l, (m->m += 5) * (MAX_MT + 1), NULL);
                m->t = c_realloc(m->t, m->m, NULL);
                m->h = c_realloc(m->h, (m->m) * sizeof(char *), NULL);
                m->f = c_realloc(m->f, (m->m) * sizeof(int), NULL);
        }

        /* label */
        strncpy(m->l + (MAX_MT + 1) * m->n, t, MAX_MT);
        m->l[MAX_MT] = 0;

        /* type and group flag */
        m->t[m->n] = p | (g ? CM_NG : 0);

        /* not hidden, by default */
        if (m->n % 2)
                m->t[m->n] |= CM_HI;

        /* dismiss flag */
        if (d == -1)
                d = (p == CM_B) ? 0 : 1;
        if (d)
                m->t[m->n] |= CM_DI;

        /* short help and flags */
        m->h[m->n] = h;
        m->f[m->n] = f;

        /* identifier */
        id = m->n + ((m - CM) << CM_IT_WD);
        ++(m->n);
        return (id);
}
#endif
/* (devel)

The function show_hint
----------------------

Messages are put on the status line (on the bottom of the
application X window) using the show_hint service. The show_hint
service receives two parameters: an integer f and a string (the
message).

If f is 0, then the message is "discardable". It won't be
displayed if a permanent message is currently being displayed.

If f is 1, then the message is "fixed". It won't be erased by a
subsequent show_hint call informing as message the empty string
(in practical terms, the pointer motion won't clear the message).

If f is 2, then the message is "permanent" (the message will be
cleared only by other fixed or permanent message).

If f is 3, clear any current message.

*/
void show_hint(int f, char *s, ...) {
        va_list args;
        static int fixed = 0;

        /* enter_wait disables show_hint */
        if (waiting_key)
                return;

        va_start(args, s);

        if (f == 3) {
                // UNPATCHED: redraw_stline = 1;
                mb[0] = 0;
                fixed = 0;
        }

        /* fixed or permanent entry: redraw unconditionally */
        else if (f) {
                if (vsnprintf(mb, MMB, s, args) < 0)
                        mb[0] = 0;
                else if ((trace) && (mb[0] != 0))
                        tr(mb);
                // UNPATCHED: redraw_stline = 1;
                fixed = f;
        }

        else {

                /*
                   If the last hint was fixed, then refresh only if the
                   new hint is nonempty.
                 */
                if (fixed == 1) {
                        if (s[0] != 0) {
                                if (vsnprintf(mb, MMB, s, args) < 0)
                                        mb[0] = 0;
                                // UNPATCHED: redraw_stline = 1;
                                fixed = 0;
                        }
                }

                /*
                   If the last hint was not fixed, then refresh only if
                   the hint changed.
                 */
                else if ((fixed == 0) && (strcmp(mb, s) != 0)) {
                        if (vsnprintf(mb, MMB, s, args) < 0)
                                mb[0] = 0;
                        // UNPATCHED: redraw_stline = 1;
                }
        }

        va_end(args);

/*
    printf("request (priority %d, mclip=%d, redraw=%d) to draw the message \"%s\"\n",f,mclip,redraw_stline,s);
*/

}

/*

Enter one waiting state, to permit the OCR tasks to accept
asynchronously a user input from the keyboard.

The parameter m is the wait mode (the variable waiting_key stores
the current wait mode):

    Mode 1: wait any key.
    Mode 2: wait ENTER or ESC.
    Mode 3: wait y, Y, n or N.
    Mode 4: wait a string.
    Mode 5: wait <, >, + or -.

Note that all OCR tasks remain stopped while the variable
waiting_key is nonzero.

On cases 2 and 3, the return status will be informed through the
variable key_pressed as follows:

    ENTER, y, or Y .. 1
    ESC, n or N    .. 0

On case 4, the string will return on inp_str.

If f==1, appends the valid inputs to the message to be displayed
on the status line.

*/
void enter_wait(char *s, int f, int m) {
        char b[MMB + 1];

        /* already waiting */
        if (waiting_key)
                return;

        /* build the message to warn the user */
        b[MMB] = 0;
        if (s == NULL)
                b[0] = 0;
        else {
                strncpy(b, s, MMB);
                if (f == 1)
                        strncat(b, " ", MMB - strlen(b));
        }
        if (f == 1) {
                if (m == 1)
                        strncat(b, "(PRESS ANY KEY)", MMB - strlen(mb));
                else if (m == 2)
                        strncat(b, "(ENTER to confirm, ESC to ignore)",
                                MMB - strlen(mb));
                else if (m == 3)
                        strncat(b, "(y/n)", MMB - strlen(mb));
        }

        /* display the message and enter wait mode */
        show_hint(2, b);
        waiting_key = m;

        /*
           In mode 4, the informed message is preserved because the editing
           service will need to redisplay it.
         */
        strncpy(inp_str, s, MMB);
        inp_str[MMB] = 0;
        inp_str_ps = strlen(inp_str);
}

#if 0
/*

Context-dependent "right" action.

*/
void right(void) {

        if ((dw[PAGE_SYMBOL].v) || (dw[PATTERN].v) ||
            (dw[PATTERN_TYPES].v)) {
                form_auto_submit();
        }

        /* select next menu of the menu bar */
        if (cmenu != NULL) {
                if (cmenu->a == 1) {
                        if ((cmenu - CM >= TOP_CM) || ((++cmenu)->a != 1))
                                cmenu = CM;
                        CY0_orig = -1;
                        force_redraw();
                        set_mclip(0);
                }
        }

        /* go to the next pattern */
        else if ((CDW == PATTERN) || (dw[TUNE_PATTERN].v)) {
                if (topp >= 0) {

                        /* find the next untransliterated pattern */
                        if (get_flag(FL_ONLY_DOUBTS)) {
                                while ((++cdfc <= topp) &&
                                       (pattern[cdfc].tr != NULL));
                                if (cdfc > topp)
                                        cdfc = 0;
                        }

                        /* or just go to the next one */
                        else if (++cdfc > topp)
                                cdfc = 0;

                        /* edit it.. */
                        edit_pattern(0);

                        /* ..and recompute skeleton */
                        dw[TUNE_PATTERN].rg = 1;
                        dw[TUNE_SKEL].rg = 1;
                        redraw_document_window();
                }
        }

        /* move the graphic cursor to the next symbol */
        else if (CDW == PAGE) {
                int l;

                if (curr_mc >= 0) {
                        if (mc[curr_mc].E >= 0)
                                curr_mc = mc[curr_mc].E;
                        else if ((l = rsymb(curr_mc, 1)) >= 0)
                                curr_mc = l;
                        check_dlimits(1);
                        if (dw[PAGE_SYMBOL].v)
                                dw[PAGE_SYMBOL].rg = 1;
                        redraw_document_window();
                }
        }

        /* go to the next pattern type */
        else if (CDW == PATTERN_TYPES) {

                if (cpt < 1)
                        cpt = 1;
                else if (cpt < toppt)
                        ++cpt;
                else
                        cpt = 1;
                dw[PATTERN_TYPES].rg = 1;
                redraw_document_window();
        }

        /* scroll right PAGE_FATBITS window */
        else if (CDW == PAGE_FATBITS) {
                ++X0;
                check_dlimits(0);
                RG = 1;
                redraw_document_window();
        }

        /* next revision act */
        else if (CDW == TUNE_ACTS) {
                if (curr_act + 1 <= topa) {
                        ++curr_act;
                        redraw_document_window();
                        dw[TUNE_ACTS].rg = 1;
                }
        }

        /* change DEBUG attachment */
        else if (CDW == DEBUG) {

                if (*cm_g_log != ' ') {
                        *cm_g_log = ' ';
                        *cm_g_vocab = 'X';
                } else if (*cm_g_vocab != ' ') {
                        *cm_g_log = 'X';
                        *cm_g_vocab = ' ';
                }
                dw[DEBUG].rg = 1;
                redraw_document_window();
        }

        /* scroll right HTML documents */
        else if (HTML) {
                step_right();
        }
}

/*

Context-dependent "left" action.

*/
void left(void) {

        if ((dw[PAGE_SYMBOL].v) || (dw[PATTERN].v) ||
            (dw[PATTERN_TYPES].v)) {
                form_auto_submit();
        }

        /* select previous menu of the menu bar */
        if (cmenu != NULL) {
                int l;

                if (cmenu->a == 1) {
                        l = cmenu - CM;
                        for (l = cmenu - CM;
                             (l > 0) && ((--cmenu)->a != 1); --l);
                        if (l == 0)
                                for (cmenu = CM + (l = TOP_CM);
                                     (l > 0) && ((--cmenu)->a != 1); --l);
                        CY0_orig = -1;
                        force_redraw();
                        set_mclip(0);
                }
        }

        /* go to the previous pattern */
        else if ((CDW == PATTERN) || (dw[TUNE_PATTERN].v)) {
                if (topp >= 0) {

                        /* find the previous untransliterated pattern */
                        if (*cm_e_od != ' ') {
                                while ((--cdfc >= 0) &&
                                       (pattern[cdfc].tr != NULL));
                                if (cdfc < 0)
                                        cdfc = topp;
                        }

                        /* or just go to the previous one */
                        else if (--cdfc < 0)
                                cdfc = topp;

                        /* edit it.. */
                        edit_pattern(0);

                        /* ..and recompute skeleton */
                        dw[TUNE_PATTERN].rg = 1;
                        dw[TUNE_SKEL].rg = 1;
                        redraw_document_window();
                }
        }

        /* move the current symbol to the previous one */
        else if (CDW == PAGE) {
                int l;

                if (curr_mc >= 0) {
                        if (mc[curr_mc].W >= 0)
                                curr_mc = mc[curr_mc].W;
                        else if ((l = lsymb(curr_mc, 1)) >= 0)
                                curr_mc = l;
                        check_dlimits(1);
                        if (dw[PAGE_SYMBOL].v)
                                dw[PAGE_SYMBOL].rg = 1;
                        redraw_document_window();
                }
        }

        /* go to the previous pattern type */
        else if (CDW == PATTERN_TYPES) {

                if ((0 <= toppt) && (toppt < cpt))
                        cpt = toppt;
                else if (cpt > 1)
                        --cpt;
                else
                        cpt = toppt;
                dw[PATTERN_TYPES].rg = 1;
                redraw_document_window();
        }

        /* scroll right PAGE_FATBITS window */
        else if (CDW == PAGE_FATBITS) {
                if (X0 > 0) {
                        --X0;
                        RG = 1;
                        redraw_document_window();
                }
        }

        /* previous revision act */
        else if (CDW == TUNE_ACTS) {
                if (curr_act > 0) {
                        --curr_act;
                        redraw_document_window();
                        dw[TUNE_ACTS].rg = 1;
                }
        }

        /* change DEBUG attachment */
        else if (CDW == DEBUG) {

                if (*cm_g_log != ' ') {
                        *cm_g_log = ' ';
                        *cm_g_vocab = 'X';
                } else if (*cm_g_vocab != ' ') {
                        *cm_g_log = 'X';
                        *cm_g_vocab = ' ';
                }
                dw[DEBUG].rg = 1;
                redraw_document_window();
        }

        /* scroll left HTML documents */
        else if (HTML) {
                step_left();
        }
}

/*

Context-dependent "up" action.

*/
void up(void) {

        /* select previous item of the active menu */
        if (cmenu != NULL) {
                if (cmenu->c > 0)
                        --(cmenu->c);
                else
                        cmenu->c = cmenu->n - 1;
                if (cmenu->c >= 0) {
                        set_mclip(1);
                        // UNPATCHED: redraw_menu = 1;
                } else
                        cmenu->c = -1;
        }

        /* move the graphic cursor to the symbol above the current one */
        else if (CDW == PAGE) {
                int l;

                if (curr_mc >= 0) {
                        if (mc[curr_mc].N >= 0)
                                curr_mc = mc[curr_mc].N;
                        else if ((l = tsymb(curr_mc, 1)) >= 0)
                                curr_mc = l;
                        check_dlimits(1);
                        if (dw[PAGE_SYMBOL].v)
                                dw[PAGE_SYMBOL].rg = 1;
                        redraw_document_window();
                }
        }

        /* scroll down HTML documents by one line */
        else if (HTML) {
                if (Y0 > 0) {
                        Y0 -= (DFH + VS);
                        if (Y0 < 0)
                                Y0 = 0;
                        redraw_document_window();
                }
        }

        /* scroll up PAGE_FATBITS window */
        else if (CDW == PAGE_FATBITS) {
                step_up();
        }
}

/*

Context-dependent "down" action.

*/
void down(void) {

        /* select next item of the active menu */
        if (cmenu != NULL) {
                if ((cmenu->c < 0) || (cmenu->c + 1 >= cmenu->n))
                        cmenu->c = 0;
                else
                        ++(cmenu->c);
                if (cmenu->c < cmenu->n) {
                        set_mclip(1);
                        // UNPATCHED: // UNPATCHED: redraw_menu = 1;
                } else
                        cmenu->c = -1;
        }

        /* move the graphic cursor to the symbol below the current one */
        else if (CDW == PAGE) {
                int l;

                if (curr_mc >= 0) {
                        if (mc[curr_mc].S >= 0)
                                curr_mc = mc[curr_mc].S;
                        else if ((l = bsymb(curr_mc, 1)) >= 0)
                                curr_mc = l;
                        check_dlimits(1);
                        if (dw[PAGE_SYMBOL].v)
                                dw[PAGE_SYMBOL].rg = 1;
                        redraw_document_window();
                }
        }

        /* scroll down HTML documents by one line */
        else if (HTML) {
                Y0 += DFH + VS;
                check_dlimits(0);
                redraw_document_window();
        }

        /* scroll up PAGE_FATBITS window */
        else if (CDW == PAGE_FATBITS) {
                step_down();
        }
}

/*

Context-dependent "prior" (pgup) action.

*/
void prior(void) {
        int d;

        if (CDW == PAGE) {
                Y0 -= VR;
                check_dlimits(0);
                redraw_document_window();
        }

        else if (HTML) {
                d = VR;
                d /= (DFH + VS);
                d *= (DFH + VS);
                Y0 -= d;
                check_dlimits(0);
                redraw_document_window();
        }

        else if (CDW == PAGE_FATBITS) {
                Y0 -= FS;
                check_dlimits(0);
                RG = 1;
                redraw_document_window();
        }
}

/*

Context-dependent "next" (pgdw) action.

*/
void next(void) {
        int d;

        if (CDW == PAGE) {
                Y0 += VR;
                check_dlimits(0);
                redraw_document_window();
        }

        else if (HTML) {
                d = VR;
                d /= (DFH + VS);
                d *= (DFH + VS);
                Y0 += d;
                check_dlimits(0);
                redraw_document_window();
        }

        else if (CDW == PAGE_FATBITS) {
                Y0 += FS;
                check_dlimits(0);
                RG = 1;
                redraw_document_window();
        }
}
#endif
/*

Check menu item availability.

*/
int mcheck(int flags) {
        /* requires one page loaded */
        if (flags & CMA_PAGE) {
                if (cpage < 0) {
                        mhelp = "must load a page to activate this item";
                        return (0);
                }
        }

        /* requires nonempty bookfont */
        if (flags & CMA_PATTERNS) {
                if (topp < 0) {
                        mhelp = "no patterns available";
                        return (0);
                }
        }

        /* requires OCR thread idle */
        if (flags & CMA_IDLE) {
                if (ocring) {
                        mhelp = "BUSY";
                        return (0);
                }
        }

        /* requires binarization */
        if (flags & CMA_CL) {
                if (!cl_ready) {
                        mhelp =
                            "this operation requires previous binarization";
                        return (0);
                }
        }

        /* requires zone */
        if (flags & CMA_ZONE) {
                if (zones <= 0) {
                        mhelp = "no zone defined";
                        return (0);
                }
        }

        /* requires implementation */
        if (flags & CMA_ABSENT) {
                mhelp = "this feature is under implementation";
                return (0);
        }

        /* requires binarization */
        if (flags & CMA_NCL) {
                if (cl_ready) {
                        mhelp = "this operation was performed already";
                        return (0);
                }
        }

        return (1);
}

/*

Menu actions.

After catching the selection of one menu item (at mactions_b1),
the associated action is performed here.

If check is nonzero, instead of selecting the item, a test is
performed to check if the item can be selected or not. Returns 1
if yes, 0 otherwise. If check is 2, a message is displayed (a
short help if the item is available, or a diagnostic
otherwise).

*/
#if 0
int cma(int it, int check) {
        /*
           Actions that perform large changes on the screen
           must set new_mclip to 0 in order to the redraw
           routines redraw the screen completely. Otherwise,
           only the menu rectangular region will be redraw.
         */
        int new_mclip = 1;

        /* menu-relative item index */
        int a = it & CM_IT_M;

        /* availability */
        int aval;

        /* invalid item */
        if ((a < 0) || ((cmenu->n) <= a)) {
                return (0);
        }

        /* compute availability */
        mhelp = cmenu->h[a];
        aval = mcheck(cmenu->f[a]);

        /* just checking and/or unavailable */
        if ((check) || (aval == 0)) {
                if (check == 2) {
                        if (aval == 0)
                                show_hint(0, "unavailable: %s", mhelp);
                        else
                                show_hint(0, mhelp);
                }
                return (aval);
        }

        /*
           Change selection on radio group.
         */
        if ((!check) && ((cmenu->t[a] & CM_TM) == CM_TR)) {
                char *s, t;
                int i;

                /* status of current item */
                s = cmenu->l + a * (MAX_MT + 1) + 1;
                t = *s;

                /* search group start */
                for (i = a; (i >= 0) &&
                     (((cmenu->t)[a] & CM_TM) == CM_TR) &&
                     (((cmenu->t)[i] & CM_NG) == 0); --i);
                if (i < 0)
                        ++i;

                /* deactivate all within group */
                do {
                        cmenu->l[i * (MAX_MT + 1) + 1] = ' ';
                        ++i;
                } while ((i < cmenu->n) &&
                         (((cmenu->t)[a] & CM_TM) == CM_TR) &&
                         (((cmenu->t)[i] & CM_NG) == 0));

                /* toggle current */
                if (t == ' ') {
                        *s = 'X';
                } else {
                        *s = ' ';
                }
        }

        /*
           Toggle binary item
         */
        else if ((!check) && ((cmenu->t[a] & CM_TM) == CM_TB)) {
                char *s;

                /* toggle */
                s = cmenu->l + a * (MAX_MT + 1) + 1;
                *s = (*s == ' ') ? 'X' : ' ';
                // UNPATCHED: redraw_menu = 1;
        }

        /*
           Items on the "File" menu
         */
        if (cmenu - CM == CM_F) {


                /* save session */
                if (it == CM_F_SAVE) {

                        /* force */
                        sess_changed = 1;
                        font_changed = 1;
                        hist_changed = 1;
                        form_changed = 1;

                        start_ocr(-2, OCR_SAVE, 0);
                }

                /* save zone */
                else if (it == CM_F_SZ) {

                        if ((pixmap != NULL) && (cl_ready == 0))
                                wrzone("zone.pgm", 1);
                        else
                                wrzone("zone.pbm", 1);
                        new_mclip = 0;
                }

                /* save entire page replacing symbols by patterns */
                else if (it == CM_F_SR) {

                        if (cl_ready)
                                wrzone("tozip.pbm", 0);
                        new_mclip = 0;
                }

                /* write report */
                else if (it == CM_F_REP) {

                        write_report("report.txt");
                        new_mclip = 0;
                }

                /* QUIT the program */
                else if (it == CM_F_QUIT) {

                        finish = 2;
                        start_ocr(-2, OCR_SAVE, 0);
                        new_mclip = 0;
                }
        }

        /*
           Items on "Edit" menu
         */
        else if (cmenu - CM == CM_E) {

                /* sort criteria */
                if ((it == CM_E_SM) ||
                    (it == CM_E_SP) ||
                    (it == CM_E_ST) ||
                    (it == CM_E_SW) ||
                    (it == CM_E_SH) || (it == CM_E_SN)) {

                        dw[PATTERN_LIST].rg = 1;
                        new_mclip = 0;
                }

                /* remove untransliterated patterns */
                else if (it == CM_E_DU) {

                        rm_untrans();
                        dw[PATTERN_LIST].rg = 1;
                        new_mclip = 0;
                }

                else if (it == CM_E_SETPT) {

                        if (!ocring) {
                                ocr_other = 1;
                                start_ocr(-2, OCR_OTHER, 0);
                        }
                        new_mclip = 0;
                }

                /* search barcode */
                else if (it == CM_E_SEARCHB) {

                        if (cl_ready) {
                                if (!ocring) {
                                        ocr_other = 2;
                                        start_ocr(-2, OCR_OTHER, 0);
                                }
                        } else if (cpage < 0)
                                show_hint(2,
                                          "load a page before trying this feature");
                        else
                                show_hint(2,
                                          "perform segmentation before using this feature");
                        new_mclip = 0;
                }

                else if (it == CM_E_THRESH) {

                        if ((pixmap != NULL) && (cl_ready == 0) &&
                            (pm_t == 8)) {

                                /* select the manual global binarizer */
                                bin_method = 1;
                                dw[TUNE].rg = 1;

                                /* proceed */
                                if (!ocring) {
                                        ocr_other = 3;
                                        start_ocr(-2, OCR_OTHER, 0);
                                }
                                new_mclip = 0;
                        } else if ((pixmap != NULL) && (pm_t == 8))
                                show_hint(2,
                                          "cannot threshold after binarization");
                        else
                                show_hint(2, "no graymap loaded");
                }
#ifdef PATT_SKEL
                else if (it == CM_E_RSKEL) {

                        reset_skel_parms();
                        dw[TUNE_PATTERN].rg = 1;
                        dw[TUNE_SKEL].rg = 1;
                        new_mclip = 0;
                }
#endif

                /* others */
                else if ((it == CM_E_DOUBTS) ||
                         (it == CM_E_RESCAN) ||
                         (it == CM_E_FILL) ||
                         (it == CM_E_PP) ||
                         (it == CM_E_FILL_C) || (it == CM_E_PP_C)) {

                }
        }

        /*
           Items on "View" menu
         */
        else if (cmenu - CM == CM_V) {

                /* set FAT BITS mode */
                /*
                   if ((cpage >= 0) && (it == CM_V_FAT)) {
                   if (CDW != PAGE_FATBITS) {
                   if ((CX0_orig >= DM) && (CY0_orig >= DT)) {
                   X0 = (CX0_orig-DM) * RF;
                   Y0 = (CY0_orig-DT) * RF;
                   }
                   setview(PAGE_FATBITS);
                   check_dlimits(1);
                   new_mclip = 0;
                   }
                   }
                 */

                /* change font */
                if ((it == CM_V_SMALL) ||
                    (it == CM_V_MEDIUM) ||
                    (it == CM_V_LARGE) || (it == CM_V_DEF)) {

                        /* set new font */
                        set_xfont();

                        /* recompute window size and components placement */
                        set_mclip(0);

                        /* recompute the placement of the windows */
                        setview(CDW);

                        /* must regenerate HTML documents */
                        {
                                int i;

                                for (i = 0; i <= TOP_DW; ++i)
                                        if (dw[i].html)
                                                dw[i].rg = 1;
                                new_mclip = 0;
                        }

                        new_mclip = 0;
                }

                /*
                   Recompute the geometry and redisplay the window to
                   reflect the new value of the flag *cm_hide.
                 */
                if (it == CM_V_HIDE) {

                        setview(CDW);
                        new_mclip = 0;
                }

                /* generate again the list of patterns */
                else if (it == CM_V_OF) {

                        dw[PATTERN_LIST].rg = 1;
                        new_mclip = 0;
                }

                /* Toggle view HTML source flag: must regenerate HTML documents */
                else if (it == CM_V_VHS) {
                        int i;

                        for (i = 0; i <= TOP_DW; ++i)
                                if (dw[i].html)
                                        dw[i].rg = 1;
                        new_mclip = 0;
                }

                /* recompute form contents */
                else if (it == CM_V_WCLIP) {

                        if (curr_mc >= 0) {
                                dw[PAGE_SYMBOL].rg = 1;
                                redraw_document_window();
                        }
                        if ((0 <= cdfc) && (cdfc <= topp)) {
                                dw[PATTERN].rg = 1;
                                redraw_document_window();
                        }
                        new_mclip = 0;
                }

                /* toggle alphabet mapping display */
                else if (it == CM_V_MAP) {

                        /* recompute window size and the placement of the components */
                        comp_wnd_size(-1, -1);

                        /* recompute the placement of the windows */
                        setview(CDW);

                        new_mclip = 0;
                }

                /* these statuses must redraw the window */
                else if (it == CM_V_CC) {

                        new_mclip = 0;
                        redraw_document_window();
                }

                /* toggle show skeleton tuning */
                else if (it == CM_V_ST) {

                        new_mclip = 0;
                }

                /* start presentation */
                else if (it == CM_V_PRES) {

#ifdef HAVE_SIGNAL
                        if (pmode == 0)
                                cpresentation(1);
#else
                        show_hint(1, "Presentation unsupported!");
#endif
                        new_mclip = 0;
                }

                /*
                   Clear status line to display progress warnings.
                 */
                else if ((it == CM_V_MAT) || (it == CM_V_CMP) ||
                         (it == CM_V_MAT_K) || (it == CM_V_CMP_K)) {

                        show_hint(1, "");
                }
        }

        /*
           Items on "Options" menu
         */
        else if (cmenu - CM == CM_O) {

                /* toggle PAGE only */
                if (it == CM_O_PGO) {

                        if ((0 <= cpage) && (cpage < npages)) {

                                if ((pixmap != NULL) && (pm_t == 8) &&
                                    (!cl_ready)) {
                                        *cm_o_pgo = 'X';
                                        show_hint(1,
                                                  "PAGE only is mandatory for graymaps");
                                } else {
                                        if (*cm_o_pgo == ' ') {
                                                opage_j1 = page_j1 =
                                                    PT + TH + (PH -
                                                               TH) / 3;
                                                opage_j2 = page_j2 =
                                                    PT + TH + 2 * (PH -
                                                                   TH) / 3;
                                        }
                                        setview(PAGE);
                                        check_dlimits(0);
                                }
                        }
                        new_mclip = 0;
                }

                /* others */
                else if ((it == CM_O_CURR) ||
                         (it == CM_O_ALL) ||
                         (it == CM_O_DKEYS) || (it == CM_O_AMENU)) {

                }
        }

        /*
           Items on "DEBUG" menu
         */
        else if (cmenu - CM == CM_G) {

                if ((it == CM_G_ALIGN) ||
                    (it == CM_G_GLINES) ||
                    (it == CM_G_LB) ||
                    (it == CM_G_ABAGAR) ||
                    (it == CM_G_BO) ||
                    (it == CM_G_IO) || (it == CM_G_SUM)) {

                        new_mclip = 0;
                        redraw_document_window();
                }

                /* clear matches */
                else if (it == CM_G_CM) {

                        clear_cm();
                        dw[PATTERN_LIST].rg = 1;
                        new_mclip = 0;
                }

                /* attach DEBUG window */
                else if ((it == CM_G_VOCAB) || (it == CM_G_LOG)) {

                        dw[DEBUG].rg = 1;
                        new_mclip = 0;
                        redraw_document_window();
                }

                /* enter debug window */
                else if (it == CM_G_DW) {

                        setview(DEBUG);
                        new_mclip = 0;
                }
        }

        /*
           Items on "PAGE options" menu
         */
        else if (cmenu - CM == CM_D) {
                int i, j, x, y, t;

                {
                        int last_dw;

                        last_dw = CDW;
                        CDW = PAGE;
                        i = (CX0_orig - DM) / GS;
                        j = (CY0_orig - DT) / GS;
                        x = X0 + i * RF;
                        y = Y0 + j * RF;
                        t = symbol_at(x, y);
                        CDW = last_dw;
                }

                /* use as pattern */
                if ((t >= 0) && (it == CM_D_TS)) {

                        new_mclip = 0;
                        this_pattern = mc[t].bm;
                }

                /* OCR this symbol */
                else if ((t >= 0) && (it == CM_D_OS)) {

                        if (ocring == 0) {
                                new_mclip = 0;
                                justone = 1;
                                curr_mc = t;
                                start_ocr(-2, OCR_CLASS, 1);
                        }
                }

                /* make (CX0_orig,CY0_orig) the bottom left */
                else if (it == CM_D_HERE) {

                        i = (CX0_orig - DM) / GS;
                        j = (CY0_orig - DT) / GS;

                        if ((i > 0) && (j > 0)) {
                                X0 += i * RF;
                                Y0 -= (VR - j - 1) * RF;
                                check_dlimits(1);
                        }
                        new_mclip = 0;
                }

                /* focus symbol t on PAGE_FATBITS */
                else if ((t >= 0) && (it == CM_D_FOCUS)) {

                        setview(PAGE_FATBITS);

                        i = (mc[t].l + mc[t].r - HR) / 2;
                        j = (mc[t].t + mc[t].b - VR) / 2;

                        if ((i > 0) && (j > 0)) {
                                X0 = i * RF;
                                Y0 = j * RF;
                                RG = 1;
                                check_dlimits(1);
                        }
                        new_mclip = 0;
                }

                /* merge fragment with the current symbol */
                else if (it == CM_D_ADD) {

                        if ((curr_mc >= 0) && (t >= 0)) {
                                start_ocr(-2, OCR_REV, 0);
                                to_arg = t;
                                to_rev = REV_MERGE;
                        }

                        new_mclip = 0;
                }

                /* create symbol link */
                else if (it == CM_D_SLINK) {

                        if ((t >= 0) && (mc[t].sw < 0)) {
                                start_ocr(-2, OCR_REV, 0);
                                to_arg = t;
                                to_rev = REV_SLINK;
                        }

                        new_mclip = 0;
                }

                /* disassemble symbol */
                else if (it == CM_D_DIS) {

                        if ((curr_mc >= 0) && (t >= 0)) {
                                start_ocr(-2, OCR_REV, 0);
                                to_rev = REV_DIS;
                        }

                        new_mclip = 0;
                }

                /* create accent link */
                else if (it == CM_D_ALINK) {

                        if ((t >= 0) && (mc[t].sw < 0)) {
                                start_ocr(-2, OCR_REV, 0);
                                to_arg = t;
                                to_rev = REV_ALINK;
                        }

                        new_mclip = 0;
                }

                /* diagnose symbol pairing */
                else if (it == CM_D_SDIAG) {

                        if ((curr_mc >= 0) && (t >= 0))
                                diag_pairing(t);

                        new_mclip = 0;
                }

                /* diagnose word pairing */
                else if (it == CM_D_WDIAG) {

                        if ((curr_mc >= 0) && (t >= 0))
                                diag_wpairing(t);

                        new_mclip = 0;
                }

                /* diagnose lines */
                else if (it == CM_D_LDIAG) {
                        int a, b, r;

                        if ((curr_mc >= 0) &&
                            (mc[curr_mc].sw >= 0) &&
                            ((a = word[mc[curr_mc].sw].tl) >= 0) &&
                            (t >= 0) &&
                            (mc[t].sw >= 0) &&
                            ((b = word[mc[t].sw].tl) >= 0)) {

                                if (a == b)
                                        show_hint(2, "same lines!");

                                else {
                                        r = cmpln(a, b);
                                        if (cmpln_diag == 0)
                                                show_hint(2,
                                                          "could not decide");
                                        else if (cmpln_diag == 1)
                                                show_hint(2,
                                                          "the lines belong to different zones");
                                        else if (cmpln_diag == 2)
                                                show_hint(2,
                                                          "decided by baseline analysis");
                                        else if (cmpln_diag == 3)
                                                show_hint(2,
                                                          "vertical disjunction");
                                        else if (cmpln_diag == 4)
                                                show_hint(2,
                                                          "extremities medium vertical position");
                                        else if (cmpln_diag == 5)
                                                show_hint(2,
                                                          "extremities medium horizontal position");
                                        else if (cmpln_diag == 6)
                                                show_hint(2,
                                                          "vertical disjunction");
                                }
                        }

                        new_mclip = 0;
                }

                /* diagnose merging */
                else if (it == CM_D_GM) {

                        if ((curr_mc >= 0) && (t >= 0))
                                diag_geomerge(t);

                        new_mclip = 0;
                }

                /* these statuses must redraw the window */
                else if ((it == CM_D_PIXELS) ||
                         (it == CM_D_CLOSURES) ||
                         (it == CM_D_SYMBOLS) ||
                         (it == CM_D_WORDS) || (it == CM_D_PTYPE)) {

                        new_mclip = 0;
                        redraw_document_window();
                }

                /* report scale */
                else if (it == CM_D_RS) {

                        new_mclip = 0;
                        // UNPATCHED: redraw_tab = 1;
                }

                /* display box instead of symbol */
                else if (it == CM_D_BB) {

                        new_mclip = 0;
                        // UNPATCHED: redraw_tab = 1;
                        if (dw[PAGE].v)
                                redraw_document_window();
                }

        }

        /*
           Items on "PAGE_FATBITS options" menu
         */
        else if (cmenu - CM == CM_B) {
                int i, j, x, y, t;

                {
                        int last_dw;

                        last_dw = CDW;
                        CDW = PAGE_FATBITS;
                        i = (CX0_orig - DM) / GS;
                        j = (CY0_orig - DT) / GS;
                        x = X0 + i * RF;
                        y = Y0 + j * RF;
                        t = closure_at(x, y, 0);
                        CDW = last_dw;
                }

                /* make (CX0_orig,CY0_orig) the bottom left */
                if (it == CM_B_HERE) {

                        if ((i > 0) && (j > 0)) {
                                X0 += i * RF;
                                Y0 -= (VR - j - 1) * RF;
                                dw[PAGE_FATBITS].rg = 1;
                                check_dlimits(1);
                        }
                        new_mclip = 0;
                }

                /* centralize the specified closure */
                else if (it == CM_B_CENTRE) {

                        if (t >= 0) {
                                i = (cl[t].l + cl[t].r) / 2;
                                j = (cl[t].b + cl[t].t) / 2;
                                X0 = i - HR / 2;
                                Y0 = j - VR / 2;
                                dw[PAGE_FATBITS].rg = 1;
                                check_dlimits(1);
                        }
                        new_mclip = 0;
                }

                /*
                   when changing these presentation flags, some
                   windows must be regenerated.
                 */
                else if ((it == CM_B_SKEL) ||
                         (it == CM_B_BORDER) ||
                         (it == CM_B_HS) || (it == CM_B_HB)) {

                        /* some modes force FSxFS */
                        setview(CDW);

                        /* must copy pattern to cfont again */
                        p2cf();
                        dw[PAGE_FATBITS].rg = 1;
                        new_mclip = 0;
                }

                /* compute border path */
                else if (it == CM_B_BP) {

                        if (t >= 0) {
                                closure_border_path(t);
                        }

                        new_mclip = 0;
                }

                /* compute straight lines on border using pixel distance */
                else if (it == CM_B_SLD) {
                        short r[256];

                        if (t >= 0) {
                                closure_border_slines(t, 8, 2, 0.36, r,
                                                      128, 0);
                        }

                        new_mclip = 0;
                }

                /* compute straight lines on border using correlation */
                else if (it == CM_B_SLC) {
                        short r[256];

                        if (t >= 0) {
                                closure_border_slines(t, 8, 1, 0.9, r, 128,
                                                      0);
                        }

                        new_mclip = 0;
                }

                /* apply isbar test */
                else if (it == CM_B_ISBAR) {

                        if (t >= 0) {
                                int r;
                                float sk, bl;

                                if ((r = isbar(t, &sk, &bl)) > 0) {
                                        show_hint(2,
                                                  "seems to be a bar, skew=%1.3f (rad), len=%3.1f mm",
                                                  sk, bl);
                                } else {
                                        char *d;

                                        if (r == -1)
                                                d = "too small";
                                        else if (r == -2)
                                                d = "too large";
                                        else if (r == -3)
                                                d = "too wide";
                                        else if (r == -4)
                                                d = "too complex";
                                        else if (r == -5)
                                                d = "nonparallel borders";
                                        else if (r == -6)
                                                d = "too thin/non straight";
                                        else
                                                d = "unknown";
                                        show_hint(2,
                                                  "does not seem to be a bar (%s)",
                                                  d);
                                }
                        }

                        new_mclip = 0;
                }

                /* detect extremities */
                else if (it == CM_B_DX) {

                        if (t >= 0) {
                                int n;

                                n = dx(t, -1, -1);
                                show_hint(2, "%d extremities found", n);
                        }

                        new_mclip = 0;
                }
        }

        /*
           Items on "Pattern" menu
         */
        else if (cmenu - CM == CM_R) {
                int pages;

                pages = (*cm_o_all != ' ') ? -1 : -2;

                /* cannot start new operation */
                if (ocring) {
                }

                /* start preproc */
                else if (it == CM_R_PREPROC) {

                        start_ocr(pages, OCR_PREPROC, 0);
                }

                /* start detection of blocks */
                else if (it == CM_R_BLOCK) {

                        start_ocr(pages, OCR_BLOCK, 0);
                }

                /* start binarization and segmentation */
                else if (it == CM_R_SEG) {

                        start_ocr(pages, OCR_SEG, 0);
                }

                /* start consisting structures */
                else if (it == CM_R_CONS) {

                        start_ocr(pages, OCR_CONS, 0);
                }

                /* start preparing patterns */
                else if (it == CM_R_OPT) {

                        start_ocr(pages, OCR_OPT, 0);
                }

                /* start reading revision data */
                else if (it == CM_R_REV) {

                        start_ocr(pages, OCR_REV, 0);
                }

                /* start classification */
                else if (it == CM_R_CLASS) {

                        start_ocr(pages, OCR_CLASS, 0);
                }

                /* start geometric merging */
                else if (it == CM_R_GEOMERGE) {

                        start_ocr(pages, OCR_GEOMERGE, 0);
                }

                /* start */
                else if (it == CM_R_BUILD) {

                        start_ocr(pages, OCR_BUILD, 0);
                }

                /* start spelling */
                else if (it == CM_R_SPELL) {

                        start_ocr(pages, OCR_SPELL, 0);
                }

                /* start building words and lines */
                else if (it == CM_R_OUTP) {

                        start_ocr(pages, OCR_OUTP, 0);
                }

                /* start generating doubts */
                else if (it == CM_R_DOUBTS) {

                        start_ocr(pages, OCR_DOUBTS, 0);
                }

                new_mclip = 0;
        }

        /*
           Items on "Alphabets" menu
         */
        else if (cmenu - CM == CM_A) {

                set_bl_alpha();
                // UNPATCHED: redraw_map = 1;
        }

        /* must dismiss the context menu */
        if (cmenu->t[a] & CM_DI) {
                cmenu = NULL;
                CX0_orig = CY0_orig = -1;

                /* workaround for some PAGE redraw problems */
                if (dw[PAGE].v)
                        new_mclip = 0;

                set_mclip(new_mclip);
                force_redraw();
        } else {
                // UNPATCHED: redraw_menu = 1;
        }

        return (1);
}
#endif
/*

Process keyboard events.

*/
#if 0
void kactions(void) {
        /* statuses */
        int ctrl, haveif;
        static int cx = 0;

        /* other */
        int ls;
        edesc *c;
        int n;

        /* Buffers required by the KeyPress event */
        unsigned char buffer[101];
        //KeySym ks;
        //XComposeStatus cps;

        /*
           We're waiting any keyboard event.
         */
        /* any keyboard event unblocks the OCR */
        if (waiting_key == 1) {
                waiting_key = 0;
                show_hint(3, NULL);
                if (nopropag == 0)
                        return;
        }

        n = XLookupString(&(xev.xkey), (char *) buffer, 100, &ks, &cps);

        /*
           We're waiting ENTER or ESC keypresses. Discard anything else.
         */
        if (waiting_key == 2) {
                if ((ks == 0xff0d) || (ks == 0xff1b)) {
                        waiting_key = 0;
                        mb[0] = 0;
                        // UNPATCHED: redraw_stline = 1;
                        key_pressed = (ks == 0xff0d);
                }
                return;
        }

        /*
           We're waiting y, Y, n or N keypresses. Discard anything else.
         */
        /* Discard events other than y, Y, n or N */
        if (waiting_key == 3) {
                if ((ks == 'y') || (ks == 'Y') || (ks == 'n') ||
                    (ks == 'N')) {
                        waiting_key = 0;
                        mb[0] = 0;
                        // UNPATCHED: redraw_stline = 1;
                        key_pressed = ((ks == 'y') || (ks == 'Y'));
                }
                return;
        }

        /*
           We're waiting <, >, +, -, ENTER or ESC keypresses. Discard anything else.
         */
        if (waiting_key == 5) {
                if ((ks == '>') || (ks == '<') || (ks == '+') ||
                    (ks == '-')) {
                        waiting_key = 0;
                        mb[0] = 0;
                        // UNPATCHED: redraw_stline = 1;
                        key_pressed = ks;
                } else if ((ks == 0xff0d) || (ks == 0xff1b)) {
                        waiting_key = 0;
                        mb[0] = 0;
                        // UNPATCHED: redraw_stline = 1;
                        key_pressed = 0;
                }
                return;
        }

        /* the C-g prefix */
        ctrl = (xev.xkey.state & ControlMask);
        if (ctrl && (ks == 103)) {

                cx = 0;
                return;
        }

        /* the C-x prefix */
        ctrl = (xev.xkey.state & ControlMask);
        if (ctrl && (ks == 120)) {

                cx = 1;
                return;
        }

        /* C-x special actions */
        if (cx) {

                switch (ks) {

#ifdef TEST
                        /* C-x b: barcode detection */
                case 98:
                        search_barcode();
                        break;
#endif

                        /* C-x d: dump current symbol */
                case 100:
                        if ((curr_mc < 0) || (tops < curr_mc)) {
                                warn("invalid symbol");
                        } else if (pixel_mlist(curr_mc) < 0) {
                                warn("symbol too large");
                        } else {
                                dump_cb();
                        }
                        break;

                        /* C-x x: pp_test */
                case 120:
                        test();
                        break;

                        /*

                         ** EXPERIMENTAL STUFF **
                         ------------------------

                         Don't try unless you really know what you're doing!

                         */

#ifdef TEST
                        /* C-x c: clusterization test */
                case 99:
                        test_clusterize();
                        break;

                        /* C-x i: run all internal tests */
                case 105:
                        internal_tests();
                        break;

                        /* C-x p: enter pattern (action) */
                case 112:
                        setview(PATTERN_ACTION);
                        break;

                        /* C-x r: reset */
                case 114:
                        reset();
                        break;

                        /* C-x s: Candido de Figueiredo post-segmentation blockfind */
                case 115:
                        cf_block();
                        break;

                        /* C-x t: image info */
                case 116:
                        if (!ocring) {
                                ocr_other = 4;
                                start_ocr(-2, OCR_OTHER, 0);
                        }
                        break;

                        /* C-x v: test dump_dict */
                case 118:
                        /*
                           dump_dict(1,NULL);
                           while (dump_dict(0,NULL));
                           printf("%s\n",text);
                         */
                        dict_load();
                        break;
#endif

                }
                cx = 0;
                return;
        }

        /* Is there an active text input field? */
        c = ge + curr_hti;
        if ((0 <= curr_hti) && (curr_hti <= topge) &&
            (c->type == GE_INPUT)) {
                haveif = 1;

                /* ignore ESC prefix */
                if (nopropag) {
                        nopropag = 0;
                        mb[0] = 0;
                        // UNPATCHED: redraw_stline = 1;
                }
        } else if (waiting_key == 4)
                haveif = 2;
        else
                haveif = 0;

        /* Control-L */
        if ((n == 1) && (buffer[0] == 12))
                force_redraw();

        /* Backspace within text input field */
        else if ((n == 1) && (haveif) && (ks == 0xffff)) {

                if (haveif == 1) {
                        if ((ls = strlen(c->txt)) > 0)
                                (c->txt)[ls - 1] = 0;

                        // UNPATCHED: redraw_inp = 1;
                        form_changed = 1;
                } else {
                        if ((ls = strlen(inp_str)) > inp_str_ps)
                                (inp_str)[ls - 1] = 0;

                        redraw_inp_str();
                }
        }

        /* Visible char */
        else if ((n == 1) && (' ' <= buffer[0])) {
                static int l = -1;
                int v;

                /* apply composition */
                if (*cm_o_dkeys == 'X')
                        v = compose(&l, buffer[0]);
                else {
                        v = 1;
                        l = buffer[0];
                }

                /* add to HTML input field */
                if (haveif == 1) {
                        ls = strlen(c->txt) + 1;
                        if (v) {
                                if ((ls >= c->tsz) && (c->tsz < MFTL + 1))
                                        c->txt = c_realloc(c->txt, c->tsz =
                                                           MFTL + 1, NULL);
                                if (ls < c->tsz) {
                                        (c->txt)[ls - 1] = l;
                                        (c->txt)[ls] = 0;
                                        l = -1;
                                }
                        }
                        // UNPATCHED: redraw_inp = 1;
                        form_changed = 1;
                }

                /* add to HTML input field */
                else if (haveif == 2) {
                        ls = strlen(inp_str) + 1;
                        if (v) {
                                if (ls < MMB) {
                                        (inp_str)[ls - 1] = l;
                                        (inp_str)[ls] = 0;
                                        l = -1;
                                }
                        }
                        redraw_inp_str();
                }

                /* submit symbol transliteration from PAGE window */
                else if (v && dw[PAGE].v && (curr_mc >= 0)) {

                        /*
                           Remark: we initialize the transliteration buffer to_tr
                           after the start_ocr call becase start_ocr clear the
                           buffer to_tr.
                         */
                        start_ocr(-2, OCR_REV, 0);
                        to_tr[0] = l;
                        to_tr[1] = 0;
                        to_rev = REV_TR;
                        l = -1;
                }

                /* add pattern from the spyhole buffer */
                else if (v && dw[PAGE].v && (spyhole(0, 0, 2, 0) == 1)) {

                        cdfc = -1;
                        start_ocr(-2, OCR_REV, 0);
                        to_tr[0] = l;
                        to_tr[1] = 0;
                        to_rev = REV_PATT;
                        l = -1;
                }

                /* submit pattern transliteration from PATTERN window */
                else if (v && dw[PATTERN].v) {
                        pdesc *s;

                        if ((0 <= cdfc) && (cdfc <= topp)) {
                                s = pattern + cdfc;

                                /*
                                   Remark: we initialize the transliteration buffer to_tr
                                   after the start_ocr call becase start_ocr clear the
                                   buffer to_tr.
                                 */
                                start_ocr(-2, OCR_REV, 0);
                                to_tr[0] = l;
                                to_tr[1] = 0;
                                to_rev = REV_PATT;
                                l = -1;
                        }
                }

                /*
                   switch page (for tests)
                   -----------------------

                   Manually reads the first, second or third page. To use
                   this feature, press "1", "2" or "3" on the PAGE (list)
                   window (of course there must exist at least pages on
                   the current directory). This is used only for debugging
                   purposes.
                 */
                /*
                   else if (CDW == PAGE_LIST) {
                   if (l == '1')
                   start_ocr(0,OCR_LOAD,0);
                   else if (l == '2')
                   start_ocr(1,OCR_LOAD,0);
                   else if (l == '3')
                   start_ocr(2,OCR_LOAD,0);
                   l = -1;
                   }
                 */

                /* show the GPL */
                else if (v && ((l == 'l') || (l == 'L')) &&
                         (CDW == WELCOME)) {
                        setview(GPL);
                        redraw_document_window();
                        l = -1;
                }
        }

        /* Special keys */
        else
                switch (ks) {

                        /* Left */
                case 0xff51:
                        left();
                        break;

                        /* Up */
                case 0xff52:
                        up();
                        break;

                        /* Right */
                case 0xff53:
                        right();
                        break;

                        /* Down */
                case 0xff54:
                        down();
                        break;

                        /* Prior (pgup) */
                case 0xff55:
                        prior();
                        break;

                        /* Next (pgdw) */
                case 0xff56:
                        next();
                        break;

                        /* END */
                case 0xff57:
                        if ((XRES - HR) >= 0) {
                                X0 = XRES - HR;
                        }
                        break;

                        /* HOME */
                case 0xff50:
                        X0 = 0;
                        break;

                        /* ENTER */
                case 0xff0d:

                        /* select current menu item */
                        if (cmenu != NULL) {
                                cma(cmenu->c + ((cmenu - CM) << CM_IT_WD),
                                    0);
                        }

                        /*
                           ENTER pressed with active input field
                           provoke form submission.
                         */
                        else if (((CDW == PAGE_SYMBOL) ||
                                  (CDW == TUNE) ||
                                  (CDW == TUNE_SKEL) ||
                                  (CDW == PATTERN) ||
                                  (CDW == PATTERN_TYPES)) &&
                                 (haveif == 1)) {

                                form_auto_submit();
                        }

                        /* finished edition of input string */
                        else if (haveif == 2) {
                                waiting_key = 0;
                                redraw_inp_str();
                                memmove(inp_str, inp_str + inp_str_ps,
                                        strlen(inp_str) - inp_str_ps + 1);
                        }

                        break;

                        /* ESC */
                case 0xff1b:

                        /* dismiss current menu */
                        if (cmenu != NULL)
                                dismiss(1);

                        /* toggle BAD property */
                        else {

                                button[bbad] ^= 1;
                                // UNPATCHED: redraw_button = bbad;
                                form_changed = 1;
                                form_auto_submit();

                                /*
                                   nopropag = 1-nopropag;
                                   if (nopropag)
                                   enter_wait("ESC",0,1);
                                 */
                        }

                        /* skip current doubt */
                        /*
                           else if (CDW == PAGE_SYMBOL) {
                           ++curr_doubt;
                           edit_curr_doubt();
                           }
                         */

                        break;

                        /* C-x prefix */
                        /* case 0xff1b: */

                default:
                        db("unattended keycode %ld", ks);
                        break;
                }
}
#endif

/*

This is mostly a trigger to execute actions when a menu is
opened. Anyway, it also initializes some variables.

*/
#if 0
void open_menu(int x, int y, int m) {
        CX0 = x;
        CY0 = y;
        if (cmenu == NULL)
                form_auto_submit();
        cmenu = CM + m;
        // UNPATCHED: redraw_menu = 1;
        show_hint(3, NULL);

}
#endif

/*

Spyhole operations. The operation performed depends on the value
of the op parameter:

If (op == 0), try to apply the spyhole at (x,y). "Apply the
spyhole at (x,y)" means to backup a rectangle around (x,y),
search a "good" threshold to use in this rectangle. Returns 1 if
found, 0 otherwise.

If (op == 1), just give back the spyhole buffer (if any) to the
graymap and returns. Returns 0.

If (op == 2), copy the binarized symbol (if any) to cb. Returns 1
if success, 0 otherwise.

If (op == 3), copy the binarized symbol (if any) to
cmp_bmp. Returns 1 if success, 0 otherwise.

If (op == 4) make as if (op == 0), but do not backup the spyhole
nor apply the binarization result to the pixmap, nor compute the
next threshold (faster, non-visual).

The parameter et is the expect threshold. It's meaningful only
when (op==0) or (op==4).

Thresholds not multiple of TS are not tried.

WARNING: the following parameters are currently hardcoded:

    TS
    sh_hs
    nhs
    relax

*/
int spyhole(int x, int y, int op, int et) {
        /*
           Bitmap buffers: bb is the main buffer. For each threshold,
           the binarization result is computed using bb, as well as
           the result of the spanning of the component from the seed
           (x,y). bc stores the bitmap got from the preferred
           threshold until now. bd and be are used to store, in some
           cases, the bitmap before the spanning of the component,
           in order to detect merging.
         */
        unsigned char *bb, *bc, *bd, *be;
        int bpl, bb_sz, snp;

        /*
           When spyhole() finds a usable threshold, it stores
           the threshold result in this buffer, so it can
           be recovered afterwards using op==2.
         */
        static unsigned bs[BMS / 4];
        static int bsf = 0, bs_x0, bs_y0;

        /* the spyhole static buffer */
        static unsigned char *eg = NULL;
        static int sh_sz, last_x0 = -1, last_y0, last_dx, last_dy = 0;

        /* the spyhole rectangle */
        int x0, y0, dx, dy, sh_hs;

        /* current, best and maximum tried thresholds */
        int thresh, bt, mt;

        /* first usable threshold */
        int ft;

        /* best class found */
        int bcp;

        /* the seed */
        int r, u, v, u0, v0, nhs;

        /* other */
        int j, k, l, ret;
        unsigned char *p, *q;

        /* white and black threshold limits */
        int WL, BL;

        /* threshold search control */
        int L, R, st, tries;

        /* per-threshold width, height, classification result and seed */
        int W[257], H[257], K[257], SX[257], SY[257];

        /*
           Threshold Stepper. It's expected to be 1, 2, 4, 8 or 16. The
           larger it is, the faster the service should run, and less
           sensible it becomes.
         */
        static int TS = 4;

        /* relaxed mode */
        int relax = 1;

        /* spyhole half-size */
        sh_hs = FS;

        /*
           Copy the binarized symbol to cb and returns.
         */
        if (op == 2) {

                /* no available symbol */
                if (bsf == 0)
                        return (0);

                /* copy bs to cb */
                bm2cb((unsigned char *) bs, 0, 0);
                return (1);
        }

        /*
           Copy the binarized symbol to cmp_bmp and returns.
         */
        if (op == 3) {

                /* no available symbol */
                if (bsf == 0)
                        return (0);

                /* copy bs to cmp_bmp */
                memcpy(cmp_bmp, bs, BMS);
                cmp_bmp_x0 = bs_x0;
                cmp_bmp_y0 = bs_y0;
                return (1);
        }

        /*
           give back the last spyhole (if any) to the pixmap and
           invalidate the buffer.
         */
        if (last_x0 >= 0) {

                q = eg;
                for (j = last_y0, l = last_dy; l > 0; --l, ++j) {
                        p = pixmap + j * XRES + last_x0;
                        for (k = last_dx; k > 0; --k, ++p, ++q) {
                                *p = *q;
                        }
                }
                last_x0 = -1;
                bsf = 0;
        }

        /* done if operation 1 */
        if (op == 1) {

                /*
                   display the progress of the extraction of symbols
                   performed by the local binarizer.
                 */
                if (get_flag(FL_SHOW_LOCALBIN_PROGRESS)) {
                        /* UNPATCHED:
                           if (CDW != PAGE)
                           setview(PAGE);
                           redraw_document_window();
                         */
                }

                return (0);
        }

        /* try to extend to a large enough spyhole rectangle */
        {
                if ((x0 = x - sh_hs) < 0)
                        x0 = 0;
                dx = x - x0;
                if (x + sh_hs <= XRES)
                        dx += sh_hs;
                else
                        dx += (XRES - x);

                if ((y0 = y - sh_hs) < 0)
                        y0 = 0;
                dy = y - y0;
                if (y + sh_hs <= YRES)
                        dy += sh_hs;
                else
                        dy += (YRES - y);
        }

        /*
           The seed neighborhood half size. This may be seen as the
           error (in pixels) admitted when the user clicks the mouse
           to create the spyhole.
         */
        nhs = 30;

        /*
           Not large enough: give up. The minimum width nhs is
           mandatory.
         */
        u0 = x - x0;
        v0 = y - y0;
        if ((u0 < nhs) || ((x + nhs) >= (x0 + dx)) || (v0 < nhs) ||
            ((y + nhs) >= (y0 + dy))) {
                return (0);
        }

        /* think 10% as a minimum, 90% as a maximum */
        if (et > (230 - TS))
                et = 230 - TS;
        if (et < (26 + TS))
                et = 26 + TS;

        /* round the expected threshold */
        if (((et % TS) * 2) > TS)
                et = (et - (et % TS)) + TS;
        else
                et = (et - (et % TS));

        /* alloc bitmap buffers */
        bpl = (dx / 8) + ((dx % 8) != 0);
        bb_sz = dy * bpl;
        bb = alloca(bb_sz);
        bc = alloca(bb_sz);
        bd = alloca(bb_sz);
        be = alloca(bb_sz);

        /*
           Initial thresholds.
         */
        bt = -1;
        bcp = -1;
        ft = -1;

        /* prepare threshold analysis */
        for (j = 0; j < 256; ++j)
                W[j] = SX[j] = -1;
        L = R = WL = BL = -1;
        st = 1;
        snp = -1;

        /*
           This loop searches a "good" thresholding value. To
           analyse if one threshold is "good", the classifier
           is invoked.

           Various thresholding values around the expected
           threshold (the parameter et) are tried, until the
           classifier suceeds to classify the bitmap obtained
           by thresholding, or until the limits on both sides are
           reached. "Limits", in this context, are the "merge limits"
           and the "white" and "black" limits, to be explained below.

           As we enlarge the threshold, at some point the component
           spanned from (x,y) merges some neighbour. The last
           threshold value for which the merge does not occur is
           called "right merge limit".

           Similarly, as we reduce the threshold, at some point
           the component breaks into two or more pieces. The last
           threshold value for which the component didn't
           break is called "left merge limit".

           These limits are detected using the concept of spare
           pixels. Let t be a reference threshold. Let bd be the
           bitmap obtained by thresholding by t. Let c be another
           threshold. Let bb be the bitmap obtained by thresholding
           by c and subsequently spanning the component from
           (x,y). The spare pixels are the pixels black in bd but
           white in bb. As we enlarge c, if a merge occurs, then
           the number of spare pixels reduce. As we reduce c,
           if a break occurs, the number of spare pixels enlarge.

           Note that this behaviour probably depends on the
           method used to print the page being OCRized. The
           Clara OCR development is using exclusively ink-printed
           materials (old books). Laser-printed materials may
           behave differently.

           Now let's explain the other two limits.

           If the threshold gets too low, then we cannot span a
           component from (x,y). The largest threshold for which
           no component is found is called "white limit". If the
           threshold gets too high, then it results a completely
           black rectangle (or something close to it). The lowest
           threshold that produces a black rectangle is called
           "black limit".

           The thresholds larger than the right merge limit
           are also studied, in order to check if the expected
           threshold is not too low. Symbols with thin parts
           brake too easily. In fact, such symbols may require
           thresholds larger than the expected one, in order to
           have all pieces merged.
         */
        for (bsf = 0, st = 1, mt = -1, thresh = et, tries = 0; st <= 4;) {

                if (thresh > mt)
                        mt = thresh;

                /* apply the binarizer line by line */
                memset(bb, 0, bb_sz);
                for (j = y0, l = dy; l > 0; --l, ++j) {
                        int m;

                        q = bb + (j - y0) * bpl;
                        m = 128;
                        p = pixmap + j * XRES + x0;

                        for (k = dx; k > 0; --k, ++p) {
                                if (*p < thresh)
                                        *q |= m;
                                if ((m >>= 1) == 0) {
                                        ++q;
                                        m = 128;
                                }
                        }
                }

                /* do we already have an interior pixel? */
                if (SX[thresh] >= 0) {
                        u = SX[thresh];
                        v = SY[thresh];
                        r = 1;
                } else {
                        u = u0;
                        v = v0;
                        r = 0;
                }

                /* try to search an interior pixel close to (x,y) */
                for (k = l = 0; (l < nhs) && (r == 0);) {

                        /*
                           In relaxed mode we're happy if (u,v) is black. The first
                           implementation of spyhole() used the same definition of
                           interior pixel than border_path(). However, to implement
                           the detection of merging, a second, relaxed definition was
                           added to border_path(). Also, the integration of spyhole()
                           with pbm2bm() required to relax here the definition of
                           interior pixel. The old definition is still available,
                           though.
                         */
                        if (relax) {
                                r = pix(bb, bpl, u, v);
                        }

                        /*
                           In non-relaxed mode we apply the same definition
                           of interior pixel used by border_path().
                         */
                        else {
                                /* non-optimized case (25%) */
                                if (((u - 1) % 8) >= 6) {
                                        r = pix(bb, bpl, u + 1, v) &&
                                            pix(bb, bpl, u - 1, v) &&
                                            pix(bb, bpl, u - 1, v - 1) &&
                                            pix(bb, bpl, u, v - 1) &&
                                            pix(bb, bpl, u + 1, v - 1) &&
                                            pix(bb, bpl, u - 1, v + 1) &&
                                            pix(bb, bpl, u, v + 1) &&
                                            pix(bb, bpl, u + 1, v + 1);
                                }

                                /* 3-bit optimization (75%) */
                                else {
                                        r = pix(bb, bpl, u + 1, v) &&
                                            pix(bb, bpl, u - 1, v) &&
                                            pix3(bb, bpl, u - 1, v - 1) &&
                                            pix3(bb, bpl, u - 1, v + 1);
                                }
                        }

                        /* no, (u,v) isn't interior */
                        if (r == 0) {

                                /* the path finished */
                                if (--k < 0) {

                                        /* radius and path size */
                                        ++l;
                                        k = 8 * l - 1;
                                }

                                /*
                                   Compute next path position. To describe the method,
                                   let's draw the path for radius 1:

                                   7 6 5
                                   0   4
                                   1 2 3

                                   The corners are n*l-1 for n=2,4,6,8. We divide the
                                   path into four lines:

                                   Top line: 7, 6.
                                   Right line: 5, 4.
                                   Bottom line: 3, 2.
                                   Left line: 1, 0.

                                   The code below implements the general case.
                                 */
                                if (l < nhs) {

                                        /* top line: linear positions [(8*l-1)..(6*l-1)[ */
                                        if (k > 6 * l - 1) {
                                                u = (u0 - l) +
                                                    ((8 * l - 1) - k);
                                                v = v0 - l;
                                        }

                                        /* right line: linear positions [(6*l-1)..(4*l-1)[ */
                                        else if (k > 4 * l - 1) {
                                                u = u0 + l;
                                                v = (v0 - l) +
                                                    ((6 * l - 1) - k);
                                        }

                                        /* bottom line: linear positions [(4*l-1)..(2*l-1)[ */
                                        else if (k > 2 * l - 1) {
                                                u = (u0 + l) -
                                                    ((4 * l - 1) - k);
                                                v = v0 + l;
                                        }

                                        /* left line: linear positions [(2*l-1)..(0*l-1)[ */
                                        else {
                                                u = u0 - l;
                                                v = (v0 + l) -
                                                    ((2 * l - 1) - k);
                                        }
                                }
                        }
                }

                /* found one interior pixel */
                if (l < nhs) {

                        /*
                           apply the just found interior pixel to all
                           thresholds above the current one
                         */
                        for (j = thresh; (j <= 256) && (SX[j] < 0); ++j) {
                                SX[j] = u;
                                SY[j] = v;
                        }

                        /*
                           Copy the threshold result. We'll use it to
                           detect merging.
                         */
                        if (bt < 0) {

                                /*
                                   copy to the true buffer that stores the
                                   threshold result
                                 */
                                if (ft < 0)
                                        memcpy(bd, bb, bb_sz);

                                /*
                                   copy to an auxiliar buffer, because if the
                                   classification succeeds, we'll swap bd and be.
                                 */
                                else
                                        memcpy(be, bb, bb_sz);
                        }
                }

                /*
                   No interior pixel found: the current
                   threshold produced only a big white rectangle
                   (or something close to it).
                 */
                if (l >= nhs) {
                        W[thresh] = 0;
                        H[thresh] = 0;
                }

                /*
                   If we got one interior pixel, try to span its
                   component and classify it.
                 */
                else if ((r =
                          border_path(bb, dx, dy, NULL, 0, u, v,
                                      1)) == 0) {

                        /*
                           This threshold was already analysed.
                         */
                        if (W[thresh] >= 0) {
                        }

                        /*
                           Copy to cmp_bmp. The service justify_bitmap() returns
                           true only if the component is not too large.
                         */
                        else if (justify_bitmap(bb, dx, dy)) {

                                bsf = 1;

                                /* remember the symbol dimensions */
                                W[thresh] = cmp_bmp_w;
                                H[thresh] = cmp_bmp_h;

                                /*
                                   No successfull classification was achieved until now.
                                 */
                                if (bt < 0) {

                                        /* try to classify it */
                                        classify(-1, NULL, 0);
                                        while (classify
                                               (0, bmpcmp_skel, 3));

                                        /* success! */
                                        if (cp_result >= 0) {

                                                /* assign class to threshold */
                                                K[thresh] = bcp =
                                                    cp_result;

                                                /* take threshold as the best one */
                                                bt = thresh;

                                                /* remember the bitmap in full and reduced sizes */
                                                if (op == 0)
                                                        memcpy(bc, bb,
                                                               bb_sz);
                                                memcpy(bs, cmp_bmp, BMS);

                                                /* absolute top left coordinates */
                                                bs_x0 = x0 + cmp_bmp_dx;
                                                bs_y0 = y0 + cmp_bmp_dy;

                                                /*
                                                   swap bd and be because the pre-spanning bitmap
                                                   is being pointed by be, not bd.
                                                 */
                                                if (ft >= 0) {
                                                        unsigned char *a;

                                                        a = bd;
                                                        bd = be;
                                                        be = a;
                                                }

                                                /* compute the number of spare pixels */
                                                for (p = bd, q = bb, j =
                                                     snp = 0; j < bb_sz;
                                                     ++j, ++p, ++q)
                                                        snp +=
                                                            np8[*p ^
                                                                ((*p) &
                                                                 (*q))];
                                        }

                                        /*
                                           if no usable result until now, then adopt the current
                                           threshold value temporarily (the first usable result
                                           is expected to be close to et).
                                         */
                                        else if (ft < 0) {

                                                /* take threshold as the first one */
                                                ft = thresh;

                                                /* remember the bitmap in full and reduced sizes */
                                                if (op == 0)
                                                        memcpy(bc, bb,
                                                               bb_sz);
                                                memcpy(bs, cmp_bmp, BMS);

                                                /* absolute top left coordinates */
                                                bs_x0 = x0 + cmp_bmp_dx;
                                                bs_y0 = y0 + cmp_bmp_dy;

                                                /* compute the number of spare pixels */
                                                for (p = bd, q = bb, j =
                                                     snp = 0; j < bb_sz;
                                                     ++j, ++p, ++q)
                                                        snp +=
                                                            np8[*p ^
                                                                ((*p) &
                                                                 (*q))];
                                        }
                                }
                        }

                        /* the component is too large */
                        else {

                                /*
                                   Remember the symbol dimensions (in this case,
                                   cmp_bmp_w or cmp_bmp_h may not be the true
                                   width or height, however justify_bitmap() assures
                                   that at least one of them is "too big").
                                 */
                                W[thresh] = cmp_bmp_w;
                                H[thresh] = cmp_bmp_h;
                        }
                }

                /*
                   Should not happen.. border_path() rejected
                   our interior pixel.
                 */
                else {
                        db("oops.. bad code: border_path disagree with spyhole");
                }

                /*
                   abagar: display the progress of the binary search
                 */
                if (abagar) {
                        printf("W[%d]=%d, H[%d]=%d\n", thresh, W[thresh],
                               thresh, H[thresh]);
                }

                /*
                   At this point, the analysis of the current threshold
                   finished. Now we need to discover the next thing to
                   do. Maybe selecting a new threshold to try the classifier,
                   or start searching the right merge limit, etc.
                 */
                {
                        /* step 1: scanning around the expected threshold */
                        if (st == 1) {
                                int d, f, t, n;

                                /* compute the number of spare pixels */
                                for (p = bd, q = bb, j = n = 0; j < bb_sz;
                                     ++j, ++p, ++q)
                                        n += np8[*p ^ ((*p) & (*q))];

                                /* overpassed left merge limit */
                                if ((snp >= 0) && (thresh < et) &&
                                    (n > (snp + 25))) {
                                        L = thresh + TS;
                                }

                                /*
                                   overpassed right merge limit
                                 */
                                else if ((snp >= 0) && (thresh > et) &&
                                         (n < (snp - 25))) {
                                        R = thresh - TS;
                                        memcpy(be, bb, bb_sz);
                                }

                                /* achieved black limit */
                                if ((d = swh(W[thresh], H[thresh])) > 0) {
                                        BL = thresh;
                                        if (thresh > et)
                                                R = thresh - TS;
                                }

                                /* achieved white limit */
                                else if (d < 0) {
                                        WL = thresh;
                                        if (thresh < et)
                                                L = thresh + TS;
                                }

                                /*
                                   If the classification succeeded, compute the
                                   next threshold, or do nothing more if fast mode.
                                 */
                                if (bt >= 0) {

                                        if (op == 4)
                                                st = 5;
                                        else
                                                st = 2;
                                }

                                /* otherwise prepare next try */
                                else
                                        for (f = t = 0; f == 0;) {

                                                /* compute new threshold value */
                                                ++tries;
                                                if (tries & 1)
                                                        thresh =
                                                            et +
                                                            ((tries +
                                                              1) / 2) * TS;
                                                else
                                                        thresh =
                                                            et -
                                                            (tries / 2) *
                                                            TS;

                                                /* the new threshold value is outside the valid ranges */
                                                if ((thresh <= 0) ||
                                                    (thresh >= 256) ||
                                                    ((L >= 0) &&
                                                     (thresh < L)) ||
                                                    ((R >= 0) &&
                                                     (thresh > R)) ||
                                                    ((WL >= 0) &&
                                                     (thresh <= WL)) ||
                                                    ((BL >= 0) &&
                                                     (thresh >= BL))) {

                                                        /* two consecutive failures: give up */
                                                        if (++t >= 2) {

                                                                /* no successfull threshold: abandon completely */
                                                                if ((ft <
                                                                     0) &&
                                                                    (bt <
                                                                     0))
                                                                        st = 5;

                                                                /* try to find upper limit */
                                                                else
                                                                        st = 2;

                                                                f = 1;
                                                        }
                                                }

                                                /* the new threshold value can be tryed */
                                                else {
                                                        f = 1;
                                                }
                                        }
                        }

                        /*
                           step 2: prepare to search upper limit using binary search
                         */
                        if (st == 2) {

                                /* if step 1 already found the right limit, jump to step 4 */
                                if (R >= 0) {
                                        L = R + TS;
                                        st = 4;
                                }

                                /* otherwise prepare binary search */
                                else {
                                        L = (bt >= 0) ? bt : ft;
                                        R = 256;
                                        thresh = (L + R) / 2;
                                        st = 3;
                                }
                        }

                        /*
                           step 3: search upper limit using binary search. The
                           hypothesis is L is too low, R is large enough.
                         */
                        else if (st == 3) {
                                int n;

                                /* compute the number of spare pixels */
                                for (p = bd, q = bb, j = n = 0; j < bb_sz;
                                     ++j, ++p, ++q)
                                        n += np8[*p ^ ((*p) & (*q))];

                                /* shrink interval */
                                if (n < (snp - 25)) {

                                        /* ((W[thresh] > (W[L]+5)) ||
                                           (H[thresh] > (H[L]+5))) { */

                                        R = thresh;
                                        memcpy(be, bb, bb_sz);
                                } else {
                                        L = thresh;
                                }

                                /* achieved black limit */
                                if (swh(W[thresh], H[thresh]) > 0) {
                                        BL = thresh;
                                }

                                /* prepare next try or next step */
                                if ((L + 1) >= R) {
                                        L = R;
                                        st = 4;
                                } else {
                                        thresh = (L + R) / 2;
                                }
                        }

                        /* step 4: should we try the upper threshold segment? */
                        if (st == 4) {

                                /* no */
                                st = 5;
                        }
                }
        }

        /* did we get an usable threshold? */
        ret = ((bt >= 0) || (ft >= 0));

        /*
           abagar: display the binary search result.
         */
        if (abagar) {
                printf("bt = %d ft = %d\n", bt, ft);
        }

        /* spyhole buffer */
        if (op == 0) {

                /* copy the symbol to the spyhole buffer */
                if (dx * dy > sh_sz) {
                        eg = c_realloc(eg, sh_sz = dx * dy, NULL);
                }
                q = eg;
                for (j = y0, l = dy; l > 0; --l, ++j) {
                        p = pixmap + j * XRES + x0;
                        for (k = dx; k > 0; --k, ++p, ++q) {
                                *q = *p;
                        }
                }

                /*
                   If no threshold is found usable, then let a white
                   rectangle be the "best" threshold result.
                 */
                if (ret == 0) {
                        memset(bc, 0, bb_sz);
                        memset(be, 0, bb_sz);
                }

                /* copy the best threshold result to the pixmap */
                for (j = y0, l = dy; l > 0; --l, ++j) {
                        int m;
                        unsigned char *o;

                        q = bc + (j - y0) * bpl;
                        o = be + (j - y0) * bpl;
                        m = 128;
                        p = pixmap + j * XRES + x0;

                        for (k = dx; k > 0; --k, ++p) {

                                /*
                                   Pixels on the best threshold result are painted
                                   black. The others are painted gray when below the
                                   next tryable treshold, or white otherwise.

                                   Remark: if you're thinking about adding a black frame
                                   to the spyhole, it's not an easy task because it
                                   depends on the reduction factor. If you paint black
                                   the pixels on the horizontal or vertical limits of
                                   the spyhole, the frame may be drawn only partially
                                   due to the sampling of the pixels to be copied to
                                   the display.
                                 */
                                if (*q & m)
                                        *p = 0;
                                else if (*o & m)
                                        *p = 192;
                                else
                                        *p = 255;

                                /* prepare mask */
                                if ((m >>= 1) == 0) {
                                        ++q;
                                        ++o;
                                        m = 128;
                                }
                        }
                }

                /* remember the spyhole coords */
                last_x0 = x0;
                last_y0 = y0;
                last_dx = dx;
                last_dy = dy;

                /* tell the user about the result */
                if (bt >= 0) {
                        int i, l;
                        char *t, a[MMB + 1];

                        i = id2idx(bcp);
                        snprintf(a, MMB,
                                 "threshold %d got successfull classification ",
                                 bt);
                        l = strlen(a);
                        if ((t = pattern[i].tr) != NULL)
                                snprintf(a + l, MMB, "\"%s\" (next %d)", t,
                                         L);
                        else
                                snprintf(a + l, MMB, "(next %d)", L);
                        a[MMB] = 0;
                        show_hint(2, a);
                } else if (ft >= 0)
                        show_hint(2, "using threshold %d (next %d)", ft,
                                  L);
                else
                        show_hint(2, "could not find a useful threshold");

                /* must refresh the display */
                redraw_document_window();
        }

        /* accumulate tries */
        sh_tries += tries;

        /* operations 0 and 4 result */
        return (ret);
}

/*

Mouse actions activated by mouse button 1.

*/
#if 0
int mactions_b1(int help, int x, int y) {
        int i, j, c, sb;
        int pb, ob;

        /* coordinates depends on scrollbars hiding */
        if (*cm_v_hide == ' ')
                sb = 10 + RW;
        else
                sb = 0;

        /*

           Context menu
           ------------

         */
        if (cmenu != NULL) {

                if (help) {

                        return (1);
                }

                /* process current item */
                if ((0 <= cmenu->c) && (cmenu->c < cmenu->n)) {

                        cma(cmenu->c + ((cmenu - CM) << CM_IT_WD), 0);
                        return (0);
                }

                /* dismiss context menu */
                else {
                        dismiss(1);
                }
        }

        /* change to selected window */

        /*

           Menu bar
           --------

         */
        if ((i = mb_item(x, y)) >= 0) {

                if (help) {

                        /* select automagically */
                        if ((*cm_o_amenu == 'X') && (cmenu == NULL)) {

                                open_menu(x, y, i);
                                return (1);
                        }

                        return (1);
                }

                open_menu(x, y, i);
                return (0);
        }

        /*

           Selection of tabs
           -----------------

         */
        if ((PM + 10 <= x) &&
            (x < PM + 10 + (TABN - 1) * (TW + 20) + TW) &&
            (PT <= y) && (y <= PT + TH) &&
            ((x - PM - 10) % (20 + TW) <= TW)) {

                i = (x - PM - 10) / (20 + TW);

                /*
                   cannot circulate modes during OCR because the
                   data structures are not ready (in some cases
                   would segfault).
                 */
                if (ocring) {

                        /* do nothing */
                }

                /* show document */
                else if (i == PAGE) {

                        if (help) {
                                if (tab == PAGE)
                                        if (cpage >= 0)
                                                show_hint(0,
                                                          "click to circulate modes");
                                        else
                                                show_hint(0,
                                                          "select a file to load");
                                else
                                        show_hint(0,
                                                  "click to enter page modes");
                                return (1);
                        }

                        if ((CDW == PAGE_FATBITS) || (cpage == -1))
                                setview(PAGE_LIST);

                        else if (CDW == PAGE_LIST)
                                setview(PAGE);

                        else if (dw[PAGE].v)
                                setview(PAGE_DOUBTS);

                        else if (CDW == PAGE_DOUBTS)
                                setview(PAGE_FATBITS);

                        else
                                setview(PAGE_CM);
                }

                /* font editor */
                else if (i == PATTERN) {

                        if (help) {
                                if (tab == PATTERN)
                                        show_hint(0,
                                                  "click to circulate modes");
                                else
                                        show_hint(0,
                                                  "click to browse and edit the patterns");
                                return (1);
                        }

                        if (tab != PATTERN) {

                                /*
                                   BUG: this call will renderize again the PROPS
                                   form. So if the user edit the input buffer,
                                   change the tab, and after go back to PATTERN,
                                   the data on the input field will be lost.
                                 */
                                /*
                                   edit_pattern();
                                 */

                                setview(PATTERN_CM);
                        } else if (dw[PATTERN].v)
                                setview(PATTERN_LIST);

                        else if (dw[PATTERN_LIST].v)
                                setview(PATTERN_TYPES);

                        else
                                setview(PATTERN);
                }

                /* broser of acts */
                else if (i == TUNE) {

                        if (help) {
                                if (tab != TUNE)
                                        show_hint(0,
                                                  "click to tune the OCR");
                                return (1);
                        }

                        if (tab != TUNE) {

                                setview(TUNE_CM);
                        } else if (dw[TUNE].v) {

                                setview(TUNE_SKEL);
                        } else if (dw[TUNE_SKEL].v) {

                                setview(TUNE_ACTS);
                        } else
                                setview(TUNE);
                }
        }

        /*

           Alphabet map
           ------------

         */
        else if ((*cm_v_map != ' ') &&
                 (alpha != NULL) &&
                 (PM - 50 <= x) &&
                 (x < PM - 50 + alpha_cols * 12 + alpha_cols - 1) &&
                 (PT + 1 <= y) &&
                 (y < PT + 1 + alpha_sz * 15 + alpha_sz - 1)) {

                /* map column */
                i = (x - PM + 50) / 13;

                /* map row */
                j = (y - PT - 1) / 16;

                if (help) {
                        char lb[MMB + 1];

                        lb[0] = lb[MMB] = 0;
                        if ((0 <= j) && (j < alpha_sz)) {

                                /* digits */
                                if ((i == 0) && (alpha == number))
                                        snprintf(lb, MMB,
                                                 "this is the digit \"%s\"",
                                                 alpha[j].ln);

                                /* first column is the capital case */
                                else if ((i == 0) && (alpha_cols >= 2) &&
                                         (alpha[j].cc > 0))
                                        snprintf(lb, MMB,
                                                 "this is the letter \"%s\", capital",
                                                 alpha[j].ln);

                                /* second column is the small case */
                                else if ((i == 1) && (alpha_cols >= 2) &&
                                         (alpha[j].sc > 0))
                                        snprintf(lb, MMB,
                                                 "this is the letter \"%s\", small",
                                                 alpha[j].ln);

                                /* first column for alphabets without small case */
                                else if ((i == 0) && (alpha_cols < 2) &&
                                         (alpha[j].cc > 0))
                                        snprintf(lb, MMB,
                                                 "this is the letter \"%s\"",
                                                 alpha[j].ln);

                                /* third column is the transliteration */
                                else if ((i == 2) && (alpha_cols >= 3))
                                        snprintf(lb, MMB,
                                                 "this is the latin keyboard map for letter \"%s\"",
                                                 alpha[j].ln);
                        }
                        show_hint(0, lb);
                        return (1);
                }
        }

        /*

           window
           ------

         */
        else if ((DM <= x) && (x < DM + DW) && (DT <= y) && (y < DT + DH)) {

                i = (x - DM) / GS;
                j = (y - DT) / GS;

#ifdef FUN_CODES
                /*
                   Toggle fun code 1.
                 */
                if (CDW == WELCOME) {

                        if (help) {
                                return (1);
                        }

                        if (fun_code == 0) {
                                fun_code = 1;
                                redraw_document_window();
                                new_alrm(50000, 1);
                        } else if (fun_code == 1) {
                                fun_code = 0;
                                redraw_document_window();
                        }

                        return (0);
                }
#endif

                /*
                   define OCR zone

                   +-----+
                   |1   4|
                   |     |
                   |2   3|
                   +-----+
                 */
                if ((dw[PAGE].v) && (button[bzone] != 0)) {
                        int k, s, p;
                        char lb[MMB + 1];

                        if (help) {
                                show_hint(0, "");
                                return (1);
                        }

                        p = 8 * zones;
                        if ((0 <= curr_corner) && (curr_corner < 2)) {
                                limits[p + 4 * curr_corner] = X0 + i * RF;
                                limits[p + 4 * curr_corner + 1] =
                                    Y0 + j * RF;
                                if (++curr_corner > 1) {

                                        limits[p + 2] = limits[p + 0];
                                        limits[p + 3] = limits[p + 5];
                                        limits[p + 6] = limits[p + 4];
                                        limits[p + 7] = limits[p + 1];

                                        button[bzone] = 0;
                                        ++zones;
                                        sess_changed = 1;
                                        // UNPATCHED: redraw_zone = 1;
                                }
                                lb[MMB] = 0;
                                s = snprintf(lb, MMB, "limits:");
                                for (k = 0; k < 4; ++k) {
                                        s += snprintf(lb + s, MMB,
                                                      " (%d,%d)",
                                                      limits[p + 2 * k],
                                                      limits[p + 2 * k +
                                                             1]);
                                }
                                show_hint(1, lb);
                        }

                        else {
                                db("inconsistent value for limits_top");
                                button[bzone] = 0;
                        }

                        redraw_button = -1;
                }

                /* inform position at PAGE_FATBITS */
                else if (CDW == PAGE_FATBITS) {

                        /*
                           Display information about the pixel (i,j).
                         */
                        if (help) {
                                int k, l, u, v, p;
                                cldesc *c;
                                float a;

                                /* no closure by here: display nothing */
                                if ((k =
                                     closure_at(X0 + i, Y0 + j, 0)) < 0)
                                        return (1);

                                c = cl + k;

                                /* make (i,j) closure-relative */
                                u = X0 + i - c->l;
                                v = Y0 + j - c->t;

                                /* (i,j) is outside the closure boundaries */
                                if ((u < 0) || (v < 0) || (u > c->r - c->l)
                                    || (v > c->b - c->t))
                                        return (1);

#ifdef MEMCHECK
                                checkidx(topfp, FS * FS, "s_dist");
#endif

                                /* search pixel on the flea path */
                                for (l = 0, p = -1;
                                     (l <= topfp) && (p < 0); ++l) {
                                        if ((fp[2 * l] == u) &&
                                            (fp[2 * l + 1] == v))
                                                p = l;
                                }

                                /* display path data at (i,j) */
                                if (p >= 0) {
                                        a = s2a(fsl[3 * p], fsl[3 * p + 1],
                                                fsl[3 * p + 2], NULL, -1,
                                                0);
                                        a *= (180 / PI);
                                        show_hint(0,
                                                  "at (%d,%d), pp=%3.3f, a=%0.3f (deg)",
                                                  u, v, fpp[p], a);
                                }

                                /* display the extremities heuristic results */
                                /*
                                   else
                                   dx(k,u,v);
                                 */

                                /* display the pixel coordinates */
                                else {
                                        show_hint(0, "at (%d,%d)", u, v);
                                }

                                return (1);
                        }
                }

                /* paint pixel */
                else if ((*cm_e_pp != ' ') && (CDW == PATTERN)) {

                        if (help) {
                                show_hint(0, "paint pixel");
                                return (1);
                        }

                        COLOR = BLACK;
                        pp(i, j);
                        redraw_document_window();
                }

                /* clear pixel */
                else if ((*cm_e_pp_c != ' ') && (CDW == PATTERN)) {

                        if (help) {
                                show_hint(0, "clear pixel");
                                return (1);
                        }

                        COLOR = WHITE;
                        pp(i, j);
                        redraw_document_window();
                }

                /* fill region with current color */
                else if ((*cm_e_fill != ' ') && (CDW == PATTERN)) {

                        if (help) {
                                show_hint(0, "paint region");
                                return (1);
                        }

                        COLOR = BLACK;
                        c = cfont[i + j * FS];
                        pp(i, j);
                        if (c != cfont[i + j * FS]) {
                                cfont[i + j * FS] = c;
                                pr(i, j, c);
                        }
                        redraw_document_window();
                }

                /* clear region with current color */
                else if ((*cm_e_fill_c != ' ') && (CDW == PATTERN)) {

                        if (help) {
                                show_hint(0, "clear region");
                                return (1);
                        }

                        COLOR = WHITE;
                        c = cfont[i + j * FS];
                        pp(i, j);
                        if (c != cfont[i + j * FS]) {
                                cfont[i + j * FS] = c;
                                pr(i, j, c);
                        }
                        redraw_document_window();
                }

                /* go to hyperref */
                else if (HTML) {

                        /* no help for this item */
                        if (help) {

                                /* show_hint(0,""); */
                                return (1);
                        }

                        show_hint(1, "");
                        html_action(x, y);
                }

                /* select symbol */
                else if (CDW == PAGE) {
                        int k;

                        /*
                           when OCRing, some data structures may not be ready
                           to the code below.
                         */
                        if (ocring)
                                return (0);

                        /* get current symbol (if any) */
                        k = symbol_at(X0 + i * RF, Y0 + j * RF);

                        if (help) {
                                sdesc *m;
                                char *a;
                                char lb[MMB + 1];

                                lb[0] = 0;
                                mb[MMB] = 0;
                                if (k >= 0) {
                                        m = mc + k;
                                        a = ((m->tr) ==
                                             NULL) ? "?" : ((m->tr)->t);
                                } else {
                                        m = NULL;
                                        a = NULL;
                                }

                                /*
                                   Display current word.
                                 */
                                if (*cm_d_words != ' ') {

                                        /* no current word */
                                        if ((k < 0) || (m->sw < 0))
                                                return (0);
                                        snprintf(lb, MMB, "word %d%s",
                                                 m->sw,
                                                 (word[m->sw].f & F_ITALIC)
                                                 ? " italic" : "");
                                }

                                /*
                                   Display pixel coordinates instead of symbol id.
                                 */
                                else if (*cm_d_pixels != ' ') {
                                        int x, y;

                                        x = X0 + i * RF;
                                        y = Y0 + j * RF;
                                        if ((cl_ready) || (pm_t != 8))
                                                show_hint(0, "at (%d,%d)",
                                                          x, y);
                                        else
                                                show_hint(0,
                                                          "at (%d,%d), color %d",
                                                          x, y,
                                                          pixmap[y * XRES +
                                                                 x]);
                                        return (0);
                                }

                                /*
                                   Display absent symbols on type 0 to
                                   help building the bookfont (experimental).
                                 */
                                else if (*cm_d_ptype != ' ') {
                                        int i, j;
                                        unsigned char c[256];
                                        pdesc *p;

                                        for (i = 0; i <= 255; ++i) {
                                                c[i] = 0;
                                        }

                                        for (i = 0; i <= topp; ++i) {
                                                p = pattern + i;
                                                if ((p->pt == 0) &&
                                                    (p->tr != NULL) &&
                                                    (strlen(p->tr) == 1)) {

                                                        ++(c
                                                           [((unsigned char
                                                              *) (p->tr))
                                                            [0]]);
                                                }
                                        }

                                        j = 0;

                                        for (i = 'a';
                                             (j < MMB) && (i <= 'z');
                                             ++i) {
                                                if (c[i] < 5)
                                                        lb[j++] = i;
                                        }
                                        for (i = 'A';
                                             (j < MMB) && (i <= 'Z');
                                             ++i) {
                                                if (c[i] < 5)
                                                        lb[j++] = i;
                                        }
                                        for (i = '0';
                                             (j < MMB) && (i <= '9');
                                             ++i) {
                                                if (c[i] < 5)
                                                        lb[j++] = i;
                                        }

                                        lb[j] = 0;

                                }

                                /*
                                   Show current symbol.
                                 */
                                else {
                                        /* no current symbol */
                                        if (k < 0)
                                                return (0);

                                        if ((mc[k].r - mc[k].l + 1 > FS) ||
                                            (mc[k].b - mc[k].t + 1 > FS))
                                                snprintf(lb, MMB,
                                                         "symbol %d (too large): \"%s\" (word %d)",
                                                         k, a, m->sw);
                                        else
                                                snprintf(lb, MMB,
                                                         "symbol %d: \"%s\" (word %d)",
                                                         k, a, m->sw);
                                }
                                show_hint(0, lb);
                                return (1);
                        }

                        show_hint(1, "");

                        /* non-binarized image: apply spyhole */
                        if (cl_ready == 0) {
                                int x, y;

                                {

                                        /* absolute coordinates */
                                        x = X0 + i * RF;
                                        y = Y0 + j * RF;

                                        /* abagar debug mode */
                                        if ((zones > 0) &&
                                            (*cm_g_abagar != ' ')) {

                                                /* "abagar" n bug */
                                                x = (limits[0] +
                                                     limits[6]) / 2;
                                                y = (limits[1] +
                                                     limits[3]) / 2;

                                                printf
                                                    ("remark: the seed was changed to (%d,%d)\n",
                                                     x, y);
                                        }

                                        spyhole(x, y, 0, thresh_val);
                                }
                        }

                        /* binarized image: select symbol using the graphic cursor */
                        else if ((curr_mc = k) >= 0) {
                                dw[PAGE_SYMBOL].rg = 1;
                                symb2buttons(curr_mc);
                                redraw_document_window();
                        }
                }
        }

        /* Application Buttons */
        else if ((10 <= x) && (x <= (10 + BW)) &&
                 (PT <= y) && (y <= PT + BUTT_PER_COL * (BH + VS))) {

                i = (y - PT) / (BH + VS);
                if ((y - PT - (i * (BH + VS))) <= BH) {

                        /* (book)

                           The Application Buttons
                           -----------------------

                           The application buttons are those displayed on the
                           left portion of the Clara X window. They're
                           labelled "zoom", "OCR", etc. Three types of
                           buttons are available. There are on/off buttons
                           (like "italic"), multi-state buttons (like the
                           alphabet button), where the state is informed by
                           the current label, and there are buttons that
                           merely capture mouse clicks, like the "zoom"
                           button. Some are sensible both to mouse button 1
                           and to mouse button 3, others are sensible only to
                           mouse button 1.

                         */

                        /* (book)

                           zoom - enlarge or reduce bitmaps. The mouse buttom 1
                           enlarge bitmaps, the mouse button 3 reduce bitmaps.
                           The bitmaps to enlarge or reduce are determined
                           by the current window. If the PAGE window is active,
                           then the scanned document is enlarged or reduced.
                           If the PAGE (fatbits) or the PATTERN window is active,
                           then the grid is enlarged or reduced. If the PAGE
                           (symbol) or the PATTERN (props) or the PATTERN (list)
                           window is active, then the web clip is enlarged or
                           reduced.

                         */
                        if (i == bzoom) {

                                if (help) {
                                        show_hint(0,
                                                  "use mouse button 1 to enlarge, 3 to reduce, 2 to max/med/min");
                                        return (1);
                                }

                                if ((dw[PAGE].v) &&
                                    ((CDW != PAGE_SYMBOL) ||
                                     (*cm_v_wclip == ' '))) {

                                        CDW = PAGE;
                                        if (RF > 1) {
                                                --RF;
                                                setview(PAGE);
                                                check_dlimits(1);
                                        }
                                }

                                else if ((CDW == PAGE_FATBITS) ||
                                         (dw[TUNE_PATTERN].v)) {
                                        if (ZPS < 7) {
                                                ZPS += 2;
                                                comp_wnd_size(-1, -1);
                                                setview(CDW);
                                                copychar();
                                        }
                                }

                                else if ((CDW == PATTERN) ||
                                         (CDW == PATTERN_LIST) ||
                                         (CDW == PATTERN_TYPES) ||
                                         (CDW == PATTERN_ACTION) ||
                                         (CDW == PAGE_SYMBOL) ||
                                         (CDW == PAGE_DOUBTS)) {

                                        if (plist_rf > 1) {
                                                --plist_rf;
                                                dw[PATTERN].rg = 1;
                                                dw[PATTERN_LIST].rg = 1;
                                                dw[PATTERN_TYPES].rg = 1;
                                                dw[PATTERN_ACTION].rg = 1;
                                                dw[PAGE_SYMBOL].rg = 1;
                                                dw[PAGE_DOUBTS].rg = 1;
                                                redraw_document_window();
                                        }
                                }
                        }

                        /* move to document start */
                        /*
                           else if (i == sarrow) {

                           if (help) {
                           show_hint(0,"button 1 is HOME, 2 is END");
                           return(1);
                           }
                           X0 = 0;
                           redraw_document_window();
                           }
                         */

                        /* move to document top */
                        /*
                           else if (i == tarrow) {

                           if (help) {
                           show_hint(0,"button 1 is TOP, 2 is BOTTOM");
                           return(1);
                           }
                           Y0 = 0;
                           redraw_document_window();
                           }
                         */

                        /* (book)

                           OCR - start a full OCR run on the current page or on
                           all pages, depending on the state of the "Work on
                           current page only" item of the Options menu.

                         */
                        else if (i == bocr) {

                                if (help) {
                                        show_hint(0,
                                                  "button 1 starts full OCR, button 3 open OCR steps menu");
                                        return (1);
                                }

                                /* OCR current page */
                                if (*cm_o_curr != ' ') {
                                        start_ocr(-2, -1, 0);
                                }

                                /* OCR all pages */
                                else {
                                        start_ocr(-1, -1, 0);
                                }
                        }

                        /* (book)

                           stop - stop the current OCR run (if any). OCR
                           does not stop immediately, but will stop
                           as soon as possible.

                         */
                        else if (i == bstop) {

                                if (help) {
                                        show_hint(0, "stop OCR");
                                        return (1);
                                }

                                /* stop */
                                if (ocring) {
                                        stop_ocr = 1;
                                        button[bstop] = 1;
                                        redraw_button = bstop;
                                }
                        }

                        /* (book)

                           zone - start definition of the OCR zone. Currently
                           zoning in Clara OCR is useful only for saving the
                           zone can as a PBM file, using the "save zone" item
                           on the "File" menu. By now, only one zone can be
                           defined and the OCR operations consider the
                           entire document, ignoring the zone.

                         */
                        else if (i == bzone) {

                                if (help) {
                                        if (button[bzone] == 0)
                                                show_hint(0,
                                                          "button 1 adds zone, 3 deletes all them");
                                        else
                                                show_hint(0, "");
                                        return (1);
                                }

                                if ((dw[PAGE].v) && (cpage >= 0)) {

                                        if (zones >= MAX_ZONES) {
                                                show_hint(2,
                                                          "cannot add another zone");
                                        } else {
                                                curr_corner = 0;
                                                button[bzone] = 1;
                                                show_hint(1,
                                                          "enter ocr limits: top left, botton right");
                                                redraw_button = -1;
                                        }
                                }
                        }

                        /* (book)

                           type - read-only button, set accordingly to the pattern
                           type of the current symbol or pattern. The various letter
                           sizes or styles (normal, footnote, etc) used by the book are
                           numbered from 0 by Clara OCR ("type 0", "type 1", etc).

                         */
                        else if (i == btype) {

                                /* former behaviour (per-symbol selection) */
                                /*
                                   if (help) {
                                   if (*cm_o_ftro != ' ')
                                   show_hint(0,"this is the current pattern type");
                                   else
                                   show_hint(0,"buttons 1 or 2 change the pattern type");
                                   return(1);
                                   }

                                   if ((dw[PATTERN].v) && (*cm_o_ftro == ' ')) {
                                   if (++button[btype] > toppt)
                                   button[btype] = 0;

                                   redraw_button = btype;
                                   form_changed = 1;
                                   form_auto_submit();
                                   }
                                 */

                                if (help) {
                                        show_hint(0,
                                                  "pattern type for current symbol/pattern");
                                        return (1);
                                }
                        }

                        /* (book)

                           bad - toggles the button state. The bad flag
                           is used to identify damaged bitmaps.

                         */
                        else if (i == bbad) {

                                if (help) {
                                        show_hint(0,
                                                  "click to toggle the bad flag");
                                        return (1);
                                }

                                button[bbad] ^= 1;
                                redraw_button = bbad;
                                form_changed = 1;
                                form_auto_submit();
                        }

                        /*

                           test - start test 

                         */
                        else if (i == btest) {

                                if (help) {
                                        show_hint(0, "start test");
                                        return (1);
                                }

                                if (!ocring) {
                                        ocr_other = 5;
                                        start_ocr(-2, OCR_OTHER, 0);
                                        show_hint(0, "running test...");
                                }

                        }

                        /* (book)

                           latin/greek/etc - read-only button, set accordingly to
                           the alphabet of the current symbol or pattern.

                         */
                        else if (i == balpha) {

                                /* former behaviour (per-symbol selection) */
                                /*
                                   if (help) {
                                   show_hint(0,"circulate alphabets and word types");
                                   return(1);
                                   }
                                   if (++button[balpha] >= nalpha)
                                   button[balpha] = 0;
                                   redraw_button = balpha;
                                   // UNPATCHED: redraw_map = 1;
                                   set_alpha();
                                 */
                                /*
                                   form_changed = 1;
                                   form_auto_submit();
                                 */

                                if (help) {
                                        show_hint(0,
                                                  "alphabet of current symbol/pattern");
                                        return (1);
                                }
                        }
                }
        }

        /* first resize area */
        else if ((tab == PAGE) && (dw[PAGE].v) && (dw[PAGE_OUTPUT].v) && (DM <= x) && (x < DM + DW) && (pb = dw[PAGE].dt + dw[PAGE].dh) && (((pb + sb <= y) && (y < dw[PAGE_OUTPUT].dt))        /* ||
                                                                                                                                                                                                   ((pb <= y) && (y < pb+10)) */
                 )) {

                if (help) {
                        show_hint(0, "click and drag to resize windows");
                        return (1);
                }

                sliding = 3;

                /* must redraw to clear the ellipses */
                redraw_document_window();
        }

        /* second resize area */
        else if ((tab == PAGE) && (dw[PAGE_OUTPUT].v) && (dw[PAGE_SYMBOL].v) && (DM <= x) && (x < DM + DW) && (ob = dw[PAGE_OUTPUT].dt + dw[PAGE_OUTPUT].dh) && (((ob + sb <= y) && (y < dw[PAGE_SYMBOL].dt))   /* ||
                                                                                                                                                                                                                   ((ob <= y) && (y < ob+10)) */
                 )) {

                if (help) {
                        show_hint(0, "click and drag to resize windows");
                        return (1);
                }

                sliding = 4;

                /* must redraw to clear the ellipses */
                redraw_document_window();
        }

        /* vertical scrollbar */
        else if ((*cm_v_hide == ' ') &&
                 (DM + DW + 10 <= x) &&
                 (x <= DM + DW + 10 + RW) && (DT <= y) && (y < DT + DH)) {

                /* steppers */
                if (y <= DT + RW) {

                        if (help) {
                                show_hint(0,
                                          "button 1 step up, button 3 go to top");
                                return (1);
                        }

                        step_up();
                } else if (y >= DT + DH - RW) {

                        if (help) {
                                if (DS)
                                        show_hint(0,
                                                  "button 1 step down, button 3 go to bottom");
                                return (1);
                        }

                        step_down();
                }

                /* cursor */
                else {

                        if (help) {
                                if (DS)
                                        show_hint(0,
                                                  "drag or click button 1 or 2 to move");
                                return (1);
                        }

                        gettimeofday(&tclick, NULL);
                        sliding = 2;
                        s0 = y;
                }

                return (0);
        }

        /* horizontal scrollbar */
        else if ((DM <= x) && (x < DM + DW) &&
                 (DT + DH + 10 <= y) && (y <= DT + DH + 10 + RW)) {

                /* steppers */
                if (x <= DM + RW) {

                        if (help) {
                                show_hint(0,
                                          "button 1 step left, button 3 go to left limit");
                                return (1);
                        }

                        step_left();
                } else if (x >= DM + DW - RW) {

                        if (help) {
                                show_hint(0,
                                          "button 1 step right, button 3 go to right limit");
                                return (1);
                        }

                        step_right();
                }

                /* cursor */
                else {

                        if (help) {
                                if (DS)
                                        show_hint(0,
                                                  "drag or click button 1 or 2 to move");
                                return (1);
                        }

                        gettimeofday(&tclick, NULL);
                        sliding = 1;
                        s0 = x;
                }

                return (0);
        }

        /*
           if (help)
           show_hint(0,"");
         */

        return (0);
}
#endif

/*

Get pointer coordinates.

*/
void get_pointer(int *x, int *y) {
        UNIMPLEMENTED();
}

/*

Flashes a message about the graphic element currently under the
pointer.

This is the service responsible to make appear messages when
you move the pointer across the Clara X window.

In some cases (as for HTML anchors) the element changes color.

*/
#if 0
void show_info(void) {
        int m, n, px, py;

        /* get pointer coordinates */
        get_pointer(&px, &py);

        if (cmenu != NULL) {

                /* reselect automagically */
                n = mb_item(px, py);
                if ((n >= 0) && (n != cmenu - CM)) {
                        dismiss(1);
                        open_menu(px, py, n);
                        set_mclip(0);
                        return;
                }

                if (cmenu->c >= 0) {
                        cma(cmenu->c + ((cmenu - CM) << CM_IT_WD), 2);
                }
                return;
        }

        /*
           The function show_htinfo hilights the HTML element
           currently under the pointer, but that will mess with the
           active menu (if any), so we avoid calling show_htinfo
           when a menu is active.

           The function show_htinfo also destroys
           the menu clipping. But the menu clipping is activated when
           the menu is dismissed, in order to redraw just the menu
           rectangle, and not the entire application window. So we
           avoid calling show_htinfo when mclip is active.
         */
        if ((cmenu != NULL) || (mclip))
                return;

        /*
           The function mactions flashes a message for most
           elements on the application window (buttons, tabs,
           symbols on the PAGE window and others).
         */
        m = mactions_b1(1, px, py);

        /*
           Flashes a message for the HTML element currently under
           the pointer.
         */
        if ((DM <= px) && (px < DM + DW) && (DT <= py) && (py < DT + DH)) {

                if ((HTML) || ((CDW == PAGE) && (*cm_d_bb != ' '))) {
                        XRectangle r;

                        r.x = DM;
                        r.y = DT;
                        r.width = DW;
                        r.height = DH;
                        XSetClipRectangles(xd, xgc, 0, 0, &r, 1, Unsorted);

                        show_htinfo(px, py);

                        r.x = 0;
                        r.y = 0;
                        r.width = WW;
                        r.height = WH;
                        XSetClipRectangles(xd, xgc, 0, 0, &r, 1, Unsorted);

                        if (curr_hti < 0)
                                show_hint(0, "");
                }
        }

        else if (!m)
                show_hint(0, "");
}
#endif
#if 0
/*

Mouse actions activated by mouse button 3.

*/
void mactions_b3(int help, int x, int y) {
        int i;

        /* change to selected window */
        select_dw(x, y);

        /*

           Open PAGE or PAGE_FATBITS options menu.

         */
        if ((DM <= x) && (x < DM + DW) && (DT <= y) && (y < DT + DH)) {

                /* PAGE context menu */
                if ((CDW == PAGE) || (CDW == PAGE_FATBITS)) {

                        open_menu(x, y, ((CDW == PAGE) ? CM_D : CM_B));
                }
        }

        /* vertical scrollbar */
        else if ((*cm_v_hide == ' ') &&
                 (DM + DW + 10 <= x) &&
                 (x <= DM + DW + 10 + RW) && (DT <= y) && (y < DT + DH)) {

                /* steppers */
                if (y <= DT + RW) {
                        Y0 = 0;
                        redraw_document_window();
                } else if (y >= DT + DH - RW) {
                        if ((GRY - VR) >= 0)
                                Y0 = GRY - VR;
                        redraw_document_window();
                }

                /* page down */
                else
                        next();

                return;
        }

        /* horizontal scrollbar */
        else if ((*cm_v_hide == ' ') &&
                 (DM <= x) && (x < DM + DW) &&
                 (DT + DH + 10 <= y) && (y <= DT + DH + 10 + RW)) {

                /* steppers */
                if (x <= DM + RW) {
                        X0 = 0;
                        redraw_document_window();
                } else if (x >= DM + DW - RW) {
                        if ((GRX - HR) >= 0)
                                X0 = GRX - HR;
                        redraw_document_window();
                }

                /* page right */
                else if ((CDW == PAGE_FATBITS) || (CDW == PAGE) ||
                         (CDW == PAGE_OUTPUT)) {
                        X0 += HR;
                        if (X0 > (XRES - HR))
                                X0 = XRES - HR;
                        if (CDW == PAGE_FATBITS)
                                RG = 1;
                        redraw_document_window();
                }

                return;
        }

        /*

           Application Buttons
           -------------------

         */
        else if ((10 <= x) && (x <= (10 + BW))) {

                i = (y - PT) / (BH + VS);
                if ((y - PT - (i * (BH + VS))) <= BH) {

                        /* end */
                        /*
                           if (i == earrow) {
                           if ((XRES - HR) >= 0)
                           X0 = XRES - HR;
                           redraw_document_window();
                           }
                         */

                        /* bottom */
                        /*
                           else if (i == barrow) {
                           if ((YRES - VR) >= 0)
                           Y0 = YRES - VR;
                           redraw_document_window();
                           }
                         */

                        /* zoom - */
                        if (i == bzoom) {

                                if ((dw[PAGE].v) &&
                                    ((CDW != PAGE_SYMBOL) ||
                                     (*cm_v_wclip == ' '))) {

                                        CDW = PAGE;
                                        if (RF < MRF) {
                                                ++RF;
                                                setview(PAGE);
                                                check_dlimits(1);
                                        }
                                }

                                else if ((CDW == PAGE_FATBITS) ||
                                         (CDW == TUNE_PATTERN)) {
                                        if (ZPS > 1) {
                                                ZPS -= 2;
                                                comp_wnd_size(-1, -1);
                                                setview(CDW);
                                                copychar();
                                        }
                                }

                                else if ((CDW == PATTERN) ||
                                         (CDW == PATTERN_LIST) ||
                                         (CDW == PATTERN_TYPES) ||
                                         (CDW == PATTERN_ACTION) ||
                                         (CDW == PAGE_SYMBOL) ||
                                         (CDW == PAGE_DOUBTS)) {

                                        if (plist_rf < 8) {
                                                ++plist_rf;
                                                dw[PATTERN].rg = 1;
                                                dw[PATTERN_LIST].rg = 1;
                                                dw[PATTERN_TYPES].rg = 1;
                                                dw[PATTERN_ACTION].rg = 1;
                                                dw[PAGE_SYMBOL].rg = 1;
                                                dw[PAGE_DOUBTS].rg = 1;
                                                redraw_document_window();
                                        }
                                }
                        }

                        /* open OCR operations menu */
                        else if (i == bocr) {
                                open_menu(x, y, CM_R);
                        }

                        /* delete OCR zones */
                        else if (i == bzone) {
                                if (zones > 0) {
                                        zones = 0;
                                        redraw_document_window();
                                        for (i = 0; i <= tops; ++i)
                                                mc[i].c = -1;
                                        sess_changed = 1;
                                }
                        }

                        /* change pattern type */
                        /*
                           else if (i == btype) {

                           if (*cm_o_ftro == ' ') {
                           if (--button[btype] < 0)
                           button[btype] = 99;
                           redraw_button = btype;
                           }
                           }
                         */

                        /*

                           page largest view

                           Show the scanned page using a 1:1 relation
                           between document pixels and display pixels.
                         */
                        /*
                           if (i == bzp) {
                           nrf = 1;
                           }
                         */

                        /*

                           page smallest view

                           Show the scanned page using the largest reduction
                           factor. The largest reduction factor is the minimum
                           one that makes the entire document fit on its window.
                         */
                        /*
                           else if (i == bzl) {
                           nrf = MRF;
                           }
                         */

                        /* change reduction factor */
                        /*
                           if (nrf != dw[PAGE].rf) {
                           setview(PAGE);
                           RF = nrf;
                           check_dlimits(1);
                           force_redraw();
                           }
                         */
                }
        }
}

/*

Mouse actions activated by mouse button 2.

*/
void mactions_b2(int help, int x, int y) {
        int i;

        /* change to selected window */
        select_dw(x, y);

        /*


         */
        if ((DM <= x) && (x < DM + DW) && (DT <= y) && (y < DT + DH)) {

                return;
        }

        /* vertical scrollbar */
        else if ((*cm_v_hide == ' ') &&
                 (DM + DW + 10 <= x) &&
                 (x <= DM + DW + 10 + RW) && (DT <= y) && (y < DT + DH)) {

                return;
        }

        /* horizontal scrollbar */
        else if ((*cm_v_hide == ' ') &&
                 (DM <= x) && (x < DM + DW) &&
                 (DT + DH + 10 <= y) && (y <= DT + DH + 10 + RW)) {

                return;
        }

        /*

           Application Buttons
           -------------------

         */
        else if ((10 <= x) && (x <= (10 + BW))) {

                i = (y - PT) / (BH + VS);
                if ((y - PT - (i * (BH + VS))) <= BH) {

                        /* zoom max/min */
                        if (i == bzoom) {
                                int o;

                                if ((dw[PAGE].v) &&
                                    ((CDW != PAGE_SYMBOL) ||
                                     (*cm_v_wclip == ' '))) {

                                        CDW = PAGE;
                                        if ((o = RF) < (MRF / 2))
                                                RF = MRF / 2;
                                        else if (RF < MRF)
                                                RF = MRF;
                                        else
                                                RF = 1;
                                        if (RF != o) {
                                                setview(PAGE);
                                                check_dlimits(1);
                                        }
                                }

                                else if ((CDW == PAGE_FATBITS) ||
                                         (CDW == TUNE_PATTERN)) {

                                        /* TODO */
                                }

                                else if ((CDW == PATTERN) ||
                                         (CDW == PATTERN_LIST) ||
                                         (CDW == PATTERN_TYPES) ||
                                         (CDW == PAGE_SYMBOL)) {

                                        if ((o = plist_rf) < 8)
                                                plist_rf = 8;
                                        else
                                                plist_rf = 1;

                                        if (plist_rf != o) {
                                                ++plist_rf;
                                                dw[PATTERN].rg = 1;
                                                dw[PATTERN_LIST].rg = 1;
                                                dw[PATTERN_TYPES].rg = 1;
                                                dw[PAGE_SYMBOL].rg = 1;
                                                redraw_document_window();
                                        }
                                }
                        }
                }
        }
}
#endif

/*

Get the menu item under the pointer.

*/
int get_item(int x, int y) {
        UNIMPLEMENTED();        // removed
        return 0;
}

/*

Get and process X events.

This function is continuously being called to check the existence
of X events to be processed and to process them (if any).

Remark: The flag have_s_ev must be cleared only when returning from
this service.

*/
#if 0
void xevents(void) {
        int no_ev;
        static int expose_queued = 0;
        static struct timeval rt = { 0, 0 };
        struct timeval t;
        unsigned d;

        /*
           No new event. Events are got from the Xserver or
           directly from the buffer xev. Some internal procedures
           feed xev conveniently and set the flag have_s_ev.
         */
        if ((!have_s_ev) &&
            (XCheckWindowEvent(xd, XW, evmasks, &xev) == 0)) {

                /*
                   Compute age of the expose event queued.
                 */
                if (expose_queued) {
                        gettimeofday(&t, NULL);
                        d = (t.tv_sec - rt.tv_sec) * 1000000;
                        if (d == 0)
                                d += (t.tv_usec - rt.tv_usec);
                        else
                                d += -rt.tv_usec + t.tv_usec;
                } else {

                        /* whenever absent, age is.. zero */
                        d = 0;

                        /*
                           When OCRing effectively, redraw components as requested,
                           and return immediately.
                         */
                        if (ocring) {
                                redraw();
                                if (!waiting_key)
                                        return;
                        }
                }

                /*
                   If there is not a 0.05 seconds old expose event
                   queued, then redraw components as requested (unless
                   there is an expose event queued), wait 0.1 seconds to
                   not burn CPU, and return.
                 */
                if (d < 50000) {

                        /* redraw components */
                        if (!expose_queued)
                                redraw();

                        /* wait 0.1 seconds */
                        if ((!ocring) || (waiting_key)) {
                                t.tv_sec = 0;
                                t.tv_usec = 100000;
                                select(0, NULL, NULL, NULL, &t);
                        }

                        /* dismiss menus that became unfocused */
                        if (cmenu != NULL)
                                dismiss(0);

                        /* show local info */
                        if (((!ocring) || waiting_key) && (focused) &&
                            (!sliding))
                                show_info();

                        return;
                }

                else {

                        /* attend the old expose event */
                        if (expose_queued) {
                                /* db("attending old expose event"); */
                                force_redraw();
                                redraw();
                                expose_queued = 0;
                        }
                }

                /* tell the code below to not try to process events */
                no_ev = 1;
        }

        /*
           Remember that we got a new event from the queue.
         */
        else {
                no_ev = 0;
                /* db("got event %d",xev.type); */
        }

        /* just got or lost focus */
        if (xev.type == FocusIn) {
                force_redraw();
                focused = 1;
                have_s_ev = 0;
                return;
        } else if (xev.type == FocusOut) {
                focused = 0;
                have_s_ev = 0;
                return;
        }

        /* a button press must clear the message line */
        if (xev.type == ButtonPress) {
                show_hint(3, NULL);
        }

        /*
           Remember time of the expose event (it's the event
           being processed now). This event will be processed
           as soon as it becomes sufficiently old, in order to
           avoid expensive redrawing of the entire window.
         */
        if ((!no_ev) && (xev.type == Expose)) {

                gettimeofday(&rt, NULL);
                expose_queued = 1;
                have_s_ev = 0;
                return;
        }

        /* no new event */
        if (no_ev) {
                have_s_ev = 0;
                return;
        }

        /*
           Redraw as requested. In some cases, (and only then), we should
           clear expose_queue too, but it's hard to distinguish such cases, so
           we prefer to maintain the expose_queued status and redraw the window
           entirely again after some delay.
         */
        else {
                redraw();
        }

        /*
           There is an active menu. Mouse button 1 will select an
           item, ESC will dismiss the menu, and arrows will
           browse the menus.
         */
        if (cmenu != NULL) {
                bev = (XButtonEvent *) & xev;

                /* selected item */
                if ((xev.type == ButtonPress) && (bev->button == Button1)) {
                        mactions_b1(0, xev.xbutton.x, xev.xbutton.y);
                }

                /* key */
                else if (xev.type == KeyPress) {
                        kactions();
                }

                /* just hilight the current item */
                else if (xev.type == MotionNotify) {
                        int j;

                        j = get_item(xev.xbutton.x, xev.xbutton.y);
                        if (j >= 0)
                                j &= CM_IT_M;
                        if (j != cmenu->c) {
                                cmenu->c = j;
                                // UNPATCHED: redraw_menu = 1;
                        }
                }
        }

        /* Application window resize */
        else if (xev.type == ConfigureNotify) {
                Window root;
                int x, y;
                unsigned w, h, b, d;

                XGetGeometry(xd, XW, &root, &x, &y, &w, &h, &b, &d);
                if ((w != WW) || (h != WH)) {
                        /* db("resizing from %d %d to %d %d",WW,WH,w,h); */
                        comp_wnd_size(w, h);
                        set_mclip(0);
                        X0 = Y0 = 0;
                        setview(CDW);
                }
        }

        /* motion notify */
        else if (xev.type == MotionNotify) {
                slide();
        }

        /* Mouse button release finishes sliding modes */
        else if (xev.type == ButtonRelease) {
                unsigned d;

                if (sliding) {

                        /* sliding delay */
                        gettimeofday(&t, NULL);
                        d = (t.tv_sec - tclick.tv_sec) * 1000000;
                        if (d == 0)
                                d += (t.tv_usec - tclick.tv_usec);
                        else
                                d += -tclick.tv_usec + t.tv_usec;

                        /* a quick press-release on mode 1 means "page left" */
                        if ((sliding == 1) && (d < 200000)) {

                                if ((CDW == PAGE_FATBITS) || (CDW == PAGE)
                                    || (CDW == PAGE_OUTPUT)) {
                                        X0 -= HR;
                                        if (X0 < 0)
                                                X0 = 0;
                                        if (CDW == PAGE_FATBITS)
                                                RG = 1;
                                        redraw_document_window();
                                }
                        }

                        /* a quick press-release on mode 2 means "page up" */
                        if ((sliding == 2) && (d < 200000))
                                prior();

                        if (curr_mc >= 0)
                                redraw_document_window();
                }
                sliding = 0;
        }

        /* Process other events */
        else {

                switch (xev.type) {

                case ButtonPress:

                        bev = (XButtonEvent *) & xev;
                        if (bev->button == Button1) {
                                mactions_b1(0, xev.xbutton.x,
                                            xev.xbutton.y);
                        } else if (bev->button == Button2) {
                                mactions_b2(0, xev.xbutton.x,
                                            xev.xbutton.y);
                        } else if (bev->button == Button3) {
                                mactions_b3(0, xev.xbutton.x,
                                            xev.xbutton.y);
                        }

                        break;

                case KeyPress:
                        kactions();
                        break;

                default:
                        db("unattended X event %d (ok)", xev.type);
                        break;
                }
        }

        have_s_ev = 0;
}
#endif
/*

Synchronize the variables alpha, alpha_sz and alpha_cols with
the current status of the alphabet selection button.

*/
#if 0
void set_alpha(void) {
        int i;

        /* get current alphabet and its size */
        i = inv_balpha[(int) (button[balpha])];
        if (i == LATIN) {
                alpha = latin;
                alpha_sz = latin_sz;
                alpha_cols = 2;
        } else if (i == GREEK) {
                alpha = (alpharec *) greek;
                alpha_sz = greek_sz;
                alpha_cols = 3;
        } else if (i == CYRILLIC) {
                alpha = cyrillic;
                alpha_sz = cyrillic_sz;
                alpha_cols = 2;
        } else if (i == HEBREW) {
                alpha = hebrew;
                alpha_sz = hebrew_sz;
                alpha_cols = 1;
        } else if (i == ARABIC) {
                alpha = arabic;
                alpha_sz = arabic_sz;
                alpha_cols = 1;
        } else if (i == NUMBER) {
                alpha = number;
                alpha_sz = number_sz;
                alpha_cols = 1;
        } else
                alpha = NULL;
}
#endif
#if 0
/*

Set the default font. Clara uses one same font for all (menus,
button labels, HTML rendering, etc. The font to be set is
obtained from the Options menu status.

*/

void set_xfont(void) {
        XCharStruct *x;

        /* free current font */
        if (dfont != NULL)
                XFreeFont(xd, dfont);

        /* use small font */
        if (*cm_v_small == 'X') {
                if (((dfont = XLoadQueryFont(xd, "6x13")) == NULL) &&
                    verbose)
                        warn("oops.. could not read font 6x13\n");
        }

        /* use medium font */
        else if (*cm_v_medium == 'X') {
                if (((dfont = XLoadQueryFont(xd, "9x15")) == NULL) &&
                    verbose)
                        warn("oops.. could not read font 9x15\n");
        }

        /* use large font */
        else if (*cm_v_large == 'X') {
                if (((dfont = XLoadQueryFont(xd, "10x20")) == NULL) &&
                    verbose)
                        warn("oops.. could not read font 10x20\n");
        }

        /* use the font informed as command-line parameter */
        else {
                if (((dfont = XLoadQueryFont(xd, xfont)) == NULL) &&
                    verbose)
                        warn("oops.. could not read font %s\n", xfont);
        }

        /* try "fixed" */
        if ((dfont == NULL) && (strcmp(xfont, "fixed") != 0)) {
                if (((dfont = XLoadQueryFont(xd, "fixed")) == NULL) &&
                    verbose)
                        warn("oops.. could not read font fixed\n");
        }

        /* no font loaded: abandon */
        if (dfont == NULL) {
                fatal(XE, "could not load a font, giving up");
        }

        /* text font height and width */
        x = &(dfont->max_bounds);
        DFH = (DFA = x->ascent) + (DFD = x->descent);
        DFW = x->width;
}
#endif
/*

Reset the parameters of all windows.

*/
#if 0
void init_dws(int m) {
        int i;

        for (i = 0; i <= TOP_DW; ++i) {
                dw[i].x0 = 0;
                dw[i].y0 = 0;
                dw[i].hr = 0;
                dw[i].vr = 0;
                dw[i].grx = 0;
                dw[i].gry = 0;
                dw[i].v = 0;
                dw[i].ds = 1;
                dw[i].gs = 1;
                dw[i].hm = 0;
                if (m) {
                        dw[i].p_ge = NULL;
                        dw[i].p_gesz = 0;
                }
                dw[i].p_topge = -1;
                dw[i].p_curr_hti = -1;
                dw[i].ulink = 1;
                dw[i].hs = 1;
                dw[i].rg = 1;
        }

        dw[WELCOME].ds = 0;

        /*
           In HTML windows that mix tables with other free elements
           we define a nonzero left margin for aesthetic.
         */
        dw[TUNE].hm = DFW;
        dw[PATTERN_TYPES].hm = DFW;
        UNIMPLEMENTED();
}
#endif

static void null_callback(GtkMenuItem *object, void *user_ptr) {
        UNIMPLEMENTED();
}

static void run_ocr_all_callback(GtkMenuItem *object, void *user_ptr) {
        if (get_flag(FL_CURRENT_ONLY))
                start_ocr(-2, -1, 0);
        else
                start_ocr(-1, -1, 0);
}

static void quit_callback(GtkMenuItem *object, void *user_ptr) {
        gui_ready = FALSE;
        start_ocr(-2, OCR_SAVE, 0);
        continue_ocr();
        gtk_main_quit();
}
static void save_session_cb(GtkMenuItem *object, void *user_ptr) {
        start_ocr(-2, OCR_SAVE, 0);
}

static void set_flag_cb(GtkCheckMenuItem *object, void *user_ptr) {
        flag_t fl = GPOINTER_TO_INT(user_ptr);
        set_flag(fl,object->active);
}

/*

Context menu initializer.

*/
/* orig: cmi */
static void init_context_menus(void) {

        /* (book)

           File menu
           ---------

           This menu is activated from the menu bar on the top of the
           application X window.
         */

        GtkWidget *submenu, *mi;
        GSList *group;

        menubar = gtk_menu_bar_new();


#define MENU(title)                                                     \
    do {                                                                \
        mi = gtk_menu_item_new_with_label(title);			\
        submenu = gtk_menu_new();                                       \
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(mi),submenu);           \
        gtk_menu_shell_append(GTK_MENU_SHELL(menubar),mi);              \
        group = NULL;                                                   \
    } while (0)

#define MENUFLOAT(var)				    \
    do {					    \
        var = submenu = gtk_menu_new();             \
        group = NULL;				    \
    } while(0)
#define MITEM(text,cb)                                          \
    do {                                                        \
        mi = gtk_menu_item_new_with_label(text);                \
        g_signal_connect(G_OBJECT(mi),"activate",               \
                         G_CALLBACK(cb),NULL);                  \
        gtk_menu_shell_append(GTK_MENU_SHELL(submenu),mi);      \
    } while(0)
#define MCHECK_(text,cb,ptr)                                    \
    do {                                                        \
        mi = gtk_check_menu_item_new_with_label(text);          \
        g_signal_connect(G_OBJECT(mi),"activate",               \
                         G_CALLBACK(cb),ptr);                   \
        gtk_menu_shell_append(GTK_MENU_SHELL(submenu),mi);      \
    } while(0)
#define MCHECK(text,cb) MCHECK_(text,cb,NULL)
#define MRADIO_START()  group = NULL
#define MRADIO(text,cb)                                                 \
    do {                                                                \
        mi = gtk_radio_menu_item_new_with_label(group,text);            \
        g_signal_connect(G_OBJECT(mi),"activate",                       \
                         G_CALLBACK(cb),NULL);                          \
	group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(mi)); \
        gtk_menu_shell_append(GTK_MENU_SHELL(submenu),mi);              \
    } while(0)

#define MSEP()                                                  \
    do {                                                        \
        group = NULL;                                           \
        mi = gtk_separator_menu_item_new();                     \
        gtk_menu_shell_append(GTK_MENU_SHELL(submenu),mi);      \
    } while(0)

        MENU("File");

        /* (book)

           Save session

           Save on disk the page session (file page.session), the patterns
           (file "pattern") and the revision acts (file "acts").
         */
        MITEM("Save session", save_session_cb);   // CM_F_SAVE, CMA_PAGE A

        /* (book)

           Save first zone

           Save on disk the first zone as the file zone.pbm.
         */
        MITEM("Save first zone", null_callback);        // CM_F_SZ, CMA_PAGE|CMA_ZONE A

        /* (book)

           Save replacing symbols

           Save on disk the entire page replacing symbols by patterns,
           to achieve better compression rates (mostly to produce small
           web images).
         */
        MITEM("Save replacing symbols", null_callback); // CM_F_SR, CMA_PAGE|CMA_CL A

        /* (book)

           Write report

           Save the contents of the PAGE LIST window to the file
           report.txt on the working directory.
         */
        MITEM("Write report", null_callback);   // CM_F_REP, 0 A

        /* (book)

           Quit the program

           Just quit the program (asking before if the session is to
           be saved.
         */

        MITEM("Quit", quit_callback);   // CM_F_QUIT, 0 A

        /* (book)

           Edit menu
           ---------

           This menu is activated from the menu bar on the top of the
           application X window.
         */
        MENU("Edit");

        /* (book)

           Only doubts

           When selected, the right or the left arrows used on the
           PATTERN or the PATTERN PROPS windows will move to the next
           or the previous untransliterated patterns.
         */
        MCHECK_("Only doubts", set_flag_cb, GINT_TO_POINTER(FL_ONLY_DOUBTS));   // CM_E_OD, 0 B

        /* (book)

           Re-scan all patterns

           When selected, the classification heuristic will retry
           all patterns for each symbol. This is required when trying
           to resolve the unclassified symbols using a second
           classification method.
         */
        MCHECK_("Re-scan all patterns", set_flag_cb, GINT_TO_POINTER(FL_RESCAN));  // CM_E_RESCAN, 0 B

        /* (book)

           Auto-classify.

           When selected, the engine will re-run the classifier after
           each new pattern trained by the user. So if various letters
           "a" remain unclassified, training one of them will perhaps
           recognize some othersm helping to complete the recognition.
         */
        MCHECK_("Auto-classify", set_flag_cb, GINT_TO_POINTER(FL_AUTO_CLASSIFY)); // CM_E_AC, 0 B

        MSEP();
        /* (book)

           Fill region

           When selected, the mouse button 1 will fill the region
           around one pixel on the pattern bitmap under edition on the
           font tab.
         */
        MRADIO("Fill region", null_callback);   // CM_E_FILL, 0 R

        /* (book)

           Paint pixel

           When selected, the mouse button 1 will paint individual
           pixels on the pattern bitmap under edition on the font tab.
         */
        MRADIO("Paint pixel", null_callback);   // CM_E_PP, 0 R

        /* (book)

           Clear region

           When selected, the mouse button 1 will clear the region
           around one pixel on the pattern bitmap under edition on the
           font tab.
         */
        MRADIO("Clear region", null_callback);  // CM_E_FILL_C, 0 R

        /* (book)

           Clear pixel

           When selected, the mouse button 1 will clear individual
           pixels on the pattern bitmap under edition on the font tab.
         */
        MRADIO("Clear pixel", null_callback);   // CM_E_PP_C, 0 R

        /* (book)

           Sort patterns by page

           When selected, the pattern list window will divide
           the patterns in blocks accordingly to their (page)
           sources.
         */
        MSEP();
        MCHECK("Sort patterns by page", null_callback); // CM_E_SP, 0 B

        /* (book)

           Sort patterns by matches

           When selected, the pattern list window will use as the
           first criterion when sorting the patterns, the number of
           matches of each pattern.
         */
        MCHECK("Sort patterns by matches", null_callback);      // CM_E_SM, 0 B

        /* (book)

           Sort patterns by transliteration

           When selected, the pattern list window will use as the
           second criterion when sorting the patterns, their
           transliterations.
         */
        MCHECK("Sort patterns by transliteration", null_callback);      // CM_E_ST, 0 B

        /* (book)

           Sort patterns by number of pixels

           When selected, the pattern list window will use as the
           third criterion when sorting the patterns, their
           number of pixels.
         */
        MCHECK("Sort patterns by number of pixels", null_callback);     // CM_E_SN, 0 B

        /* (book)

           Sort patterns by width

           When selected, the pattern list window will use as the
           fourth criterion when sorting the patterns, their
           widths.
         */
        MCHECK("Sort patterns by width", null_callback);        // CM_E_SW, 0 B

        /* (book)

           Sort patterns by height

           When selected, the pattern list window will use as the
           fifth criterion when sorting the patterns, their
           heights.
         */
        MCHECK("Sort patterns by height", null_callback);       // CM_E_SH, 0 B

        /* (book)

           Del Untransliterated patterns

           Remove from the font all untransliterated fonts.
         */
        MSEP();
        MITEM("Delete Untransliterated patterns", null_callback);       // CM_E_DU, 0 A

        /* (book)

           Set pattern type.

           Set the pattern type for all patterns marked as "other".
         */
        MITEM("Set pattern type", null_callback);       // CM_E_SETPT, 0 A

        /* (book)

           Search barcode.

           Try to find a barcode on the loaded page.
         */
        MITEM("Search barcode", null_callback); // CM_E_SEARCHB, 0 A

        /* (book)

           Instant thresholding.

           Perform on-the-fly global thresholding.
         */
        MITEM("Instant thresholding", null_callback);   // CM_E_THRESH, 0 A

#ifdef PATT_SKEL
        /* (book)

           Reset skeleton parameters

           Reset the parameters for skeleton computation for all
           patterns.
         */
        MITEM("Reset skeleton parameters", null_callback);      // CM_E_RSKEL, 0 A
#endif

        /* (book)

           View menu
           ---------

           This menu is activated from the menu bar on the top of the
           application X window.
         */
        MENU("View");

#if 0
        /* (book)

           Small font

           Use a small X font (6x13).
         */
        MRADIO("Small font (6x13)", null_callback);     // CM_V_SMALL, 0 R

        /* (book)

           Medium font

           Use the medium font (9x15).
         */
        MRADIO("Medium font (9x15)", null_callback);    // CM_V_MEDIUM, 0 R

        /* (book)

           Small font

           Use a large X font (10x20).
         */
        MRADIO("Large font (10x20)", null_callback);    // CM_V_LARGE, 0 R

        /* (book)

           Default font

           Use the default font (7x13 or "fixed" or the one informed
           on the command line).
         */
        MRADIO("Default font", null_callback);  // CM_V_DEF, 0 R

        /* (book)

           Hide scrollbars

           Toggle the hide scrollbars flag. When active, this
           flag hides the display of scrolllbar on all windows.
         */

        MCHECK("Hide scrollbars", null_callback);       // CM_V_HIDE, 0 B
#endif

        /* (book)

           Omit fragments

           Toggle the hide fragments flag. When active,
           fragments won't be included on the list of
           patterns.
         */
        MCHECK("Omit fragments", null_callback);        // CM_V_OF, 0 B

#if 0
        /* (book)

           Show HTML source

           Show the HTML source of the document, instead of the
           graphic rendering.
         */
        MSEP();
        MCHECK("Show HTML source", null_callback);      // CM_V_VHS, 0 B

        /* (book)

           Show web clip

           Toggle the web clip feature. When enabled, the PAGE_SYMBOL
           window will include the clip of the document around the
           current symbol that will be used through web revision.
         */
        MCHECK("Show web clip", null_callback); // CM_V_WCLIP, 0 B

        /* (book)

           Show alphabet map

           Toggle the alphabet map display. When enabled, a mapping
           from Latin letters to the current alphabet will be
           displayed.
         */
        MCHECK("Show alphabet map", null_callback);     // CM_V_MAP, 0 B
#endif

        /* (book)

           Show current class

           Identify the symbols on the current class using
           a gray ellipse.
         */
        MCHECK("Show current class", null_callback);    // CM_V_CC, 0 B

        MSEP();
        /* (book)

           Show matches

           Display bitmap matches when performing OCR.
         */
        MRADIO("Show matches", null_callback);  // CM_V_MAT, 0 R

        /* (book)

           Show comparisons

           Display all bitmap comparisons when performing OCR.
         */
        MRADIO("Show comparisons", null_callback);      // CM_V_CMP, 0 R

        /* (book)

           Show matches

           Display bitmap matches when performing OCR,
           waiting a key after each display.
         */
        MRADIO("Show matches and wait", null_callback); // CM_V_MAT_K, 0 R

        /* (book)

           Show comparisons and wait

           Display all bitmap comparisons when performing OCR,
           waiting a key after each display.
         */
        MRADIO("Show comparisons and wait", null_callback);     // CM_V_CMP_K, 0 R

        /* (book)

           Show skeleton tuning

           Display each candidate when tuning the skeletons of the
           patterns.
         */
        MCHECK("Show skeleton tuning", null_callback);  // CM_V_ST, 0 B

        /* (book)

           OCR steps menu
           --------------

           This menu is activated when the mouse button 3 is pressed on
           the OCR button. It allows running specific OCR steps (all
           steps run in sequence when the OCR button is pressed).

         */

        MENU("OCR");

        MITEM("Run all steps", run_ocr_all_callback);
        MSEP();
        /* (book)

           Preproc.

           Start preproc.
         */
        MITEM("Preprocessing", null_callback);  // CM_R_PREPROC, CMA_PAGE A

        /* (book)

           Detect blocks

           Start detecting text blocks.

         */
        MITEM("Detect blocks", null_callback);  // CM_R_BLOCK, CMA_PAGE A

        /* (book)

           Segmentation.

           Start binarization and segmentation.
         */
        MITEM("Segmentation", null_callback);   // CM_R_SEG, CMA_PAGE|CMA_NCL  A

        /* (book)

           Consist structures

           All OCR data structures are submitted to consistency
           tests. This is under implementation.
         */
        MITEM("Consist structures", null_callback);     // CM_R_CONS, 0 A

        /* (book)

           Prepare patterns

           Compute the skeletons and analyse the patterns for
           the achievement of best results by the classifier. Not
           fully implemented yet.
         */
        MITEM("Prepare patterns", null_callback);       // CM_R_OPT, 0 A

        /* (book)

           Read revision data

           Revision data from the web interface is read, and
           added to the current OCR training knowledge.
         */
        MITEM("Read revision data", null_callback);     // CM_R_REV, CMA_PAGE|CMA_CL A

        /* (book)

           Classification

           start classifying the symbols of the current
           page or of all pages, depending on the state of the
           "Work on current page only" item of the Options menu. It
           will also build the font automatically if the
           corresponding item is selected on the Options menu.

         */
        MITEM("Classification", null_callback); // CM_R_CLASS, CMA_PAGE|CMA_CL A

        /* (book)

           Geometric merging

           Merge closures on symbols depending on their
           geometry.
         */
        MITEM("Geometric merging", null_callback);      // CM_R_GEOMERGE, CMA_PAGE|CMA_CL A

        /* (book)

           Build words and lines

           Start building the words and lines. These heuristics will
           be applied on the
           current page or on all pages, depending on the state of
           the "Work on current page only" item of the Options menu.

         */
        MITEM("Build words and lines", null_callback);  // CM_R_BUILD, CMA_PAGE|CMA_CL A

        /* (book)

           Generate spelling hints

           Remark: this is not implemented yet.

           Start filtering through ispell to generate
           transliterations for unknow symbols or alternative
           transliterations for known symbols. Clara will use the
           dictionaries available for the languages selected on
           the Languages menu. Filtering will be performed on the
           current page or on all pages, depending on the state of
           the "Work on current page only" item of the Options menu.

         */
        MITEM("Generate spelling hints", null_callback);        // CM_R_SPELL, CMA_PAGE|CMA_CL A

        /* (book)

           Generate output

           The OCR output is generated to be displayed on the
           "PAGE (output)" window. The output is also saved to the
           file page.html.
         */
        MITEM("Generate output", null_callback);        // CM_R_OUTP, CMA_PAGE|CMA_CL A

        /* (book)

           Generate web doubts

           Files containing symbols to be revised through the web
           interface are created on the "doubts" subdirectory of
           the work directory. This step is performed only when
           Clara OCR is started with the -W command-line switch.
         */
        MITEM("Generate web doubts", null_callback);    // CM_R_DOUBTS, CMA_PAGE|CMA_CL A


        /* (book)

           PAGE options menu
           -----------------

           This menu is activated when the mouse button 3 is pressed on
           the PAGE window.

         */

        MENU("PageCTX");

        /* (book)

           See in fatbits

           Change to PAGE_FATBITS focusing this symbol.
         */
        MITEM("See in fatbits", null_callback); // CM_D_FOCUS, CMA_CL A

        /* (book)

           Bottom left here

           Scroll the window contents in order to the
           current pointer position become the bottom left.
         */
        MITEM("Bottom left here", null_callback);       // CM_D_HERE, 0 A

        /* (book)

           Use as pattern

           The pattern of the class of this symbol will be the unique
           pattern used on all subsequent symbol classifications. This
           feature is intended to be used with the "OCR this symbol"
           feature, so it becomes possible to choose two symbols to
           be compared, in order to test the classification routines.

         */
        MITEM("Use as pattern", null_callback); // CM_D_TS, CMA_CL A

        /* (book)

           OCR this symbol

           Starts classifying only the symbol under the pointer. The
           classification will re-scan all patterns even if the "re-scan
           all patterns" option is unselected.
         */
        MITEM("OCR this symbol", null_callback);        // CM_D_OS, CMA_CL A

        /* (book)

           Merge with current symbol

           Merge this fragment with the current symbol.
         */
        MITEM("Merge with current symbol", null_callback);      // CM_D_ADD, CMA_CL A

        /* (book)

           Link as next symbol

           Create a symbol link from the current symbol (the one
           identified by the graphic cursor) to this symbol.
         */
        MITEM("Link as next symbol", null_callback);    // CM_D_SLINK, CMA_CL A

        /* (book)

           Disassemble symbol

           Make the current symbol nonpreferred and each of its
           components preferred.
         */
        MITEM("Disassemble symbol", null_callback);     // CM_D_DIS, CMA_CL A

        /* (book)

           Link as accent

           Create an accent link from the current symbol (the one
           identified by the graphic cursor) to this symbol.
         */
        MITEM("Link as accent", null_callback); // CM_D_ALINK, CMA_CL A

        /* (book)

           Diagnose symbol pairing

           Run locally the symbol pairing heuristic to try to join
           this symbol to the word containing the current symbol.
           This is useful to know why the OCR is not joining two
           symbols on one same word.
         */
        MITEM("Diagnose symbol pairing", null_callback);        // CM_D_SDIAG, CMA_CL A

        /* (book)

           Diagnose word pairing

           Run locally the word pairing heuristic to try to join
           this word with the word containing the current symbol
           on one same line. This is useful to know why the OCR is
           not joining two words on one same line.
         */
        MITEM("Diagnose word pairing", null_callback);  // CM_D_WDIAG, CMA_CL A

        /* (book)

           Diagnose lines

           Run locally the line comparison heuristic to decide
           which is the preceding line.
         */
        MITEM("Diagnose lines", null_callback); // CM_D_LDIAG, CMA_CL A

        /* (book)

           Diagnose merging

           Run locally the geometrical merging heuristic to try
           to merge this piece to the current symbol.
         */
        MITEM("Diagnose merging", null_callback);       // CM_D_GM, CMA_CL A

        /* (book)

           Show pixel coords

           Present the coordinates and color of the current pixel.
         */
        MSEP();
        MRADIO("Show pixel coords", null_callback);     // CM_D_PIXELS, 0 R

        /* (book)

           Show closures

           Identify the individual closures when displaying the
           current document.
         */
        MRADIO("Show closures", null_callback); // CM_D_CLOSURES, 0 R

        /* (book)

           Show symbols

           Identify the individual symbols when displaying the
           current document.
         */
        MRADIO("Show symbols", null_callback);  // CM_D_SYMBOLS, 0 R

        /* (book)

           Show words

           Identify the individual words when displaying the
           current document.
         */
        MRADIO("Show words", null_callback);    // CM_D_WORDS, 0 R

        /* (book)

           Show pattern type

           Display absent symbols on pattern type 0, to help
           building the bookfont.
         */
        MRADIO("Show pattern type", null_callback);     // CM_D_PTYPE, 0 R

        /* (book)

           Report scale

           Report the scale on the tab when the PAGE window is
           active.
         */
        MSEP();
        MCHECK("Report scale", null_callback);  // CM_D_RS, 0 B

        /* (book)

           Display box instead of symbol

           On the PAGE window displays the bounding boxes instead of
           the symbols themselves. This is useful when designing new
           heuristics.
         */
        MCHECK("Display box instead of symbol", null_callback); // CM_D_BB, 0 B

        /* (book)

           PAGE_FATBITS options menu
           -------------------------

           This menu is activated when the mouse button 3 is pressed on
           the PAGE.

         */
        MENUFLOAT(mnuFatbits);

        /* (book)

           Bottom left here

           Scroll the window contents in order to the
           current pointer position become the bottom left.
         */
        MITEM("Bottom left here", null_callback);       // CM_B_HERE, 0 A

        /* (book)

           Centralize

           Scroll the window contents in order to the
           centralize the closure under the pointer.
         */
        MITEM("Centralize", null_callback);     // CM_B_CENTRE, 0 A

        /* (book)

           Build border path

           Build the closure border path and activate the flea.
         */
        MITEM("Build border path", null_callback);      // CM_B_BP, 0 A

        /* (book)

           Search straight lines (linear)

           Build the closure border path and search straight lines
           there using linear distances.
         */
        MITEM("Search straight lines (linear)", null_callback); // CM_B_SLD, 0 A

        /* (book)

           Search straight lines (quadratic)

           Build the closure border path and search straight lines
           there using correlation.
         */
        MITEM("Search straight lines (quadratic)", null_callback);      // CM_B_SLC, 0 A

        /* (book)

           Is bar?

           Apply the isbar test on the closure.
         */
        MITEM("Is bar?", null_callback);        // CM_B_ISBAR, 0 A

        /* (book)

           Detect extremities?

           Detect closure extremities.
         */
        MITEM("Detect extremities", null_callback);     // CM_B_DX, 0 A

        /* (book)

           Show skeletons

           Show the skeleton on the windows PAGE_FATBITS. The
           skeletons are computed on the fly.
         */
        MSEP();
        MRADIO("Show skeletons", null_callback);        // CM_B_SKEL, 0 R

        /* (book)

           Show border

           Show the border on the window PAGE_FATBITS. The
           border is computed on the fly.
         */
        MRADIO("Show border", null_callback);   // CM_B_BORDER, 0 R

        /* (book)

           Show pattern skeleton

           For each symbol, will show the skeleton of its best match
           on the PAGE (fatbits) window.
         */
        MRADIO("Show pattern skeleton", null_callback); // CM_B_HS, 0 R

        /* (book)

           Show pattern border

           For each symbol, will show the border of its best match
           on the PAGE (fatbits) window.
         */
        MRADIO("Show pattern border", null_callback);   // CM_B_HB, 0 R


        /* (book)

           Alphabets menu
           --------------

           This item selects the alphabets that will be available on the
           alphabets button.
         */
        MENU("Alphabets");

        /* (book)

           Arabic

           This is a provision for future support of Arabic
           alphabet.
         */
        MCHECK("Arabic", null_callback);        // CM_A_ARABIC, CMA_ABSENT B

        /* (book)

           Cyrillic

           This is a provision for future support of Cyrillic
           alphabet.
         */
        MCHECK("Cyrillic", null_callback);      // CM_A_CYRILLIC, CMA_ABSENT B

        /* (book)

           Greek

           This is a provision for future support of Greek
           alphabet.
         */
        MCHECK("Greek", null_callback); // CM_A_GREEK, CMA_ABSENT B

        /* (book)

           Hebrew

           This is a provision for future support of Hebrew
           alphabet.
         */
        MCHECK("Hebrew", null_callback);        // CM_A_HEBREW, CMA_ABSENT B

        /* (book)

           Kana

           This is a provision for future support of Kana
           alphabet.
         */
        MCHECK("Kana", null_callback);  // CM_A_KANA, CMA_ABSENT B

        /* (book)

           Latin

           Words that use the Latin alphabet include those from
           the languages of most Western European countries (English,
           German, French, Spanish, Portuguese and others).
         */
        MCHECK("Latin", null_callback); // CM_A_LATIN, 0 B

        /* (book)

           Number

           Numbers like
           1234, +55-11-12345678 or 2000.
         */
        MSEP();
        MCHECK("Numeric", null_callback);       // CM_A_NUMBER, 0 B

        /* (book)

           Ideogram

           Ideograms.
         */
        MCHECK("Ideograms", null_callback);     // CM_A_IDEOGRAM, CMA_ABSENT B


#ifdef LANG_MENU
        /* (book)

           Languages menu
           --------------

           This menu selects the languages used on the book. It will
           define the dictionaries that will be applied when trying
           to generate the transliteration of unknown symbols or
           generating alternative transliterations or marking a
           word for future revision.
         */
        MENU("Languages");

        /* (book)

           English (USA)

           Toggle American English spell checking for
           each word found on the page.
         */
        MCHECK("English (USA)", null_callback); // CM_L_EN, 0 B

        /* (book)

           English (UK)

           Toggle British English spell checking for
           each word found on the page.
         */
        MCHECK("English (UK)", null_callback);  // CM_L_UK, 0 B

        /* (book)

           French

           Toggle French spell checking for
           each word found on the page.
         */
        MCHECK("French", null_callback);        // CM_L_FR, 0 B

        /* (book)

           German

           Toggle german spell checking for
           each word found on the page.
         */
        MCHECK("German", null_callback);        // CM_L_DE, 0 B

        /* (book)

           Greek

           Toggle greek spell checking for
           each word found on the page.
         */
        MCHECK("Greek", null_callback); // CM_L_GR, 0 B

        /* (book)

           Portuguese (Brazil)

           Toggle brazilian protuguese spell checking for
           each word found on the page.
         */
        MCHECK("Portuguese (Brazil)", null_callback);   // CM_L_BR, 0 B

        /* (book)

           Portuguese (Portugal)

           Toggle Portuguese spell checking for
           each word found on the page.
         */
        MCHECK("Portuguese (Portugal)", null_callback); // CM_L_PT, 0 B

        /* (book)

           Russian

           Toggle Russian spell checking for
           each word found on the page.
         */
        MCHECK("Russian", null_callback);       // CM_L_RU, 0 B

#endif

        /* (book)

           Options menu
           ------------
         */
        MENU("Options");

        /* (book)

           Work on current page only

           OCR operations (classification, merge, etc) will be
           performed only on the current page.
         */
        MRADIO("Work on current page only", null_callback);     // CM_O_CURR, 0 R

        /* (book)


           Work on all pages

           OCR operations (classification, merge, etc) will be
           performed on all pages.
         */
        MRADIO("Work on all pages", null_callback);     // CM_O_ALL, 0 R

        /* (book)

           Emulate deadkeys

           Toggle the emulate deadkeys flag. Deadkeys are useful for
           generating accented characters. Deadkeys emulation are disabled
           by default The emulation of deadkeys may be set on startup
           through the -i command-line switch.
         */
        MSEP();
        MCHECK("Emulate deadkeys", null_callback);      // CM_O_DKEYS, 0 B

        /* (book)

           Menu auto popup

           Toggle the automenu feature. When enabled, the menus on the
           menu bar will pop automatically when the pointer reaches
           the menu bar.
         */
        MCHECK("Menu auto popup", null_callback);       // CM_O_AMENU, 0 B

        /* (book)

           PAGE only

           When selected, the PAGE tab will display only the PAGE window. The
           windows PAGE_OUTPUT and PAGE_SYMBOL will be hidden.
         */
        MCHECK("PAGE only", null_callback);     // CM_O_PGO, CMA_Q_DISMISS B

        /* (book)

           DEBUG menu
           ----------

         */
        MENU("Debug");

        /* (book)

           Show unaligned symbols

           The symbols with unknown alignment are displayed using gray
           ellipses. This option overrides the "show current class"
           feature.
         */
        MCHECK("Show unaligned symbols", null_callback);        // CM_G_ALIGN, 0 B

        /* (book)

           Show localbin progress

           Display the progress of the extraction of symbols
           performed by the local binarizer.
         */
        MCHECK("Show localbin progress", null_callback);        // CM_G_LB, 0 B

        /* (book)

           Activate Abagar

           Abagar is a powerful debug mode used to detect binarization
           problems. It works as follows: first load a pgm image and create
           a zone around the problematic region (typically a single letter).
           Now make sure the "Activate Agabar" option (debug menu) is on,
           and ask segmentation. Various operations will dump data when
           the segmentation enters the zone.

         */
        MCHECK("Activate Abagar", null_callback);       // CM_G_ABAGAR, 0 B

        /* (book)

           Show lines (geometrical)

           Identify the lines (computed using geometrical criteria)
           when displaying the current document.
         */
        MRADIO("Show lines (geometrical)", null_callback);      // CM_G_GLINES, 0 R

        /* (book)

           Bold Only

           Restrict the show symbols or show words feature
           (PAGE menu) to bold symbols or words.
         */
        MSEP();
        MRADIO("Bold Only", null_callback);     // CM_G_BO, 0 R

        /* (book)

           Italic Only

           Restrict the show symbols or show words feature
           (PAGE menu) to bold symbols or words.
         */
        MRADIO("Italic Only", null_callback);   // CM_G_IO, 0 R

        /* (book)

           Search unexpected mismatches

           Compare all patters with same type and transliteration. Must
           be used with the "Show comparisons and wait" option on
           to diagnose symbol comparison problems.
         */
        MSEP();
        MCHECK("Search unexpected mismatches", null_callback);  // CM_G_SUM, 0 B

        /* (book)

           Attach vocab.txt

           Attach the contents of the file vocab.txt of the current
           directory (if any) to the DEBUG window. Used to test the
           text analisys code.
         */
        MSEP();
        MRADIO("Attach vocab.txt", null_callback);      // CM_G_VOCAB, 0 R

        /* (book)

           Attach log

           Attach the contents of message log to the DEBUG window.
           Used to inspect the log.
         */
        MRADIO("Attach log", null_callback);    // CM_G_LOG, 0 R

        /* (book)

           Reset match counters

           Change to zero the cumulative matches field of all
           patterns.
         */
        MSEP();
        MITEM("Reset match counters", null_callback);   // CM_G_CM, 0 A

        /* (book)

           Enter debug window

         */
        MITEM("Enter debug window", null_callback);     // CM_G_DW, 0 A

}

enum {
        PL_COL_PAGENO,
        PL_COL_FILENAME,
        PL_COL_NRUNS,
        PL_COL_TIME,
        PL_COL_WORDS,
        PL_COL_SYMBOLS,
        PL_COL_DOUBTS,
        PL_COL_CLASSES,
        PL_NCOL,
};


void resync_pagelist(int pageno) {
        int i = pageno;
        if (!gui_ready)
                return;
        if (pageStore != NULL && pageIters != NULL) {
                gtk_list_store_set(pageStore, pageIters + i,
                                   PL_COL_NRUNS, dl_r[i],
                                   PL_COL_TIME, dl_t[i],
                                   PL_COL_WORDS, dl_w[i],
                                   PL_COL_SYMBOLS, dl_ne[i],
                                   PL_COL_DOUBTS, dl_db[i],
                                   PL_COL_CLASSES, dl_c[i], -1);
        }
}



void int_data_func(GtkTreeViewColumn *tree_column,
                   GtkCellRenderer *cell,
                   GtkTreeModel *model,
                   GtkTreeIter *iter, gpointer colno) {
        gchar *str;
        gint val;
        gtk_tree_model_get(model, iter, GPOINTER_TO_INT(colno), &val, -1);
        str = g_strdup_printf("%d", val);
        g_object_set(cell, "text", str, NULL);
        g_free(str);
}

void timespan_data_func(GtkTreeViewColumn *tree_column,
                        GtkCellRenderer *cell,
                        GtkTreeModel *model,
                        GtkTreeIter *iter, gpointer colno) {
        gint val;

        gtk_tree_model_get(model, iter, GPOINTER_TO_INT(colno), &val, -1);
        g_object_set(cell, "text", ht(val), NULL);
}

void fact_data_func(GtkTreeViewColumn *tree_column,
                    GtkCellRenderer *cell,
                    GtkTreeModel *model,
                    GtkTreeIter *iter,
                    gpointer data __attribute__ ((unused))) {
        gint classes, symbols;
        gfloat val;
        gchar *str;
        gtk_tree_model_get(model, iter, PL_COL_CLASSES, &classes,
                           PL_COL_SYMBOLS, &symbols, -1);
        if (symbols > 0)
                val = ((float) symbols) / classes;
        else
                val = 1.0;
        str = g_strdup_printf("%.2f", 100 * val);
        g_object_set(cell, "text", str,
                     //                 "value",((gint)(100 * val)),
                     NULL);
        g_free(str);
}

void recog_data_func(GtkTreeViewColumn *tree_column,
                     GtkCellRenderer *cell,
                     GtkTreeModel *model,
                     GtkTreeIter *iter, gpointer colno) {
        gint doubts, symbols;
        gfloat val;
        gchar *str;
        gtk_tree_model_get(model, iter, PL_COL_DOUBTS, &doubts,
                           PL_COL_SYMBOLS, &symbols, -1);
        if (symbols > 0)
                val = ((float) symbols - doubts) / symbols;
        else
                val = 0.0;
        str = g_strdup_printf("%.3g%%", 100 * val);
        g_object_set(cell,
                     "text", str, "value", ((gint) (100 * val)), NULL);
        g_free(str);
}

GtkTreeViewColumn *gtk_tree_view_column_new_with_data_func(const gchar *title,
                                                           GtkCellRenderer *cell,
                                                           GtkTreeCellDataFunc func,
                                                           gpointer func_data,
                                                           GDestroyNotify destroy,
                                                           gboolean expands) {
        GtkTreeViewColumn *col = gtk_tree_view_column_new();

        gtk_tree_view_column_set_title(col, title);
        gtk_tree_view_column_pack_start(col, cell, TRUE);
        gtk_tree_view_column_set_cell_data_func(col, cell, func, func_data,
                                                destroy);


        g_object_set(col, "expand", expands, "resizable", TRUE, NULL);
        /*
           {
           va_list args;
           gchar* attribute;
           gint srccol;
           va_start(args, destroy);

           attribute =  va_arg(args, gchar*);
           gtk_tree_view_column_clear_attributes(col, cell);

           while(attribute != NULL) {
           srccol = va_arg(args, gint);
           gtk_tree_view_column_add_attribute(col, cell, attribute, srccol);
           attribute = va_arg(args, gchar*);
           }

           va_end(args);
           }
         */
        return col;
}

/* handlers */

void page_list_callback(GtkTreeSelection *sel, gpointer userdata) {
        GtkTreeModel *model;
        GtkTreeIter it;
        if (gtk_tree_selection_get_selected(sel, &model, &it)) {
                int val;
                gtk_tree_model_get(model, &it, PL_COL_PAGENO, &val, -1);
                start_ocr(val, OCR_LOAD, 0);
        }
}

static void page_view_symbol_changed_cb(ClaraDocView *docView, int symNo,
                                        gpointer data G_GNUC_UNUSED) {
        curr_mc = symNo;
}

static void page_view_translit_given_cb(ClaraDocView *docView, int symNo,
                                        const gchar *translit,
                                        gpointer data G_GNUC_UNUSED) {
        g_print("Translit: %d -> %s\n", symNo, translit);
        if (symNo >= 0) {
                start_ocr(-2, OCR_REV, 0);
                strcpy(to_tr, translit);
                to_rev = REV_TR;
        }
}

// constructors...

static GtkWidget *create_page_list_window() {
        int i, pn;
        pageStore = gtk_list_store_new(PL_NCOL, G_TYPE_INT, G_TYPE_STRING,      /* filename */
                                       G_TYPE_INT,      /* runs */
                                       G_TYPE_INT,      // time
                                       G_TYPE_INT,      // words
                                       G_TYPE_INT,      // symbols
                                       G_TYPE_INT,      // doubts
                                       G_TYPE_INT);     // classes
        pageIters = malloc(sizeof(GtkTreeIter) * npages);
        memset(pageIters, 0, sizeof(GtkTreeIter) * npages);

        for (i = pn = 0; i < npages; i++) {
                if (i > 0)
                        while (pagelist[pn++]); /* pagelist is a continuous stream
                                                 * of null-terminated strings. Skip
                                                 * to the next one.  There is
                                                 * intentionally no while body.
                                                 */
                gtk_list_store_insert_with_values(pageStore, pageIters + i, i,
                                                  PL_COL_PAGENO, i,
                                                  PL_COL_FILENAME, pagelist + pn,        // filename
                                                  PL_COL_NRUNS, dl_r[i],
                                                  PL_COL_TIME, dl_t[i],
                                                  PL_COL_WORDS, dl_w[i],
                                                  PL_COL_SYMBOLS, dl_ne[i],
                                                  PL_COL_DOUBTS, dl_db[i],
                                                  PL_COL_CLASSES, dl_c[i],
                                                  -1);
        }


        GtkWidget *tree =
            gtk_tree_view_new_with_model(GTK_TREE_MODEL(pageStore));
        gtk_tree_view_append_column(GTK_TREE_VIEW(tree),
                                    gtk_tree_view_column_new_with_attributes
                                    ("File", gtk_cell_renderer_text_new(),
                                     "text", PL_COL_FILENAME, NULL));
        g_object_set(gtk_tree_view_get_column(GTK_TREE_VIEW(tree), 0),
                     "resizable", TRUE, NULL);

        gtk_tree_view_append_column(GTK_TREE_VIEW(tree),
                                    gtk_tree_view_column_new_with_data_func
                                    ("Runs", gtk_cell_renderer_text_new(),
                                     int_data_func,
                                     GINT_TO_POINTER(PL_COL_NRUNS), NULL,
                                     FALSE));

        gtk_tree_view_append_column(GTK_TREE_VIEW(tree),
                                    gtk_tree_view_column_new_with_data_func
                                    ("Time", gtk_cell_renderer_text_new(),
                                     timespan_data_func,
                                     GINT_TO_POINTER(PL_COL_TIME), NULL,
                                     FALSE));

        gtk_tree_view_append_column(GTK_TREE_VIEW(tree),
                                    gtk_tree_view_column_new_with_data_func
                                    ("Words", gtk_cell_renderer_text_new(),
                                     int_data_func,
                                     GINT_TO_POINTER(PL_COL_WORDS), NULL,
                                     FALSE));

        gtk_tree_view_append_column(GTK_TREE_VIEW(tree),
                                    gtk_tree_view_column_new_with_data_func
                                    ("Symbols",
                                     gtk_cell_renderer_text_new(),
                                     int_data_func,
                                     GINT_TO_POINTER(PL_COL_SYMBOLS), NULL,
                                     FALSE));

        gtk_tree_view_append_column(GTK_TREE_VIEW(tree),
                                    gtk_tree_view_column_new_with_data_func
                                    ("Doubts",
                                     gtk_cell_renderer_text_new(),
                                     int_data_func,
                                     GINT_TO_POINTER(PL_COL_DOUBTS), NULL,
                                     FALSE));

        gtk_tree_view_append_column(GTK_TREE_VIEW(tree),
                                    gtk_tree_view_column_new_with_data_func
                                    ("Classes",
                                     gtk_cell_renderer_text_new(),
                                     int_data_func,
                                     GINT_TO_POINTER(PL_COL_CLASSES), NULL,
                                     FALSE));

        gtk_tree_view_append_column(GTK_TREE_VIEW(tree),
                                    gtk_tree_view_column_new_with_data_func
                                    ("Fact", gtk_cell_renderer_text_new(),
                                     fact_data_func, NULL, NULL, FALSE));
        gtk_tree_view_append_column(GTK_TREE_VIEW(tree),
                                    gtk_tree_view_column_new_with_data_func
                                    ("Recog",
                                     gtk_cell_renderer_progress_new(),
                                     recog_data_func, NULL, NULL, TRUE));
        GtkTreeSelection *sel =
            gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
        gtk_tree_selection_set_mode(sel, GTK_SELECTION_BROWSE);

        g_signal_connect(sel, "changed", G_CALLBACK(page_list_callback),
                         NULL);
        GtkWidget *scroller = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),
                                       GTK_POLICY_AUTOMATIC,
                                       GTK_POLICY_AUTOMATIC);
        gtk_container_add(GTK_CONTAINER(scroller), tree);
        gtk_widget_show_all(scroller);
        return scroller;
        //
}

void new_page() {
        if (gui_ready) {
                clara_doc_view_new_page(CLARA_DOC_VIEW(wDocView));
        }
        redraw_document_window();
}

void redraw_document_window() {
        if (gui_ready) {
                gtk_widget_queue_draw(wDocView);
                rebuild_page_contents();
        }
}

static void rebuild_page_contents() {
        GtkTextIter it;
        int lineNo;
        gboolean new_word;
        GtkTextMark *mark_start;
        gtk_text_buffer_set_text(tbOutput, "foo", -1);
        gtk_text_buffer_get_end_iter(tbOutput, &it);
        mark_start =
            gtk_text_buffer_create_mark(tbOutput, "word-start", &it, TRUE);
        for (lineNo = 0; lineNo <= topln; ++lineNo) {
                gboolean put_ch = FALSE;
                int wordNo, chr;
                for (wordNo = line[lineNo].f; wordNo >= 0;) {
                        new_word = TRUE;
                        for (chr = word[wordNo].F; chr >= 0;
                             chr = mc[chr].E) {
                                if (new_word && mc[chr].tc != DOT &&
                                    mc[chr].tc != COMMA) {
                                        new_word = FALSE;
                                        if(put_ch)
                                                gtk_text_buffer_insert(tbOutput,
                                                                       &it, " ",
                                                                       -1);
                                        gtk_text_buffer_move_mark_by_name
                                            (tbOutput, "word-start", &it);
                                }
                                // TODO: handle multipart characters.
                                if (mc[chr].tr == NULL) {
                                        put_ch = TRUE;
                                        gtk_text_buffer_insert_with_tags_by_name
                                            (tbOutput, &it, "\342\230\271",
                                             -1, "invalid", NULL);
                                } else {
                                        gtk_text_buffer_insert(tbOutput,
                                                               &it,
                                                               mc[chr].
                                                               tr->t, -1);
                                }
                                gtk_text_buffer_get_end_iter(tbOutput, &it);
                                put_ch = TRUE;

                        }

                        if (!new_word) {
                                GtkTextIter wordStart;
                                gtk_text_buffer_get_iter_at_mark(tbOutput,
                                                                 &wordStart,
                                                                 gtk_text_buffer_get_mark
                                                                 (tbOutput,
                                                                  "word-start"));
                                if (word[wordNo].f & F_ITALIC)
                                        gtk_text_buffer_apply_tag_by_name
                                            (tbOutput, "italic",
                                             &wordStart, &it);

                                if (word[wordNo].f & F_BOLD)
                                        gtk_text_buffer_apply_tag_by_name
                                            (tbOutput, "bold", &wordStart,
                                             &it);
                        }
                        wordNo = word[wordNo].E;
                }
                if (put_ch)
                        gtk_text_buffer_insert(tbOutput, &it, "\n", -1);
        }
}

static GtkWidget *create_page_view_window(void) {
        GtkWidget *vp1, *vp2, *wText, *wInfo, *scroller, *vb1, *wZoom;

        wDocView = clara_doc_view_new();
        g_signal_connect(wDocView, "transliteration-given",
                         G_CALLBACK(page_view_translit_given_cb), NULL);
        g_signal_connect(wDocView, "symbol-selected",
                         G_CALLBACK(page_view_symbol_changed_cb), NULL);
        scroller = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),
                                       GTK_POLICY_AUTOMATIC,
                                       GTK_POLICY_AUTOMATIC);
        //gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scroller),wDocView);
        gtk_container_add(GTK_CONTAINER(scroller), wDocView);

        vb1 = gtk_vbox_new(FALSE, 2);
        GtkAdjustment *zoomAdj;
        g_object_get(wDocView, "zoom-adjustment", &zoomAdj, NULL);
        wZoom = gtk_hscale_new(zoomAdj);
        g_object_set(wZoom, "digits", 2, NULL);
        gtk_box_pack_start(GTK_BOX(vb1), wZoom, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vb1), gtk_hseparator_new(), FALSE,
                           FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vb1), scroller, TRUE, TRUE, 0);

        vp1 = gtk_vpaned_new();
        vp2 = gtk_vpaned_new();

        tbOutput = gtk_text_buffer_new(NULL);


        g_object_ref_sink(tbOutput);
        wText = gtk_text_view_new_with_buffer(tbOutput);
        PangoFontDescription *defaultFace = pango_font_description_from_string("Monospace");
        gtk_widget_modify_font(wText, defaultFace);
        pango_font_description_free(defaultFace);

        //tbOutput = gtk_text_view_get_buffer(GTK_TEXT_VIEW(wText));
        gtk_text_buffer_create_tag(tbOutput, "bold",
                                   "weight", PANGO_WEIGHT_BOLD,
                                   "weight-set", TRUE, NULL);
        gtk_text_buffer_create_tag(tbOutput, "italic",
                                   "style", PANGO_STYLE_ITALIC,
                                   "style-set", TRUE, NULL);
        gtk_text_buffer_create_tag(tbOutput, "invalid",
                                   "foreground", "red",
                                   "foreground-set", TRUE, NULL);

        wInfo = gtk_text_view_new();



        gtk_paned_add1(GTK_PANED(vp1), vb1);
        gtk_paned_add2(GTK_PANED(vp1), vp2);

        scroller = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),
                                       GTK_POLICY_AUTOMATIC,
                                       GTK_POLICY_AUTOMATIC);
        gtk_container_add(GTK_CONTAINER(scroller), wText);

        gtk_paned_add1(GTK_PANED(vp2), scroller);
        gtk_paned_add2(GTK_PANED(vp2), wInfo);

        gtk_widget_show_all(vp1);
        return vp1;

}

/*

GUI initialization.

*/
void xpreamble() {

        if (!g_thread_supported())
                g_thread_init(NULL);
        /* X preamble */
        if ((batch_mode == 0) && (wnd_ready == 0)) {
                const char *displayname = gdk_get_display_arg_name();
                GdkDisplay *dsp = gdk_display_open(displayname);
                if (dsp == NULL) {
                        if (displayname != NULL)
                                fatal(XE, "cannot connect display %s",
                                      displayname);
                        else
                                fatal(XE, "cannot connect display");
                }
                gdk_display_manager_set_default_display
                    (gdk_display_manager_get(), dsp);
        }

        if (batch_mode == 0) {
                /* context menus initialization */
                init_context_menus();

                /* set alphabets button label */
                // UNPATCHED: set_bl_alpha();
        }

        /* buffers for revision bitmaps */
        //UNPATCHED: rw = c_realloc(NULL,sizeof(char)*RWX*RWY,NULL);

        /* alloc colors and create application X window */
        if ((batch_mode == 0) && (wnd_ready == 0)) {
                //int i,j,n,r,x,y;

                mainwin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
                g_signal_connect(mainwin, "delete-event",
                                 G_CALLBACK(quit_callback), NULL);
                // TODO: fix batch mode to use pixmaps as appropriate.
                // xw = (use_xb) ? pm : XW;

                GtkWidget *top = gtk_vbox_new(FALSE, 0);
                GtkWidget *tpane = gtk_hpaned_new();
                gtk_box_pack_start(GTK_BOX(top), menubar, FALSE, FALSE, 0);

                wPageList = create_page_list_window();
                gtk_paned_add1(GTK_PANED(tpane), wPageList);

                nbViews = gtk_notebook_new();

                //gtk_notebook_append_page(GTK_NOTEBOOK(nbViews), create_page_list_window(), gtk_label_new("Page List"));
                gtk_notebook_append_page(GTK_NOTEBOOK(nbViews),
                                         create_page_view_window(),
                                         gtk_label_new("Page View"));
                //gtk_notebook_append_page(GTK_NOTEBOOK(nbViews), gtk_text_view_new(), gtk_label_new("Page (Fatbits)"));

                gtk_notebook_append_page(GTK_NOTEBOOK(nbViews),
                                         gtk_text_view_new(),
                                         gtk_label_new("Patterns"));

                {
                        GtkWidget *scrolled_window =
                            gtk_scrolled_window_new(NULL, NULL);
                        WebKitWebView *web_view =
                            WEBKIT_WEB_VIEW(webkit_web_view_new());
                        gtk_container_add(GTK_CONTAINER(scrolled_window),
                                          GTK_WIDGET(web_view));
                        webkit_web_view_load_html_string(web_view, "",
                                                         "local://");
                        gtk_widget_show_all(scrolled_window);

                        gtk_notebook_append_page(GTK_NOTEBOOK(nbViews),
                                                 scrolled_window,
                                                 gtk_label_new("WEBBY"));
                }

                gtk_paned_add2(GTK_PANED(tpane), nbViews);

                gtk_box_pack_start(GTK_BOX(top), tpane, TRUE, TRUE, 0);
                sbStatus = gtk_statusbar_new();
                gtk_box_pack_start(GTK_BOX(top), sbStatus, FALSE, FALSE,
                                   0);
                gtk_container_add(GTK_CONTAINER(mainwin), top);
                gtk_widget_show_all(mainwin);

                /* window ready flag */
                wnd_ready = 1;
        }

        /* initialize document windows */
        // UNPATCHED: init_dws(1);

        /* PAGE special settings */
        //UNPATCHED: dw[PAGE].rf = MRF;

        /* set the window title */
        if (batch_mode == 0) {
                gtk_window_set_title(GTK_WINDOW(mainwin), "Clara OCR");
        }
        gui_ready = TRUE;

}

/*

Copy the viewable region on the PAGE (fatbits) window to
cfont.

*/
#define MAXHL 10
void copychar(void) {
        int l, r, t, b, w, h;
        int k, cs[MAXHL], dx[MAXHL], dy[MAXHL], n;
        sdesc *y;

        /* make sure we're at the correct window */
        CDW = PAGE_FATBITS;

        /* mc limits */
        l = X0;
        r = X0 + HR - 1;
        t = Y0;
        b = Y0 + VR - 1;
        w = r - l + 1;
        h = b - t + 1;

        /* clear cfont */
        memset(cfont, WHITE, HR * VR);

        /* clear the border path */
        topfp = -1;

        /* look for closures that intersect the rectangle (l,r,t,b) */
        for (n = 0, k = 0; k <= topps; ++k) {
                int i, j;
                int ia, ib, ja, jb;

                /*
                   Does the symbol k have intersection with the
                   region we're interested on?
                 */
                y = mc + ps[k];
                if ((intersize(y->l, y->r, l, r, NULL, NULL) > 0) &&
                    (intersize(y->t, y->b, t, b, NULL, NULL) > 0)) {

                        /*
                           Is the symbol k entirely contained in the
                           region we're interested on?
                         */
                        if ((n < MAXHL) && (l <= y->l) && (y->r <= r) &&
                            (t <= y->t) && (y->b <= b)) {
                                dx[n] = y->l - l;
                                dy[n] = y->t - t;
                                cs[n] = ps[k];
                                ++n;
                        }

                        /*

                           Now we need to compute the values of i and j for which
                           (l+i,t+j) is both a point of the closure k and of
                           the rectangle (l,r,t,b). The minimum and maximum such
                           values will be ia, ib, ja and jb:

                           i = ia, ia+1, ..., ib
                           j = ja, ja+1, ..., jb

                         */
                        if (y->l <= l)
                                ia = 0;
                        else
                                ia = y->l - l;
                        if (y->r >= r)
                                ib = HR - 1;
                        else
                                ib = y->r - l;
                        if (y->t <= t)
                                ja = 0;
                        else
                                ja = y->t - t;
                        if (y->b >= b)
                                jb = VR - 1;
                        else
                                jb = y->b - t;

                        /* copy the sampled pixels to the buffer */
                        for (i = ia; i <= ib; ++i) {
                                for (j = ja; j <= jb; ++j) {

                                        if (spixel(y, l + i, t + j) ==
                                            BLACK)
                                                cfont[i + j * HR] = BLACK;
                                        /*
                                           else
                                           cfont[i+j*HR] = WHITE;
                                         */
                                }
                        }
                }
        }

        /* compute the skeleton on the fly */
        if (get_flag(FL_SHOW_SKELETONS) || get_flag(FL_SHOW_BORDER)) {
                int u, v, d, e, p;

                memset(cb, WHITE, LFS * FS);
                for (u = 0; u < FS; ++u)
                        for (v = 0; v < FS; ++v)
                                cb[u + v * LFS] = cfont[u + v * HR];
                if (get_flag(FL_SHOW_SKELETONS))
                        skel(-1, -1, 0, 0);
                else
                        cb_border(0, 0);
                for (v = 0; v < FS; ++v) {
                        d = v * LFS;
                        e = v * HR;
                        for (u = 0; u < FS; ++u)
                                if ((p = cb[u + d]) == GRAY)
                                        cfont[u + e] = BLACK;
                                else if (p == BLACK)
                                        cfont[u + e] = WHITE;
                }
        }

        for (k = 0; k < n; ++k) {
                int u, v, d, e, p;

                /* Show the pattern border */
                if (get_flag(FL_SHOW_PATTERN_BORDER) &&
                    (mc[cs[k]].bm >= 0)) {
                        {
                                int i, j, h;
                                unsigned char m, *p;
                                pdesc *d;

                                memset(cb, WHITE, LFS * FS);
                                d = pattern + id2idx(mc[cs[k]].bm);
                                for (j = 0; j < d->bh; ++j) {
                                        p = d->b + BLS * j;
                                        m = 128;
                                        h = (j + dy[k]) * LFS;
                                        for (i = 0; i < d->bw; ++i) {
                                                if ((*p & m) != 0)
                                                        cb[i + dx[k] + h] =
                                                            BLACK;
                                                if ((m >>= 1) <= 0) {
                                                        ++p;
                                                        m = 128;
                                                }
                                        }
                                }
                        }
                        cb_border(0, 0);
                        for (v = 0; v < FS; ++v) {
                                d = v * LFS;
                                e = v * HR;
                                for (u = 0; u < FS; ++u)
                                        if ((p = cb[u + d]) == GRAY)
                                                cfont[u + e] |= D_MASK;
                        }
                }

                /* Show the pattern skeleton */
                if (get_flag(FL_SHOW_PATTERN_SKEL) && (mc[cs[k]].bm >= 0)) {
                        int i, j, h;
                        unsigned char m, *p;
                        pdesc *d;

                        d = pattern + id2idx(mc[cs[k]].bm);
                        dx[k] += (d->bw - d->sw) / 2;
                        dy[k] += (d->bh - d->sh) / 2;
                        for (j = 0; j < d->sh; ++j) {
                                p = d->s + BLS * j;
                                m = 128;
                                h = (j + dy[k]) * HR;
                                for (i = 0; i < d->sw; ++i) {
                                        if ((*p & m) != 0)
                                                cfont[i + dx[k] + h] =
                                                    WHITE;
                                        if ((m >>= 1) <= 0) {
                                                ++p;
                                                m = 128;
                                        }
                                }
                        }
                }
        }

        redraw_document_window();
        // UNPATCHED: redraw_grid = 1;
        dw[PAGE_FATBITS].rg = 0;
}

void show_message(const char *msg) {
        // set the status message of sbStatus to msg.
        UNIMPLEMENTED();
}
