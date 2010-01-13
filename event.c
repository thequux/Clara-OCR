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
#include "common.h"
#include "gui.h"

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

/* Indexes of the menus */
int CM_F;
int CM_E;
int CM_T;
int CM_V;
int CM_D;
int CM_B;
int CM_O;
int CM_A;
int CM_L;
int CM_R;
int CM_G;

/* items on File menu */
int CM_F_LOAD;
int CM_F_SAVE;
int CM_F_SZ;
int CM_F_SR;
int CM_F_REP;
int CM_F_QUIT;

/* items on Edit menu */
int CM_E_DOUBTS;
int CM_E_RESCAN;
int CM_E_FILL;
int CM_E_FILL_C;
int CM_E_PP;
int CM_E_PP_C;
int CM_E_SP;
int CM_E_SM;
int CM_E_ST;
int CM_E_SW;
int CM_E_SH;
int CM_E_SN;
int CM_E_DU;
int CM_E_SETPT;
int CM_E_SEARCHB;
int CM_E_THRESH;
int CM_E_AC;
#ifdef PATT_SKEL
int CM_E_RSKEL;
#endif

/* items on VIEW menu */
int CM_V_SMALL;
int CM_V_MEDIUM;
int CM_V_LARGE;
int CM_V_DEF;
int CM_V_HIDE;
int CM_V_OF;
int CM_V_VHS;
int CM_V_WCLIP;
int CM_V_MAP;
int CM_V_CC;
int CM_V_MAT;
int CM_V_CMP;
int CM_V_MAT_K;
int CM_V_CMP_K;
int CM_V_ST;
int CM_V_PRES;

/* items on Options menu */
int CM_O_CURR;
int CM_O_ALL;
int CM_O_DKEYS;
int CM_O_AMENU;
int CM_O_PGO;

/* items on the Alphabet menu */
int CM_A_ARABIC;
int CM_A_CYRILLIC;
int CM_A_GREEK;
int CM_A_HEBREW;
int CM_A_LATIN;
int CM_A_KANA;
int CM_A_NUMBER;
int CM_A_IDEOGRAM;

/* items on the Languages menu */
int CM_L_BR;
int CM_L_DE;
int CM_L_EN;
int CM_L_FR;
int CM_L_GR;
int CM_L_IT;
int CM_L_PT;
int CM_L_RU;
int CM_L_UK;

/* items on DEBUG menu */
int CM_G_ALIGN;
int CM_G_LB;
int CM_G_GLINES;
int CM_G_CM;
int CM_G_BO;
int CM_G_IO;
int CM_G_SUM;
int CM_G_DW;
int CM_G_VOCAB;
int CM_G_LOG;
int CM_G_ABAGAR;

/* items on PAGE options menu */
int CM_D_TS;
int CM_D_OS;
int CM_D_HERE;
int CM_D_ADD;
int CM_D_SLINK;
int CM_D_ALINK;
int CM_D_DIS;
int CM_D_SDIAG;
int CM_D_WDIAG;
int CM_D_LDIAG;
int CM_D_GM;
int CM_D_FOCUS;
int CM_D_PIXELS;
int CM_D_CLOSURES;
int CM_D_SYMBOLS;
int CM_D_WORDS;
int CM_D_PTYPE;
int CM_D_RS;
int CM_D_BB;

/* items on PAGE_FATBITS options menu */
int CM_B_HERE;
int CM_B_CENTRE;
int CM_B_SKEL;
int CM_B_BORDER;
int CM_B_HS;
int CM_B_HB;
int CM_B_BP;
int CM_B_SLC;
int CM_B_SLD;
int CM_B_ISBAR;
int CM_B_DX;

/* items on OCR steps menu */
int CM_R_PREPROC;
int CM_R_SEG;
int CM_R_CONS;
int CM_R_OPT;
int CM_R_REV;
int CM_R_BLOCK;
int CM_R_CLASS;
int CM_R_GEOMERGE;
int CM_R_BUILD;
int CM_R_SPELL;
int CM_R_OUTP;
int CM_R_DOUBTS;

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
int redraw_button,
    redraw_bg,
    redraw_grid,
    redraw_stline,
    redraw_dw,
    redraw_inp,
    redraw_tab,
    redraw_zone,
    redraw_menu,
    redraw_j1,
    redraw_j2,
    redraw_pbar=0,
    redraw_map,
    redraw_flea;

/*

cfont is the display buffer. It contains a matrix of pixels to
be displayed on the windows PAGE_FATBITS and PATTERN.

*/
char cfont[4*FS*4*FS];

/* buffer of messages */
char mb[MMB+1],mba[MMB+1];

/* font used to display text messages, its height and width */
XFontStruct *dfont = NULL;
int DFH,DFW,DFA,DFD;

/* vertical separation between text lines */
int VS = 2;

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
dwdesc dw[TOP_DW+1];

/*

Restricted clip window for the PAGE window.

*/
int PHR,PVR,PDM,PDT;

/* active window and active mode for each tab */
int CDW=-1;
int PAGE_CM=PAGE,PATTERN_CM=PATTERN,TUNE_CM=TUNE;

/* Xcolors */
XColor white,black,gray,darkgray,vdgray;
int glevels=-1;

/*

Mode for display matches.

This variable selects how the matches of symbols and patterns
from the bookfont are displayed. When 0, the GUI will use black
to the symbol pixels and gray to the pattern skeleton
pixels. When 1, swap these colors.

*/
int dmmode=1;

/* the event */
XEvent xev;
XButtonEvent *bev;
int have_s_ev;

/* Context */
char *displayname = NULL;
Display *xd=NULL;
GC xgc;
char *cschema;
int mclip = 0;

/*

The window and its buffer.

*/
Window XW;
XID xw;
Pixmap pm;
int pmw,pmh;
int have_xw=0,have_pm=0,use_xb=1;

/* default name for each tab */
char *tabl[] = {"page","patterns","tune","history"};

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
int ZPS=1;

/* current color */
int COLOR=BLACK;

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
int zones=0;
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
int waiting_key = 0,key_pressed;
char inp_str[MMB+1];
int inp_str_ps;

/*

This is the flea path.

*/
unsigned short fp[2*sizeof(short)*FS*FS];
float fsl[3*FS*FS],fpp[FS*FS];
int topfp,fpsz,curr_fp=0,last_fp=-1;

/*

Spyhole stuff.

*/
int sh_tries;

