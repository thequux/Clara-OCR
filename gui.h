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

gui.h: Declarations that depend on X11

*/

#include <time.h>
#include <sys/time.h>
#include <gtk/gtk.h>
#include <glib.h>
/*

X display and contexts.

*/
extern char *displayname;
// extern Display *xd;
// extern GC xgc;

/* The application X window */
// extern Window XW;
// extern XID xw;
// extern Pixmap pm;
extern int have_xw, have_pm, use_xb;

/* Xcolors */
// extern XColor white,black,gray,darkgray,vdgray;


/* default font */
//extern XFontStruct *dfont;
//extern int DFH,DFW,DFA,DFD;

/* the event */
//extern int have_s_ev;
//extern XEvent xev;

/* some geometric parameters */
//extern int WW, WH;

/* vertical separation between text lines */
//extern int VS;

/* redraw flags */
/*extern int redraw_button,
    redraw_bg,
    redraw_grid,
    redraw_stline,
    redraw_dw,
    redraw_inp,
    redraw_tab,
    redraw_zone,
    redraw_menu,
    redraw_pbar,
    redraw_j1,
    redraw_j2,
    redraw_map,
    redraw_flea;
*/
/*

Graphic elements

*/
#define GC_CL 1
#define GC_T 2

/* (devel)

Graphic elements
----------------

The rendering of each element on the HTML page creates one graphic
element ("GE" for short).

Free text is rendered to one GE of type GE_TEXT per word. This is
a "feature". The rendering procedures are currently unable to put
more than one text word per GE.

IMG tags are rendered to one GE of type GE_IMG. Note that the
value of the SRC element cannot be the name of a file containing
a image, but must be "internal" or "pattern/n". These are
keywords to the web clip and the bitmap the pattern "n". The
value of the SRC attribute is stored on the "txt" field of the
GE.

INPUT tags with TYPE=TEXT are rendered to one GE of type
GE_INPUT. The predefined value of the field (attribute VALUE) is
stored on the field "txt" of the GE. The name of the field
(attribute NAME) is stored on the field "arg" of the GE.

The Clara OCR HTML support added INPUT tags with TYPE=NUMBER. They're
rendered like TYPE=TEXT, but two steppers are added to faster
selection. So such tags will create three GEs (left stepper, input
field, and right stepper).

INPUT tags with TYPE=CHECKBOX are rendered to one GE of type
GE_CBOX. The variable name (attribute NAME) is stored on the "arg"
field. The argument to VALUE is stored on the field "txt". The status
of the checkbox is stored on the "iarg" field (2 means "checked", 0
means "not checked").

INPUT tags with TYPE=RADIO are rendered just like CHECKBOX. The
only difference is the type GE_RADIO instead GE_CBOX.

SELECT tags (starting a SELECT element) are rendered to one GE of
type SELECT. In fact, the entire SELECT element is stored on only
one GE. Each SELECT element originates one standard context menu,
as implemented by the Clara GUI. The "iarg" field stores the menu
index. The free text on each OPTION element is stored as an item
label on the context menu. The implementation of the SELECT
element is currently buggy: (a) for each renderization, one entry
on the array of context menus will be allocated, and will never
be freed, and (b) The attribute NAME of the SELECT won't be
stored anywhere.

INPUT tags with TYPE=SUBMIT are rendered to one GE of type
GE_SUBMIT. The value of the attribute VALUE is stored on the "txt"
field. The value of the ACTION attribute is stored on the field
"arg". The field "a" will store HTA_SUBMIT.

TD tags are rendered to one GE of type GE_RECT. The value of the
BGCOLOR attribute is stored on the "bg" field as a code (only the
colors known by the Clara GUI are supported: WHITE, BLACK, GRAY,
DARKGRAY and VDGRAY). The coordianates of the cell within the table
are stored on the fields "tr" and "tc".

All other supported tags do not generate GEs.

*/
typedef struct {
        char type;              /* graphic element type */
        int l, r, t, b;         /* {left,right,top,bottom}-most coordinate */
        int tsz;                /* text buffer size */
        char *txt;              /* text buffer */
        char a;                 /* action type (0, HTA_HREF, HTA_HREF_C or HTA_SUBMIT) */
        int argsz;              /* size of arg buffer */
        char *arg;              /* HREF URL */
        int iarg;               /* integer argument */
        int tr, tc;             /* table row and column */
        int bg;                 /* background color */
} edesc;