/*

Create a new menu.

The initial number of items is m and the menu title is tt. If a==1,
the menu is added to the menu bar. If a==2, it must be activated by
some special action, like clicking the mouse button 3 somewhere.

*/
cmdesc *addmenu(int a,char *tt,int m)
{
    /* enlarge CM */
    if (++TOP_CM >= CM_SZ) {
        int a=0;

        /*
            The pointer to the currently active menu (if any) must
            be recomputed accordingly to the new dynamic CM address.
        */
        if (cmenu != NULL)
            a = cmenu-CM;
        CM = c_realloc(CM,sizeof(cmdesc)*(CM_SZ+=30),NULL);
        if (cmenu != NULL)
            cmenu = CM + a;
    }

    /* Create the menu */
    CM[TOP_CM].a = a;
    strncpy(CM[TOP_CM].tt,tt,MAX_MT);
    CM[TOP_CM].tt[MAX_MT] = 0;
    if (m <= 0)
        m = 5;
    CM[TOP_CM].m = m;
    CM[TOP_CM].n = 0;
    CM[TOP_CM].l = c_realloc(NULL,m*(MAX_MT+1)+m,NULL);
    CM[TOP_CM].t = c_realloc(NULL,m,NULL);
    CM[TOP_CM].h = c_realloc(NULL,m*sizeof(char *),NULL);
    CM[TOP_CM].f = c_realloc(NULL,m*sizeof(int),NULL);
    CM[TOP_CM].c = -1;
    return(CM+TOP_CM);
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
int additem(cmdesc *m,char *t,int p,int g,int d,char *h,int f)
{
    int id;

    /* maximum number of items reached: enlarge */
    if (m->n >= m->m) {
        int o;

        o = m->m;
        m->l = c_realloc(m->l,(m->m+=5)*(MAX_MT+1),NULL);
        m->t = c_realloc(m->t,m->m,NULL);
        m->h = c_realloc(m->h,(m->m)*sizeof(char *),NULL);
        m->f = c_realloc(m->f,(m->m)*sizeof(int),NULL);
    }

    /* label */
    strncpy(m->l+(MAX_MT+1)*m->n,t,MAX_MT);
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
    id = m->n + ((m-CM)<< CM_IT_WD);
    ++(m->n);
    return(id);
}

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
void show_hint(int f,char *s, ...)
{
    va_list args;
    static int fixed = 0;

    /* enter_wait disables show_hint */
    if (waiting_key)
        return;

    va_start(args,s);

    if (f == 3) {
        redraw_stline = 1;
        mb[0] = 0;
        fixed = 0;
    }

    /* fixed or permanent entry: redraw unconditionally */
    else if (f) {
        if (vsnprintf(mb,MMB,s,args) < 0)
            mb[0] = 0;
        else if ((trace) && (mb[0]!=0))
            tr(mb);
        redraw_stline = 1;
        fixed = f;
    }

    else {

        /*
            If the last hint was fixed, then refresh only if the
            new hint is nonempty.
        */
        if (fixed == 1) {
            if (s[0] != 0) {
                if (vsnprintf(mb,MMB,s,args) < 0)
                    mb[0] = 0;
                redraw_stline = 1;
                fixed = 0;
            }
        }

        /*
            If the last hint was not fixed, then refresh only if
            the hint changed.
        */
        else if ((fixed == 0) && (strcmp(mb,s) != 0)) {
            if (vsnprintf(mb,MMB,s,args) < 0)
                mb[0] = 0;
            redraw_stline = 1;
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
void enter_wait(char *s,int f,int m)
{
    char b[MMB+1];	

    /* already waiting */
    if (waiting_key)
        return;

    /* build the message to warn the user */
    b[MMB] = 0;
    if (s == NULL)
        b[0] = 0;
    else {
        strncpy(b,s,MMB);
        if (f == 1)
            strncat(b," ",MMB-strlen(b));
    }
    if (f == 1) {
        if (m == 1)
            strncat(b,"(PRESS ANY KEY)",MMB-strlen(mb));
        else if (m == 2)
            strncat(b,"(ENTER to confirm, ESC to ignore)",MMB-strlen(mb));
        else if (m == 3)
            strncat(b,"(y/n)",MMB-strlen(mb));
    }

    /* display the message and enter wait mode */
    show_hint(2,b);
    waiting_key = m;

    /*
        In mode 4, the informed message is preserved because the editing
        service will need to redisplay it.
    */
    strncpy(inp_str,s,MMB);
    inp_str[MMB] = 0;
    inp_str_ps = strlen(inp_str);
}

/*

Step right the current window.

*/
void step_right(void)
{
    if (CDW == PAGE) {
        X0 += FS;
        check_dlimits(0);
        redraw_dw = 1;
    }
    else if (CDW == PAGE_FATBITS) {
        ++X0;
        check_dlimits(0);
        RG = 1;
        redraw_dw = 1;
    }
    else if (HTML) {
        X0 += DFW;
        check_dlimits(0);
        redraw_dw = 1;
    }
}

/*

Step left the current window.

*/
void step_left(void)
{
    if (CDW == PAGE) {
        X0 -= FS;
        check_dlimits(0);
        redraw_dw = 1;
    }
    else if (CDW == PAGE_FATBITS) {
        --X0;
        check_dlimits(0);
        RG = 1;
        redraw_dw = 1;
    }
    else if (HTML) {
        if (X0 > 0) {
            X0 -= DFW;
            if (X0 < 0)
                X0 = 0;
            redraw_dw = 1;
        }
    }
}

/*

Step up the current window.

*/
void step_up(void)
{
    if (CDW == PAGE) {
        Y0 -= FS;
        check_dlimits(0);
        redraw_dw = 1;
    }
    else if (CDW == PAGE_FATBITS) {
        --Y0;
        check_dlimits(0);
        RG = 1;
        redraw_dw = 1;
    }
    else if (HTML) {
        if (Y0 >= DFW) {
            Y0 -= DFW;
            redraw_dw = 1;
        }
    }
}

/*

Step down the current window.

*/
void step_down(void)
{
    if (CDW == PAGE) {
        Y0 += FS;
        check_dlimits(0);
        redraw_dw = 1;
    }
    else if (CDW == PAGE_FATBITS) {
        ++Y0;
        check_dlimits(0);
        RG = 1;
        redraw_dw = 1;
    }
    else if (HTML) {
        Y0 += DFW;
        check_dlimits(0);
        redraw_dw = 1;
    }
}

/*

Context-dependent "right" action.

*/
void right(void)
{

    if ((dw[PAGE_SYMBOL].v) || (dw[PATTERN].v) || (dw[PATTERN_TYPES].v)) {
        form_auto_submit();
    }

    /* select next menu of the menu bar */
    if (cmenu != NULL) {
        if (cmenu->a == 1) {
            if ((cmenu-CM >= TOP_CM) || ((++cmenu)->a!=1))
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
            if (*cm_e_od != ' ') {
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
            redraw_dw = 1;
        }
    }

    /* move the graphic cursor to the next symbol */
    else if (CDW == PAGE) {
        int l;

        if (curr_mc >= 0) {
            if (mc[curr_mc].E >= 0)
                curr_mc = mc[curr_mc].E;
            else if ((l=rsymb(curr_mc,1)) >= 0)
                curr_mc = l;
            check_dlimits(1);
            if (dw[PAGE_SYMBOL].v)
                dw[PAGE_SYMBOL].rg = 1;
            redraw_dw = 1;
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
        redraw_dw = 1;
    }

    /* scroll right PAGE_FATBITS window */
    else if (CDW == PAGE_FATBITS) {
        ++X0;
        check_dlimits(0);
        RG = 1;
        redraw_dw = 1;
    }

    /* next revision act */
    else if (CDW == TUNE_ACTS) {
        if (curr_act+1 <= topa) {
            ++curr_act;
            redraw_dw = 1;
            dw[TUNE_ACTS].rg = 1;
        }
    }

    /* change DEBUG attachment */
    else if (CDW == DEBUG) {

        if (*cm_g_log != ' ') {
            *cm_g_log = ' ';
            *cm_g_vocab = 'X';
        }
        else if (*cm_g_vocab != ' ') {
            *cm_g_log = 'X';
            *cm_g_vocab = ' ';
        }
        dw[DEBUG].rg = 1;
        redraw_dw = 1;
    }

    /* scroll right HTML documents */
    else if (HTML) {
        step_right();
    }
}

/*

Context-dependent "left" action.

*/
void left(void)
{

    if ((dw[PAGE_SYMBOL].v) || (dw[PATTERN].v) || (dw[PATTERN_TYPES].v)) {
        form_auto_submit();
    }

    /* select previous menu of the menu bar */
    if (cmenu != NULL) {
        int l;

        if (cmenu->a == 1) {
            l = cmenu-CM;
            for (l=cmenu-CM; (l > 0) && ((--cmenu)->a!=1); --l);
            if (l == 0)
                for (cmenu=CM+(l=TOP_CM); (l > 0) && ((--cmenu)->a!=1); --l);
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
            redraw_dw = 1;
        }
    }

    /* move the current symbol to the previous one */
    else if (CDW == PAGE) {
        int l;

        if (curr_mc >= 0) {
            if (mc[curr_mc].W >= 0)
                curr_mc = mc[curr_mc].W;
            else if ((l=lsymb(curr_mc,1)) >= 0)
                curr_mc = l;
            check_dlimits(1);
            if (dw[PAGE_SYMBOL].v)
                dw[PAGE_SYMBOL].rg = 1;
            redraw_dw = 1;
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
        redraw_dw = 1;
    }

    /* scroll right PAGE_FATBITS window */
    else if (CDW == PAGE_FATBITS) {
        if (X0 > 0) {
            --X0;
            RG = 1;
            redraw_dw = 1;
        }
    }

    /* previous revision act */
    else if (CDW == TUNE_ACTS) {
        if (curr_act > 0) {
            --curr_act;
            redraw_dw = 1;
            dw[TUNE_ACTS].rg = 1;
        }
    }

    /* change DEBUG attachment */
    else if (CDW == DEBUG) {

        if (*cm_g_log != ' ') {
            *cm_g_log = ' ';
            *cm_g_vocab = 'X';
        }
        else if (*cm_g_vocab != ' ') {
            *cm_g_log = 'X';
            *cm_g_vocab = ' ';
        }
        dw[DEBUG].rg = 1;
        redraw_dw = 1;
    }

    /* scroll left HTML documents */
    else if (HTML) {
        step_left();
    }
}

/*

Context-dependent "up" action.

*/
void up(void)
{

    /* select previous item of the active menu */
    if (cmenu != NULL) {
        if (cmenu->c > 0)
            --(cmenu->c);
        else
            cmenu->c = cmenu->n - 1;
        if (cmenu->c >= 0) {
            set_mclip(1);
            redraw_menu = 1;
        }
        else
            cmenu->c = -1;
    }

    /* move the graphic cursor to the symbol above the current one */
    else if (CDW == PAGE) {
        int l;

        if (curr_mc >= 0) {
            if (mc[curr_mc].N >= 0)
                curr_mc = mc[curr_mc].N;
            else if ((l=tsymb(curr_mc,1)) >= 0)
                curr_mc = l;
            check_dlimits(1);
            if (dw[PAGE_SYMBOL].v)
                dw[PAGE_SYMBOL].rg = 1;
            redraw_dw = 1;
        }
    }

    /* scroll down HTML documents by one line */
    else if (HTML) {
        if (Y0 > 0) {
            Y0 -= (DFH+VS);
            if (Y0 < 0)
                Y0 = 0;
            redraw_dw = 1;
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
void down(void)
{

    /* select next item of the active menu */
    if (cmenu != NULL) {
        if ((cmenu->c < 0) || (cmenu->c+1 >= cmenu->n))
            cmenu->c = 0;
        else
            ++(cmenu->c);
        if (cmenu->c < cmenu->n) {
            set_mclip(1);
            redraw_menu = 1;
        }
        else
            cmenu->c = -1;
    }

    /* move the graphic cursor to the symbol below the current one */
    else if (CDW == PAGE) {
        int l;

        if (curr_mc >= 0) {
            if (mc[curr_mc].S >= 0)
                curr_mc = mc[curr_mc].S;
            else if ((l=bsymb(curr_mc,1)) >= 0)
                curr_mc = l;
            check_dlimits(1);
            if (dw[PAGE_SYMBOL].v)
                dw[PAGE_SYMBOL].rg = 1;
            redraw_dw = 1;
        }
    }

    /* scroll down HTML documents by one line */
    else if (HTML) {
        Y0 += DFH+VS;
        check_dlimits(0);
        redraw_dw = 1;
    }

    /* scroll up PAGE_FATBITS window */
    else if (CDW == PAGE_FATBITS) {
        step_down();
    }
}

/*

Context-dependent "prior" (pgup) action.

*/
void prior(void)
{
    int d;

    if (CDW == PAGE) {
        Y0 -= VR;
        check_dlimits(0);
        redraw_dw = 1;
    }

    else if (HTML) {
        d = VR;
        d /= (DFH+VS);
        d *= (DFH+VS);
        Y0 -= d;
        check_dlimits(0);
        redraw_dw = 1;
    }

    else if (CDW == PAGE_FATBITS) {
        Y0 -= FS;
        check_dlimits(0);
        RG = 1;
        redraw_dw = 1;
    }
}

/*

Context-dependent "next" (pgdw) action.

*/
void next(void)
{
    int d;

    if (CDW == PAGE) {
        Y0 += VR;
        check_dlimits(0);
        redraw_dw = 1;
    }

    else if (HTML) {
        d = VR;
        d /= (DFH+VS);
        d *= (DFH+VS);
        Y0 += d;
        check_dlimits(0);
        redraw_dw = 1;
    }

    else if (CDW == PAGE_FATBITS) {
        Y0 += FS;
        check_dlimits(0);
        RG = 1;
        redraw_dw = 1;
    }
}

/*

Check menu item availability.

*/
int mcheck(int flags)
{
    /* requires one page loaded */
    if (flags & CMA_PAGE) {
        if (cpage < 0) {
            mhelp = "must load a page to activate this item";
            return(0);
        }
    }

    /* requires nonempty bookfont */
    if (flags & CMA_PATTERNS) {
        if (topp < 0) {
            mhelp = "no patterns available";
            return(0);
        }
    }

    /* requires OCR thread idle */
    if (flags & CMA_IDLE) {
        if (ocring) {
            mhelp = "BUSY";
            return(0);
        }
    }

    /* requires binarization */
    if (flags & CMA_CL) {
        if (!cl_ready) {
            mhelp = "this operation requires previous binarization";
            return(0);
        }
    }

    /* requires zone */
    if (flags & CMA_ZONE) {
        if (zones <= 0) {
            mhelp = "no zone defined";
            return(0);
        }
    }

    /* requires implementation */
    if (flags & CMA_ABSENT) {
        mhelp = "this feature is under implementation";
        return(0);
    }

    /* requires binarization */
    if (flags & CMA_NCL) {
        if (cl_ready) {
            mhelp = "this operation was performed already";
            return(0);
        }
    }

    return(1);
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
int cma(int it,int check)
{
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
        return(0);
    }

    /* compute availability */
    mhelp = cmenu->h[a];
    aval = mcheck(cmenu->f[a]);

    /* just checking and/or unavailable */
    if ((check) || (aval == 0)) {
        if (check == 2) {
            if (aval == 0)
                show_hint(0,"unavailable: %s",mhelp);
            else
                show_hint(0,mhelp);
        }
        return(aval);
    }

    /*
        Change selection on radio group.
    */
    if ((!check) && ((cmenu->t[a] & CM_TM) == CM_TR)) {
        char *s,t;
        int i;

        /* status of current item */
        s = cmenu->l + a*(MAX_MT+1)+1;
        t = *s;

        /* search group start */
        for (i=a; (i>=0) &&
                  (((cmenu->t)[a] & CM_TM) == CM_TR) &&
                  (((cmenu->t)[i] & CM_NG) == 0); --i);
        if (i < 0)
            ++i;

        /* deactivate all within group */
        do {
            cmenu->l[i*(MAX_MT+1)+1] = ' ';
            ++i;
        } while ((i<cmenu->n) &&
                 (((cmenu->t)[a] & CM_TM) == CM_TR) &&
                 (((cmenu->t)[i] & CM_NG) == 0));

        /* toggle current */
        if (t == ' ') {
            *s = 'X';
        }
        else {
            *s = ' ';
        }
    }

    /*
        Toggle binary item
    */
    else if ((!check) && ((cmenu->t[a] & CM_TM) == CM_TB)) {
        char *s;

        /* toggle */
        s = cmenu->l + a*(MAX_MT+1)+1;
        *s = (*s == ' ') ? 'X' : ' ';
        redraw_menu = 1;
    }

    /*
        Items on the "File" menu
    */
    if (cmenu-CM == CM_F) {

        /* Load page */
        if (it == CM_F_LOAD) {

            setview(PAGE_LIST);
            new_mclip = 0;
        }

        /* save session */
        else if (it == CM_F_SAVE) {

            /* force */
            sess_changed = 1;
            font_changed = 1;
            hist_changed = 1;
            form_changed = 1;

            start_ocr(-2,OCR_SAVE,0);
        }

        /* save zone */
        else if (it == CM_F_SZ) {

            if ((pixmap != NULL) && (cl_ready == 0))
                wrzone("zone.pgm",1);
            else
                wrzone("zone.pbm",1);
            new_mclip = 0;
        }

        /* save entire page replacing symbols by patterns */
        else if (it == CM_F_SR) {

            if (cl_ready)
                wrzone("tozip.pbm",0);
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
            start_ocr(-2,OCR_SAVE,0);
            new_mclip = 0;
        }
    }

    /*
        Items on "Edit" menu
    */
    else if (cmenu-CM == CM_E) {

        /* sort criteria */
        if ((it == CM_E_SM) ||
            (it == CM_E_SP) ||
            (it == CM_E_ST) ||
            (it == CM_E_SW) ||
            (it == CM_E_SH) ||
            (it == CM_E_SN)) {

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
                start_ocr(-2,OCR_OTHER,0);
            }
            new_mclip = 0;
        }

        /* search barcode */
        else if (it == CM_E_SEARCHB) {

            if (cl_ready) {
                if (!ocring) {
                    ocr_other = 2;
                    start_ocr(-2,OCR_OTHER,0);
                }
            }
            else if (cpage < 0)
                show_hint(2,"load a page before trying this feature");
            else
                show_hint(2,"perform segmentation before using this feature");
            new_mclip = 0;
        }

        else if (it == CM_E_THRESH) {

            if ((pixmap != NULL) && (cl_ready == 0) && (pm_t == 8)) {

                /* select the manual global binarizer */
                bin_method = 1;
                dw[TUNE].rg = 1;

                /* proceed */
                if (!ocring) {
                    ocr_other = 3;
                    start_ocr(-2,OCR_OTHER,0);
                }
                new_mclip = 0;
            }
            else if ((pixmap != NULL) && (pm_t == 8))
                show_hint(2,"cannot threshold after binarization");
            else
                show_hint(2,"no graymap loaded");
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
                 (it == CM_E_FILL_C) ||
                 (it == CM_E_PP_C)) {

        }
    }

    /*
       Items on "View" menu
    */
    else if (cmenu-CM == CM_V) {

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
            (it == CM_V_LARGE) ||
            (it == CM_V_DEF)) {

            /* set new font */
            set_xfont();

            /* recompute window size and components placement */
            comp_wnd_size(-1,-1);
            set_mclip(0);

            /* recompute the placement of the windows */
            setview(CDW);

            /* must regenerate HTML documents */
            {
                int i;

                for (i=0; i<=TOP_DW; ++i)
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

            for (i=0; i<=TOP_DW; ++i)
                if (dw[i].html)
                    dw[i].rg = 1;
            new_mclip = 0;
        }

        /* recompute form contents */
        else if (it == CM_V_WCLIP) {

            if (curr_mc >= 0) {
                dw[PAGE_SYMBOL].rg = 1;
                redraw_dw = 1;
            }
            if ((0<=cdfc) && (cdfc<=topp)) {
                dw[PATTERN].rg = 1;
                redraw_dw = 1;
            }
            new_mclip = 0;
        }

        /* toggle alphabet mapping display */
        else if (it == CM_V_MAP) {

            /* recompute window size and the placement of the components */
            comp_wnd_size(-1,-1);

            /* recompute the placement of the windows */
            setview(CDW);

            new_mclip = 0;
        }

        /* these statuses must redraw the window */
        else if  (it == CM_V_CC) {

            new_mclip = 0;
            redraw_dw = 1;
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
            show_hint(1,"Presentation unsupported!");
#endif
            new_mclip = 0;
        }

        /*
            Clear status line to display progress warnings.
        */
        else if ((it == CM_V_MAT) || (it == CM_V_CMP) ||
                 (it == CM_V_MAT_K) || (it == CM_V_CMP_K)) {

            show_hint(1,"");
        }
    }

    /*
        Items on "Options" menu
    */
    else if (cmenu-CM == CM_O) {

        /* toggle PAGE only */
        if (it == CM_O_PGO) {

            if ((0 <= cpage) && (cpage < npages)) {

                if ((pixmap != NULL) && (pm_t == 8) && (!cl_ready)) {
                    *cm_o_pgo = 'X';
                    show_hint(1,"PAGE only is mandatory for graymaps");
                }
                else {
                    if (*cm_o_pgo == ' ') {
                        opage_j1 = page_j1 = PT+TH+(PH-TH)/3;
                        opage_j2 = page_j2 = PT+TH+2*(PH-TH)/3;
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
                 (it == CM_O_DKEYS) ||
                 (it == CM_O_AMENU)) {

        }
    }

    /*
        Items on "DEBUG" menu
    */
    else if (cmenu-CM == CM_G) {

        if ((it == CM_G_ALIGN)  ||
            (it == CM_G_GLINES) ||
            (it == CM_G_LB)     ||
            (it == CM_G_ABAGAR) ||
            (it == CM_G_BO)     ||
            (it == CM_G_IO)     ||
            (it == CM_G_SUM)) {

            new_mclip = 0;
            redraw_dw = 1;
        }

        /* clear matches */
        else if (it == CM_G_CM) {

            clear_cm();
            dw[PATTERN_LIST].rg = 1;
            new_mclip = 0;
        }

        /* attach DEBUG window */
        else if ((it == CM_G_VOCAB) ||
                 (it == CM_G_LOG)) {

            dw[DEBUG].rg = 1;
            new_mclip = 0;
            redraw_dw = 1;
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
    else if (cmenu-CM == CM_D) {
        int i,j,x,y,t;

        {
            int last_dw;

            last_dw = CDW;
            CDW = PAGE;
            i = (CX0_orig-DM) / GS;
            j = (CY0_orig-DT) / GS;
            x = X0 + i*RF;
            y = Y0 + j*RF;
            t = symbol_at(x,y);
            CDW = last_dw;
        }

        /* use as pattern */
        if ((t>=0) && (it == CM_D_TS)) {

            new_mclip = 0;
            this_pattern = mc[t].bm;
        }

        /* OCR this symbol */
        else if ((t>=0) && (it == CM_D_OS)) {

            if (ocring == 0) {
                new_mclip = 0;
                justone = 1;
                curr_mc = t;
                start_ocr(-2,OCR_CLASS,1);
            }
        }

        /* make (CX0_orig,CY0_orig) the bottom left */
        else if (it == CM_D_HERE) {
	
            i = (CX0_orig-DM) / GS;
            j = (CY0_orig-DT) / GS;

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
                start_ocr(-2,OCR_REV,0);
                to_arg = t;
                to_rev = REV_MERGE;
            }

            new_mclip = 0;
        }

        /* create symbol link */
        else if (it == CM_D_SLINK) {

            if ((t >= 0) && (mc[t].sw<0)) {
                start_ocr(-2,OCR_REV,0);
                to_arg = t;
                to_rev = REV_SLINK;
            }

            new_mclip = 0;
        }

        /* disassemble symbol */
        else if (it == CM_D_DIS) {

            if ((curr_mc >= 0) && (t >= 0)) {
                start_ocr(-2,OCR_REV,0);
                to_rev = REV_DIS;
            }

            new_mclip = 0;
        }

        /* create accent link */
        else if (it == CM_D_ALINK) {

            if ((t >= 0) && (mc[t].sw<0)) {
                start_ocr(-2,OCR_REV,0);
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
            int a,b,r;

            if ((curr_mc >= 0) &&
                (mc[curr_mc].sw >= 0) &&
                ((a=word[mc[curr_mc].sw].tl) >= 0) &&
                (t >= 0) &&
                (mc[t].sw >= 0) &&
                ((b=word[mc[t].sw].tl) >= 0)) {

                if (a == b)
                    show_hint(2,"same lines!");

                else {
                    r = cmpln(a,b);
                    if (cmpln_diag == 0)
                        show_hint(2,"could not decide");
                    else if (cmpln_diag == 1)
                        show_hint(2,"the lines belong to different zones");
                    else if (cmpln_diag == 2)
                        show_hint(2,"decided by baseline analysis");
                    else if (cmpln_diag == 3)
                        show_hint(2,"vertical disjunction");
                    else if (cmpln_diag == 4)
                        show_hint(2,"extremities medium vertical position");
                    else if (cmpln_diag == 5)
                        show_hint(2,"extremities medium horizontal position");
                    else if (cmpln_diag == 6)
                        show_hint(2,"vertical disjunction");
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
                 (it == CM_D_WORDS) ||
                 (it == CM_D_PTYPE)) {

            new_mclip = 0;
            redraw_dw = 1;
        }

        /* report scale */
        else if (it == CM_D_RS) {

            new_mclip = 0;
            redraw_tab = 1;
        }

        /* display box instead of symbol */
        else if (it == CM_D_BB) {

            new_mclip = 0;
            redraw_tab = 1;
            if (dw[PAGE].v)
                redraw_dw = 1;
        }

    }

    /*
        Items on "PAGE_FATBITS options" menu
    */
    else if (cmenu-CM == CM_B) {
        int i,j,x,y,t;

        {
            int last_dw;

            last_dw = CDW;
            CDW = PAGE_FATBITS;
            i = (CX0_orig-DM) / GS;
            j = (CY0_orig-DT) / GS;
            x = X0 + i*RF;
            y = Y0 + j*RF;
            t = closure_at(x,y,0);
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
                X0 = i - HR/2;
                Y0 = j - VR/2;
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
                 (it == CM_B_HS) ||
                 (it == CM_B_HB)) {

            /* some modes force FSxFS */
            setview(CDW);

            /* must copy pattern to cfont again */
            p2cf();
            dw[PAGE_FATBITS].rg = 1;
            new_mclip = 0;
        }

        /* compute border path */
        else if (it == CM_B_BP) {

            if (t>=0) {
                closure_border_path(t);
            }

            new_mclip = 0;
        }

        /* compute straight lines on border using pixel distance */
        else if (it == CM_B_SLD) {
            short r[256];

            if (t>=0) {
                closure_border_slines(t,8,2,0.36,r,128,0);
            }

            new_mclip = 0;
        }

        /* compute straight lines on border using correlation */
        else if (it == CM_B_SLC) {
            short r[256];

            if (t>=0) {
                closure_border_slines(t,8,1,0.9,r,128,0);
            }

            new_mclip = 0;
        }

        /* apply isbar test */
        else if (it == CM_B_ISBAR) {

            if (t >= 0) {
                int r;
                float sk,bl;

                if ((r=isbar(t,&sk,&bl)) > 0) {
                    show_hint(2,"seems to be a bar, skew=%1.3f (rad), len=%3.1f mm",sk,bl);
                }
                else {
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
                    show_hint(2,"does not seem to be a bar (%s)",d);
                }
            }

            new_mclip = 0;
        }

        /* detect extremities */
        else if (it == CM_B_DX) {

            if (t >= 0) {
                int n;

                n = dx(t,-1,-1);
                show_hint(2,"%d extremities found",n);
            }

            new_mclip = 0;
        }
    }

    /*
        Items on "Pattern" menu
    */
    else if (cmenu-CM == CM_R) {
        int pages;

        pages = (*cm_o_all != ' ') ? -1 : -2;

        /* cannot start new operation */
        if (ocring) {
        }

        /* start preproc */
        else if (it == CM_R_PREPROC) {

            start_ocr(pages,OCR_PREPROC,0);
        }

        /* start detection of blocks */
        else if (it == CM_R_BLOCK) {

            start_ocr(pages,OCR_BLOCK,0);
        }

        /* start binarization and segmentation */
        else if (it == CM_R_SEG) {

            start_ocr(pages,OCR_SEG,0);
        }

        /* start consisting structures */
        else if (it == CM_R_CONS) {

            start_ocr(pages,OCR_CONS,0);
        }

        /* start preparing patterns */
        else if (it == CM_R_OPT) {

            start_ocr(pages,OCR_OPT,0);
        }

        /* start reading revision data */
        else if (it == CM_R_REV) {

            start_ocr(pages,OCR_REV,0);
        }

        /* start classification */
        else if (it == CM_R_CLASS) {

            start_ocr(pages,OCR_CLASS,0);
        }

        /* start geometric merging */
        else if (it == CM_R_GEOMERGE) {

            start_ocr(pages,OCR_GEOMERGE,0);
        }

        /* start */
        else if (it == CM_R_BUILD) {

            start_ocr(pages,OCR_BUILD,0);
        }

        /* start spelling */
        else if (it == CM_R_SPELL) {

            start_ocr(pages,OCR_SPELL,0);
        }

        /* start building words and lines */
        else if (it == CM_R_OUTP) {

            start_ocr(pages,OCR_OUTP,0);
        }

        /* start generating doubts */
        else if (it == CM_R_DOUBTS) {

            start_ocr(pages,OCR_DOUBTS,0);
        }

        new_mclip = 0;
    }

    /*
        Items on "Alphabets" menu
    */
    else if (cmenu-CM == CM_A) {

        set_bl_alpha();
        redraw_map = 1;
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
    }
    else {
        redraw_menu = 1;
    }

    return(1);
}

/*

Press key (for presentations).

*/
int press(char c,int *delay)
{
    xev.type = KeyPress;
    switch (c) {
        case 'r': xev.xkey.keycode = 27; break;
        case 'h': xev.xkey.keycode = 43; break;
    }
    have_s_ev = 1;
    *delay = 500000;
    return(0);
}

/*

Click menu item (for presentations).

*/
int mselect(int item,int *delay)
{
    int m,x,y=MH/2,px,py;
    XButtonEvent *be;

    m = item >> CM_IT_WD;
    x = ((CM[m].p*DFW) + ((CM[m].p+strlen(CM[m].tt))*DFW)) / 2;

    /* get pointer coordinates */
    get_pointer(&px,&py);

    /* go to menu */
    if (((cmenu == NULL) || ((cmenu-CM) != m)) &&
         ((px != x) || (py != y))) {

        XWarpPointer(xd,None,XW,0,0,0,0,x,y);
        *delay = 1000000;
        return(1);
    }

    /* open menu */
    else if (cmenu == NULL) {

        xev.type = ButtonPress;
        xev.xbutton.x = px;
        xev.xbutton.y = py;
        be = (XButtonEvent *) &xev;
        be->button = Button1;
        have_s_ev = 1;
        *delay = 500000;
        return(1);
    }

    /* no item selected */
    else if ((cmenu->c < 0) ||
             ((cmenu->c + ((cmenu-CM) << CM_IT_WD)) != item)) {

        int i;

        i = item & CM_IT_M;
        x = CX0 + CW - 10;
        y = i * (DFH + VS) + 5 + CY0 + DFH;
        XWarpPointer(xd,None,XW,0,0,0,0,x,y);
        *delay = 2000000;
        return(1);
    }

    /* click it */
    else {

        xev.type = ButtonPress;
        xev.xbutton.x = px;
        xev.xbutton.y = py;
        be = (XButtonEvent *) &xev;
        be->button = Button1;
        have_s_ev = 1;
        *delay = 100000;
        return(0);
    }
}

/*

Click symbol (for presentations).

*/
int sselect(int k,int *delay)
{
    int x,y,px,py;
    XButtonEvent *be;

    /* get pointer coordinates */
    get_pointer(&px,&py);

    /* go to tab */
    if (!dw[PAGE].v) {
        y = PT + TH/2;
        x = PM + 10 + (PAGE) * (20+TW) + 10 + TW/2;

        /* move pointer */
        if ((abs(px-x) > (TW/4)) || (abs(py-y) > TH/4))  {
            XWarpPointer(xd,None,XW,0,0,0,0,x,y);
            *delay = 1000000;
            return(1);
        }

        /* click the tab */
        else {
            xev.type = ButtonPress;
            xev.xbutton.x = px;
            xev.xbutton.y = py;
            be = (XButtonEvent *) &xev;
            be->button = Button1;
            have_s_ev = 1;
            *delay = 500000;
            return(1);
        }
    }

    /* make sure that symbol k is visible */
    else {
        int w,h;

        curr_mc = k;
        CDW = PAGE;
        x = DM + (((mc[k].l + mc[k].r) / 2) - X0) / RF;
        y = DT + (((mc[k].t + mc[k].b) / 2) - Y0) / RF;
        w = (mc[k].r - mc[k].l + 1) / RF;
        h = (mc[k].b - mc[k].t + 1) / RF;

        /* make the symbol visible */
        if ((mc[curr_mc].l < X0) ||
            (mc[curr_mc].r >= X0 + HR) ||
            (mc[curr_mc].b < Y0) ||
            (mc[curr_mc].t >= Y0 + VR)) {

            check_dlimits(1);
            force_redraw();
            *delay = 1000000;
            return(1);
        }

        /* move to symbol */
        else if ((abs(px-x) > w/4) || (abs(py-y) > w/4)) {

            XWarpPointer(xd,None,XW,0,0,0,0,x,y);
            *delay = 1000000;
            return(1);
        }

        /* click it */
        else {
            xev.type = ButtonPress;
            xev.xbutton.x = px;
            xev.xbutton.y = py;
            be = (XButtonEvent *) &xev;
            be->button = Button1;
            have_s_ev = 1;
            *delay = 500000;
            return(0);
        }
    }
}

/*

Click button (for presentations).

*/
int bselect(int k,int *delay)
{
    int x,y,px,py;
    XButtonEvent *be;

    /* get pointer coordinates */
    get_pointer(&px,&py);

    {
        int w,h;

        curr_mc = k;
        CDW = PAGE;
        x = 10 + BW/2;
        y = PT + k * (BH+VS) + BH/2;
        w = BW;
        h = BH;

        /* warp to button */
        if ((abs(px-x) > w/4) || (abs(py-y) > w/4)) {

            XWarpPointer(xd,None,XW,0,0,0,0,x,y);
            *delay = 1000000;
            return(1);
        }

        /* click it */
        else {
            xev.type = ButtonPress;
            xev.xbutton.x = px;
            xev.xbutton.y = py;
            be = (XButtonEvent *) &xev;
            be->button = Button1;
            have_s_ev = 1;
            *delay = 30000000;
            return(0);
        }
    }
}

/*

Load file (for presentations).

*/
int fselect(char *p,int *delay)
{
    int x,y,px,py;
    XButtonEvent *be;

    /* get pointer coordinates */
    get_pointer(&px,&py);

    /* go to tab */
    if (!dw[PAGE_LIST].v) {
        y = PT + TH/2;
        x = PM + 10 + (PAGE) * (20+TW) + 10 + TW/2;

        /* move pointer */
        if ((abs(px-x) > (TW/4)) || (abs(py-y) > TH/4))  {
            XWarpPointer(xd,None,XW,0,0,0,0,x,y);
            *delay = 1000000;
            return(1);
        }

        /* click the tab */
        else {
            xev.type = ButtonPress;
            xev.xbutton.x = px;
            xev.xbutton.y = py;
            be = (XButtonEvent *) &xev;
            be->button = Button1;
            have_s_ev = 1;
            *delay = 500000;
            return(1);
        }
    }

    /* make sure that file imre.pbm is visible */
    else {
        int i,w,h;

        for (i=0; (i<=topge) &&
                  ((ge[i].type != GE_TEXT) ||
                   (strcmp(p,ge[i].txt) != 0)); ++i);

        if (i > topge) {

            printf("nao achei %s\n",p);
            return(0);
        }

        curr_hti = i;

        x = DM + (((ge[i].l + ge[i].r) / 2) - X0) / RF;
        y = DT + (((ge[i].t + ge[i].b) / 2) - Y0) / RF;
        w = (ge[i].r - ge[i].l + 1) / RF;
        h = (ge[i].b - ge[i].t + 1) / RF;

        /* make the link visible */
        if ((ge[curr_hti].l < X0) ||
            (ge[curr_hti].r >= X0 + HR) ||
            (ge[curr_hti].b < Y0) ||
            (ge[curr_hti].t >= Y0 + VR)) {

            check_dlimits(1);
            force_redraw();
            *delay = 1000000;
            return(1);
        }

        /* move to link */
        else if ((abs(px-x) > w/4) || (abs(py-y) > w/4)) {

            XWarpPointer(xd,None,XW,0,0,0,0,x,y);
            *delay = 1000000;
            return(1);
        }

        /* click it */
        else {
            xev.type = ButtonPress;
            xev.xbutton.x = px;
            xev.xbutton.y = py;
            be = (XButtonEvent *) &xev;
            be->button = Button1;
            have_s_ev = 1;
            *delay = 5000000;
            return(0);
        }
    }
}

/*

Process keyboard events.

*/
void kactions(void)
{
    /* statuses */
    int ctrl,haveif;
    static int cx=0;

    /* other */
    int ls;
    edesc *c;
    int n;

    /* Buffers required by the KeyPress event */
    unsigned char buffer[101];
    KeySym ks;
    XComposeStatus cps;

    /*
        We're waiting any keyboard event.
    */
    /* any keyboard event unblocks the OCR */
    if (waiting_key == 1) {
        waiting_key = 0;
        show_hint(3,NULL);
        if (nopropag == 0)
            return;
    }

    n = XLookupString(&(xev.xkey),(char *)buffer,100,&ks,&cps);

    /*
        We're waiting ENTER or ESC keypresses. Discard anything else.
    */
    if (waiting_key == 2) {
        if ((ks == 0xff0d) || (ks == 0xff1b)) {
            waiting_key = 0;
            mb[0] = 0;
            redraw_stline = 1;
            key_pressed = (ks == 0xff0d);
        }
        return;
    }

    /*
        We're waiting y, Y, n or N keypresses. Discard anything else.
    */
    /* Discard events other than y, Y, n or N */
    if (waiting_key == 3) {
        if ((ks == 'y') || (ks == 'Y') || (ks == 'n') || (ks == 'N')) {
            waiting_key = 0;
            mb[0] = 0;
            redraw_stline = 1;
            key_pressed = ((ks == 'y') || (ks == 'Y'));
        }
        return;
    }

    /*
        We're waiting <, >, +, -, ENTER or ESC keypresses. Discard anything else.
    */
    if (waiting_key == 5) {
        if ((ks == '>') || (ks == '<') || (ks == '+') || (ks == '-')) {
            waiting_key = 0;
            mb[0] = 0;
            redraw_stline = 1;
            key_pressed = ks;
        }
        else if ((ks == 0xff0d) || (ks == 0xff1b)) {
            waiting_key = 0;
            mb[0] = 0;
            redraw_stline = 1;
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
                }
                else if (pixel_mlist(curr_mc) < 0) {
                    warn("symbol too large");
                }
                else {
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
                    start_ocr(-2,OCR_OTHER,0);
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
    if ((0 <= curr_hti) && (curr_hti <= topge) && (c->type == GE_INPUT)) {
        haveif = 1;

        /* ignore ESC prefix */
        if (nopropag) {
            nopropag = 0;
            mb[0] = 0;
            redraw_stline = 1;
        }
    }
    else if (waiting_key == 4)
        haveif = 2;
    else
        haveif = 0;

    /* Control-L */
    if ((n == 1) && (buffer[0] == 12))
        force_redraw();

    /* Backspace within text input field */
    else if ((n == 1) && (haveif) && (ks == 0xffff)) {

        if (haveif == 1) {
            if ((ls=strlen(c->txt)) > 0)
                (c->txt)[ls-1] = 0;

            redraw_inp = 1;
            form_changed = 1;
        }
        else {
            if ((ls=strlen(inp_str)) > inp_str_ps)
                (inp_str)[ls-1] = 0;

            redraw_inp_str();
        }
    }

    /* Visible char */
    else if ((n == 1) && (' ' <= buffer[0])) {
        static int l=-1;
        int v;

        /* apply composition */
        if (*cm_o_dkeys == 'X')
            v = compose(&l,buffer[0]);
        else {
            v = 1;
            l = buffer[0];
        }

        /* add to HTML input field */
        if (haveif == 1) {
            ls = strlen(c->txt) + 1;
            if (v) {
                if ((ls >= c->tsz) && (c->tsz < MFTL+1))
                    c->txt = c_realloc(c->txt,c->tsz=MFTL+1,NULL);
                if (ls < c->tsz) {
                    (c->txt)[ls-1] = l;
                    (c->txt)[ls] = 0;
                    l = -1;
                }
            }
            redraw_inp = 1;
            form_changed = 1;
        }

        /* add to HTML input field */
        else if (haveif == 2) {
            ls = strlen(inp_str) + 1;
            if (v) {
                if (ls < MMB) {
                    (inp_str)[ls-1] = l;
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
            start_ocr(-2,OCR_REV,0);
            to_tr[0] = l;
            to_tr[1] = 0;
            to_rev = REV_TR;
            l = -1;
        }

        /* add pattern from the spyhole buffer */
        else if (v && dw[PAGE].v && (spyhole(0,0,2,0)==1)) {

            cdfc = -1;
            start_ocr(-2,OCR_REV,0);
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
                start_ocr(-2,OCR_REV,0);
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
        else if (v && ((l=='l') || (l=='L')) && (CDW == WELCOME)) {
            setview(GPL);
            redraw_dw = 1;
            l = -1;
        }
    }

    /* Special keys */
    else switch (ks) {

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
                cma(cmenu->c + ((cmenu-CM) << CM_IT_WD),0);
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
                memmove(inp_str,inp_str+inp_str_ps,strlen(inp_str)-inp_str_ps+1);
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
                redraw_button = bbad;
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
            db("unattended keycode %ld",ks);
            break;
    }
}

/*

Select the window where the mouse pointer is.

*/
void select_dw(int x,int y)
{
    int a,last_dw;

    get_pointer(&x,&y);

    /* loop all active windows */
    last_dw = CDW;

    if (*cm_v_hide == ' ')
        a = 10+RW;
    else
        a = 0;

    for (CDW=0; CDW<=TOP_DW; ++CDW) {
        if ((dw[CDW].v) &&
            (DM-10 <= x) && (x < DM+DW+(DS?a:10)) &&
            (DT-10 <= y) && (y < DT+DH+(DS?a:10))) {

            if (CDW != last_dw) {
                redraw_tab = 1;
            }
            return;
        }
    }

    /* no one was selected */
    CDW = last_dw;
}

/*

Detect the item from the menu bar currently under the pointer.

*/
int mb_item(int x,int y)
{
    if ((0 < y) && (y < MH)) {
        int i;

        for (i=0; i<=TOP_CM; ++i) {
            if ((CM[i].a == 1) &&
                (CM[i].p * DFW <= x) &&
                (x <= (CM[i].p+strlen(CM[i].tt)) * DFW)) {
                    return(i);
            }
        }
        return(-2);
    }
    return(-1);
}

/*

This is mostly a trigger to execute actions when a menu is
opened. Anyway, it also initializes some variables.

*/
void open_menu(int x,int y,int m)
{
    CX0 = x;
    CY0 = y;
    if (cmenu == NULL)
        form_auto_submit();
    cmenu = CM + m;
    redraw_menu = 1;
    show_hint(3,NULL);
}

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
int spyhole(int x,int y,int op,int et)
{
    /*
        Bitmap buffers: bb is the main buffer. For each threshold,
        the binarization result is computed using bb, as well as
        the result of the spanning of the component from the seed
        (x,y). bc stores the bitmap got from the preferred
        threshold until now. bd and be are used to store, in some
        cases, the bitmap before the spanning of the component,
        in order to detect merging.
    */
    unsigned char *bb,*bc,*bd,*be;
    int bpl,bb_sz,snp;

    /*
        When spyhole() finds a usable threshold, it stores
        the threshold result in this buffer, so it can
        be recovered afterwards using op==2.
    */
    static unsigned bs[BMS/4];
    static int bsf = 0,bs_x0,bs_y0;

    /* the spyhole static buffer */
    static unsigned char *eg=NULL;
    static int sh_sz,last_x0=-1,last_y0,last_dx,last_dy = 0;

    /* the spyhole rectangle */
    int x0,y0,dx,dy,sh_hs;

    /* current, best and maximum tried thresholds */
    int thresh,bt,mt;

    /* first usable threshold */
    int ft;

    /* best class found */
    int bcp;

    /* the seed */
    int r,u,v,u0,v0,nhs;

    /* other */
    int j,k,l,ret;
    unsigned char *p,*q;

    /* white and black threshold limits */
    int WL,BL;

    /* threshold search control */
    int L,R,st,tries;

    /* per-threshold width, height, classification result and seed */
    int W[257],H[257],K[257],SX[257],SY[257];

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
            return(0);

        /* copy bs to cb */
        bm2cb((unsigned char *)bs,0,0);
        return(1);
    }

    /*
        Copy the binarized symbol to cmp_bmp and returns.
    */
    if (op == 3) {

        /* no available symbol */
        if (bsf == 0)
            return(0);

        /* copy bs to cmp_bmp */
        memcpy(cmp_bmp,bs,BMS);
        cmp_bmp_x0 = bs_x0;
        cmp_bmp_y0 = bs_y0;
        return(1);
    }

    /*
        give back the last spyhole (if any) to the pixmap and
        invalidate the buffer.
    */
    if (last_x0 >= 0) {

        q = eg;
        for (j=last_y0, l=last_dy; l>0; --l, ++j) {
            p = pixmap + j*XRES + last_x0;
            for (k=last_dx; k>0; --k, ++p, ++q) {
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
        if (*cm_g_lb != ' ') {
            if (CDW != PAGE)
                setview(PAGE);
            redraw_dw = 1;
        }

        return(0);
    }

    /* try to extend to a large enough spyhole rectangle */
    {
        if ((x0=x-sh_hs) < 0)
            x0 = 0;
        dx = x - x0;
        if (x+sh_hs <= XRES)
            dx += sh_hs;
        else
            dx += (XRES-x);
    
        if ((y0=y-sh_hs) < 0)
            y0 = 0;
        dy = y - y0;
        if (y+sh_hs <= YRES)
            dy += sh_hs;
        else
            dy += (YRES-y);
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
    if ((u0 < nhs) || ((x+nhs) >= (x0+dx)) || (v0 < nhs) || ((y+nhs) >= (y0+dy))) {
        return(0);
    }

    /* think 10% as a minimum, 90% as a maximum */
    if (et > (230-TS))
        et = 230-TS;
    if (et < (26+TS))
        et = 26+TS;

    /* round the expected threshold */
    if (((et % TS) * 2) > TS)
        et = (et-(et%TS)) + TS;
    else
        et = (et-(et%TS));

    /* alloc bitmap buffers */
    bpl = (dx / 8) + ((dx%8) != 0);
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
    for (j=0; j<256; ++j)
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
    for (bsf=0, st=1, mt=-1, thresh=et, tries=0; st <= 4; ) {

        if (thresh > mt)
            mt = thresh;

        /* apply the binarizer line by line */
        memset(bb,0,bb_sz);
        for (j=y0, l=dy; l>0; --l, ++j) {
            int m;

            q = bb + (j-y0)*bpl;
            m = 128;
            p = pixmap + j*XRES + x0;

            for (k=dx; k>0; --k, ++p) {
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
        }
        else {
            u = u0;
            v = v0;
            r = 0;
        }

        /* try to search an interior pixel close to (x,y) */
        for (k=l=0; (l<nhs) && (r==0); ) {

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
                r = pix(bb,bpl,u,v);
            }

            /*
                In non-relaxed mode we apply the same definition
                of interior pixel used by border_path().
            */
            else {
                /* non-optimized case (25%) */
                if (((u-1)%8) >= 6) {
                    r = pix(bb,bpl,u+1,v) &&
                        pix(bb,bpl,u-1,v) &&
                        pix(bb,bpl,u-1,v-1) &&
                        pix(bb,bpl,u,v-1) &&
                        pix(bb,bpl,u+1,v-1) &&
                        pix(bb,bpl,u-1,v+1) &&
                        pix(bb,bpl,u,v+1) &&
                        pix(bb,bpl,u+1,v+1);
                }

                /* 3-bit optimization (75%) */
                else {
                    r = pix(bb,bpl,u+1,v) &&
                        pix(bb,bpl,u-1,v) &&
                        pix3(bb,bpl,u-1,v-1) &&
                        pix3(bb,bpl,u-1,v+1);
                }
            }

            /* no, (u,v) isn't interior */
            if (r == 0) {

                /* the path finished */
                if (--k < 0) {

                    /* radius and path size */
                    ++l;
                    k = 8*l-1;
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
                    if (k > 6*l-1) {
                        u = (u0-l) + ((8*l-1)-k);
                        v = v0-l;
                    }

                    /* right line: linear positions [(6*l-1)..(4*l-1)[ */
                    else if (k > 4*l-1) {
                        u = u0+l;
                        v = (v0-l) + ((6*l-1)-k);
                    }

                    /* bottom line: linear positions [(4*l-1)..(2*l-1)[ */
                    else if (k > 2*l-1) {
                        u = (u0+l) - ((4*l-1)-k);
                        v = v0+l;
                    }

                    /* left line: linear positions [(2*l-1)..(0*l-1)[ */
                    else {
                        u = u0-l;
                        v = (v0+l) - ((2*l-1)-k);
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
            for (j=thresh; (j<=256) && (SX[j] < 0); ++j) {
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
                    memcpy(bd,bb,bb_sz);

                /*
                    copy to an auxiliar buffer, because if the
                    classification succeeds, we'll swap bd and be.
                */
                else
                    memcpy(be,bb,bb_sz);
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
        else if ((r = border_path(bb,dx,dy,NULL,0,u,v,1)) == 0) {

            /*
                This threshold was already analysed.
            */
            if (W[thresh] >= 0) {
            }

            /*
                Copy to cmp_bmp. The service justify_bitmap() returns
                true only if the component is not too large.
            */
            else if (justify_bitmap(bb,dx,dy)) {

                bsf = 1;

                /* remember the symbol dimensions */
                W[thresh] = cmp_bmp_w;
                H[thresh] = cmp_bmp_h;

                /*
                    No successfull classification was achieved until now.
                */
                if (bt < 0) {

                    /* try to classify it */
                    classify(-1,NULL,0);
                    while (classify(0,bmpcmp_skel,3));

                    /* success! */
                    if (cp_result >= 0) {

                        /* assign class to threshold */
                        K[thresh] = bcp = cp_result;

                        /* take threshold as the best one */
                        bt = thresh;

                        /* remember the bitmap in full and reduced sizes */
                        if (op == 0)
                            memcpy(bc,bb,bb_sz);
                        memcpy(bs,cmp_bmp,BMS);

                        /* absolute top left coordinates */
                        bs_x0 = x0 + cmp_bmp_dx;
                        bs_y0 = y0 + cmp_bmp_dy;

                        /*
                            swap bd and be because the pre-spanning bitmap
                            is being pointed by be, not bd.
                        */
                        if (ft >= 0) {
                            unsigned char *a;

                            a  = bd;
                            bd = be;
                            be = a;
                        }

                        /* compute the number of spare pixels */
                        for (p=bd,q=bb, j=snp=0; j<bb_sz; ++j, ++p, ++q)
                            snp += np8[*p ^ ((*p) & (*q))];
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
                            memcpy(bc,bb,bb_sz);
                        memcpy(bs,cmp_bmp,BMS);

                        /* absolute top left coordinates */
                        bs_x0 = x0 + cmp_bmp_dx;
                        bs_y0 = y0 + cmp_bmp_dy;

                        /* compute the number of spare pixels */
                        for (p=bd,q=bb, j=snp=0; j<bb_sz; ++j, ++p, ++q)
                            snp += np8[*p ^ ((*p) & (*q))];
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
            printf("W[%d]=%d, H[%d]=%d\n",thresh,W[thresh],thresh,H[thresh]);
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
                int d,f,t,n;

                /* compute the number of spare pixels */
                for (p=bd,q=bb, j=n=0; j<bb_sz; ++j, ++p, ++q)
                    n += np8[*p ^ ((*p) & (*q))];

                /* overpassed left merge limit */
                if ((snp >= 0) && (thresh < et) && (n > (snp+25))) {
                    L = thresh + TS;
                }

                /*
                    overpassed right merge limit
                */
                else if ((snp >= 0) && (thresh > et) && (n < (snp-25))) {
                    R = thresh - TS;
                    memcpy(be,bb,bb_sz);
                }

                /* achieved black limit */
                if ((d=swh(W[thresh],H[thresh])) > 0) {
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
                else for (f=t=0; f==0; ) {

                    /* compute new threshold value */
                    ++tries;
                    if (tries&1)
                        thresh = et + ((tries+1)/2)*TS;
                    else
                        thresh = et - (tries/2)*TS;

                    /* the new threshold value is outside the valid ranges */
                    if ((thresh <= 0) ||
                        (thresh >= 256) ||
                        ((L >= 0) && (thresh < L)) ||
                        ((R >= 0) && (thresh > R)) ||
                        ((WL >= 0) && (thresh <= WL)) ||
                        ((BL >= 0) && (thresh >= BL))) {

                        /* two consecutive failures: give up */
                        if (++t >= 2) {

                            /* no successfull threshold: abandon completely */
                            if ((ft < 0) && (bt < 0))
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
                    thresh = (L+R)/2;
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
                for (p=bd,q=bb, j=n=0; j<bb_sz; ++j, ++p, ++q)
                    n += np8[*p ^ ((*p) & (*q))];

                /* shrink interval */
                if (n < (snp-25)) {

                   /* ((W[thresh] > (W[L]+5)) ||
                    (H[thresh] > (H[L]+5))) { */

                    R = thresh;
                    memcpy(be,bb,bb_sz);
                }
                else {
                    L = thresh;
                }

                /* achieved black limit */
                if (swh(W[thresh],H[thresh]) > 0) {
                    BL = thresh;
                }

                /* prepare next try or next step */
                if ((L+1) >= R) {
                    L = R;
                    st = 4;
                }
                else {
                    thresh = (L+R)/2;
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
        printf("bt = %d ft = %d\n",bt,ft);
    }

    /* spyhole buffer */
    if (op == 0) {

        /* copy the symbol to the spyhole buffer */
        if (dx*dy > sh_sz) {
            eg = c_realloc(eg,sh_sz=dx*dy,NULL);
        }
        q = eg;
        for (j=y0, l=dy; l>0; --l, ++j) {
            p = pixmap + j*XRES + x0;
            for (k=dx; k>0; --k, ++p, ++q) {
                *q = *p;
            }
        }

        /*
            If no threshold is found usable, then let a white
            rectangle be the "best" threshold result.
        */
        if (ret == 0) {
            memset(bc,0,bb_sz);
            memset(be,0,bb_sz);
        }

        /* copy the best threshold result to the pixmap */
        for (j=y0, l=dy; l>0; --l, ++j) {
            int m;
            unsigned char *o;

            q = bc + (j-y0)*bpl;
            o = be + (j-y0)*bpl;
            m = 128;
            p = pixmap + j*XRES + x0;

            for (k=dx; k>0; --k, ++p) {

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
            int i,l;
            char *t,a[MMB+1];

            i = id2idx(bcp);
            snprintf(a,MMB,"threshold %d got successfull classification ",bt);
            l = strlen(a);
            if ((t=pattern[i].tr) != NULL)
                snprintf(a+l,MMB,"\"%s\" (next %d)",t,L);
            else
                snprintf(a+l,MMB,"(next %d)",L);
            a[MMB] = 0;
            show_hint(2,a);
        }
        else if (ft >= 0)
            show_hint(2,"using threshold %d (next %d)",ft,L);
        else
            show_hint(2,"could not find a useful threshold");

        /* must refresh the display */
        redraw_dw = 1;
    }

    /* accumulate tries */
    sh_tries += tries;

    /* operations 0 and 4 result */
    return(ret);
}

/*

Mouse actions activated by mouse button 1.

*/
int mactions_b1(int help,int x,int y)
{
    int i,j,c,sb;
    int pb,ob;

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

            return(1);
        }

        /* process current item */
        if ((0 <= cmenu->c) && (cmenu->c < cmenu->n)) {

            cma(cmenu->c + ((cmenu-CM) << CM_IT_WD),0);
            return(0);
        }

        /* dismiss context menu */
        else {
            dismiss(1);
        }
    }

    /* change to selected window */
    select_dw(x,y);

    /*

        Menu bar
        --------

    */
    if ((i=mb_item(x,y)) >= 0) {

        if (help) {

            /* select automagically */
            if ((*cm_o_amenu == 'X') && (cmenu == NULL)) {

                open_menu(x,y,i);
                return(1);
            }

            return(1);
        }

        open_menu(x,y,i);
        return(0);
    }

    /*

        Selection of tabs
        -----------------

    */
    if ((PM+10 <= x) &&
        (x < PM+10+(TABN-1)*(TW+20)+TW) &&
        (PT <= y) && (y <= PT+TH) &&
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
                        show_hint(0,"click to circulate modes");
                    else
                        show_hint(0,"select a file to load");
                else
                    show_hint(0,"click to enter page modes");
                return(1);
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
                    show_hint(0,"click to circulate modes");
                else
                    show_hint(0,"click to browse and edit the patterns");
                return(1);
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
            }
            else if (dw[PATTERN].v)
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
                    show_hint(0,"click to tune the OCR");
                return(1);
            }

            if (tab != TUNE) {

                setview(TUNE_CM);
            }
            else if (dw[TUNE].v) {

                setview(TUNE_SKEL);
            }
            else if (dw[TUNE_SKEL].v) {

                setview(TUNE_ACTS);
            }
            else
                setview(TUNE);
        }
    }

    /*

        Alphabet map
        ------------

    */
    else if ((*cm_v_map != ' ') &&
             (alpha != NULL) &&
             (PM-50 <= x) && (x < PM-50+alpha_cols*12+alpha_cols-1) &&
             (PT+1 <= y) && (y < PT+1+alpha_sz*15+alpha_sz-1)) {

        /* map column */
        i = (x-PM+50) / 13;

        /* map row */
        j = (y-PT-1) / 16;

        if (help) {
            char lb[MMB+1];

            lb[0] = lb[MMB] = 0;
            if ((0 <= j) && (j < alpha_sz)) {

                /* digits */
                if ((i == 0) && (alpha == number))
                    snprintf(lb,MMB,"this is the digit \"%s\"",alpha[j].ln);

                /* first column is the capital case */
                else if ((i == 0) && (alpha_cols >= 2) && (alpha[j].cc > 0))
                    snprintf(lb,MMB,"this is the letter \"%s\", capital",alpha[j].ln);

                /* second column is the small case */
                else if ((i == 1) && (alpha_cols >= 2) && (alpha[j].sc > 0))
                    snprintf(lb,MMB,"this is the letter \"%s\", small",alpha[j].ln);

                /* first column for alphabets without small case */
                else if ((i == 0) && (alpha_cols < 2) && (alpha[j].cc > 0))
                    snprintf(lb,MMB,"this is the letter \"%s\"",alpha[j].ln);

                /* third column is the transliteration */
                else if ((i == 2) && (alpha_cols >= 3))
                    snprintf(lb,MMB,"this is the latin keyboard map for letter \"%s\"",alpha[j].ln);
            }
            show_hint(0,lb);
            return(1);
        }
    }

    /*

        window
        ------

    */
    else if ((DM <= x) && (x < DM+DW) &&
             (DT <= y) && (y < DT+DH)) {

        i = (x-DM) / GS;
        j = (y-DT) / GS;

#ifdef FUN_CODES
        /*
            Toggle fun code 1.
        */
        if (CDW == WELCOME) {

            if (help) {
                return(1);
            }

            if (fun_code == 0) {
                fun_code = 1;
                redraw_dw = 1;
                new_alrm(50000,1);
            }
            else if (fun_code == 1) {
                fun_code = 0;
                redraw_dw = 1;
            }

            return(0);
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
            int k,s,p;
            char lb[MMB+1];

            if (help) {
                show_hint(0,"");
                return(1);
            }

            p = 8 * zones;
            if ((0 <= curr_corner) && (curr_corner < 2)) {
                limits[p+4*curr_corner] = X0 + i * RF;
                limits[p+4*curr_corner+1] = Y0 + j * RF;
                if (++curr_corner > 1) {

                    limits[p+2] = limits[p+0];
                    limits[p+3] = limits[p+5];
                    limits[p+6] = limits[p+4];
                    limits[p+7] = limits[p+1];

                    button[bzone] = 0;
                    ++zones;
                    sess_changed = 1;
                    redraw_zone = 1;
                }
                lb[MMB] = 0;
                s = snprintf(lb,MMB,"limits:");
                for (k=0; k<4; ++k) {
                    s += snprintf(lb+s,MMB," (%d,%d)",
                                  limits[p+2*k],limits[p+2*k+1]);
                }
                show_hint(1,lb);
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
                int k,l,u,v,p;
                cldesc *c;
                float a;

                /* no closure by here: display nothing */
                if ((k=closure_at(X0+i,Y0+j,0)) < 0)
                    return(1);

                c = cl + k;

                /* make (i,j) closure-relative */
                u = X0+i-c->l;
                v = Y0+j-c->t;

                /* (i,j) is outside the closure boundaries */
                if ((u<0) || (v<0) || (u>c->r-c->l) || (v>c->b-c->t))
                    return(1);

#ifdef MEMCHECK	
                checkidx(topfp,FS*FS,"s_dist");
#endif		

                /* search pixel on the flea path */
                for (l=0, p=-1; (l<=topfp) && (p<0); ++l) {
                    if ((fp[2*l]==u) && (fp[2*l+1]==v))
                        p = l;
                }

                /* display path data at (i,j) */
                if (p>=0) {
                    a = s2a(fsl[3*p],fsl[3*p+1],fsl[3*p+2],NULL,-1,0);
                    a *= (180/PI);
                    show_hint(0,"at (%d,%d), pp=%3.3f, a=%0.3f (deg)",u,v,fpp[p],a);
                }

                /* display the extremities heuristic results */
                /*
                else
                    dx(k,u,v);
		*/

                /* display the pixel coordinates */
                else {
                    show_hint(0,"at (%d,%d)",u,v);
                }

                return(1);
            }
        }

        /* paint pixel */
        else if ((*cm_e_pp != ' ') && (CDW == PATTERN)) {

            if (help) {
                show_hint(0,"paint pixel");
                return(1);
            }

            COLOR = BLACK;
            pp(i,j);
            redraw_dw = 1;
        }

        /* clear pixel */
        else if ((*cm_e_pp_c != ' ') && (CDW == PATTERN)) {

            if (help) {
                show_hint(0,"clear pixel");
                return(1);
            }

            COLOR = WHITE;
            pp(i,j);
            redraw_dw = 1;
        }

        /* fill region with current color */
        else if ((*cm_e_fill != ' ') && (CDW == PATTERN)) {

            if (help) {
                show_hint(0,"paint region");
                return(1);
            }

            COLOR = BLACK;
            c = cfont[i+j*FS];
            pp(i,j);
            if (c != cfont[i+j*FS]) {
                cfont[i+j*FS] = c;
                pr(i,j,c);
            }
            redraw_dw = 1;
        }

        /* clear region with current color */
        else if ((*cm_e_fill_c != ' ') && (CDW == PATTERN)) {

            if (help) {
                show_hint(0,"clear region");
                return(1);
            }

            COLOR = WHITE;
            c = cfont[i+j*FS];
            pp(i,j);
            if (c != cfont[i+j*FS]) {
                cfont[i+j*FS] = c;
                pr(i,j,c);
            }
            redraw_dw = 1;
        }

        /* go to hyperref */
        else if (HTML) {

            /* no help for this item */
            if (help) {

                /* show_hint(0,""); */
                return(1);
            }

            show_hint(1,"");
            html_action(x,y);
        }

        /* select symbol */
        else if (CDW == PAGE) {
            int k;

            /*
                when OCRing, some data structures may not be ready
                to the code below.
            */
            if (ocring)
                return(0);

            /* get current symbol (if any) */
            k = symbol_at(X0+i*RF,Y0+j*RF);

            if (help) {
                sdesc *m;
                char *a;
                char lb[MMB+1];

                lb[0] = 0;
                mb[MMB] = 0;
                if (k >= 0) {
                    m = mc + k;
                    a = ((m->tr) == NULL) ? "?" : ((m->tr)->t);
                }
                else {
                    m = NULL;
                    a = NULL;
                }

                /*
                    Display current word.
                */
                if (*cm_d_words != ' ') {

                    /* no current word */
                    if ((k < 0) || (m->sw < 0))
                        return(0);
                    snprintf(lb,MMB,"word %d%s",m->sw,
                              (word[m->sw].f & F_ITALIC) ? " italic" : "");
                }

                /*
                    Display pixel coordinates instead of symbol id.
                */
                else if (*cm_d_pixels != ' ') {
                    int x,y;

                    x = X0 + i*RF;
                    y = Y0 + j*RF;
                    if ((cl_ready) || (pm_t != 8))
                        show_hint(0,"at (%d,%d)",x,y);
                    else
                        show_hint(0,"at (%d,%d), color %d",x,y,pixmap[y*XRES+x]);
                    return(0);
	        }

                /*
                    Display absent symbols on type 0 to
                    help building the bookfont (experimental).
                */
                else if (*cm_d_ptype != ' ') {
                    int i,j;
                    unsigned char c[256];
                    pdesc *p;

                    for (i=0; i<=255; ++i) {
                        c[i] = 0;
                    }

                    for (i=0; i<=topp; ++i) {
                        p = pattern + i;
                        if ((p->pt == 0) &&
                            (p->tr != NULL) &&
                            (strlen(p->tr) == 1)) {

                            ++(c[((unsigned char *)(p->tr))[0]]);
                        }
                    }

                    j = 0;

                    for (i='a'; (j<MMB) && (i<='z'); ++i) {
                        if (c[i] < 5)
                            lb[j++] = i;
                    }
                    for (i='A'; (j<MMB) && (i<='Z'); ++i) {
                        if (c[i] < 5)
                            lb[j++] = i;
                    }
                    for (i='0'; (j<MMB) && (i<='9'); ++i) {
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
                        return(0);

                    if ((mc[k].r-mc[k].l+1>FS) || (mc[k].b-mc[k].t+1>FS))
                        snprintf(lb,MMB,"symbol %d (too large): \"%s\" (word %d)",k,a,m->sw);
                    else
                        snprintf(lb,MMB,"symbol %d: \"%s\" (word %d)",k,a,m->sw);
                }
                show_hint(0,lb);
                return(1);
            }

            show_hint(1,"");

            /* non-binarized image: apply spyhole */
            if (cl_ready == 0) {
                int x,y;

                {

                    /* absolute coordinates */
                    x = X0 + i*RF;
                    y = Y0 + j*RF;

                    /* abagar debug mode */
                    if ((zones>0) && (*cm_g_abagar != ' ')) {

                        /* "abagar" n bug */
                        x = (limits[0]+limits[6]) / 2;
                        y = (limits[1]+limits[3]) / 2;

                        printf("remark: the seed was changed to (%d,%d)\n",x,y);
                    }

                    spyhole(x,y,0,thresh_val);
                }
            }

            /* binarized image: select symbol using the graphic cursor */
            else if ((curr_mc = k) >= 0) {
                dw[PAGE_SYMBOL].rg = 1;
                symb2buttons(curr_mc);
                redraw_dw = 1;
            }
        }
    }

    /* Application Buttons */
    else if ((10 <= x) && (x <= (10+BW)) &&
             (PT <= y) && (y <= PT + BUTT_PER_COL*(BH+VS))) {

        i = (y-PT) / (BH+VS);
        if ((y - PT - (i*(BH+VS))) <= BH) {

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
                    show_hint(0,"use mouse button 1 to enlarge, 3 to reduce, 2 to max/med/min");
                    return(1);
                }

                if ((dw[PAGE].v) &&
                    ((CDW != PAGE_SYMBOL) || (*cm_v_wclip==' '))) {

                    CDW = PAGE;
                    if (RF > 1) {
                        --RF;
                        setview(PAGE);
                        check_dlimits(1);
                    }
                }

                else if ((CDW == PAGE_FATBITS) || (dw[TUNE_PATTERN].v)) {
                    if (ZPS < 7) {
                        ZPS += 2;
                        comp_wnd_size(-1,-1);
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
                        redraw_dw = 1;
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
                redraw_dw = 1;
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
                redraw_dw = 1;
            }
            */

            /* (book)

                OCR - start a full OCR run on the current page or on
                all pages, depending on the state of the "Work on
                current page only" item of the Options menu.

            */
            else if (i == bocr) {

                if (help) {
                    show_hint(0,"button 1 starts full OCR, button 3 open OCR steps menu");
                    return(1);
                }

                /* OCR current page */
                if (*cm_o_curr != ' ') {
                    start_ocr(-2,-1,0);
                }

                /* OCR all pages */
                else {
                    start_ocr(-1,-1,0);
                }
            }

            /* (book)

                stop - stop the current OCR run (if any). OCR
                does not stop immediately, but will stop
                as soon as possible.

            */
            else if (i == bstop) {

                if (help) {
                    show_hint(0,"stop OCR");
                    return(1);
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
                        show_hint(0,"button 1 adds zone, 3 deletes all them");
                    else
                        show_hint(0,"");
                    return(1);
                }

                if ((dw[PAGE].v) && (cpage >= 0)) {

                    if (zones >= MAX_ZONES) {
                        show_hint(2,"cannot add another zone");
                    }
                    else {
                        curr_corner = 0;
                        button[bzone] = 1;
                        show_hint(1,"enter ocr limits: top left, botton right");
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
                    show_hint(0,"pattern type for current symbol/pattern");
                    return(1);
                }
            }

            /* (book)

                bad - toggles the button state. The bad flag
                is used to identify damaged bitmaps.

            */
            else if (i == bbad) {

                if (help) {
                    show_hint(0,"click to toggle the bad flag");
                    return(1);
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
                    show_hint(0,"start test");
                    return(1);
                }

                if (!ocring) {
                    ocr_other = 5;
                    start_ocr(-2,OCR_OTHER,0);
                    show_hint(0,"running test...");
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
                redraw_map = 1;
                set_alpha();
                */
                /*
                form_changed = 1;
                form_auto_submit();
                */

                if (help) {
                    show_hint(0,"alphabet of current symbol/pattern");
                    return(1);
                }
            }
        }
    }

    /* first resize area */
    else if ((tab == PAGE) && (dw[PAGE].v) && (dw[PAGE_OUTPUT].v) &&
             (DM <= x) && (x < DM+DW) &&
             (pb=dw[PAGE].dt+dw[PAGE].dh) &&
             (((pb+sb <= y) && (y < dw[PAGE_OUTPUT].dt)) /* ||
              ((pb <= y) && (y < pb+10))*/ )) {

        if (help) {
            show_hint(0,"click and drag to resize windows");
            return(1);
        }

        sliding = 3;

        /* must redraw to clear the ellipses */
        redraw_dw = 1;
    }

    /* second resize area */
    else if ((tab == PAGE) && (dw[PAGE_OUTPUT].v) && (dw[PAGE_SYMBOL].v) &&
             (DM <= x) && (x < DM+DW) &&
             (ob=dw[PAGE_OUTPUT].dt+dw[PAGE_OUTPUT].dh) &&
             (((ob+sb <= y) && (y < dw[PAGE_SYMBOL].dt)) /* ||
              ((ob <= y) && (y < ob+10)) */ )) {

        if (help) {
            show_hint(0,"click and drag to resize windows");
            return(1);
        }

        sliding = 4;

        /* must redraw to clear the ellipses */
        redraw_dw = 1;
    }

    /* vertical scrollbar */
    else if ((*cm_v_hide == ' ') &&
        (DM+DW+10 <= x) &&
        (x <= DM+DW+10+RW) &&
        (DT <= y) && (y < DT+DH)) {

        /* steppers */
        if (y <= DT+RW) {

            if (help) {
                show_hint(0,"button 1 step up, button 3 go to top");
                return(1);
            }

            step_up();
        }
        else if (y >= DT+DH-RW) {

            if (help) {
                if (DS)
                    show_hint(0,"button 1 step down, button 3 go to bottom");
                return(1);
            }

            step_down();
        }

        /* cursor */
        else {

            if (help) {
                if (DS)
                    show_hint(0,"drag or click button 1 or 2 to move");
                return(1);
            }

            gettimeofday(&tclick,NULL);
            sliding = 2;
            s0 = y;
        }

        return(0);
    }

    /* horizontal scrollbar */
    else if ((DM <= x) && (x < DM+DW) &&
         (DT+DH+10 <= y) &&
         (y <= DT+DH+10+RW)) {

        /* steppers */
        if (x <= DM+RW) {

            if (help) {
                show_hint(0,"button 1 step left, button 3 go to left limit");
                return(1);
            }

            step_left();
        }
        else if (x >= DM+DW-RW) {

            if (help) {
                show_hint(0,"button 1 step right, button 3 go to right limit");
                return(1);
            }

            step_right();
        }

        /* cursor */
	else {

            if (help) {
                if (DS)
                    show_hint(0,"drag or click button 1 or 2 to move");
                return(1);
            }

            gettimeofday(&tclick,NULL);
            sliding = 1;
            s0 = x;
        }

        return(0);
    }

    /*
    if (help)
        show_hint(0,"");
    */

    return(0);
}

/*

Get pointer coordinates.

*/
void get_pointer(int *x,int *y)
{
    Window root,child;
    int rx,ry,cx,cy;
    unsigned mask;

    /* get pointer coordinates */
    rx = ry = -1;
    XQueryPointer(xd,XW,&root,&child,&rx,&ry,&cx,&cy,&mask);
    *x = cx;
    *y = cy;
}

/*

Flashes a message about the graphic element currently under the
pointer.

This is the service responsible to make appear messages when
you move the pointer across the Clara X window.

In some cases (as for HTML anchors) the element changes color.

*/
void show_info(void) {
    int m,n,px,py;

    /* get pointer coordinates */
    get_pointer(&px,&py);

    if (cmenu != NULL) {

        /* reselect automagically */
        n = mb_item(px,py);
        if ((n >= 0) && (n != cmenu-CM)) {
            dismiss(1);
            open_menu(px,py,n);
            set_mclip(0);
            return;
        }

        if (cmenu->c >= 0) {
            cma(cmenu->c + ((cmenu-CM) << CM_IT_WD),2);
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
    m = mactions_b1(1,px,py);

    /*
        Flashes a message for the HTML element currently under
        the pointer.
    */
    if ((DM <= px) && (px < DM+DW) &&
       (DT <= py) && (py < DT+DH)) {

        if ((HTML) || ((CDW==PAGE) && (*cm_d_bb != ' '))) {
            XRectangle r;

            r.x = DM;
            r.y = DT;
            r.width = DW;
            r.height = DH;
            XSetClipRectangles(xd,xgc,0,0,&r,1,Unsorted);

            show_htinfo(px,py);

            r.x = 0;
            r.y = 0;
            r.width = WW;
            r.height = WH;
            XSetClipRectangles(xd,xgc,0,0,&r,1,Unsorted);

            if (curr_hti < 0)
                show_hint(0,"");
        }
    }

    else if (!m)
        show_hint(0,"");
}

/*

Mouse actions activated by mouse button 3.

*/
void mactions_b3(int help,int x,int y)
{
    int i;

    /* change to selected window */
    select_dw(x,y);

    /*

        Open PAGE or PAGE_FATBITS options menu.

    */
    if ((DM <= x) && (x < DM+DW) &&
        (DT <= y) && (y < DT+DH)) {

        /* PAGE context menu */
        if ((CDW == PAGE) || (CDW == PAGE_FATBITS)) {

            open_menu(x,y,((CDW == PAGE) ? CM_D : CM_B));
        }
    }

    /* vertical scrollbar */
    else if ((*cm_v_hide == ' ') &&
             (DM+DW+10 <= x) &&
             (x <= DM+DW+10+RW) &&
             (DT <= y) && (y < DT+DH)) {

        /* steppers */
        if (y <= DT+RW) {
            Y0 = 0;
            redraw_dw = 1;
        }
        else if (y >= DT+DH-RW) {
            if ((GRY - VR) >= 0)
                Y0 = GRY - VR;
            redraw_dw = 1;
        }

        /* page down */
        else
            next();

        return;
    }

    /* horizontal scrollbar */
    else if ((*cm_v_hide == ' ') &&
             (DM <= x) && (x < DM+DW) &&
             (DT+DH+10 <= y) &&
             (y <= DT+DH+10+RW)) {

        /* steppers */
        if (x <= DM+RW) {
            X0 = 0;
            redraw_dw = 1;
        }
        else if (x >= DM+DW-RW) {
            if ((GRX - HR) >= 0)
                X0 = GRX - HR;
            redraw_dw = 1;
        }

        /* page right */
        else if ((CDW==PAGE_FATBITS) || (CDW==PAGE) || (CDW==PAGE_OUTPUT)) {
            X0 += HR;
            if (X0 > (XRES-HR))
                X0 = XRES-HR;
            if (CDW == PAGE_FATBITS)
                RG = 1;
            redraw_dw = 1;
        }

        return;
    }

    /*

        Application Buttons
        -------------------

    */
    else if ((10 <= x) && (x <= (10+BW))) {

        i = (y-PT) / (BH+VS);
        if ((y - PT - (i*(BH+VS))) <= BH) {

            /* end */
            /*
            if (i == earrow) {
                if ((XRES - HR) >= 0)
                    X0 = XRES - HR;
                redraw_dw = 1;
            }
            */

            /* bottom */
            /*
            else if (i == barrow) {
                if ((YRES - VR) >= 0)
                    Y0 = YRES - VR;
                redraw_dw = 1;
            }
            */

            /* zoom - */
            if (i == bzoom) {

                if ((dw[PAGE].v) &&
                    ((CDW != PAGE_SYMBOL) || (*cm_v_wclip==' '))) {

                    CDW = PAGE;
                    if (RF < MRF) {
                        ++RF;
                        setview(PAGE);
                        check_dlimits(1);
                    }
                }

                else if ((CDW == PAGE_FATBITS) || (CDW == TUNE_PATTERN)) {
                    if (ZPS > 1) {
                        ZPS -= 2;
                        comp_wnd_size(-1,-1);
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
                        redraw_dw = 1;
                    }
                }
            }

            /* open OCR operations menu */
            else if (i == bocr) {
                open_menu(x,y,CM_R);
            }

            /* delete OCR zones */
            else if (i == bzone) {
                if (zones > 0) {
                    zones = 0;
                    redraw_dw = 1;
                    for (i=0; i<=tops; ++i)
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
void mactions_b2(int help,int x,int y)
{
    int i;

    /* change to selected window */
    select_dw(x,y);

    /*


    */
    if ((DM <= x) && (x < DM+DW) &&
        (DT <= y) && (y < DT+DH)) {

        return;
    }

    /* vertical scrollbar */
    else if ((*cm_v_hide == ' ') &&
             (DM+DW+10 <= x) &&
             (x <= DM+DW+10+RW) &&
             (DT <= y) && (y < DT+DH)) {

        return;
    }

    /* horizontal scrollbar */
    else if ((*cm_v_hide == ' ') &&
             (DM <= x) && (x < DM+DW) &&
             (DT+DH+10 <= y) &&
             (y <= DT+DH+10+RW)) {

        return;
    }

    /*

        Application Buttons
        -------------------

    */
    else if ((10 <= x) && (x <= (10+BW))) {

        i = (y-PT) / (BH+VS);
        if ((y - PT - (i*(BH+VS))) <= BH) {

            /* zoom max/min */
            if (i == bzoom) {
                int o;

                if ((dw[PAGE].v) &&
                    ((CDW != PAGE_SYMBOL) || (*cm_v_wclip==' '))) {

                    CDW = PAGE;
                    if ((o=RF) < (MRF/2))
                        RF = MRF/2;
                    else if (RF < MRF)
                        RF = MRF;
                    else
                        RF = 1;
                    if (RF != o) {
                        setview(PAGE);
                        check_dlimits(1);
                    }
                }

                else if ((CDW == PAGE_FATBITS) || (CDW == TUNE_PATTERN)) {

                    /* TODO */
                }

                else if ((CDW == PATTERN) ||
                         (CDW == PATTERN_LIST) ||
                         (CDW == PATTERN_TYPES) ||
                         (CDW == PAGE_SYMBOL)) {

                    if ((o=plist_rf) < 8)
                        plist_rf = 8;
                    else
                        plist_rf = 1;

                    if (plist_rf != o) {
                        ++plist_rf;
                        dw[PATTERN].rg = 1;
                        dw[PATTERN_LIST].rg = 1;
                        dw[PATTERN_TYPES].rg = 1;
                        dw[PAGE_SYMBOL].rg = 1;
                        redraw_dw = 1;
                    }
                }
            }
        }
    }
}

/*

Get the menu item under the pointer.

*/
int get_item(int x,int y) {
    int j;

    if ((cmenu != NULL) &&
        (CX0 <= x) && (x <= CX0+CW) &&
        (CY0 <= y) && (y <= CY0+CH)) {

        /* compute action */
        j = (y - CY0 - 5) / (DFH + VS);
        if (j < 0)
            j = 0;
        else if (j >= cmenu->n)
            j = cmenu->n - 1;
        j += ((cmenu-CM) << CM_IT_WD);
        return(j);
    }
    return(-1);
}

/*

Get and process X events.

This function is continuously being called to check the existence
of X events to be processed and to process them (if any).

Remark: The flag have_s_ev must be cleared only when returning from
this service.

*/
void xevents(void)
{
    int no_ev;
    static int expose_queued=0;
    static struct timeval rt = {0,0};
    struct timeval t;
    unsigned d;

    /*
        No new event. Events are got from the Xserver or
        directly from the buffer xev. Some internal procedures
        feed xev conveniently and set the flag have_s_ev.
    */
    if ((!have_s_ev) && (XCheckWindowEvent(xd,XW,evmasks,&xev) == 0)) {

        /*
            Compute age of the expose event queued.
        */
        if (expose_queued) {
            gettimeofday(&t,NULL);
            d = (t.tv_sec - rt.tv_sec) * 1000000;
            if (d == 0)
                d += (t.tv_usec - rt.tv_usec);
            else
                d += -rt.tv_usec + t.tv_usec;
        }
        else {

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
                select(0,NULL,NULL,NULL,&t);
            }

            /* dismiss menus that became unfocused */
            if (cmenu != NULL)
                dismiss(0);

            /* show local info */
            if (((!ocring) || waiting_key) && (focused) && (!sliding))
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
    }
    else if (xev.type == FocusOut) {
        focused = 0;
        have_s_ev = 0;
        return;
    }

    /* a button press must clear the message line */
    if (xev.type == ButtonPress) {
        show_hint(3,NULL);
    }

    /*
        Remember time of the expose event (it's the event
        being processed now). This event will be processed
        as soon as it becomes sufficiently old, in order to
        avoid expensive redrawing of the entire window.
    */
    if ((!no_ev) && (xev.type == Expose)) {

        gettimeofday(&rt,NULL);
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
        bev = (XButtonEvent *) &xev;

        /* selected item */
        if ((xev.type == ButtonPress) && (bev->button == Button1)) {
            mactions_b1(0,xev.xbutton.x,xev.xbutton.y);
        }

        /* key */
        else if (xev.type == KeyPress) {
            kactions();
        }

        /* just hilight the current item */
        else if (xev.type == MotionNotify) {
            int j;

            j = get_item(xev.xbutton.x,xev.xbutton.y);
            if (j >= 0)
                j &= CM_IT_M;
            if (j != cmenu->c) {
                cmenu->c = j;
                redraw_menu = 1;
            }
        }
    }

    /* Application window resize */
    else if (xev.type == ConfigureNotify) {
        Window root;
        int x,y;
        unsigned w,h,b,d;

        XGetGeometry(xd,XW,&root,&x,&y,&w,&h,&b,&d);
        if ((w != WW) || (h != WH)) {
            /* db("resizing from %d %d to %d %d",WW,WH,w,h); */
            comp_wnd_size(w,h);
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
            gettimeofday(&t,NULL);
            d = (t.tv_sec - tclick.tv_sec) * 1000000;
            if (d == 0)
                d += (t.tv_usec - tclick.tv_usec);
            else
                d += -tclick.tv_usec + t.tv_usec;

            /* a quick press-release on mode 1 means "page left" */
            if ((sliding == 1) && (d < 200000)) {

                if ((CDW==PAGE_FATBITS) || (CDW==PAGE) || (CDW==PAGE_OUTPUT)) {
                    X0 -= HR;
                    if (X0 < 0)
                        X0 = 0;
                    if (CDW == PAGE_FATBITS)
                        RG = 1;
                    redraw_dw = 1;
                }
            }

            /* a quick press-release on mode 2 means "page up" */
            if ((sliding == 2) && (d < 200000))
                prior();

            if (curr_mc >= 0)
                redraw_dw = 1;
        }
        sliding = 0;
    }

    /* Process other events */
    else {

        switch (xev.type) {

            case ButtonPress:

                bev = (XButtonEvent *) &xev;
                if (bev->button == Button1) {
                    mactions_b1(0,xev.xbutton.x,xev.xbutton.y);
                }
                else if (bev->button == Button2) {
                    mactions_b2(0,xev.xbutton.x,xev.xbutton.y);
                }
                else if (bev->button == Button3) {
                    mactions_b3(0,xev.xbutton.x,xev.xbutton.y);
                }

                break;

            case KeyPress:
                kactions();
                break;

            default:
                db("unattended X event %d (ok)",xev.type);
                break;
        }
    }

    have_s_ev = 0;
}

/*

Synchronize the variables alpha, alpha_sz and alpha_cols with
the current status of the alphabet selection button.

*/
void set_alpha(void)
{
    int i;

    /* get current alphabet and its size */
    i = inv_balpha[(int)(button[balpha])];
    if (i == LATIN) {
        alpha = latin;
        alpha_sz = latin_sz;
        alpha_cols = 2;
    }
    else if (i == GREEK) {
        alpha = (alpharec *) greek;
        alpha_sz = greek_sz;
        alpha_cols = 3;
    }
    else if (i == CYRILLIC) {
        alpha = cyrillic;
        alpha_sz = cyrillic_sz;
        alpha_cols = 2;
    }
    else if (i == HEBREW) {
        alpha = hebrew;
        alpha_sz = hebrew_sz;
        alpha_cols = 1;
    }
    else if (i == ARABIC) {
        alpha = arabic;
        alpha_sz = arabic_sz;
        alpha_cols = 1;
    }
    else if (i == NUMBER) {
        alpha = number;
        alpha_sz = number_sz;
        alpha_cols = 1;
    }
    else
        alpha = NULL;
}

/*

Set the application window name.

*/
void swn(char *n)
{
    char *list[2];
    XTextProperty xtp;

    list[0] = n;
    list[1] = NULL;
    XStringListToTextProperty(list,1,&xtp);
    XSetWMName(xd,XW,&xtp);
}

/*

Set the default font. Clara uses one same font for all (menus,
button labels, HTML rendering, etc. The font to be set is
obtained from the Options menu status.

*/
void set_xfont(void)
{
    XCharStruct *x;

    /* free current font */
    if (dfont != NULL)
        XFreeFont(xd,dfont);

    /* use small font */
    if (*cm_v_small == 'X') {
        if (((dfont = XLoadQueryFont(xd,"6x13")) == NULL) && verbose)
            warn("oops.. could not read font 6x13\n");
    }

    /* use medium font */
    else if (*cm_v_medium == 'X') {
        if (((dfont = XLoadQueryFont(xd,"9x15")) == NULL) && verbose)
            warn("oops.. could not read font 9x15\n");
    }

    /* use large font */
    else if (*cm_v_large == 'X') {
        if (((dfont = XLoadQueryFont(xd,"10x20")) == NULL) && verbose)
            warn("oops.. could not read font 10x20\n");
    }

    /* use the font informed as command-line parameter */
    else {
        if (((dfont = XLoadQueryFont(xd,xfont)) == NULL) && verbose)
            warn("oops.. could not read font %s\n",xfont);
    }

    /* try "fixed" */
    if ((dfont == NULL) && (strcmp(xfont,"fixed") != 0)) {
        if (((dfont = XLoadQueryFont(xd,"fixed")) == NULL) && verbose)
            warn("oops.. could not read font fixed\n");
    }

    /* no font loaded: abandon */
    if (dfont == NULL) {
        fatal(XE,"could not load a font, giving up");
    }

    /* text font height and width */
    x = &(dfont->max_bounds);
    DFH = (DFA=x->ascent) + (DFD=x->descent);
    DFW = x->width;
}

/*

Reset the parameters of all windows.

*/
void init_dws(int m)
{
    int i;

    for (i=0; i<=TOP_DW; ++i) {
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
}

/*

Context menu initializer.

*/
void cmi(void)
{
    cmdesc *c;

    /* (book)

        File menu
        ---------

        This menu is activated from the menu bar on the top of the
        application X window.
    */
    c = addmenu(1,"File",0);

    /* (book)

        Load page

        Enter the page list to select a page to be loaded.
    */
    CM_F_LOAD = additem(c,"Load page",CM_TA,0,-1,
                        "",0);


    /* (book)

        Save session

        Save on disk the page session (file page.session), the patterns
        (file "pattern") and the revision acts (file "acts").
    */
    CM_F_SAVE = additem(c,"Save session",CM_TA,0,-1,
                        "",CMA_PAGE);

    /* (book)

        Save first zone

        Save on disk the first zone as the file zone.pbm.
    */
    CM_F_SZ = additem(c,"Save first zone",CM_TA,0,-1,
                      "",CMA_PAGE|CMA_ZONE);

    /* (book)

        Save replacing symbols

        Save on disk the entire page replacing symbols by patterns,
        to achieve better compression rates (mostly to produce small
        web images).
    */
    CM_F_SR = additem(c,"Save replacing symbols",CM_TA,0,-1,
                      "",CMA_PAGE|CMA_CL);

    /* (book)

        Write report

        Save the contents of the PAGE LIST window to the file
        report.txt on the working directory.
    */
    CM_F_REP = additem(c,"Write report",CM_TA,0,-1,
                       "",0);

    CM_F = c - CM;

    /* (book)

        Quit the program

        Just quit the program (asking before if the session is to
        be saved.
    */
    CM_F_QUIT = additem(c,"Quit",CM_TA,0,-1,
                        "",0);
    CM_F = c - CM;

    /* (book)

        Edit menu
        ---------

        This menu is activated from the menu bar on the top of the
        application X window.
    */
    c = addmenu(1,"Edit",0);

    /* (book)

        Only doubts

        When selected, the right or the left arrows used on the
        PATTERN or the PATTERN PROPS windows will move to the next
        or the previous untransliterated patterns.
    */
    CM_E_DOUBTS = additem(c,"[ ] Only doubts",CM_TB,0,-1,
                          "",0);

    /* (book)

        Re-scan all patterns

        When selected, the classification heuristic will retry
        all patterns for each symbol. This is required when trying
        to resolve the unclassified symbols using a second
        classification method.
    */
    CM_E_RESCAN = additem(c,"[ ] Re-scan all patterns",CM_TB,0,-1,
                          "",0);

    /* (book)

        Auto-classify.

        When selected, the engine will re-run the classifier after
        each new pattern trained by the user. So if various letters
        "a" remain unclassified, training one of them will perhaps
        recognize some othersm helping to complete the recognition.
    */
    CM_E_AC = additem(c,"[ ] Auto-classify",CM_TB,0,-1,
                          "",0);

    /* (book)

        Fill region

        When selected, the mouse button 1 will fill the region
        around one pixel on the pattern bitmap under edition on the
        font tab.
    */
    CM_E_FILL = additem(c,"[ ] Fill region",CM_TR,1,-1,
                        "",0);

    /* (book)

        Paint pixel

        When selected, the mouse button 1 will paint individual
        pixels on the pattern bitmap under edition on the font tab.
    */
    CM_E_PP = additem(c,"[ ] Paint pixel",CM_TR,0,-1,
                      "",0);

    /* (book)

        Clear region

        When selected, the mouse button 1 will clear the region
        around one pixel on the pattern bitmap under edition on the
        font tab.
    */
    CM_E_FILL_C = additem(c,"[ ] Clear region",CM_TR,0,-1,
                          "",0);

    /* (book)

        Clear pixel

        When selected, the mouse button 1 will clear individual
        pixels on the pattern bitmap under edition on the font tab.
    */
    CM_E_PP_C = additem(c,"[ ] Clear pixel",CM_TR,0,-1,
                        "",0);

    /* (book)

        Sort patterns by page

        When selected, the pattern list window will divide
        the patterns in blocks accordingly to their (page)
        sources.
    */
    CM_E_SP = additem(c,"[ ] Sort patterns by page",CM_TB,1,-1,
                      "",0);

    /* (book)

        Sort patterns by matches

        When selected, the pattern list window will use as the
        first criterion when sorting the patterns, the number of
        matches of each pattern.
    */
    CM_E_SM = additem(c,"[ ] Sort patterns by matches",CM_TB,0,-1,
                      "",0);

    /* (book)

        Sort patterns by transliteration

        When selected, the pattern list window will use as the
        second criterion when sorting the patterns, their
        transliterations.
    */
    CM_E_ST = additem(c,"[ ] Sort patterns by transliteration",CM_TB,0,-1,
                      "",0);

    /* (book)

        Sort patterns by number of pixels

        When selected, the pattern list window will use as the
        third criterion when sorting the patterns, their
        number of pixels.
    */
    CM_E_SN = additem(c,"[ ] Sort patterns by number of pixels",CM_TB,0,-1,
                      "",0);

    /* (book)

        Sort patterns by width

        When selected, the pattern list window will use as the
        fourth criterion when sorting the patterns, their
        widths.
    */
    CM_E_SW = additem(c,"[ ] Sort patterns by width",CM_TB,0,-1,
                      "",0);

    /* (book)

        Sort patterns by height

        When selected, the pattern list window will use as the
        fifth criterion when sorting the patterns, their
        heights.
    */
    CM_E_SH = additem(c,"[ ] Sort patterns by height",CM_TB,0,-1,
                      "",0);

    /* (book)

        Del Untransliterated patterns

        Remove from the font all untransliterated fonts.
    */
    CM_E_DU = additem(c,"Del Untransliterated patterns",CM_TA,1,-1,
                      "",0);

    /* (book)

        Set pattern type.

        Set the pattern type for all patterns marked as "other".
    */
    CM_E_SETPT = additem(c,"Set pattern type",CM_TA,0,-1,
                         "",0);

    /* (book)

        Search barcode.

        Try to find a barcode on the loaded page.
    */
    CM_E_SEARCHB = additem(c,"Search barcode",CM_TA,0,-1,
                           "",0);

    /* (book)

        Instant thresholding.

        Perform on-the-fly global thresholding.
    */
    CM_E_THRESH = additem(c,"Instant thresholding",CM_TA,0,-1,
                          "",0);

#ifdef PATT_SKEL
    /* (book)

        Reset skeleton parameters

        Reset the parameters for skeleton computation for all
        patterns.
    */
    CM_E_RSKEL = additem(c,"Reset skeleton parameters",CM_TA,0,-1,
                         "",0);
#endif

    CM_E = c - CM;

    /* (book)

        View menu
        ---------

        This menu is activated from the menu bar on the top of the
        application X window.
    */
    c = addmenu(1,"View",0);

    /* (book)

        Small font

        Use a small X font (6x13).
    */
    CM_V_SMALL = additem(c,"[ ] Small font (6x13)",CM_TR,0,1,
                         "",0);

    /* (book)

        Medium font

        Use the medium font (9x15).
    */
    CM_V_MEDIUM = additem(c,"[ ] Medium font (9x15)",CM_TR,0,1,
                          "",0);

    /* (book)

        Small font

        Use a large X font (10x20).
    */
    CM_V_LARGE = additem(c,"[ ] Large font (10x20)",CM_TR,0,1,
                         "",0);

    /* (book)

        Default font

        Use the default font (7x13 or "fixed" or the one informed
        on the command line).
    */
    CM_V_DEF = additem(c,"[ ] Default font",CM_TR,0,1,
                       "",0);

    /* (book)

        Hide scrollbars

        Toggle the hide scrollbars flag. When active, this
        flag hides the display of scrolllbar on all windows.
    */
    CM_V_HIDE = additem(c,"[ ] Hide scrollbars",CM_TB,0,1,
                        "",0);

    /* (book)

        Omit fragments

        Toggle the hide fragments flag. When active,
        fragments won't be included on the list of
        patterns.
    */
    CM_V_OF = additem(c,"[ ] Omit fragments",CM_TB,0,-1,
                      "",0);

    /* (book)

        Show HTML source

        Show the HTML source of the document, instead of the
        graphic rendering.
    */
    CM_V_VHS = additem(c,"[ ] Show HTML source",CM_TB,1,1,
                       "",0);

    /* (book)

        Show web clip

        Toggle the web clip feature. When enabled, the PAGE_SYMBOL
        window will include the clip of the document around the
        current symbol that will be used through web revision.
    */
    CM_V_WCLIP = additem(c,"[ ] Show web clip",CM_TB,0,1,
                         "",0);

    /* (book)

        Show alphabet map

        Toggle the alphabet map display. When enabled, a mapping
        from Latin letters to the current alphabet will be
        displayed.
    */
    CM_V_MAP = additem(c,"[ ] Show alphabet map",CM_TB,0,1,
                       "",0);

    /* (book)

        Show current class

        Identify the symbols on the current class using
        a gray ellipse.
    */
    CM_V_CC = additem(c,"[ ] Show current class",CM_TB,0,-1,
                      "",0);

    /* (book)

        Show matches

        Display bitmap matches when performing OCR.
    */
    CM_V_MAT = additem(c,"[ ] Show matches",CM_TR,1,-1,
                       "",0);

    /* (book)

        Show comparisons

        Display all bitmap comparisons when performing OCR.
    */
    CM_V_CMP = additem(c,"[ ] Show comparisons",CM_TR,0,-1,
                       "",0);

    /* (book)

        Show matches

        Display bitmap matches when performing OCR,
        waiting a key after each display.
    */
    CM_V_MAT_K = additem(c,"[ ] Show matches and wait",CM_TR,0,-1,
                         "",0);

    /* (book)

        Show comparisons and wait

        Display all bitmap comparisons when performing OCR,
        waiting a key after each display.
    */
    CM_V_CMP_K = additem(c,"[ ] Show comparisons and wait",CM_TR,0,-1,
                         "",0);

    /* (book)

        Show skeleton tuning

        Display each candidate when tuning the skeletons of the
        patterns.
    */
    CM_V_ST = additem(c,"[ ] Show skeleton tuning",CM_TB,0,-1,
                      "",0);

    /* (book)

        Presentation

        Perform a presentation. This item is visible on the menu
        only when the program is started with the -A option.
    */
    if (allow_pres)
        CM_V_PRES = additem(c,"Presentation",CM_TA,1,1,
                            "",0);
    CM_V = c - CM;

    /* (book)

        OCR steps menu
        --------------

        This menu is activated when the mouse button 3 is pressed on
        the OCR button. It allows running specific OCR steps (all
        steps run in sequence when the OCR button is pressed).

    */
    c = addmenu(0,"OCR operations",0);

    /* (book)

        Preproc.

        Start preproc.
    */
    CM_R_PREPROC = additem(c,"Preprocessing",CM_TA,0,-1,
                          "",CMA_PAGE);

    /* (book)

        Detect blocks

        Start detecting text blocks.

    */
    CM_R_BLOCK = additem(c,"Detect blocks",CM_TA,0,-1,
                         "",CMA_PAGE);

    /* (book)

        Segmentation.

        Start binarization and segmentation.
    */
    CM_R_SEG = additem(c,"Segmentation",CM_TA,0,-1,
                       "",CMA_PAGE|CMA_NCL);

    /* (book)

        Consist structures

        All OCR data structures are submitted to consistency
        tests. This is under implementation.
    */
    CM_R_CONS = additem(c,"Consist structures",CM_TA,0,-1,
                        "",0);

    /* (book)

        Prepare patterns

        Compute the skeletons and analyse the patterns for
        the achievement of best results by the classifier. Not
        fully implemented yet.
    */
    CM_R_OPT = additem(c,"Prepare patterns",CM_TA,0,-1,
                       "",0);

    /* (book)

        Read revision data

        Revision data from the web interface is read, and
        added to the current OCR training knowledge.
    */
    CM_R_REV = additem(c,"Read revision data",CM_TA,0,-1,
                       "",CMA_PAGE|CMA_CL);

    /* (book)

        Classification

        start classifying the symbols of the current
        page or of all pages, depending on the state of the
        "Work on current page only" item of the Options menu. It
        will also build the font automatically if the
        corresponding item is selected on the Options menu.

    */
    CM_R_CLASS = additem(c,"Classification",CM_TA,0,-1,
                         "",CMA_PAGE|CMA_CL);

    /* (book)

        Geometric merging

        Merge closures on symbols depending on their
        geometry.
    */
    CM_R_GEOMERGE = additem(c,"Geometric merging",CM_TA,0,-1,
                            "",CMA_PAGE|CMA_CL);

    /* (book)

        Build words and lines

        Start building the words and lines. These heuristics will
        be applied on the
        current page or on all pages, depending on the state of
        the "Work on current page only" item of the Options menu.

    */
    CM_R_BUILD = additem(c,"Build words and lines",CM_TA,0,-1,
                         "",CMA_PAGE|CMA_CL);

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
    CM_R_SPELL = additem(c,"Generate spelling hints",CM_TA,0,-1,
                         "",CMA_PAGE|CMA_CL);

    /* (book)

        Generate output

        The OCR output is generated to be displayed on the
        "PAGE (output)" window. The output is also saved to the
        file page.html.
    */
    CM_R_OUTP = additem(c,"Generate output",CM_TA,0,-1,
                        "",CMA_PAGE|CMA_CL);

    /* (book)

        Generate web doubts

        Files containing symbols to be revised through the web
        interface are created on the "doubts" subdirectory of
        the work directory. This step is performed only when
        Clara OCR is started with the -W command-line switch.
    */
    CM_R_DOUBTS = additem(c,"Generate web doubts",CM_TA,0,-1,
                          "",CMA_PAGE|CMA_CL);

    CM_R = c - CM;

    /* (book)

        PAGE options menu
        -----------------

        This menu is activated when the mouse button 3 is pressed on
        the PAGE window.

    */
    c = addmenu(0,"PAGE",0);

    /* (book)

        See in fatbits

        Change to PAGE_FATBITS focusing this symbol.
    */
    CM_D_FOCUS = additem(c,"See in fatbits",CM_TA,0,-1,
                         "",CMA_CL);

    /* (book)

        Bottom left here

        Scroll the window contents in order to the
        current pointer position become the bottom left.
    */
    CM_D_HERE = additem(c,"Bottom left here",CM_TA,0,-1,
                        "",0);

    /* (book)

        Use as pattern

        The pattern of the class of this symbol will be the unique
        pattern used on all subsequent symbol classifications. This
        feature is intended to be used with the "OCR this symbol"
        feature, so it becomes possible to choose two symbols to
        be compared, in order to test the classification routines.

    */
    CM_D_TS = additem(c,"Use as pattern",CM_TA,0,-1,
                      "",CMA_CL);

    /* (book)

        OCR this symbol

        Starts classifying only the symbol under the pointer. The
        classification will re-scan all patterns even if the "re-scan
        all patterns" option is unselected.
    */
    CM_D_OS = additem(c,"OCR this symbol",CM_TA,0,-1,
                      "",CMA_CL);

    /* (book)

        Merge with current symbol

        Merge this fragment with the current symbol.
    */
    CM_D_ADD = additem(c,"Merge with current symbol",CM_TA,0,-1,
                       "",CMA_CL);

    /* (book)

        Link as next symbol

        Create a symbol link from the current symbol (the one
        identified by the graphic cursor) to this symbol.
    */
    CM_D_SLINK = additem(c,"Link as next symbol",CM_TA,0,-1,
                         "",CMA_CL);

    /* (book)

        Disassemble symbol

        Make the current symbol nonpreferred and each of its
        components preferred.
    */
    CM_D_DIS = additem(c,"Disassemble symbol",CM_TA,0,-1,
                       "",CMA_CL);

    /* (book)

        Link as accent

        Create an accent link from the current symbol (the one
        identified by the graphic cursor) to this symbol.
    */
    CM_D_ALINK = additem(c,"Link as accent",CM_TA,0,-1,
                         "",CMA_CL);

    /* (book)

        Diagnose symbol pairing

        Run locally the symbol pairing heuristic to try to join
        this symbol to the word containing the current symbol.
        This is useful to know why the OCR is not joining two
        symbols on one same word.
    */
    CM_D_SDIAG = additem(c,"Diagnose symbol pairing",CM_TA,0,-1,
                         "",CMA_CL);

    /* (book)

        Diagnose word pairing

        Run locally the word pairing heuristic to try to join
        this word with the word containing the current symbol
        on one same line. This is useful to know why the OCR is
        not joining two words on one same line.
    */
    CM_D_WDIAG = additem(c,"Diagnose word pairing",CM_TA,0,-1,
                         "",CMA_CL);

    /* (book)

        Diagnose lines

        Run locally the line comparison heuristic to decide
        which is the preceding line.
    */
    CM_D_LDIAG = additem(c,"Diagnose lines",CM_TA,0,-1,
                         "",CMA_CL);

    /* (book)

        Diagnose merging

        Run locally the geometrical merging heuristic to try
        to merge this piece to the current symbol.
    */
    CM_D_GM = additem(c,"Diagnose merging",CM_TA,0,-1,
                      "",CMA_CL);

    /* (book)

        Show pixel coords

        Present the coordinates and color of the current pixel.
    */
    CM_D_PIXELS = additem(c,"[ ] Show pixel coords",CM_TR,1,1,
                          "",0);

    /* (book)

        Show closures

        Identify the individual closures when displaying the
        current document.
    */
    CM_D_CLOSURES = additem(c,"[ ] Show closures",CM_TR,0,1,
                            "",0);

    /* (book)

        Show symbols

        Identify the individual symbols when displaying the
        current document.
    */
    CM_D_SYMBOLS = additem(c,"[ ] Show symbols",CM_TR,0,1,
                           "",0);

    /* (book)

        Show words

        Identify the individual words when displaying the
        current document.
    */
    CM_D_WORDS = additem(c,"[ ] Show words",CM_TR,0,1,
                         "",0);

    /* (book)

        Show pattern type

        Display absent symbols on pattern type 0, to help
        building the bookfont.
    */
    CM_D_PTYPE = additem(c,"[ ] Show pattern type",CM_TR,0,1,
                         "",0);

    /* (book)

        Report scale

        Report the scale on the tab when the PAGE window is
        active.
    */
    CM_D_RS = additem(c,"[ ] Report scale",CM_TB,1,1,
                      "",0);

    /* (book)

        Display box instead of symbol

        On the PAGE window displays the bounding boxes instead of
        the symbols themselves. This is useful when designing new
        heuristics.
    */
    CM_D_BB = additem(c,"[ ] Display box instead of symbol",CM_TB,0,1,
                      "",0);

    CM_D = c - CM;

    /* (book)

        PAGE_FATBITS options menu
        -------------------------

        This menu is activated when the mouse button 3 is pressed on
        the PAGE.

    */
    c = addmenu(0,"PAGE_FATBITS",0);

    /* (book)

        Bottom left here

        Scroll the window contents in order to the
        current pointer position become the bottom left.
    */
    CM_B_HERE = additem(c,"Bottom left here",CM_TA,0,-1,
                        "",0);

    /* (book)

        Centralize

        Scroll the window contents in order to the
        centralize the closure under the pointer.
    */
    CM_B_CENTRE = additem(c,"Centralize",CM_TA,0,-1,
                          "",0);

    /* (book)

        Build border path

        Build the closure border path and activate the flea.
    */
    CM_B_BP = additem(c,"Build border path",CM_TA,0,-1,
                      "",0);

    /* (book)

        Search straight lines (linear)

        Build the closure border path and search straight lines
        there using linear distances.
    */
    CM_B_SLD = additem(c,"Search straight lines (linear)",CM_TA,0,-1,
                       "",0);

    /* (book)

        Search straight lines (quadratic)

        Build the closure border path and search straight lines
        there using correlation.
    */
    CM_B_SLC = additem(c,"Search straight lines (quadratic)",CM_TA,0,-1,
                       "",0);

    /* (book)

        Is bar?

        Apply the isbar test on the closure.
    */
    CM_B_ISBAR = additem(c,"Is bar?",CM_TA,0,-1,
                         "",0);

    /* (book)

        Detect extremities?

        Detect closure extremities.
    */
    CM_B_DX = additem(c,"Detect extremities",CM_TA,0,-1,
                      "",0);

    /* (book)

        Show skeletons

        Show the skeleton on the windows PAGE_FATBITS. The
        skeletons are computed on the fly.
    */
    CM_B_SKEL = additem(c,"[ ] Show skeletons",CM_TR,1,-1,
                        "",0);

    /* (book)

        Show border

        Show the border on the window PAGE_FATBITS. The
        border is computed on the fly.
    */
    CM_B_BORDER = additem(c,"[ ] Show border",CM_TR,0,-1,
                          "",0);

    /* (book)

        Show pattern skeleton

        For each symbol, will show the skeleton of its best match
        on the PAGE (fatbits) window.
    */
    CM_B_HS = additem(c,"[ ] Show pattern skeleton",CM_TR,0,-1,
                      "",0);

    /* (book)

        Show pattern border

        For each symbol, will show the border of its best match
        on the PAGE (fatbits) window.
    */
    CM_B_HB = additem(c,"[ ] Show pattern border",CM_TR,0,-1,
                      "",0);

    CM_B = c - CM;

    /* (book)

        Alphabets menu
        --------------

        This item selects the alphabets that will be available on the
        alphabets button.
    */
    c = addmenu(1,"Alphabets",0);

    /* (book)

        Arabic

        This is a provision for future support of Arabic
        alphabet.
    */
    CM_A_ARABIC = additem(c,"[ ] Arabic",CM_TB,0,-1,
                          "",CMA_ABSENT);

    /* (book)

        Cyrillic

        This is a provision for future support of Cyrillic
        alphabet.
    */
    CM_A_CYRILLIC = additem(c,"[ ] Cyrillic",CM_TB,0,-1,
                            "",CMA_ABSENT);

    /* (book)

        Greek

        This is a provision for future support of Greek
        alphabet.
    */
    CM_A_GREEK = additem(c,"[ ] Greek",CM_TB,0,-1,
                         "",CMA_ABSENT);

    /* (book)

        Hebrew

        This is a provision for future support of Hebrew
        alphabet.
    */
    CM_A_HEBREW = additem(c,"[ ] Hebrew",CM_TB,0,-1,
                          "",CMA_ABSENT);

    /* (book)

        Kana

        This is a provision for future support of Kana
        alphabet.
    */
    CM_A_KANA = additem(c,"[ ] Kana",CM_TB,0,-1,
                        "",CMA_ABSENT);

    /* (book)

        Latin

        Words that use the Latin alphabet include those from
        the languages of most Western European countries (English,
        German, French, Spanish, Portuguese and others).
    */
    CM_A_LATIN = additem(c,"[ ] Latin",CM_TB,0,-1,
                         "",0);

    /* (book)

        Number

        Numbers like
        1234, +55-11-12345678 or 2000.
    */
    CM_A_NUMBER = additem(c,"[ ] Numeric",CM_TB,1,-1,
                          "",0);

    /* (book)

        Ideogram

        Ideograms.
    */
    CM_A_IDEOGRAM = additem(c,"[ ] Ideograms",CM_TB,0,-1,
                            "",CMA_ABSENT);

    CM_A = c - CM;

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
    c = addmenu(1,"Languages",0);

    /* (book)

        English (USA)

        Toggle American English spell checking for
        each word found on the page.
    */
    CM_L_EN = additem(c,"[ ] English (USA)",CM_TB,0,-1,
                      "",0);

    /* (book)

        English (UK)

        Toggle British English spell checking for
        each word found on the page.
    */
    CM_L_UK = additem(c,"[ ] English (UK)",CM_TB,0,-1,
                      "",0);

    /* (book)

        French

        Toggle French spell checking for
        each word found on the page.
    */
    CM_L_FR = additem(c,"[ ] French",CM_TB,0,-1,
                      "",0);

    /* (book)

        German

        Toggle german spell checking for
        each word found on the page.
    */
    CM_L_DE = additem(c,"[ ] German",CM_TB,0,-1,
                      "",0);

    /* (book)

        Greek

        Toggle greek spell checking for
        each word found on the page.
    */
    CM_L_GR = additem(c,"[ ] Greek",CM_TB,0,-1,
                      "",0);

    /* (book)

        Portuguese (Brazil)

        Toggle brazilian protuguese spell checking for
        each word found on the page.
    */
    CM_L_BR = additem(c,"[ ] Portuguese (Brazil)",CM_TB,0,-1,
                      "",0);

    /* (book)

        Portuguese (Portugal)

        Toggle Portuguese spell checking for
        each word found on the page.
    */
    CM_L_PT = additem(c,"[ ] Portuguese (Portugal)",CM_TB,0,-1,
                      "",0);

    /* (book)

        Russian

        Toggle Russian spell checking for
        each word found on the page.
    */
    CM_L_RU = additem(c,"[ ] Russian",CM_TB,0,-1,
                      "",0);

    CM_L = c - CM;
#endif

    /* (book)

        Options menu
        ------------
    */
    c = addmenu(1,"Options",0);

    /* (book)

        Work on current page only

        OCR operations (classification, merge, etc) will be
        performed only on the current page.
    */
    CM_O_CURR = additem(c,"[ ] Work on current page only",CM_TR,0,-1,
                        "",0);

    /* (book)


        Work on all pages

        OCR operations (classification, merge, etc) will be
        performed on all pages.
    */
    CM_O_ALL = additem(c,"[ ] Work on all pages",CM_TR,0,-1,
                       "",0);

    /* (book)

        Emulate deadkeys

        Toggle the emulate deadkeys flag. Deadkeys are useful for
        generating accented characters. Deadkeys emulation are disabled
        by default The emulation of deadkeys may be set on startup
        through the -i command-line switch.
    */
    CM_O_DKEYS = additem(c,"[ ] Emulate deadkeys",CM_TB,1,-1,
                         "",0);

    /* (book)

        Menu auto popup

        Toggle the automenu feature. When enabled, the menus on the
        menu bar will pop automatically when the pointer reaches
        the menu bar.
    */
    CM_O_AMENU = additem(c,"[ ] Menu auto popup",CM_TB,0,-1,
                         "",0);

    /* (book)

        PAGE only

        When selected, the PAGE tab will display only the PAGE window. The
        windows PAGE_OUTPUT and PAGE_SYMBOL will be hidden.
    */
    CM_O_PGO = additem(c,"[ ] PAGE only",CM_TB,0,1,
                       "",0);
    CM_O = c - CM;

    /* (book)

        DEBUG menu
        ----------

    */
    c = addmenu(1,"Debug",0);

    /* (book)

        Show unaligned symbols

        The symbols with unknown alignment are displayed using gray
        ellipses. This option overrides the "show current class"
        feature.
    */
    CM_G_ALIGN = additem(c,"[ ] Show unaligned symbols",CM_TB,0,-1,
                        "",0);

    /* (book)

        Show localbin progress

        Display the progress of the extraction of symbols
        performed by the local binarizer.
    */
    CM_G_LB = additem(c,"[ ] Show localbin progress",CM_TB,0,-1,
                      "",0);

    /* (book)

        Activate Abagar

        Abagar is a powerful debug mode used to detect binarization
        problems. It works as follows: first load a pgm image and create
        a zone around the problematic region (typically a single letter).
        Now make sure the "Activate Agabar" option (debug menu) is on,
        and ask segmentation. Various operations will dump data when
        the segmentation enters the zone.

    */
    CM_G_ABAGAR = additem(c,"[ ] Activate Abagar",CM_TB,0,-1,
                          "",0);

    /* (book)

        Show lines (geometrical)

        Identify the lines (computed using geometrical criteria)
        when displaying the current document.
    */
    CM_G_GLINES = additem(c,"[ ] Show lines (geometrical)",CM_TR,0,-1,
                          "",0);

    /* (book)

        Bold Only

        Restrict the show symbols or show words feature
        (PAGE menu) to bold symbols or words.
    */
    CM_G_BO = additem(c,"[ ] Bold Only",CM_TR,1,-1,
                      "",0);

    /* (book)

        Italic Only

        Restrict the show symbols or show words feature
        (PAGE menu) to bold symbols or words.
    */
    CM_G_IO = additem(c,"[ ] Italic Only",CM_TR,0,-1,
                      "",0);

    /* (book)

        Search unexpected mismatches

        Compare all patters with same type and transliteration. Must
        be used with the "Show comparisons and wait" option on
        to diagnose symbol comparison problems.
    */
    CM_G_SUM = additem(c,"[ ] Search unexpected mismatches",CM_TB,1,-1,
                       "",0);

    /* (book)

        Attach vocab.txt

        Attach the contents of the file vocab.txt of the current
        directory (if any) to the DEBUG window. Used to test the
        text analisys code.
    */
    CM_G_VOCAB = additem(c,"[ ] Attach vocab.txt",CM_TR,1,-1,
                         "",0);

    /* (book)

        Attach log

        Attach the contents of message log to the DEBUG window.
        Used to inspect the log.
    */
    CM_G_LOG = additem(c,"[ ] Attach log",CM_TR,0,-1,
                         "",0);

    /* (book)

        Reset match counters

        Change to zero the cumulative matches field of all
        patterns.
    */
    CM_G_CM = additem(c,"Reset match counters",CM_TA,1,-1,
                      "",0);

    /* (book)

        Enter debug window

    */
    CM_G_DW = additem(c,"Enter debug window",CM_TA,0,-1,
                          "",0);

    CM_G = c - CM;

    /*

        Assign menu flags to the menu labels.

    */

    /* edit menu */
    cm_e_od      = F(CM_E_DOUBTS);
    cm_e_rescan  = F(CM_E_RESCAN);
    cm_e_pp      = F(CM_E_PP);
    cm_e_pp_c    = F(CM_E_PP_C);
    cm_e_fill    = F(CM_E_FILL);
    cm_e_fill_c  = F(CM_E_FILL_C);
    cm_e_sm      = F(CM_E_SM);
    cm_e_sp      = F(CM_E_SP);
    cm_e_st      = F(CM_E_ST);
    cm_e_sw      = F(CM_E_SW);
    cm_e_sh      = F(CM_E_SH);
    cm_e_sn      = F(CM_E_SN);
    cm_e_ac      = F(CM_E_AC);

    /* view menu */
    cm_v_small    = F(CM_V_SMALL);
    cm_v_medium   = F(CM_V_MEDIUM);
    cm_v_large    = F(CM_V_LARGE);
    cm_v_def      = F(CM_V_DEF);
    cm_v_hide     = F(CM_V_HIDE);
    cm_v_of       = F(CM_V_OF);
    cm_v_wclip    = F(CM_V_WCLIP);
    cm_v_map      = F(CM_V_MAP);
    cm_v_vhs      = F(CM_V_VHS);
    cm_v_cc       = F(CM_V_CC);
    cm_v_mat      = F(CM_V_MAT);
    cm_v_mat_k    = F(CM_V_MAT_K);
    cm_v_cmp_k    = F(CM_V_CMP_K);
    cm_v_cmp      = F(CM_V_CMP);
    cm_v_st       = F(CM_V_ST);

    /* options menu */
    cm_o_dkeys   = F(CM_O_DKEYS);
    cm_o_amenu   = F(CM_O_AMENU);
    cm_o_curr    = F(CM_O_CURR);
    cm_o_all     = F(CM_O_ALL);
    cm_o_pgo     = F(CM_O_PGO);

    /* alphabets menu */
    cm_a_arabic    = F(CM_A_ARABIC);
    cm_a_cyrillic  = F(CM_A_CYRILLIC);
    cm_a_greek     = F(CM_A_GREEK);
    cm_a_hebrew    = F(CM_A_HEBREW);
    cm_a_kana      = F(CM_A_KANA);
    cm_a_latin     = F(CM_A_LATIN);
    cm_a_ideogram  = F(CM_A_IDEOGRAM);
    cm_a_number    = F(CM_A_NUMBER);

    /* debug menu */
    cm_g_align  = F(CM_G_ALIGN);
    cm_g_lb     = F(CM_G_LB);
    cm_g_abagar = F(CM_G_ABAGAR);
    cm_g_glines = F(CM_G_GLINES);
    cm_g_bo     = F(CM_G_BO);
    cm_g_io     = F(CM_G_IO);
    cm_g_sum    = F(CM_G_SUM);
    cm_g_vocab  = F(CM_G_VOCAB);
    cm_g_log    = F(CM_G_LOG);

    /* PAGE menu */
    cm_d_pixels   = F(CM_D_PIXELS);
    cm_d_closures = F(CM_D_CLOSURES);
    cm_d_symbols  = F(CM_D_SYMBOLS);
    cm_d_words    = F(CM_D_WORDS);
    cm_d_ptype    = F(CM_D_PTYPE);
    cm_d_rs      = F(CM_D_RS);
    cm_d_bb      = F(CM_D_BB);

    /* PAGE (fatbits) menu */
    cm_b_skel     = F(CM_B_SKEL);
    cm_b_border   = F(CM_B_BORDER);
    cm_b_hs       = F(CM_B_HS);
    cm_b_hb       = F(CM_B_HB);

    /* initial values */
    if (RW < 0) {
        *cm_v_hide = 'X';
        RW = - RW;
    }
    if (dkeys) {
        *cm_o_dkeys = 'X';
        RW = - RW;
    }

    /* these are on by default */
    *cm_v_of         = 'X';
    *cm_v_cc         = 'X';

    *cm_o_curr       = 'X';

    *cm_a_number     = 'X';
    *cm_a_latin      = 'X';

    *cm_g_log        = 'X';

    *cm_e_rescan     = 'X';
}

/*

GUI initialization.

*/
void xpreamble()
{
    int xscreen;
    Colormap cmap;

    /* X preamble */
    if ((batch_mode == 0) && (wnd_ready == 0)) {

        /* connect to the X server */
        if (displayname == NULL)
            xd = XOpenDisplay("");
        else
            xd = XOpenDisplay(displayname);

        if (xd == NULL) {
            if (displayname != NULL)
                fatal(XE,"cannot connect display %s",displayname);
            else
                fatal(XE,"cannot connect display");
        }
    }

    /* context menus initialization */
    cmi();

    /* set alphabets button label */
    set_bl_alpha();

    /* alloc X font */
    if (batch_mode == 0)
        set_xfont();

    /* compute minimal window size */
    comp_wnd_size(WW,WH);

    /* buffers for revision bitmaps */
    rw = c_realloc(NULL,sizeof(char)*RWX*RWY,NULL);

    /* alloc colors and create application X window */
    if ((batch_mode == 0) && (wnd_ready == 0)) {
        char *color[6];
        int i,j,n,r,x,y;
        XColor exact;

        /* get default screen */
        xscreen = DefaultScreen(xd);

        /* get colors */
        cmap = DefaultColormap(xd,xscreen);
        j = 0;
        if ((cschema!=NULL) && ((n=strlen(cschema)) > 1)) {
            color[0] = cschema;
            for (i=0; i<n; ++i) {
                if (cschema[i] == ',') {
                    cschema[i++] = 0;
                    if (j < 5)
                        color[++j] = cschema + i;
                }
            }
        }
        if ((cschema!=NULL) && (strcmp(cschema,"c") == 0)) {
            r = XAllocNamedColor(xd,cmap,"black",&black,&exact) &&
                XAllocNamedColor(xd,cmap,"pink",&gray,&exact) &&
                XAllocNamedColor(xd,cmap,"white",&white,&exact) &&
                XAllocNamedColor(xd,cmap,"violet",&darkgray,&exact) &&
                XAllocNamedColor(xd,cmap,"gray50",&vdgray,&exact);
        }
        else if (j == 4) {
            r = XAllocNamedColor(xd,cmap,color[0],&black,&exact) &&
                XAllocNamedColor(xd,cmap,color[1],&gray,&exact) &&
                XAllocNamedColor(xd,cmap,color[2],&white,&exact) &&
                XAllocNamedColor(xd,cmap,color[3],&darkgray,&exact) &&
                XAllocNamedColor(xd,cmap,color[4],&vdgray,&exact);
        }
        else {
            r = XAllocNamedColor(xd,cmap,"black",&black,&exact) &&
                XAllocNamedColor(xd,cmap,"gray80",&gray,&exact) &&
                XAllocNamedColor(xd,cmap,"white",&white,&exact) &&
                XAllocNamedColor(xd,cmap,"gray60",&darkgray,&exact) &&
                XAllocNamedColor(xd,cmap,"gray40",&vdgray,&exact);
        }

        if ((!r) && verbose) {
            warn("Clara OCR could not alloc some colors. If the\n");
            warn("application window become unreadable, try to\n");
            warn("close Clara OCR and some applications that alloc\n");
            warn("many colors (e.g. Netscape), and start Clara OCR\n");
            warn("again\n");
        }

        /*
            Create window at (WX,WY), border width 2.
        */
        if (WX < 0)
            x = y = 0;
        else {
            x = WX;
            y = WY;
        }
        XW = XCreateSimpleWindow (xd,
                DefaultRootWindow(xd),x,y,WW,WH,2,
                white.pixel,darkgray.pixel);
        xw = (use_xb) ? pm : XW;
        have_xw = 1;

        /*
            Tell the WM about our positioning prefs
        */
        if (WX >= 0) {

            /* pointer to the hints structure. */
            XSizeHints* win_size_hints = XAllocSizeHints();
            if (!win_size_hints) {
                fatal(ME,"XAllocSizeHints");
            }

            /* the hints we want to fill in. */
            win_size_hints->flags = USPosition;

            /* the position */
            win_size_hints->x = WX;
            win_size_hints->y = WY;

            /* pass the position hints to the window manager. */
            XSetWMNormalHints(xd,xw,win_size_hints);
            XFree(win_size_hints);
        }

        /* ask for events */
        XSelectInput(xd,XW,evmasks);

        /* pop this window up on the screen */
        if (XMapRaised(xd,XW) == BadWindow) {
            fatal(XE,"could not create window");
        }

        /* graphics context(s) */
        if (!use_xb) {
            xgc = XCreateGC(xd,xw,0,0);
        }

        /* window ready flag */
        wnd_ready = 1;
    }

    /* initialize document windows */
    init_dws(1);

    /* PAGE special settings */
    dw[PAGE].rf = MRF;

    /* set the window title */
    if (batch_mode == 0) {
        char s[81];

        snprintf(s,80,"Clara OCR");
        s[80] = 0;
        swn(s);
    }

}

/*

Copy the viewable region on the PAGE (fatbits) window to
cfont.

*/
#define MAXHL 10
void copychar(void)
{
    int l,r,t,b,w,h;
    int k,cs[MAXHL],dx[MAXHL],dy[MAXHL],n;
    sdesc *y;

    /* make sure we're at the correct window */
    CDW = PAGE_FATBITS;

    /* mc limits */
    l = X0;
    r = X0 + HR - 1;
    t = Y0;
    b = Y0 + VR - 1;
    w = r-l+1;
    h = b-t+1;

    /* clear cfont */
    memset(cfont,WHITE,HR*VR);

    /* clear the border path */
    topfp = -1;

    /* look for closures that intersect the rectangle (l,r,t,b) */
    for (n=0,k=0; k<=topps; ++k) {
        int i,j;
        int ia,ib,ja,jb;

        /*
            Does the symbol k have intersection with the
            region we're interested on?
        */
        y = mc + ps[k];
        if ((intersize(y->l,y->r,l,r,NULL,NULL) > 0) &&
            (intersize(y->t,y->b,t,b,NULL,NULL) > 0)) {

            /*
                Is the symbol k entirely contained in the
                region we're interested on?
            */
            if ((n<MAXHL) && (l<=y->l) && (y->r<=r) && (t<=y->t) && (y->b<=b)) {
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
            for (i=ia; i<=ib; ++i) {
                for (j=ja; j<=jb; ++j) {

                    if (spixel(y,l+i,t+j) == BLACK)
                        cfont[i+j*HR] = BLACK;
                    /*
                    else
                        cfont[i+j*HR] = WHITE;
                    */
                }
            }
        }
    }

    /* compute the skeleton on the fly */
    if ((*cm_b_skel != ' ') || (*cm_b_border != ' ')) {
        int u,v,d,e,p;

        memset(cb,WHITE,LFS*FS);
        for (u=0; u<FS; ++u)
            for (v=0; v<FS; ++v)
                cb[u+v*LFS] = cfont[u+v*HR];
        if (*cm_b_skel != ' ')
            skel(-1,-1,0,0);
        else
            cb_border(0,0);
        for (v=0; v<FS; ++v) {
            d = v*LFS;
            e = v*HR;
            for (u=0; u<FS; ++u)
                 if ((p=cb[u+d]) == GRAY)
                     cfont[u+e] = BLACK;
                 else if (p == BLACK)
                     cfont[u+e] = WHITE;
        }
    }

    for (k=0; k<n; ++k) {
        int u,v,d,e,p;

        /* Show the pattern border */
        if ((*cm_b_hb != ' ') && (mc[cs[k]].bm >= 0)) {
            {
                int i,j,h;
                unsigned char m,*p;
                pdesc *d;

                memset(cb,WHITE,LFS*FS);
                d = pattern + id2idx(mc[cs[k]].bm);
                for (j=0; j<d->bh; ++j) {
                    p = d->b + BLS*j;
                    m = 128;
                    h = (j+dy[k])*LFS;
                    for (i=0; i<d->bw; ++i) {
                        if ((*p & m) != 0)
                            cb[i+dx[k]+h] = BLACK;
                        if ((m >>= 1) <= 0) {
                            ++p;
                            m = 128;
                        }
                    }
                }
            }
            cb_border(0,0);
            for (v=0; v<FS; ++v) {
                d = v*LFS;
                e = v*HR;
                for (u=0; u<FS; ++u)
                    if ((p=cb[u+d]) == GRAY)
                        cfont[u+e] |= D_MASK;
            }
        }

        /* Show the pattern skeleton */
        if ((*cm_b_hs != ' ') && (mc[cs[k]].bm >= 0)) {
            int i,j,h;
            unsigned char m,*p;
            pdesc *d;

            d = pattern + id2idx(mc[cs[k]].bm);
            dx[k] += (d->bw - d->sw) / 2;
            dy[k] += (d->bh - d->sh) / 2;
            for (j=0; j<d->sh; ++j) {
                p = d->s + BLS*j;
                m = 128;
                h = (j+dy[k])*HR;
                for (i=0; i<d->sw; ++i) {
                    if ((*p & m) != 0)
                        cfont[i+dx[k]+h] = WHITE;
                    if ((m >>= 1) <= 0) {
                        ++p;
                        m = 128;
                    }
                }
            }
        }
    }

    redraw_dw = 1;
    redraw_grid = 1;
    dw[PAGE_FATBITS].rg = 0;
}

/*

comp_wnd_size: compute the application window size.

This routine decides about the window size. The decision process
is as follows:

1. If a window size is specified through the parameters ww
(width) and wh (height), these are mandatory.

2. If the caller specify ww==0 and wh==0, then the routine
computes the minimum width and height, and set the window size to
these values.

3. If ww==-1 and wh==-1, then the current window size is used, or
enlarged if required.

*/
void comp_wnd_size(int ww,int wh)
{
    int i,dx,dy;
    int w,h;
    unsigned cww,cwh;
    int old_pt,old_ph;

    /* remember PT and PH */
    old_pt = PT;
    old_ph = PH;

    /* tab height is compute from the font height */
    TH = DFH + 6;

    /* menu bar height */
    MH = 2 + VS + DFH + VS + 2;

    /*
        Largest alphabet label size (currently hardcoded...)
    */
    BW = 8;

    /*
        Compute button width and height (the globals BW and BH)
        from the button labels and the font size.
    */
    for (i=0; i<BUTT_PER_COL; ++i) {
        int a,b;
        char *bl;

        bl = BL[i];
        for (a=b=0; a >= 0; ++a) {
            if ((bl[a] == ',') || (bl[a] == ':') || (bl[a] == 0)) {
                if (a-b > BW)
                    BW = a - b;
                if (bl[a] == 0)
                    a = -2;
                else
                    b = a;
            }
        }
    }
    BW += 1;
    BW *= DFW;
    BH = DFH + 4*VS;

    /*
        Compute minimum plate width and height.

        The plate width and height are computed basically from FS
        and ZPS, in order to the FSxFS grid fit in the plate.

        In order to guarantee minimum visibility for the
        pattern properties window, we add 30*DFW.

        The clearance is 10. The plate height must include the
        tab height (TH).

        Remark: these formulae are inverted at the end of this
             routine, so if you change them, please change that
             code too.
    */
    PW = 10 + (FS * (ZPS+1) + 10 + 30*DFW + 10 + RW) + 10;
    PH = TH + 10 + (FS * (ZPS+1) + 10 + RW) + 10;

    /* plate horizontal and vertical margins */
    PM = 10 + BW + (((cm_v_map == NULL) || (*cm_v_map == ' ')) ? 10 : 62);
    PT = MH + 10;

    /* get current width and height, if any */
    if (have_xw) {
        Window root;
        int x,y;
        unsigned b,d;

        XGetGeometry(xd,XW,&root,&x,&y,&cww,&cwh,&b,&d);
    }
    else {
        cww = 0;
        cwh = 0;
    }

    /*
        These are the expected differences (WH-PH) and (WW-PW)
        between the total height and width and the plate height
        and width.
    */
    dy = MH + 10 + 10 + DFH + 10;
    dx = PM + 10;

    /* oops.. requested height is mandatory */
    if (wh > 0) {
        WH = wh;

        /* plate height */
        PH = WH - dy;
    }

    /*
        First we compute the minimum plate height, then we add
        dy to obtain the total width.
    */
    {
        int j;

        /*
            the buttons must not exceed the plate
            vertical limits.
        */
        if (PH < (h=BUTT_PER_COL * (BH+4+VS)))
            PH = h;

        /*
            the welcome window must fit vertically in the plate
            (15*DFH is an approximation to its height.
        */
        if (PH < (h=15 * (DFH+VS)))
            PH = h;

        /*
            the alphabet map must not exceed
            the plate vertical limits.
        */
        if ((cm_v_map != NULL) && (*cm_v_map != ' ') && (PH < (h=16*max_sz)))
            PH = h;

        /*
            if the caller requests to preserve the geometry,
            then try not changing it.
        */
        if ((wh < 0) && (PH < (h=cwh-dy)))
            PH = h;

        /*
           is there a large menu?
        */
        for (j=0; j<=TOP_CM; ++j) {
            if (PH < (h=CM[j].n * (DFH+VS)))
                PH = h;
        }

        /*
           add vertical space to the menu bar, status line and
           clearances
        */
        WH = PH + dy;
    }

    /* oops.. requested width is mandatory */
    if (ww > 0) {
        WW = ww;

        /* plate width */
        PW = WW - dx;
    }

    /*
        First we compute the minimum plate width, then we add
        dx to obtain the total width.
    */
    {
        int a,i,lw;

        /* must fit 80 characters horizontally (status line) */
        if (PW < (w=80*DFW-dx))
            PW = w;

        /*
            if the caller requests to preserve the geometry,
            then try not changing it.
        */
        if ((ww < 0) && (PW < (w=cww-dx)))
            PW = w;

        /* minimum tab width */
        for (i=0, lw=0; i<=TOP_DW; ++i)
            if (lw < (a=DFW*strlen(dwname(i))))
                lw = a;
        if (PW < (w=((lw+20)+20)*TABN))
            PW = w;

        /*
           add button width, alphabet map width (if visible) and
           clearances
        */
        WW = PW + dx;
    }

    /* finally, try a nice 3/4 */
    if ((ww == 0) && (wh == 0)) {

        if (WW*3 < WH*4) {
            WW = (WH*4) / 3;
            PW = WW - dx;
        }
        else {
            WH = (WW*3) / 4;
            PH = WH - dy;
        }
    }

    /* resize the window if the current geometry differs from (WW,WH) */
    if (have_xw) {
        if ((WW != cww) || (WH != cwh)) {
            XResizeWindow(xd,XW,WW,WH);
        }
    }

    /* alloc or reallocate the display pixmap */
    if ((use_xb) && ((!have_pm) || (WW > pmw) || (WH > pmh))) {
        int d;

        /* free the current buffer (if any) */
        if (have_pm) {
            XFreeGC(xd,xgc);
            XFreePixmap(xd,pm);
        }

        /* alloc a new one */
        d = DefaultDepth(xd,DefaultScreen(xd));
        pm = XCreatePixmap(xd,DefaultRootWindow(xd),WW,WH,d);

        /* could not alloc: switch to unbuffered mode */
        if (pm == BadAlloc) {
            warn("could not alloc buffer, switching to unbuffered mode\n");
            have_pm = 0;
            use_xb = 0;
            xw = XW;
            if (have_xw) {
                xgc = XCreateGC(xd,XW,0,0);
            }
        }

        /* success */
        else {
            have_pm = 1;
            pmw = WW;
            pmh = WH;
            xw = pm;
            xgc = XCreateGC(xd,pm,0,0);
        }
    }

    /* tab width */
    TW = PW/TABN - 20;

    /* choose (the best possible)/(a sane) ZPS */
    {
        int z,zx,zy;

        zx = ((PW - 10 - 10 - 10 - 30*DFW - 10 - RW) / FS) - 1;
        if ((zx & 1) == 0)
            zx -= 1;
        zy = ((PH - TH - 10 - 10 - 10 - RW) / FS) - 1;
        if ((zy & 1) == 0)
            zy -= 1;
        if ((z=zx) > zy)
            z = zy;

        /* the best possible */
        /*
        if ((1 <= z) && (ZPS != z))
            ZPS = z;
        */

        /* a sane */
        if ((1 <= z) && (z < ZPS))
            ZPS = z;
    }

    /*
        PAGE special settings.

        When resizing the window, we try to preserve the same
        relative height for the windows PAGE, PAGE_OUTPUT and
        PAGE_SYMBOL, that share one mode of the PAGE tab.

        The height of these three windows is determined by
        the variables page_j1 and page_j2 (first and second
        junctions). So we need to recompute them.

          +------------------------------------------+
          | File Edit...                             |
          +------------------------------------------+
          | +------+   +----+ +--------+ +-------+   |
          | | zoom |   |page| |patterns| | tune  |   |
          | +------+ +-+    +-+        +-+       +-+ |
          | +------+ | +-------------------------+ | |
          | | zone | | | (PAGE)                  | | |
          | +------+ | |                         | | |
          | +------+ | +-------------------------+ | |
          | | OCR  | |                             | | - page_j1
          | +------+ | +-------------------------+ | |
          | +------+ | | (PAGE_OUTPUT)           | | |
          | | stop | | |                         | | |
          | +------+ | +-------------------------+ | |
          |     .    |                             | | - page_j2
          |     .    | +-------------------------+ | |
          |          | | (PAGE_SYMBOL)           | | |
          |          | |                         | | |
          |          | +-------------------------+ | |
          |          +-----------------------------+ |
          |                                          |
          | (status line)                            |
          +------------------------------------------+

    */
    if (page_j1 < 0) {
        opage_j1 = page_j1 = PT+TH+(PH-TH)/3;
        opage_j2 = page_j2 = PT+TH+2*(PH-TH)/3;
    }
    else {
        float a;

        a = ((float)(old_ph-TH)) / (page_j1-old_pt-TH);
        page_j1 = PT+TH+(PH-TH)/a;
        a = ((float)(old_ph-TH)) / (page_j2-old_pt-TH);
        page_j2 = PT+TH+(PH-TH)/a;

        /*
           make sure that the height of PAGE_OUTPUT is
           at least 4*RW.
        */
        if (page_j2 - page_j1 < 20+4*RW)
            page_j1 = page_j2 - 20 - 4*RW;

        /*
            make sure that the height of PAGE is at least
            4*RW.
        */
        if (page_j1-PT < TH+30+4*RW) {
            page_j1 = PT+TH+30+4*RW;
            page_j2 = page_j1+20+4*RW;
        }

        /*
            make sure that the height of PAGE_SYMBOL is
            at least 4*RW.
        */
        if (PT+PH-page_j2 <30+4*RW) {
            page_j2 = PT+PH-30-4*RW;
            page_j1 = page_j2-20-4*RW;
        }

        opage_j1 = page_j1;
        opage_j2 = page_j2;
    }
}

#ifdef HAVE_SIGNAL
/*

This is a first draft for a presentation routine. By now it
merely changes to large font, then to small font, returns to
default font and stops.

Remark: any call to new_alrm must be done just before returning from
this service.

*/
void cpresentation(int reset)
{
    /* presentation and current state */
    static int p,s;

    if ((have_s_ev) || (ocring)) {
        new_alrm(100000,2);
        return;
    }

    /* initialize presentation */
    if (reset) {
        pmode = 1;
        p = s = 0;
        xev.type = FocusIn;
        have_s_ev = 1;
        new_alrm(100000,2);
        return;
    }

    /*
        Presentation 0: change font
    */
    if (p == 0) {
        int delay;

        if (s == 0) {

            ++s;

            if (!mselect(CM_V_LARGE,&delay))
                ++s;
        }

        else if (s == 1) {
            if (!mselect(CM_V_SMALL,&delay))
                ++s;
        }

        else if (s == 2) {
            if (!mselect(CM_V_DEF,&delay))
                ++s;
        }

        else if (s == 3) {
            if (!fselect("imre.pbm",&delay))
                ++s;
        }

        else if (s == 4) {
            if (!bselect(bocr,&delay))
                ++s;
        }

        else if (s == 5) {
            if (!sselect(5,&delay))
                ++s;
        }

        else if (s == 6) {
            if (!press('r',&delay))
                ++s;
        }

        else if (s == 7) {
            if (!sselect(235,&delay))
                ++s;
        }

        else if (s == 8) {
            if (!press('h',&delay))
                ++s;
        }

        else if (s == 9) {
            if (!bselect(bocr,&delay))
                ++s;
        }

        else if (s == 10) {
            pmode = 0;
            if (allow_pres == 2)
                finish = 1;
        }

        if (pmode) {
            if (allow_pres == 2)
                new_alrm(250000,2);
            else
                new_alrm(delay,2);
        }
    }
}
#endif