/*

Document window.

*/
typedef struct {

        /*
           Portion of the page being exhibited.
         */
        int x0, y0;
        int hr, vr;

        /*
           Portion of the application X window being used
           to display (part of) the page.
         */
        int dm, dt;
        int dw, dh;

        /* document total size */
        int grx, gry;

        /* grid separation and width */
        int gs, gw;

        /* reduction factor */
        int rf;

        /* visibility flag */
        char v;

        /* HTML flag */
        char html;

        /* HTML source flag */
        char hs;

        /* regenerate flag */
        char rg;

        /* draw scrollbars flag */
        char ds;

        /* underline links flag */
        char ulink;

        /* graphic elements */
        edesc *p_ge;
        int p_gesz;
        int p_topge;
        int p_curr_hti;

        /* left margin */
        int hm;

} dwdesc;

/*

Parameters of the current document window.

*/
extern int CDW;
extern int PAGE_CM, PATTERN_CM, HISTORY_CM, TUNE_CM;
extern dwdesc dw[];
#define X0 (dw[CDW].x0)
#define Y0 (dw[CDW].y0)
#define HR (dw[CDW].hr)
#define VR (dw[CDW].vr)
#define RF (dw[CDW].rf)

#define DM (dw[CDW].dm)
#define DT (dw[CDW].dt)
#define DW (dw[CDW].dw)
#define DH (dw[CDW].dh)

#define GS (dw[CDW].gs)
#define GW (dw[CDW].gw)

#define HTML (dw[CDW].html)
#define DS (dw[CDW].ds)
#define RG (dw[CDW].rg)
#define ULINK (dw[CDW].ulink)
#define HS (dw[CDW].hs)
#define HM (dw[CDW].hm)

#define GRX (dw[CDW].grx)
#define GRY (dw[CDW].gry)

#define ge (dw[CDW].p_ge)
#define gesz (dw[CDW].p_gesz)
#define topge (dw[CDW].p_topge)
#define curr_hti (dw[CDW].p_curr_hti)

/*

Restricted clip window for the PAGE window.

*/
extern int PHR, PVR, PDM, PDT;

/* closures for welcome page */
extern cldesc c12, c13, c14, c15, c16, c17, c18, c19;
extern int wx0, wx1, wy0, wy1;

/* tab labels */
extern char *tabl[];

/* geometry */
//extern int WH,WW,WX,WY,PH,PW;
//extern int BW,MRF,TW,TH,PM,PT;
//extern int RW;
//extern int MH;

/*

Menu ids and menu item mask..

*/
#define CM_IT_WD 5
#define CM_IT_M ((unsigned)((1 << CM_IT_WD) - 1))
#define mid(x) (CM + ((x & (~CM_IT_M)) >> CM_IT_WD))

/*

The flag associated with a menu item (if any). This flag in
fact is the second character of the label.

*/
//#define F(i) (mid(i)->l + (i&CM_IT_M)*(MAX_MT+1) + 1)

/*

GE types.

*/
#define GE_TEXT 1
#define GE_IMG 2
#define GE_INPUT 3
#define GE_CBOX 4
#define GE_SELECT 5
#define GE_SUBMIT 6
#define GE_RECT 7
#define GE_RADIO 8
#define GE_STEPPER 9

/*

Sliding mode

*/
extern int sliding, s0;

/*

Types of actions. The current implementation is unable to link a
multiword text. It links each word of the text independently. The
first word is linked to the entire text of the link and is typed
HTA_HREF. The remaining words are linked to the index of the
first word on the array of elements and are typed HTA_HREF_C.

*/
#define HTA_HREF 1
#define HTA_HREF_C 2
#define HTA_SUBMIT 3

/*

Divisors on PAGE tab.

*/
int page_j1, page_j2, opage_j1, opage_j2;

/* current color */
extern int COLOR;

/* Various GUI elements... */
GtkWidget *btnOcr, *btnStop, *mnuPage, *mnuFatbits, *mainwin;

/* services */
void edit_pattern(int change);
cmdesc *addmenu(int a, char *tt, int m);

/* html services */
void new_ge(void);
void text2ge(char *t);
void html2ge(char *ht, int lm);
int cmp_pattern(int i, int j);
int cmp_gep(int a, int b);
void ge2txt(void);
void submit_form(int hti);
void html_action(int x, int y);
void p2cf(void);

/* page generation services */
typedef enum {
        OE_FULL_HTML = 0,
        OE_ENCAP_HTML = 1,
        OE_TEXT = 2,
        OE_DJVU = 3,
} output_encap;

void mk_page_symbol(int c);
void mk_page_output(output_encap encap);
void mk_pattern_list(void);
void mk_pattern_types(void);
void mk_pattern_props(void);
void mk_page_list(void);
void mk_history(int n);
void mk_tune(void);
void mk_tune_skel(void);

/* other */
void redraw_inp_str(void);

void redraw_document_window(void);
void new_page();
void redraw_flea(void);
void show_message(const char *msg);
