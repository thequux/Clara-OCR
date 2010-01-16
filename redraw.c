/* -*- mode: c; c-basic-offset: 4 -*- */
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

redraw.c: The function redraw.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "common.h"
#include "gui.h"

/*

Menu flags.

*/
char *cm_v_small = NULL,
     *cm_v_medium = NULL,
     *cm_v_large = NULL,
     *cm_v_def = NULL,
     *cm_v_hide = NULL,
     *cm_v_of = NULL,
     *cm_v_wclip = NULL,
     *cm_v_map = NULL,
     *cm_v_vhs = NULL,
     *cm_v_cc = NULL,
     *cm_v_mat = NULL,
     *cm_v_mat_k = NULL,
     *cm_v_cmp_k = NULL,
     *cm_v_cmp = NULL,
     *cm_v_st = NULL,

     *cm_e_od = NULL,
     *cm_e_rescan = NULL,
     *cm_e_pp = NULL,
     *cm_e_pp_c = NULL,
     *cm_e_fill = NULL,
     *cm_e_fill_c = NULL,
     *cm_e_sm = NULL,
     *cm_e_st = NULL,
     *cm_e_sp = NULL,
     *cm_e_sw = NULL,
     *cm_e_sh = NULL,
     *cm_e_sn = NULL,
     *cm_e_ac = NULL,

     *cm_o_curr = NULL,
     *cm_o_all = NULL,
     *cm_o_dkeys = NULL,
     *cm_o_amenu = NULL,
     *cm_o_pgo = NULL,

     *cm_a_arabic = NULL,
     *cm_a_cyrillic = NULL,
     *cm_a_greek = NULL,
     *cm_a_hebrew = NULL,
     *cm_a_latin = NULL,
     *cm_a_kana = NULL,
     *cm_a_abbrev = NULL,
     *cm_a_number = NULL,
     *cm_a_ideogram = NULL,

     *cm_b_skel = NULL,
     *cm_b_border = NULL,
     *cm_b_hs = NULL,
     *cm_b_hb = NULL,

     *cm_d_pixels = NULL,
     *cm_d_closures = NULL,
     *cm_d_symbols = NULL,
     *cm_d_words = NULL,
     *cm_d_ptype = NULL,
     *cm_d_rs = NULL,
     *cm_d_bb = NULL,

     *cm_g_glines = NULL,
     *cm_g_lb = NULL,
     *cm_g_abagar = NULL,
     *cm_g_align = NULL,
     *cm_g_bo = NULL,
     *cm_g_io = NULL,
     *cm_g_sum = NULL,
     *cm_g_vocab = NULL,
     *cm_g_log = NULL;
#if 0
/*

Arrows used as labels in some buttons (deprecated).

*/
short gldbl[] = {
    0,2,1,2,2,2,3,2,4,2,5,2,6,2,7,2,8,2,9,2,10,2,11,2,12,2,
    1,1,2,1,3,0,4,0,
    3,1,4,1,
    1,3,2,3,3,4,4,4,
    3,3,4,3,
    6,1,7,1,8,0,9,0,
    8,1,9,1,
    6,3,7,3,8,4,9,4,
    8,3,9,3
};
short glend[] = {
    0,2,1,2,2,2,3,2,4,2,5,2,6,2,7,2,8,2,9,2,10,2,11,2,12,2,
    2,1,3,1,4,0,5,0,
    4,1,5,1,
    2,3,3,3,4,4,5,4,
    4,3,5,3,
    0,1,0,0,
    0,3,0,4
};
short glleft[] = {
    0,2,1,2,2,2,3,2,4,2,5,2,6,2,7,2,8,2,9,2,10,2,11,2,12,2,
    2,1,3,1,4,0,5,0,
    4,1,5,1,
    2,3,3,3,4,4,5,4,
    4,3,5,3,
};

/*

Menus stuff
-----------

CX0 and CY0 are the top-left window-relative coordinates of the
current context menu. When the user invokes one menu clicking the
menu bar or another sensitive region, CX0 and CY0 store the
pointer position. But when the function redraw effectively pops
the menu, it choose a "best" placement and may change CX0 and
CY0. However, theoriginal pointer position is stored into
CX0_orig and CY0_orig.

CW and CH are the width ahd height of the current menu.

The current menu (if any) is pointed by cmenu. If there is no
current menu, cmenu is NULL.

The array CM stores all registered menus. The size of CM is CM_SZ
and the top of CM (largest index of CM entry in use) is TOP_CM.

*/
int CX0,CY0,CX0_orig=-1,CY0_orig=-1,CW,CH;
cmdesc *cmenu=NULL;
cmdesc *CM=NULL;
int TOP_CM=-1,CM_SZ=0;

/*

Sliding mode

0 .. not sliding
1 .. sliding horizontally
2 .. sliding vertically
3 .. resizing PAGE/OUTPUT junction
4 .. resizing OUTPUT/SYMBOL junction

*/
int sliding = 0,s0;

/* (devel)

Geometry of the application window
----------------------------------

The source code frequently refers some global variables that define
the position and size of the main componts (the plate, buttons,
etc). Most of these variables are set by comp_wnd_size. The variables
are:

    WH  .. application window height
    WW  .. application window width
    PH  .. plate height
    PW  .. plate width
    BW  .. button width
    BH  .. button width
    MRF .. maximum reduction factor
    TW  .. tab width
    TH  .. tab height
    PM  .. plate horizontal margin
    PT  .. plate top margin
    RW  .. scrollbar width
    MH  .. menubar heigth

MRF applies to the scanned document and to the web clip.

*/
int WH,WW,WX,WY,PH,PW;
int BW,BH,MRF=4,TW,TH,PM,PT;
int RW=12;
int MH;

/*

BUTTONS

*/
char *button = NULL;
int BUTT_PER_COL=0,bl_sz=0;
char **BL = NULL;

/* number of alphabets */
int nalpha;

/*

Divisors on PAGE tab.

*/
int page_j1=-1,page_j2=-1,opage_j1=-1,opage_j2=-1;
int LAST_PAGE_DT,LAST_PAGE_DM,LAST_PAGE_DH;
int LAST_OUTPUT_DT,LAST_OUTPUT_DH;
int LAST_SYMBOL_DT,LAST_SYMBOL_DH;

/*

Really thin (1-pixel) things (like "i" or "l" or "." as
renderized by the GUI using small fonts) are not displayed unless
this flag is set.

*/
int draw_thin=1;

/*

Make sure that the current window does not surpass the
document limits, changing X0 and Y0 if necessary.

If cursoron is nonzero, make the best efforts to make
sure that the cursor in within the window, changing X0 and
Y0 if necessary.

*/
void check_dlimits(int cursoron)
{
    /* make sure the current item is visible */
    if (cursoron) {

        /* PAGE */
        if ((CDW == PAGE) && (0 <= curr_mc) && (curr_mc <= tops)) {
            if (mc[curr_mc].l < X0)
                X0 = mc[curr_mc].l - HR/2;
            if (mc[curr_mc].r >= X0 + HR)
                X0 = mc[curr_mc].r - HR/2;
            if (mc[curr_mc].b < Y0)
                Y0 = mc[curr_mc].t - VR/2;
            if (mc[curr_mc].t >= Y0 + VR)
                Y0 = mc[curr_mc].b - VR/2;
        }

        /* HTML windows */
        else if ((HTML) && (0 <= curr_hti) && (curr_hti <= topge)) {
            if (ge[curr_hti].l < X0)
                X0 = ge[curr_hti].l - HR/2;
            if (ge[curr_hti].r >= X0 + HR)
                X0 = ge[curr_hti].r - HR/2;
            if (ge[curr_hti].b < Y0)
                Y0 = ge[curr_hti].t - VR/2;
            if (ge[curr_hti].t >= Y0 + VR)
                Y0 = ge[curr_hti].b - VR/2;
        }
    }

    if (X0 > (GRX-HR))
        X0 = GRX - HR;
    if (X0 < 0)
        X0 = 0;
    if (Y0 > (GRY-VR))
        Y0 = GRY - VR;
    if (Y0 < 0)
        Y0 = 0;
}

/*

Draw button
-----------

         i        i+w-1
       j +-+-----+-+
         + +-----+ |
         | |     | |
         | |     | |
         | |     | |
         | |     | |
         | |     | |
         + +-----+ |
   j+h-1 +---------+

If p is 0, draw the button "unpressed". If p is 1, draw the
button "pressed" ("on"). If p is 2, draw the button "checked"
using an "x" character. If p is 3, draw the button "checked" but
building an "x" using XDrawLine.

*/
#if 0
void draw_button(int i,int j,int w,int h,int p,char *l)
{

    /* button background */
    XSetForeground(xd,xgc,gray.pixel);
    XFillRectangle(xd,xw,xgc,i+2,j+2,w-3,h-3);

    /* shadow */
    XSetLineAttributes(xd,xgc,1,LineSolid,CapButt,JoinRound);
    XSetForeground(xd,xgc,((p==1)?white.pixel:vdgray.pixel));
    XDrawLine(xd,xw,xgc,i,j+h-1,i+w,j+h-1);
    XDrawLine(xd,xw,xgc,i+1,j+h-2,i+w-2,j+h-2);
    XDrawLine(xd,xw,xgc,i+w-1,j+h-1,i+w-1,j);
    XDrawLine(xd,xw,xgc,i+w-2,j+h-2,i+w-2,j+1);

    /* light */
    XSetForeground(xd,xgc,((p==1)?vdgray.pixel:white.pixel));
    XDrawLine(xd,xw,xgc,i,j,i+w-1,j);
    XDrawLine(xd,xw,xgc,i+1,j+1,i+w-2,j+1);
    XDrawLine(xd,xw,xgc,i,j,i,j+h-1);
    XDrawLine(xd,xw,xgc,i+1,j+1,i+1,j+h-2);

    /* check mark */
    if (p == 2) {
        int x,y,u,v;

        if (w&1) {
            x = i+3;
            u = i+w-5;
        }
        else {
            x = i+4;
            u = i+w-5;
        }
        if (h&1) {
            y = j+3;
            v = j+h-5;
        }
        else {
            y = j+4;
            v = j+h-5;
        }
        XSetForeground(xd,xgc,black.pixel);
        XDrawLine(xd,xw,xgc,x,y,u+1,v+1);
        XDrawLine(xd,xw,xgc,x,v,u+1,y-1);
    }
    else if (p == 3)
        l = "x";

    /* label */
    if (l != NULL) {
        XTextItem xti;

        XSetForeground (xd,xgc,black.pixel);
        xti.delta = 0;
        xti.font = dfont->fid;
        xti.chars = l;
        if ((xti.nchars = strlen(xti.chars)) > 0)
            XDrawText(xd,xw,xgc,i+(w-DFW*xti.nchars)/2,
                      j+(h-DFH)/2+(dfont->max_bounds).ascent,&xti,1);
    }
}
/*

Draw arrow

    t: type (0=double arrow, 1=end arrow, 2=left arrow)
    0: orientation (0=left, 1=right, 2=up, 3=down)

*/
void draw_arrow(int t,int x,int y,int o)
{
    int i,s;
    XPoint dl[37];
    short *arr;

    arr =  (t == 0) ? gldbl : ((t == 1) ? glend : glleft);
    s =  (t == 0) ? 37 : ((t == 1) ? 29 : 25);

    if (o == 0) {
        for (i=0; i<s; ++i) {
            dl[i].x = arr[2*i]+x;
            dl[i].y = arr[2*i+1]+y-2;
        }
    }
    else if (o == 1) {
        for (i=0; i<s; ++i) {
            dl[i].x = (12-arr[2*i])+x;
            dl[i].y = arr[2*i+1]+y-2;
        }
    }
    else if (o == 2) {
        for (i=0; i<s; ++i) {
            dl[i].x = arr[2*i+1]+x-2;
            dl[i].y = arr[2*i]+y;
        }
    }
    else if (o == 3) {
        for (i=0; i<s; ++i) {
            dl[i].x = arr[2*i+1]+x-2;
            dl[i].y = (12-arr[2*i])+y;
        }
    }
    XDrawPoints(xd,xw,xgc,dl,s,CoordModeOrigin);
}

/*

Draw frame. A frame of width t is draw around the rectangle
with top left (i,j) and width w and heigth h. This rectangle
remains untouched. The frame is draw using t pixels to the
left, right, top and bottom out of the rectangle. So the
frame will be (frame pixels are shown with 'X'):


           i-t i      i+w-1 i+w-1+t
            |  |          |  |

      j-t - XXXXXXXXXXXXXXXXXX
            XXXXXXXXXXXXXXXXXX
            XXXXXXXXXXXXXXXXXX
        j - XXX+----------+XXX
            XXX|          |XXX
            XXX|          |XXX
            XXX|          |XXX
            XXX|          |XXX
            XXX|          |XXX
            XXX|          |XXX
    j+h-1 - XXX+----------+XXX
            XXXXXXXXXXXXXXXXXX
            XXXXXXXXXXXXXXXXXX
  j+h-1+t - XXXXXXXXXXXXXXXXXX


*/
void draw_frame(int i,int j,int w,int h,int t)
{
    XFillRectangle(xd,xw,xgc,i-t,j-t,w+t,t);
    XFillRectangle(xd,xw,xgc,i+w,j-t,t,h+t);
    XFillRectangle(xd,xw,xgc,i-t,j,t,h+t);
    XFillRectangle(xd,xw,xgc,i,j+h,w+t,t);
}

/*

Draw buttonbar

*/
void draw_bb(int x,int y,char *BL[],int b0)
{
    int i,j;
    XTextItem xti;

    xti.delta = 0;
    xti.font = dfont->fid;
    for (i=j=0; i<BUTT_PER_COL; ++i) {
        int k,lk,t,f;
        char tl[64];

        /* for multi-label buttons the flag f must contain 0*/
        f = 0;

        /* btype label is handled separately */
        if (i == btype) {
            sprintf(tl,"%d",button[btype]);
            xti.chars = tl;
            xti.nchars = strlen(xti.chars);
            f = 1;
        }

        /* BL stores the labels of the other buttons */
        else {

            /* extract current set of labels */
            xti.chars = BL[i];
            xti.nchars = strlen(xti.chars);

            /* extract sublabel */
            for (k=lk=t=0; k<=xti.nchars; ++k) {
                if ((k==xti.nchars) || (xti.chars[k] == ':')) {
                    f |= (k<xti.nchars);
                    if (t == button[i+b0]) {
                        xti.chars += lk;
                        xti.nchars = k-lk;
                    }
                    else
                        lk = k+1;
                    ++t;
                }
            }
        }

        /* draw button with label */
        if ((redraw_button < 0) || (redraw_button == i+b0)) {
            XSetForeground(xd,xgc,gray.pixel);
            draw_button(x,y+i*(BH+VS),BW,BH,(!f) && (button[i+b0]!=0),NULL);
            XSetForeground(xd,xgc,black.pixel);
            XDrawText(xd,xw,xgc,x+(BW-DFW*xti.nchars)/2,
                y+i*(BH+VS)+(BH-DFH)/2+(dfont->max_bounds).ascent,&xti,1);
        }
    }

    /* buttonbar frame */
    if (redraw_button < 0) {
        XSetForeground(xd,xgc,vdgray.pixel);
        draw_frame(x,y,BW,BUTT_PER_COL*(BH+VS)-VS,1);
        XSetForeground(xd,xgc,white.pixel);
        draw_frame(x-1,y-1,BW+2,BUTT_PER_COL*(BH+VS)-VS+2,1);
    }
}

/*

Draw tab t at coordinates (x,y).

*/
void draw_tab(int x,int y,int t) {
    XTextItem xti;
    int k;
    short arc[] = {
        0,0,0,1,0,2,1,0,2,0
    };
    short arc2[] = {
        0,3,1,3,1,2,1,1,2,1,3,1,3,0
    };

    /* prepare label */
    xti.delta = 0;
    xti.font = dfont->fid;
    if (tab == t)
        xti.chars = dwname(CDW);
    else
        xti.chars = tabl[t];
    xti.nchars = strlen(xti.chars);

    /* tab background and label */
    XSetForeground(xd,xgc,gray.pixel);
    XFillRectangle(xd,xw,xgc,x,y,TW,(tab!=t)?TH-2:TH+1);
    XSetForeground(xd,xgc,black.pixel);
    XDrawText(xd,xw,xgc,x+(TW-4-DFW*xti.nchars)/2,y+DFH,&xti,1);

    /* corners */
    XSetForeground (xd,xgc,darkgray.pixel);
    for (k=0; k<5; ++k) {
        XDrawPoint(xd,xw,xgc,x+arc[2*k],y+arc[2*k+1]);
        XDrawPoint(xd,xw,xgc,x+TW-1-arc[2*k],y+arc[2*k+1]);
    }

    /* current tab decoration */
    if (tab == t) {

        /* white portion frame */
        XSetForeground (xd,xgc,white.pixel);
        for (k=0; k<7; ++k)
            XDrawPoint(xd,xw,xgc,x+arc2[2*k],y+arc2[2*k+1]);
        XSetLineAttributes(xd,xgc,2,LineSolid,CapNotLast,JoinRound);
        XDrawLine(xd,xw,xgc,x+4,y,x+TW-1-3,y);
        XDrawLine(xd,xw,xgc,x,y+4,x,y+TH+1);

        /* top line of the plate frame */
        XDrawLine(xd,xw,xgc,PM,PT+TH,x,PT+TH);
        XDrawLine(xd,xw,xgc,x+TW,PT+TH,PM+PW-1,PT+TH);

        /* shadowed portion of the frame */
        XSetForeground (xd,xgc,vdgray.pixel);
        for (k=0; k<7; ++k)
            XDrawPoint(xd,xw,xgc,x+TW-1-arc2[2*k],y+arc2[2*k+1]);
        XDrawLine(xd,xw,xgc,x+TW-1,y+4,x+TW-1,y+TH+1);
        XSetLineAttributes(xd,xgc,1,LineSolid,CapNotLast,JoinRound);
    }
}
#endif

/*

Draw the bitmap r using 4 colors. The bitmap r is assumed to be a
matrix RCYxRCX of chars.

*/
void draw_bm(unsigned char *r,int RCX,int RCY,int x,int y)
{
    int i,j,u,v;

    XSetForeground (xd,xgc,white.pixel);
    for (i=u=0; u<RCX; ++i,u+=plist_rf)
        for (j=v=0; v<RCY; ++j,v+=plist_rf)
            if (r[u+v*RCX] == WHITE)
                XDrawPoint(xd,xw,xgc,x+i,y+j);

    XSetForeground (xd,xgc,gray.pixel);
    for (i=u=0; u<RCX; ++i,u+=plist_rf)
        for (j=v=0; v<RCY; ++j,v+=plist_rf)
            if (r[u+v*RCX] == GRAY)
                XDrawPoint(xd,xw,xgc,x+i,y+j);

    XSetForeground (xd,xgc,vdgray.pixel);
    for (i=u=0; u<RCX; ++i,u+=plist_rf)
        for (j=v=0; v<RCY; ++j,v+=plist_rf)
            if (r[u+v*RCX] == VDGRAY)
                XDrawPoint(xd,xw,xgc,x+i,y+j);

    XSetForeground (xd,xgc,black.pixel);
    for (i=u=0; u<RCX; ++i,u+=plist_rf)
        for (j=v=0; v<RCY; ++j,v+=plist_rf)
            if (r[u+v*RCX] == BLACK)
                XDrawPoint(xd,xw,xgc,x+i,y+j);
}

/*

Draw the pixmap r using 4 colors. The bitmap r is assumed to be a
matrix RCYxRCX of chars.

*/
void draw_pm(unsigned char *r,int RCX,int RCY,int x,int y)
{
    int i,j,t,u,v,rf,m,bpl;
    unsigned char *p;

    /* make sure CDW is the correct index */
    CDW = PAGE;
    rf = dw[PAGE].rf;

    /* black-and-white slow code, works on any X display */
    if ((disp_opt == 0) || (pm_t == 1)) {
        int np,T;
        XPoint xp[100];

        XSetForeground (xd,xgc,white.pixel);
        XFillRectangle(xd,xw,xgc,DM,DT,DW,DH);
        XSetForeground (xd,xgc,black.pixel);

        if (pm_t == 8) {

            /*
                thresholding value.
            */
            /*
            if ((ocring) &&
                (onlystep == OCR_OTHER) &&
                (ocr_other == 3))
                T = thresh_val;
            else
                T = 128;
            */
            T = thresh_val;

            for (np=0, i=0, u=X0; (u<RCX) && (i<DW); ++i,u+=rf) {
                for (j=0, v=Y0; (v<RCY) && (j<DH); ++j,v+=rf)
                    if (r[u+v*RCX] < T) {

                        if (np >= 100) {
                            XDrawPoints(xd,xw,xgc,xp,np,CoordModeOrigin);
                            np = 0;
                        }
                        xp[np].x = DM+i;
                        xp[np].y = DT+j;
                        ++np;
                    }
            }
            if (np > 0) {
                XDrawPoints(xd,xw,xgc,xp,np,CoordModeOrigin);
            }
        }

        else {

            for (np=0, i=0, u=X0; (u<RCX) && (i<DW); ++i,u+=rf) {

                m = (1 << (7-u%8));
                bpl = (XRES/8) + ((XRES%8) != 0);
                p = pixmap + Y0*bpl + u/8;

                for (j=0, v=Y0; (v<RCY) && (j<DH); ++j,v+=rf,p+=RF*bpl)
                    if (*p & m) {

                        if (np >= 100) {
                            XDrawPoints(xd,xw,xgc,xp,np,CoordModeOrigin);
                            np = 0;
                        }
                        xp[np].x = DM+i;
                        xp[np].y = DT+j;
                        ++np;
                    }
            }
            if (np > 0) {
                XDrawPoints(xd,xw,xgc,xp,np,CoordModeOrigin);
            }
        }
    }

    /* optimized per-depth code */
    else {

        /* color masks and visual (for truecolor) */
        static int ht;
        static int rd,gd,bd,rw,gw,bw;
        static Visual *tv;

        /* other */
        static XImage *image = NULL;
        static unsigned char *imb = NULL;
        static int cw=-1,ch=-1,imbsz=0,d=-1,pad,bs;
        static unsigned long cmap[256];
        int bpl;

        /* one-time initializer */
        if (d < 0) {
            XVisualInfo *vinfo;
            XVisualInfo tmpl;
            int n;
            long m;

            /* do we have truecolor? */
            tmpl.screen = DefaultScreen(xd);
            tmpl.class = TrueColor;
            m = VisualClassMask;
            vinfo = XGetVisualInfo(xd,m,&tmpl,&n);
	    
            /* yes: get visual, depth and RGB info */
            if (vinfo != NULL) {
                unsigned long rm,gm,bm;

                ht = 1;
                tv = vinfo[0].visual;
                rm = vinfo[0].red_mask;
                gm = vinfo[0].green_mask;
                bm = vinfo[0].blue_mask;
                d = vinfo[0].depth;
                XFree(vinfo);

                /* RGB displacement and width */
                for (rd=0; ((rm & 1) == 0); ++rd, rm >>= 1);
                for (rw=0; ((rm & 1) == 1); ++rw, rm >>= 1);
                for (gd=0; ((gm & 1) == 0); ++gd, gm >>= 1);
                for (gw=0; ((gm & 1) == 1); ++gw, gm >>= 1);
                for (bd=0; ((bm & 1) == 0); ++bd, bm >>= 1);
                for (bw=0; ((bm & 1) == 1); ++bw, bm >>= 1);

                /* if nonspecified, choose number of gray levels */
                if (glevels < 0) {
                    if ((d==15) || (d==16))
                        glevels = 32;
                    else if ((d==24) || (d==32))
                        glevels = 256;
                }
            }

            /* no: get default visual */
            else {
                ht = 0;
                tv = NULL;
                tv = DefaultVisual(xd,DefaultScreen(xd));
                d = DefaultDepth(xd,DefaultScreen(xd));
                glevels = 4;
            }

            /* padding */
            pad = BitmapPad(xd);
        }

        /* colormap for instant thresholding */
        if ((ocring) &&
            (onlystep == OCR_OTHER) &&
            (ocr_other == 3)) {
	
            for (i=0; i<thresh_val; ++i)
                cmap[i] = black.pixel;
            for (; i<256; ++i)
                cmap[i] = white.pixel;
        }

        /* black-and-white colormap */
        else if (glevels == 2) {

            for (i=0; i<128; ++i)
                cmap[i] = black.pixel;
            for (; i<256; ++i)
                cmap[i] = white.pixel;
        }

        /* 32 graylevels (truecolor 15 or 16bpp) */
        else if (glevels == 32) {

            for (i=j=0; i<32; ++i) {
                for (t=0; t<8; ++t,++j) {

                    cmap[j] = (((rw==5) ? i:2*i) << rd) +
                              (((gw==5) ? i:2*i) << gd) +
                              (((bw==5) ? i:2*i) << bd);
                }
            }
        }

        /* 256 graylevels (truecolor 24bpp) */
        else if (glevels == 256) {
            int i;

            for (i=0; i<256; ++i) {

                cmap[i] = (i << rd) +
                          (i << gd) +
                          (i << bd);
            }
        }

        /* standard 4-colors colormap */
        else {            
	
            for (i=0; i<64; ++i)
                cmap[i] = black.pixel;
            for (; i<128; ++i)
                cmap[i] = vdgray.pixel;
            for (; i<192; ++i)
                cmap[i] = gray.pixel;
            for (; i<256; ++i)
                cmap[i] = white.pixel;
        }

        /*
            bytes per line
            (sparse 24bpp by GL)

            QUESTION: is it ok drop (d==24) and (d==32)?
        */
        if (d == 4)
            bpl = DW;
        else if (d == 8)
            bpl = DW;
        else if ((15<=d) && (d<=16))
            bpl = 2*DW;
        else if ((d == 24) && (pad == 24))
            bpl = 3*DW;
        else if ((d == 32) || (pad == 32))
            bpl = 4*DW;
        else {
            fatal(XE,"unable to handle depth %d\n",d);
	
            /* just to avoid a compilation warning */
            bpl = 0;
        }

        /* pad bpl */
        if (pad == 16)
            bpl += (bpl % 2);
        else if (pad == 24)
            bpl += (3 - (bpl % 3));
        else if (pad == 32)
            bpl += (4 - (bpl % 4));

        /* required buffer size */
        bs = bpl * DH;

        /* need to reallocate the X image */
        if ((DW != cw) || (DH != ch)) {

            if (image != NULL) {
                image->data = NULL;
                XDestroyImage(image);
            }

            image = XCreateImage(xd,tv,d,ZPixmap,0,NULL,DW,DH,pad,bpl);
        }

        /* need to enlarge image buffer */
        if (bs > imbsz) {
            imb = c_realloc(imb,imbsz=bs,NULL);
        }

        /* attach buffer */
        image->data = imb;

        /* copy image, 4bpp */
        if (d == 4) {
            unsigned char *map;
            int rf2;

            rf2 = 2*rf;
            for (j=0, v=Y0; (v<RCY) && (j<DH); ++j,v+=rf) {
                map = (unsigned char *) ((image->data) + (j*bpl));
                for (i=0, u=X0; (u<RCX) && (i<bpl); ++i,u+=rf) {

                    map[i] = cmap[r[u+v*RCX]];
                }
            }
        }

        /* copy image, 4 bpp */
        else if (d == 8) {
            unsigned char *map;

            for (j=0, v=Y0; (v<RCY) && (j<DH); ++j,v+=rf) {
                map = (unsigned char *) ((image->data) + (j*bpl));
                for (i=0, u=X0; (u<RCX) && (i<DW); ++i,u+=rf) {

                    map[i] = cmap[r[u+v*RCX]];
                }
            }
        }

        /* copy image, 15 or 16 bpp */
        else if ((15<=d) && (d<=16)) {
            unsigned short *map;

            for (j=0, v=Y0; (v<RCY) && (j<DH); ++j,v+=rf) {
                map = (unsigned short *) ((image->data) + (j*bpl));
                for (i=0, u=X0; (u<RCX) && (i<DW); ++i,u+=rf) {

                    map[i] = cmap[r[u+v*RCX]];
                }
            }
        }

        /* copy image, 24 bpp */
        else if (d == 24) {
            unsigned char *map;
            unsigned long c;

            for (j=0, v=Y0; (v<RCY) && (j<DH); ++j,v+=rf) {
                map = (unsigned char *) ((image->data) + (j*bpl));
                for (i=0, u=X0; (u<RCX) && (i<DW); ++i,u+=rf) {

                    c = cmap[r[u+v*RCX]];
                    *(map++) = c & 255;
                    *(map++) = (c >>= 8) & 255;
                    *(map++) = (c >> 8) & 255;

                    /* sparse 24bpp (GL) */
                    if (pad == 32)
                        ++map;
                }
            }
        }

        /* copy image, 32 bpp */
        else {
            unsigned *map;

            for (j=0, v=Y0; (v<RCY) && (j<DH); ++j,v+=rf) {
                map = (unsigned *) ((image->data) + (j*bpl));
                for (i=0, u=X0; (u<RCX) && (i<DW); ++i,u+=rf) {

                    map[i] = cmap[r[u+v*RCX]];
                }
            }
        }

        /* copy to X display */
        XPutImage(xd,xw,xgc,image,0,0,DM,DT,DW,DH);
    }
}

/*

Draw a 3-d frame around (outside) the rectangle with top left corner
(x,y), width w and height h. The frame if formed by an innermost
vdgray rectangle and an outermost white rectangle. Both have a
1-pixel width.

*/
void draw_dframe(int x,int y,int w,int h)
{
    XSetForeground(xd,xgc,vdgray.pixel);
    draw_frame(x,y,w,h,1);
    XSetForeground(xd,xgc,white.pixel);
    draw_frame(x-1,y-1,w+2,h+2,1);
}

/*

Draw frames around the window. windows have three frames. The innermost
is drawn with the window background color, so it's in fact a 1-pixel
separation from the window contents and the frames. The other two
frames are the Clara GUI standard frames (vdgray and white).

*/
void draw_dw_frames(void)
{
    if (CDW == PAGE)
        XSetForeground(xd,xgc,white.pixel);
    else
        XSetForeground(xd,xgc,gray.pixel);
    draw_frame(DM,DT,DW,DH,1);
    XSetForeground(xd,xgc,vdgray.pixel);
    draw_frame(DM-1,DT-1,DW+2,DH+2,1);
    XSetForeground(xd,xgc,white.pixel);
    draw_frame(DM-2,DT-2,DW+4,DH+4,1);
}

/*

Draw stepper.

*/
void draw_stepper(int x,int y,int o,int RW)
{
    int m = RW/2;
    XPoint t[3];

    /* draw stepper pointing to left (9 o'clock) */
    if (o == 9) {

        /*
            1. draw stepper foreground ('+')

                  +
                +++
              +++++
                +++
                  +
        */
        XSetForeground(xd,xgc,gray.pixel);
        t[0].x = x;
        t[0].y = y;
        t[1].x = x+RW-2;
        t[1].y = y-m;
        t[2].x = x+RW-2;
        t[2].y = y+m;
        XFillPolygon(xd,xw,xgc,t,3,Convex,CoordModeOrigin);

        /*
            2. draw white decoration (light reflecting, '.')

                  .
                ..+
              ..+++
            ..+++++
                +++
                  +
        */
        XSetForeground(xd,xgc,white.pixel);
        XDrawLine(xd,xw,xgc,x,y,x+RW-2,y-m);
        XDrawLine(xd,xw,xgc,x+1,y,x+RW-2,y-m+1);

        /*
            3. draw dark decoration (inferior shadow, '*')

                  .
                ..+
              ..+++
            **+++++
              **+++
                **+
                  *
        */
        XSetForeground(xd,xgc,vdgray.pixel);
        XDrawLine(xd,xw,xgc,x,y,x+RW-2,y+m);
        XDrawLine(xd,xw,xgc,x+1,y,x+RW-2,y+m-1);

        /*
            3. draw dark decoration (vertical shadow, '*')

                  .**
                ..+**
              ..+++**
            **+++++**
              **+++**
                **+**
                  ***
        */
        XDrawLine(xd,xw,xgc,x+RW-2,y-m,x+RW-2,y+m);
        XDrawLine(xd,xw,xgc,x+RW-3,y-m+1,x+RW-3,y+m-1);
    }

    /* draw stepper pointing to right (3 o'clock), see comments above */
    else if (o == 3) {
        XSetForeground(xd,xgc,gray.pixel);
        t[0].x = x;
        t[0].y = y;
        t[1].x = x-RW+2;
        t[1].y = y-m;
        t[2].x = x-RW+2;
        t[2].y = y+m;
        XFillPolygon(xd,xw,xgc,t,3,Convex,CoordModeOrigin);
        XSetForeground(xd,xgc,white.pixel);
        XDrawLine(xd,xw,xgc,x,y,x-RW+2,y-m);
        XDrawLine(xd,xw,xgc,x-1,y,x-RW+2,y-m+1);
        XSetForeground(xd,xgc,vdgray.pixel);
        XDrawLine(xd,xw,xgc,x,y,x-RW+2,y+m);
        XDrawLine(xd,xw,xgc,x-1,y,x-RW+2,y+m-1);
        XDrawLine(xd,xw,xgc,x-RW+2,y-m,x-RW+2,y+m);
        XDrawLine(xd,xw,xgc,x-RW+3,y-m+1,x-RW+3,y+m-1);
    }

    /* draw stepper pointing to top (12 o'clock), see comments above */
    else if (o == 12) {
        XSetForeground(xd,xgc,gray.pixel);
        t[0].x = x;
        t[0].y = y;
        t[1].x = x-m;
        t[1].y = y+RW-2;
        t[2].x = x+m;
        t[2].y = y+RW-2;
        XFillPolygon(xd,xw,xgc,t,3,Convex,CoordModeOrigin);
        XSetForeground(xd,xgc,white.pixel);
        XDrawLine(xd,xw,xgc,x,y,x-m,y+RW-2);
        XDrawLine(xd,xw,xgc,x,y+1,x-m+1,y+RW-2);
        XSetForeground(xd,xgc,vdgray.pixel);
        XDrawLine(xd,xw,xgc,x,y,x+m,y+RW-2);
        XDrawLine(xd,xw,xgc,x,y+1,x+m-1,y+RW-2);
        XDrawLine(xd,xw,xgc,x-m,y+RW-2,x+m,y+RW-2);
        XDrawLine(xd,xw,xgc,x-m+1,y+RW-3,x+m-1,y+RW-3);
    }

    /* draw stepper pointing to bottom (6 o'clock), see comments above */
    else if (o == 6) {
        XSetForeground(xd,xgc,gray.pixel);
        t[0].x = x;
        t[0].y = y;
        t[1].x = x-m;
        t[1].y = y-RW+2;
        t[2].x = x+m;
        t[2].y = y-RW+2;
        XFillPolygon(xd,xw,xgc,t,3,Convex,CoordModeOrigin);
        XSetForeground(xd,xgc,white.pixel);
        XDrawLine(xd,xw,xgc,x,y,x-m,y-RW+2);
        XDrawLine(xd,xw,xgc,x,y-1,x-m+1,y-RW+2);
        XSetForeground(xd,xgc,vdgray.pixel);
        XDrawLine(xd,xw,xgc,x,y,x+m,y-RW+2);
        XDrawLine(xd,xw,xgc,x,y-1,x+m-1,y-RW+2);
        XDrawLine(xd,xw,xgc,x-m,y-RW+2,x+m,y-RW+2);
        XDrawLine(xd,xw,xgc,x-m+1,y-RW+3,x+m-1,y-RW+3);
    }
}

/*

Draw horizontal and vertical scrollbars.

*/
void draw_sbars(void)
{
    int a,b;

    if (*cm_v_hide == 'X')
        return;

    /*

        horizontal scrollbar
        --------------------

        We'll discuss in detail only the drawing of the horizontal
        scrollbar. The vertical is analogous.

        What's the length (b) of the cursor to display on the
        horizontal scrollbar? Well, the horizontal scrollbar length
        is DW-2*RW. The document horizontal resolution is GRX, and the
        portion being displayed on the window has horizontal resolution
        HR. So the following relation applies:

                DW-2*RW  relates to    GRX
                as b     relates to    HR

        Then

                      HR * (DW-2*RW)
                b  =  --------------
                           GRX

        Let's now compute the position (a) on the horizontal scrollbar
        where the cursor will be displayed. The following relation
        applies:

                DW-2*RW   relates to   GRX
                as a      relates to   X0

        Then

                      X0 * (DW-2*RW)
                a  =  --------------
                           GRX

        In some cases however the document width GRX may be smaller
        than DW. That happens when the window is resized but the
        document (when it's from a ML source) is not regenerated.
        So this special case must be handled separately.
    */
    if (GRX > DW) {

        /* cursor leftmost coordinate (a) and length (b) */
        a = DM+RW+X0*(DW-2*RW)/GRX;
        b = HR*(DW-2*RW)/GRX;
    }
    else {

        /* cursor leftmost coordinate (a) and length (b) */
        a = DM+RW;
        b = DW-2*RW;
    }
    if (b < 5)
        b = 5;

    /*
        Paint scrollbar background avoiding overwrite the
        cursor position to not generate flicks.
    */
    XSetForeground(xd,xgc,darkgray.pixel);
    XFillRectangle(xd,xw,xgc,DM-1,DT+DH+9,a-DM+1,RW+2);
    XFillRectangle(xd,xw,xgc,a+b,DT+DH+9,DM-1+DW+2-a-b,RW+2);
    XFillRectangle(xd,xw,xgc,a,DT+DH+9,b,1);
    XFillRectangle(xd,xw,xgc,a,DT+DH+10+RW,b,1);

    /* draw the cursor */
    draw_button(a,DT+DH+10,b,RW,0,NULL);

    /* scrollbar frames */
    XSetForeground(xd,xgc,vdgray.pixel);
    draw_frame(DM-1,DT+DH+9,DW+2,RW+2,1);
    XSetForeground(xd,xgc,white.pixel);
    draw_frame(DM-2,DT+DH+8,DW+4,RW+4,1);

    /* vertical scrollbar */
    if (GRY > DH) {

        /* cursor topmost (a) coordinate and length (b) */
        a = DT+RW+Y0*(DH-2*RW)/GRY;
        b = VR*(DH-2*RW)/GRY;
    }
    else {

        /* cursor topmost (a) coordinate and length (b) */
        a = DT+RW;
        b = DH-2*RW;
    }
    if (b < 5)
        b = 5;

    /*
        Paint scrollbar background avoiding overwrite the
        cursor position to not generate flicks.
    */
    XSetForeground(xd,xgc,darkgray.pixel);
    XFillRectangle(xd,xw,xgc,DM+DW+9,DT-1,RW+2,a-DT+1);
    XFillRectangle(xd,xw,xgc,DM+DW+9,a+b,RW+2,DT-1+DH+2-a-b);
    XFillRectangle(xd,xw,xgc,DM+DW+9,a,1,b);
    XFillRectangle(xd,xw,xgc,DM+DW+10+RW,a,1,b);
    
    /* draw the cursor */
    draw_button(DM+DW+10,a,RW,b,0,NULL);

    /* scrollbar frames */
    XSetForeground(xd,xgc,vdgray.pixel);
    draw_frame(DM+DW+9,DT-1,RW+2,DH+2,1);
    XSetForeground(xd,xgc,white.pixel);
    draw_frame(DM+DW+8,DT-2,RW+4,DH+4,1);

    /* draw steppers */
    draw_stepper(DM,DT+DH+10+RW/2,9,RW);
    draw_stepper(DM+DW,DT+DH+10+RW/2,3,RW);
    draw_stepper(DM+DW+10+RW/2,DT,12,RW);
    draw_stepper(DM+DW+10+RW/2,DT+DH,6,RW);
}

/*

Draw menubar.

*/
void draw_mbar(void)
{
    int i,n,p;
    XTextItem xti;

    /* clear menu bar area */
    XSetForeground(xd,xgc,gray.pixel);
    XFillRectangle(xd,xw,xgc,0,0,WW,MH);

    /* draw menu bar */
    XSetForeground(xd,xgc,black.pixel);
    for (i=n=0, p=1; i<=TOP_CM; ++i) {
        if (CM[i].a == 1) {

            /* prepare */
            xti.delta = 0;
            xti.font = dfont->fid;
            xti.chars = CM[i].tt;
            xti.nchars = strlen(xti.chars);

            /* title background */
            if (CM+i == cmenu) {
                XSetForeground(xd,xgc,white.pixel);
                XFillRectangle(xd,xw,xgc,(p-1)*DFW+2,3,
                               (xti.nchars+2)*DFW-4,MH-6);
                XSetForeground(xd,xgc,black.pixel);
            }

            /* menu title and absciss */
            if (xti.nchars > 0)
                XDrawText(xd,xw,xgc,p*DFW,DFH+2,&xti,1);
            CM[i].p = p;
            p += strlen(xti.chars)+2;
            ++n;
        }
    }

    /* menu bar frames */
    XSetForeground(xd,xgc,vdgray.pixel);
    draw_frame(2,2,WW-4,MH-4,1);
    XSetForeground(xd,xgc,white.pixel);
    draw_frame(1,1,WW-2,MH-2,1);
}

/*

Compute menu width and height.

    BUG: We reserve 5 pixels on the
    top and on the bottom of the menu for a white frame,
    however the routines that display the labels put
    the bottommost label too near the menu bottom. As
    we could not discover why that happens, we've just
    added 10 pixels instead of 5 when computing CH.

*/
void comp_menu_size(cmdesc *c)
{
    int tw,i;

    CW = 2*DFW;
    for (i=0; i<c->n; ++i) {
        tw = 5 + strlen(c->l+i*(MAX_MT+1))*DFW + 5;
        if (tw > CW)
            CW = tw;
    }
    CH = 5 + DFH + (c->n-1)*(DFH+VS) + 10;
}

/*

Set mclip to f.

When a menu is dismissed, the GUI must redraw the area occupied
by it. So menus are clipped in order to achieve better visual
results. This is especially useful when using unbuffered X I/O.

The clipping works as follows: by default the clipping is
inactive. Each event handler may activate it in order to the
function redraw do not touch the areas outside the menu
rectangle. The function redraw destroys the menu clipping (if
any) on finish.

*/
void set_mclip(int f)
{
    static XRectangle mrec;

    /* save menu placement on mrec and set mclip */
    if (f == 2) {
        mrec.x = CX0-2;
        mrec.y = CY0-2;
        mrec.width = CW+4;
        mrec.height = CH+4;
        mclip = 1;
        if (cmenu->a == 1) {
            mrec.y -= MH;
            mrec.height += MH;
        }
        XSetClipRectangles(xd,xgc,0,0,&mrec,1,Unsorted);
    }

    /* just set mclip using mrec */
    else if (f) {
        mclip = 1;
        XSetClipRectangles(xd,xgc,0,0,&mrec,1,Unsorted);
    }

    /* unset mclip */
    else {
        XRectangle r;

        r.x = 0;
        r.y = 0;
        r.width = WW;
        r.height = WH;
        XSetClipRectangles(xd,xgc,0,0,&r,1,Unsorted);
        mclip = 0;
    }
}

/*

Draw current menu.

*/
void draw_menu(void)
{
    int i;
    XTextItem xti;
    static int ft=1,s=-1;

    /* compute the placement of the menu */
    if (CY0_orig < 0) {

        comp_menu_size(cmenu);

        /* save original coordinates */
        CX0_orig = CX0;
        CY0_orig = CY0;

        /* menu placement for menu bar menus */
        if (cmenu->a == 1) {
            CX0 = (cmenu->p-1)*DFW;
            CY0 = MH;
        }

        /* menu placement for select boxes */
        else if ((cmenu->a == 2) && (0 <= curr_hti) && (curr_hti <= topge)) {
            CX0 = DM + ge[curr_hti].l - X0 +
                  (ge[curr_hti].r - ge[curr_hti].l - CW)/2;
            CY0 = DT + ge[curr_hti].b - Y0;
        }

        /* try to make the menu fit on the application window */
        if (CX0 + CW + 5 > WW)
            CX0 = WW - CW - 5;
        if (CX0 < 5)
            CX0 = 5;
        if (CY0 + CH + 5 > WH - 10 - DFH)
            CY0 = WH - 10 - DFH - CH - 5;
        if (CY0 < 5)
            CY0 = 5;

        /* save menu placement and set mclip */
        set_mclip(2);
        set_mclip(0);

        /* no selected item by now */
        cmenu->c = -1;

        /* first-time flag and previously selected item */
        ft = 1;
        s = -1;
    }

    /*
        Redraw entirely. This is a provision for expose events.
        When the application get focus, the menu must be entirely
        redraw.
    */
    else if (redraw_menu == 2) {
        ft = 1;
    }
    
    if (ft) {

        /* menu frame */
        XSetForeground(xd,xgc,vdgray.pixel);
        draw_frame(CX0,CY0,CW,CH,1);
        XSetForeground(xd,xgc,white.pixel);
        draw_frame(CX0-1,CY0-1,CW+2,CH+2,1);

        /* menu background */
        XFillRectangle(xd,xw,xgc,CX0,CY0,CW,CH);

    }
    
    /* clear previously selected item */
    if ((0 <= s) && (s < cmenu->n)) {
        XSetForeground(xd,xgc,white.pixel);
        XFillRectangle(xd,xw,xgc,CX0+5,
            CY0+5+s*(DFH+VS)+DFH-(dfont->max_bounds).ascent,
            CW-10,DFH-1);
    }

    /* emphasize selected item */
    if ((0 <= cmenu->c) && (cmenu->c < cmenu->n)) {
        XSetForeground(xd,xgc,gray.pixel);
        XFillRectangle(xd,xw,xgc,CX0+5,
            CY0+5+cmenu->c*(DFH+VS)+DFH-(dfont->max_bounds).ascent,
            CW-10,DFH-1);
    }

    /* labels and rules */
    XSetForeground(xd,xgc,black.pixel);
    for (i=0; i<cmenu->n; ++i) {
        int y;

        xti.delta = 0;
        xti.font = dfont->fid;
        xti.chars = cmenu->l+i*(MAX_MT+1);
        xti.nchars = strlen(xti.chars);
        y = CY0+5+i*(DFH+VS);

        /*
            Draw only nonempty labels. Draw all labels if first time,
            otherwise draw only the just selected/unselected item.
        */
        if ((xti.nchars > 0) && (ft || (i==s) || (i==cmenu->c))) {

            /*
                Change color if hidden
            */
            if (cma(i+((cmenu-CM) << CM_IT_WD),1) == 0) {
                if (i==cmenu->c)
                    XSetForeground(xd,xgc,white.pixel);
                else
                    XSetForeground(xd,xgc,gray.pixel);
            }
            else
                XSetForeground(xd,xgc,black.pixel);

            XDrawText(xd,xw,xgc,CX0+5,y+DFH,&xti,1);
        }

        /* new group starting: draw separator (horizontal line) */
        if ((cmenu->t)[i] & CM_NG) {
            XSetLineAttributes(xd,xgc,1,LineSolid,CapButt,JoinRound);
            XSetForeground(xd,xgc,darkgray.pixel);
            XDrawLine(xd,xw,xgc,CX0,y+1,CX0+CW,y+1);
            XSetForeground(xd,xgc,black.pixel);
        }
    }

    ft = 0;
    s = cmenu->c;
}

/*

Draw only one cfont pixel.

*/
void draw_cfont_at(int i,int j,int d)
{
    int p;

    if ((i<0) || (HR<=i) || (j<0) || (VR<=j)) {
        return;
    }

    p = cfont[i+j*HR]&C_MASK;
    if (d) {
        if ((p == GRAY) || (p == WHITE))
            p = BLACK;
        else
            p = WHITE;
    }

    /* white pixels */
    if (p == WHITE) {
        XSetForeground (xd,xgc,white.pixel);
        XFillRectangle(xd,xw,xgc,DM+i*GS+GW,DT+j*GS+GW,GS-GW,GS-GW);
    }

    /* gray pixels */
    else if (p == GRAY) {
        XSetForeground (xd,xgc,gray.pixel);
        XFillRectangle(xd,xw,xgc,DM+i*GS+GW,DT+j*GS+GW,GS-GW,GS-GW);
    }

    /* black pixels */
    else if (p == BLACK) {
        XSetForeground (xd,xgc,black.pixel);
        XFillRectangle(xd,xw,xgc,DM+i*GS+GW,DT+j*GS+GW,GS-GW,GS-GW);
    }

    /* distinguished pixels */
    if (((cfont[i+j*HR])&D_MASK) == D_MASK) {
        if (p == WHITE)
            XSetForeground (xd,xgc,black.pixel);
        else if (p == BLACK)
            XSetForeground (xd,xgc,white.pixel);
        else if (p == GRAY)
            XSetForeground (xd,xgc,black.pixel);
    
        XDrawLine(xd,xw,xgc,DM+i*GS+1,DT+j*GS+1,DM+(i+1)*GS,DT+(j+1)*GS);
        XDrawLine(xd,xw,xgc,DM+(i+1)*GS,DT+j*GS,DM+i*GS+1,DT+(j+1)*GS-1);
    }
}

/*

Draw the flea.

*/
void draw_flea(int i,int j,int d) {
    int t;

    /* arms */
    for (t=1; t<=2; ++t) {
        if (i >= t) {
            if (j >= t)
                draw_cfont_at(i-t,j-t,d);
            if ((j+t) < VR)
                draw_cfont_at(i-t,j+t,d);
        }
        if ((i+t) < HR) {
            if (j > 0)
                draw_cfont_at(i+t,j-t,d);
            if ((j+1) < VR)
                draw_cfont_at(i+t,j+t,d);
        }
    }

    /* center */
    draw_cfont_at(i,j,d);
}

/*

Draw cfont and the grid in fat bit mode.

*/
void draw_cfont(char *cfont)
{
    int p,i,j;

    if (redraw_grid != 0) {
        XSetLineAttributes(xd,xgc,1,LineSolid,CapNotLast,JoinRound);
        XSetForeground (xd,xgc,gray.pixel);
    
        /* grid vertical lines */
        for (i=0; i<=HR; ++i)
            XDrawLine(xd,xw,xgc,DM+i*GS,DT,DM+i*GS,DT+DH);
    
        /* grid horizontal lines */
        for (j=0; j<=VR; ++j)
            XDrawLine(xd,xw,xgc,DM,DT+j*GS,DM+DW,DT+j*GS);
        redraw_grid = 0;
    }
    
    /* white pixels */
    XSetForeground (xd,xgc,white.pixel);
    for (i=0; i<HR; ++i) {
        for (j=0; j<VR; ++j)
            if ((cfont[i+j*HR]&C_MASK) == WHITE)
                XFillRectangle(xd,xw,xgc,DM+i*GS+GW,DT+j*GS+GW,GS-GW,GS-GW);
    }
    
    /* gray pixels */
    XSetForeground (xd,xgc,gray.pixel);
    for (i=0; i<HR; ++i) {
        for (j=0; j<VR; ++j)
            if ((cfont[i+j*HR]&C_MASK) == GRAY)
                XFillRectangle(xd,xw,xgc,DM+i*GS+GW,DT+j*GS+GW,GS-GW,GS-GW);
    }
    
    /* black pixels */
    XSetForeground (xd,xgc,black.pixel);
    for (i=0; i<HR; ++i) {
        for (j=0; j<VR; ++j)
            if ((cfont[i+j*HR]&C_MASK) == BLACK)
                XFillRectangle(xd,xw,xgc,DM+i*GS+GW,DT+j*GS+GW,GS-GW,GS-GW);
    }
    
    /* distinguished pixels */
    for (i=0; i<HR; ++i) {
        for (j=0; j<VR; ++j) {
            if (((p=cfont[i+j*HR])&D_MASK) == D_MASK) {
                if ((p&C_MASK) == WHITE)
                    XSetForeground (xd,xgc,black.pixel);
                else if ((p&C_MASK) == BLACK)
                    XSetForeground (xd,xgc,white.pixel);
                else if ((p&C_MASK) == GRAY)
                    XSetForeground (xd,xgc,black.pixel);

                XDrawLine(xd,xw,xgc,DM+i*GS+1,DT+j*GS+1,DM+(i+1)*GS,DT+(j+1)*GS);
                XDrawLine(xd,xw,xgc,DM+(i+1)*GS,DT+j*GS,DM+i*GS+1,DT+(j+1)*GS-1);
            }
        }
    }
    
    /* frames */
    XSetForeground(xd,xgc,vdgray.pixel);
    draw_frame(DM+GW,DT+GW,GS*HR,GS*VR,1);
    XSetForeground(xd,xgc,white.pixel);
    draw_frame(DM+GW-1,DT+GW-1,GS*HR+2,GS*VR+2,1);
}

/*

Draw the bitmap bm reduced rf times. The reduction is performed
sampling one pixel on each rf*rf tile.

  bm  .. a raw bitmap.
  bpl .. the bytes per line rate of the bitmap.
  rh  .. number of lines of the bitmap (or 0).
  x   .. x coordinate to sample on each rf*rf tile.
  y   .. y coordinate to sample on each rf*rf tile.
  w   .. width (in pixels) of the image to produce.
  h   .. height (in pixels) of the image to produce.
  dx0 .. window-relative x display origin.
  dy0 .. window-relative y display origin.
  rf  .. reduction factor.

The caller is allowed to inform w or h larger than the reduced
bitmap. The initial clipping code will detect (and correct) a too
large w or h.

*/
void draw_rb(unsigned char *bm,int bpl,int rh,int x,int y,int w,int h,int dx0,int dy0,int rf)
{
    int a,k,i,j,np,c;
    unsigned char *p,m;
    XPoint xp[100];

    /*
        Clip horizontally. For each i (inner loop), we need
        (c/8 < bpl). Then we have (x + i*rf)/8 < bpl, or
        i < (8*bpl-x)/rf. So (8*bpl-x)/rf cannot be larger
        than w.
    */
    a = (8*bpl-x);
    a = (a/rf) - ((a%rf) == 0);
    if (a < w)
        w = a;

    /*
        Clip vertically. For each j (outer loop), we need
        (y+j*rf)<rh. Then we have j < (rh-y)/rf. So
        (rh-y)/rf cannot be larger than h. The caller is
        allowed to inform rh==0 when vertical clipping is
        not needed.
    */
    if (rh > 0) {
        a = (rh-y);
        a = (a/rf) - ((a%rf) == 0);
        if (a < h)
            h = a;
    }

    for (np=0, j=0; j<=h; ++j) {
        k = (y+j*rf) * bpl;
        for (i=0, c=x; (i<=w); ++i, c+=rf) {
            p = bm + k + c/8;
            m = ((unsigned) 1) << (7 - (c%8));
            if ((*p & m) == m) {
                if (np >= 100) {
                    XDrawPoints(xd,xw,xgc,xp,np,CoordModeOrigin);
                    np = 0;
                }
                xp[np].x = dx0+i;
                xp[np].y = dy0+j;
                ++np;
            }
        }
    }
    if (np > 0) {
        XDrawPoints(xd,xw,xgc,xp,np,CoordModeOrigin);
    }
}

/*

Draw closure b. The closure is drawn only if it intersects the
display rectangle with top left (PDM,PDT), width HR2*RF and
height VR2*RF. Generally we'll have PDM==DM, PDT==DT, PHR*RF==DW
and PVR*RF==DH. However, when redrawing only part of the PAGE
window, a caller may change these variables (and restore them
afterwards) to make the redraw area smaller and the operation
faster.

*/
void draw_closure(cldesc *b,int a)
{
    int dx0,dy0,x0,y0;
    int bpl,x,y,w,h;

    /* Clip the visible portion of the closure on x */
    x0 = X0 + (PDM-DM) * RF;
    x = (b->l < x0) ? x0 : b->l;
    if ((x%RF) > 0)
        x += RF - (x%RF);
    if ((w=x0+PHR) > b->r)
        w = b->r;
    if ((w-=x) >= RF)
        w /= RF;
    else if (!draw_thin)
        return;
    dx0 = PDM + (x-X0)/RF;

    /* Clip the visible portion of the closure on y */
    y0 = Y0 + (PDT-DT) * RF;
    y = (b->t < y0) ? y0 : b->t;
    if ((y % RF) > 0)
        y += RF - (y%RF);
    if ((h=y0+PVR) > b->b)
        h = b->b;
    if ((h-=y) >= RF)
        h /= RF;
    else if (!draw_thin)
        return;
    dy0 = PDT + (y-y0)/RF;

    /*
        At this point we have:

        (x,y) .. the (page) coordinates of the top left
                 pixel of the closure that will be
                 sampled when drawing the closure.

        w .. the number of horizontal sampled lines less 1.

        h .. the number of vertical sampled lines less 1.

        (dx0,dy0) .. the display coordinates where the
                     closure pixel with page coordinates
                     (x,y) will be drawn.
    */

    /* draw the bitmap */
    if (a <= 0) {
        bpl = ((b->r-b->l+1)/8 + (((b->r-b->l+1)%8)!=0));
        if (a == 0)
            XSetForeground(xd,xgc,black.pixel);
        else if (a == -1)
            XSetForeground(xd,xgc,white.pixel);
        else
            XSetForeground(xd,xgc,darkgray.pixel);
        draw_rb(b->bm,bpl,0,x-b->l,y-b->t,w,h,dx0,dy0,RF);
    }
}

/*

Draw symbol b.

The parameter a is used as follows:

value   action
---------------------------------------------
  0     draw closure using black foreground
  1     draw ellipse using darkgray foreground
  2     draw ellipse using black foreground
 -1     draw closure using white foreground
 -2     draw closure using darkgray foreground
 -3,-4  clear the box
 -4     ignore cm_o_bb flag and use black foreground

*/
void draw_symbol(sdesc *s,int a)
{
    int dx0,dy0,x0,y0;
    int x,y,w,h;

    /* draw the bounding box */
    if (*cm_d_bb != ' ') {
        int l,r,t,b;

        l = DM+(s->l-X0)/RF;
        r = DM+(s->r-X0)/RF;
        t = DT+(s->t-Y0)/RF;
        b = DT+(s->b-Y0)/RF;
        if ((a==-3) || (a==-4)) {
            XSetForeground(xd,xgc,white.pixel);
            XFillRectangle(xd,xw,xgc,l,t,r-l+1,b-t+1);
        }
        if (a != -4) {
            XSetForeground(xd,xgc,black.pixel);
            XDrawRectangle(xd,xw,xgc,l,t,r-l,b-t);
        }
    }

    /* draw the closures */
    if ((*cm_d_bb == ' ') || (a == -4)) {
        if (a==-4)
            a = 0;
        if (a <= 0) {
            int i;
            cldesc *c;

            for (i=0; i<s->ncl; ++i) {
                c = cl + (s->cl)[i];
                draw_closure(c,a);
            }
        }
    }

    /* Clip the visible portion of the closure on x */
    x0 = X0 + (PDM-DM) * RF;
    x = (s->l < x0) ? x0 : s->l;
    if ((x%RF) > 0)
        x += RF - (x%RF);
    if ((w=x0+PHR) > s->r)
        w = s->r;
    if ((w-=x) >= RF)
        w /= RF;
    else if (!draw_thin)
        return;
    dx0 = PDM + (x-X0)/RF;

    /* Clip the visible portion of the closure on y */
    y0 = Y0 + (PDT-DT) * RF;
    y = (s->t < y0) ? y0 : s->t;
    if ((y % RF) > 0)
        y += RF - (y%RF);
    if ((h=y0+PVR) > s->b)
        h = s->b;
    if ((h-=y) >= RF)
        h /= RF;
    else if (!draw_thin)
        return;
    dy0 = PDT + (y-y0)/RF;

    /*
        At this point we have:

        (x,y) .. the (page) coordinates of the top left
                 pixel of the closure that will be
                 sampled when drawing the closure.

        w .. the number of horizontal sampled lines less 1.

        h .. the number of vertical sampled lines less 1.

        (dx0,dy0) .. the display coordinates where the
                     closure pixel with page coordinates
                     (x,y) will be drawn.
    */

    /*
        Draw an ellipse around the symbol. The ellipse is
        centered at (dx0+w/2,dy0+h/2).
    */
    /*
    if (a > 0) {
        int dx,dy,a1,a2;

        dx = w/4 + 3;
        dy = h/4 + 3;
        if (a==1)
            XSetForeground(xd,xgc,darkgray.pixel);
        else
            XSetForeground(xd,xgc,black.pixel);
        XFillArc(xd,xw,xgc,dx0-dx,dy0-dy,w+2*dx,h+2*dy,0,23040);
    }
    */

    /*
        Draw an ellipse around the closure.
    */
    if (a > 0) {
        int dx,dy;

        w = (s->r - s->l + 1) / RF;
        h = (s->b - s->t + 1) / RF;
        dx = w/4 + 3;
        dy = h/4 + 3;
        if (a==1)
            XSetForeground(xd,xgc,darkgray.pixel);
        else
            XSetForeground(xd,xgc,black.pixel);

        XFillArc(xd,xw,xgc,
                 DM+(s->l-X0)/RF-dx/2,DT+(s->t-Y0)/RF-dy/2,
                 w+dx,h+dy,0,23040);
    }
}

/*

Draw input field.

*/
void draw_inp(edesc *c)
{
    char d[MFTL+2];
    int x,y,dx,dy;
    XTextItem xti;

    /* input field corner, width and height */
    x = DM + c->l - X0 + 2;
    y = DT + c->t - Y0 + 2;
    dx = c->r-c->l+1 - 4;
    dy = c->b-c->t+1 - VS - 4;

    /* paint input field background */
    XSetForeground (xd,xgc,darkgray.pixel);
    XFillRectangle(xd,xw,xgc,x,y,dx,dy);

    /* display text and cursor */
    XSetForeground (xd,xgc,black.pixel);
    xti.delta = 0;
    if (c->txt != NULL)
        strcpy(d,c->txt);
    else
        d[0] = 0;
    if (c-ge == curr_hti)
        strcat(d,"_");
    xti.font = dfont->fid;
    xti.chars = d;
    if ((xti.nchars = strlen(d)) > 0)
        XDrawText(xd,xw,xgc,x,y+(dfont->max_bounds).ascent,&xti,1);

    /* display frames around it */
    XSetForeground(xd,xgc,white.pixel);
    draw_frame(x,y,dx,dy,1);
    XSetForeground(xd,xgc,vdgray.pixel);
    draw_frame(x-1,y-1,dx+2,dy+2,1);

    /* oh! ugly! */
    if (use_xb) {
        XCopyArea(xd,xw,XW,xgc,x-1,y-1,dx+2,dy+2,x-1,y-1);
    }
}

/*

Draw checkboxes, select boxes and submission buttons.

*/
void draw_cbox(edesc *c)
{
    int x,y,dx,dy;
    cmdesc *cm;

    /* input field corner, width and height */
    x = DM + c->l - X0 + 2;
    y = DT + c->t - Y0 + 2;
    dx = c->r-c->l+1 - 4;
    dy = c->b-c->t+1 - 4;

    /* select box */
    if (c->type == GE_SELECT) {
        cm = CM + c->iarg;
        if ((0 <= cm->c) && (cm->c < cm->n))
            draw_button(x,y,dx,dy,0,cm->l+cm->c*(MAX_MT+1));
        else
            draw_button(x,y,dx,dy,0,NULL);
    }

    /* submission button */
    else if (c->type == GE_SUBMIT) {
        draw_button(x,y,dx,dy,0,c->txt);
    }

    /* checkbox */
    else {
        draw_button(x,y,dx,dy,c->iarg,NULL);
    }
}

/*

Draw Graphic element.

*/
void draw_ge(edesc *t)
{
    XTextItem xti;

    /* ignore elements outside the current window */
    if ((intersize(t->l,t->r,X0,X0+DW-1,NULL,NULL) <= 0) ||
        (intersize(t->t,t->b,Y0,Y0+DH-1,NULL,NULL) <= 0)) {

        return;
    }

    /* draw text GC */
    if (t->type == GE_TEXT) {
        int y;

        if (text_switch) {
            int l;

            l = topdv + strlen(t->txt) + 2;
            if (l >= dvsz) {
                dvsz = l + 4096;
                dv = c_realloc(dv,dvsz,NULL);
            }
            if (topdv < 0)
                dvfl = t - ge;
            topdv += sprintf(dv+topdv+1,"%s\n",t->txt);
            dv[topdv+1] = 0;
        }

        else {
            /* draw text */
            xti.delta = 0;
            xti.font = dfont->fid;
            xti.chars = t->txt;
            xti.nchars = strlen(xti.chars);
            XSetForeground(xd,xgc,black.pixel);
            y = DT+t->t+(dfont->max_bounds).ascent-Y0;
            XDrawText(xd,xw,xgc,DM+t->l-X0,y,&xti,1);

            /* underline anchored texts */
            if ((t->a) && ULINK) {
                XSetLineAttributes(xd,xgc,1,LineSolid,CapNotLast,JoinRound);
                XDrawLine(xd,xw,xgc,DM+t->l-X0,
                          y+1,
                          DM+t->l-X0+xti.nchars*DFW,
                          y+1);
            }
        }
    }
    
    /* draw special PBM image */
    if (t->type == GE_IMG) {

        /* draw web clip */
        if (strcmp(t->txt,"internal") == 0)
            draw_bm(rw,RWX,RWY,DM+t->l-X0+2,DT+t->t-Y0+2);

        /* draw pattern */
        else if (strncmp(t->txt,"pattern/",8) == 0) {
            int i,id;

            id = atoi(t->txt+8);
            i = id2idx(id);
            if ((0 <= i) && (i <= topp)) {
                XSetForeground(xd,xgc,white.pixel);
                XFillRectangle(xd,xw,xgc,DM+t->l-X0+2,DT+t->t-Y0+2,
                               FS/plist_rf,FS/plist_rf);
                XSetForeground(xd,xgc,black.pixel);
                draw_rb(pattern[i].b,FS/8,0,0,0,FS/plist_rf-1,FS/plist_rf-1,
                        DM+t->l-X0+2,DT+t->t-Y0+2,plist_rf);
            }
        }

        /* draw symbol */
        else if (strncmp(t->txt,"symbol/",7) == 0) {
            int id,j;
            cldesc *c;

            id = atoi(t->txt+7);
            if ((0 <= id) && (id <= tops)) {
                XSetForeground(xd,xgc,white.pixel);
                XFillRectangle(xd,xw,xgc,DM+t->l-X0+2,DT+t->t-Y0+2,
                               FS/plist_rf,FS/plist_rf);

                XSetForeground(xd,xgc,black.pixel);
                for (j=0; j<mc[id].ncl; ++j) {
                    c = cl + mc[id].cl[j];
                    draw_rb(c->bm,byteat(c->r-c->l+1),c->b-c->t+1,0,0,FS/plist_rf-1,
                            FS/plist_rf-1,DM+t->l-X0+2,DT+t->t-Y0+2,plist_rf);
                }


            }
        }

        /* draw aligned pattern */
        else if (strncmp(t->txt,"apatt/",6) == 0) {
            int i,id,w;

            id = atoi(t->txt+6);
            i = id2idx(id);
            if ((0 <= i) && (i <= topp)) {
                w = t->r - t->l + 1;
                XSetForeground(xd,xgc,white.pixel);
                XFillRectangle(xd,xw,xgc,DM+t->l-X0+2,DT+t->t-Y0+2,w,FS/plist_rf);
                XSetForeground(xd,xgc,black.pixel);
                draw_rb(pattern[i].b,FS/8,0,0,0,w,FS/plist_rf-1,
                        DM+t->l-X0+2,DT+t->t-Y0+2,plist_rf);
            }
        }

        /* draw word */
        else if (strncmp(t->txt,"word/",5) == 0) {
            int a,i,j;
            wdesc *W;
            cldesc *c;
            int bpl;

            i = atoi(t->txt+5);
#ifdef MEMCHECK
            checkidx(i,topw+1,"draw word");
#endif
            W = word + i;

            XSetForeground(xd,xgc,white.pixel);
            XFillRectangle(xd,xw,xgc,DM+t->l-X0+2,DT+t->t-Y0+2,(W->r-W->l+1)/plist_rf,
                           (W->b-W->t+1)/plist_rf);

            for (a=W->F; a>=0; a=mc[a].E) {

                if (uncertain(mc[a].tc))
                    XSetForeground(xd,xgc,gray.pixel);
                else
                    XSetForeground(xd,xgc,black.pixel);

                for (j=0; j<mc[a].ncl; ++j) {
                    c = cl + (mc[a].cl)[j];
                    bpl = ((c->r-c->l+1)/8 + (((c->r-c->l+1)%8)!=0));
                    draw_rb(c->bm,bpl,0,0,0,(c->r-c->l+1)/plist_rf,
                            (c->b-c->t+1)/plist_rf,DM+t->l-X0+2+(c->l-W->l)/plist_rf,
                            DT+t->t-Y0+2+(c->t-W->t)/plist_rf,plist_rf);
                }
            }
        }
    }
    
    /* draw input field */
    else if (t->type == GE_INPUT) {
        draw_inp(t);
    }
    
    /* draw checkbox */
    else if ((t->type == GE_CBOX) || (t->type == GE_RADIO)) {
        draw_cbox(t);
    }
    
    /* draw select */
    else if (t->type == GE_SELECT) {
        draw_cbox(t);
    }
    
    /* draw submission button */
    else if (t->type == GE_SUBMIT) {
        draw_cbox(t);
    }

    /* draw rectangle background */
    else if (t->type == GE_RECT) {
        int w,h;

        if (t->bg >= 0) {
            if (t->bg == WHITE)
                XSetForeground(xd,xgc,white.pixel);
            else if (t->bg == BLACK)
                XSetForeground(xd,xgc,black.pixel);
            else if (t->bg == GRAY)
                XSetForeground(xd,xgc,gray.pixel);
            else if (t->bg == DARKGRAY)
                XSetForeground(xd,xgc,darkgray.pixel);
            else if (t->bg == VDGRAY)
                XSetForeground(xd,xgc,vdgray.pixel);
            else
                XSetForeground(xd,xgc,darkgray.pixel);
            w = t->r - t->l;
            h = t->b - t->t;
            XFillRectangle(xd,xw,xgc,DM+t->l-X0,DT+t->t-Y0,w,h);
        }
    }

    /* draw stepper */
    else if (t->type == GE_STEPPER) {
        int x,y;

        x = DM + t->l - X0;
        y = DT + t->t + (t->b-t->t)/2 - Y0;
        if (t->iarg == 9)
            draw_stepper(x,y,9,DFH-2);
        else
            draw_stepper(x+DFH-1,y,3,DFH-2);
    }
}

/*

Clip or unclip the current window.

*/
void clip_dw(int a)
{
    XRectangle r;

    /* clip the window */
    if (a) {

        /* clip the current window */
        r.x = DM-1;
        r.y = DT-1;
        r.width = DW+2;
        r.height = DH+2;
        XSetClipRectangles(xd,xgc,0,0,&r,1,Unsorted);
    }

    /* clip the entire X window */
    else {
        r.x = 0;
        r.y = 0;
        r.width = WW;
        r.height = WH;
        XSetClipRectangles(xd,xgc,0,0,&r,1,Unsorted);
    }
}

/*

Compute the bounding box of the anchored text starting at
gc[a].

*/
void ge_bb(int a,int *l,int *r,int *t,int *b)
{
    edesc *c;

    c = ge + a;
    *l = c->l;
    *r = c->r;
    *t = c->t;
    *b = c->b;
    for (++c; c->a==HTA_HREF_C; ++c) {
        if (c->l < *l)
            *l = c->l;
        if (c->r > *r)
            *r = c->r;
        if (c->t < *t)
            *t = c->t;
        if (c->b > *b)
            *b = c->b;
    }
}

/*

Highlight graphic element.

BUG: perform synchronous X I/O.

*/
void hl_gc(edesc *c)
{
    int l,r,t,b;

    /* anchored text */
    if ((c->type == GE_TEXT) &&
        ((c->a == HTA_HREF) || (c->a == HTA_HREF_C))) {

        /* get first word in chain */
        if (c->a == HTA_HREF_C)
            c = ge + c->iarg;

        /* message on status line */
        show_hint(0,c->arg);

        /* compute chain bounding box and clear it */
        ge_bb(c->iarg,&l,&r,&t,&b);
        clip_dw(1);
        XSetForeground(xd,xgc,white.pixel);
        XFillRectangle(xd,xw,xgc,DM+l-X0,DT+t-Y0,r-l,b-t);

        /* redraw chain */
        do {
            draw_ge(c);
        } while ((++c)->a == HTA_HREF_C);

        if (use_xb) {
            XCopyArea(xd,xw,XW,xgc,DM+l-X0,DT+t-Y0,r-l,b-t,DM+l-X0,DT+t-Y0);
        }
    }

    /* text input field */
    else if (c->type == GE_INPUT) {
        draw_inp(c);
        show_hint(0,"Fill in and (optionally) press ENTER to consist");
    }

    /* submission button or anchored image */
    else if ((c->type == GE_SUBMIT) ||
             ((c->type == GE_IMG) && (c->arg != NULL))) {

        /* message on status line */
        show_hint(0,c->arg);
    }

    /* document analisys (experimental) */
    else if (CDW == DEBUG) {
        /*
        printf("now at line %d \"%s\"\n",c-ge-dvfl,c->txt);
        */
    }
}

/*

Unhighlight graphic element.

BUG: perform synchronous X I/O.

*/
void unhl_gc(edesc *c)
{
    int l,r,t,b;

    /* anchored text */
    if ((c->type == GE_TEXT) &&
        ((c->a == HTA_HREF) || (c->a == HTA_HREF_C))) {

        /* get first word in chain */
        if (c->a == HTA_HREF_C)
            c = ge + c->iarg;

        /* compute chain bounding box and clear it */
        ge_bb(c->iarg,&l,&r,&t,&b);
        clip_dw(1);
        XSetForeground(xd,xgc,gray.pixel);
        XFillRectangle(xd,xw,xgc,DM+l-X0,DT+t-Y0,r-l,b-t);

        /* redraw chain */
        do {
            draw_ge(c);
        } while ((++c)->a == HTA_HREF_C);

        if (use_xb) {
            XCopyArea(xd,xw,XW,xgc,DM+l-X0,DT+t-Y0,r-l,b-t,DM+l-X0,DT+t-Y0);
        }

        /* remove message on status line */
        show_hint(0,"");
    }

    /* text input field */
    else if (c->type == GE_INPUT) {
        mb[0] = 0;
        redraw_stline = 1;
        draw_inp(c);
        show_hint(0,"");
    }

    /* submission button */
    else if (c->type == GE_SUBMIT) {

        /* message on status line */
        show_hint(0,"");
    }
}

/*

Show info about the current HTML item or the current symbol on the
PAGE window.

This is the service responsible to "activate" a clickable element,
displayin a message about it, like "load/0" (see also hl_gc).

*/
void show_htinfo(int cx,int cy) {
    edesc *c;
    int i,k,f;

    if (CDW==PAGE) {
        c = NULL;
        k = symbol_at(X0+(cx-DM)*RF,Y0+(cy-DT)*RF);
    }

    else {

        /* search the item that contains (cx,cy) */
        cx = cx - DM + X0;
        cy = cy - DT + Y0;

        /* TODO: something better than linear search.. */
        for (k=-1, i=0; (k<0) && (i<=topge); ++i) {
            c = ge + i;
            if ((c->type != GE_RECT) &&
                (c->l<=cx) && (cx<=c->r) && (c->t<=cy) && (cy<=c->b)) {

                k = i;
            }
        }
    }

    /* for debugging */
    /*
    {
        static int last_k=-1;

        if (k != last_k) {
            printf("current item is %d, type %d\n",k,ge[k].type);
            last_k = k;
        }
    }
    */

    /* flush flag */
    f = 0;

    /* unhilight last_k */
    if ((curr_hti >= 0) && (k != curr_hti)) {
        if (CDW==PAGE) {
            draw_symbol(mc+curr_hti,-3);
        }
        else {
            c = ge + curr_hti;
            curr_hti = -1;
            unhl_gc(c);
            c = ge + k;
            f = 1;
        }
    }

    /* highlight k */
    if ((k >= 0) && (k != curr_hti)) {
        curr_hti = k;
        if (CDW==PAGE) {
            draw_symbol(mc+curr_hti,-4);
        }
        else {
            c = ge + curr_hti;
            hl_gc(c);
            f = 1;
        }
    }

    /* no current component */
    if (k < 0)
        curr_hti = -1;

    /*
        make sure the update is not buffered

        BAD: synchronous I/O
    */
    if (f) {
        clip_dw(0);
        XFlush(xd);
    }
}

/*

Set the status of the buttons that carry symbol properties from the
pattern p properties (if s==0) or from the symbol p properties (if
s!=0).

*/
void set_buttons(int s,int p)
{
    int i,a,f,t;

    /*
        Copy the symbol transliteration properties to a and f.
    */
    if (s) {
        sdesc *m;

        /* just ignore invalid indexes */
        if ((curr_mc<0) || (tops<curr_mc))
            return;
        m = mc + curr_mc;

        /*
            When untransliterated, copy the properties of the
            surrounding word properties (TO BE DONE) or
            use default values.
        */
        if (m->tr == NULL) {
            a = 0;
            t = 0;
            f = 0;
        }
        else {
            a = (m->tr)->a;
            f = (m->tr)->f;
            t = (m->bm >= 0) ? pattern[m->bm].pt : 0;
        }
    }

    /*
        Copy the pattern transliteration properties to a and f.
    */
    else {
        pdesc *s;

        /* just ignore invalid indexes */
        if ((cdfc<0) || (topp<cdfc))
            return;
        s = pattern + cdfc;
        a = s->a;
        f = s->f;
        t = s->pt;
    }

    /* set the alphabet */
    for (i=0; (i<nalpha) && (inv_balpha[i] != a); ++i);
    if (i >= nalpha)
        i = 0;
    button[balpha] = i;

    /* set the pattern type */
    button[btype] = t;

    /* bad flag */
    button[bbad] = ((f & F_BAD) != 0);

    /* redraw all buttons */
    redraw_button = -1;
}

/*

Set the status of the buttons that carry symbol properties from the
symbol s properties.

*/
void symb2buttons(int s)
{
    trdesc *t;
    int i;

    /* read values from the preferred transliteration */
    if ((t=mc[s].tr) != NULL) {
        for (i=0; (i<nalpha) && (inv_balpha[i] != t->a); ++i);
        if (i >= nalpha)
            i = 0;
        button[balpha] = i;
    }

    /* TODO: read default values from surrounding word */
    else {
    }

    /* must redraw */
    redraw_button = -1;
}

/*

Draw window.

*/
void draw_dw(int bg,int inp)
{
    int i;

    if (mclip == 0) {
        clip_dw(1);
    }

    if (HTML) {

        /* clear the window background */
        if (bg) {
            XSetForeground (xd,xgc,gray.pixel);
            XFillRectangle(xd,xw,xgc,DM-1,DT-1,DW+2,DH+2);
        }

        for (i=0; i<=topge; ++i) {
            if ((!inp) ||
                (ge[i].type == GE_INPUT) ||
                (ge[i].type == GE_CBOX) ||
                (ge[i].type == GE_RADIO)) {

                draw_ge(((edesc *)ge)+i);
            }
        }
    }

    /* draw the scanned page */
    else if ((CDW == PAGE) && (cl_ready)) {
        int i,id,p,f;

        /*
            Clip the window because the graphic cursor may
            surpass the window.
        */
        if (mclip == 0) {
            XRectangle r;

            r.x = DM;
            r.y = DT;
            r.width = DW;
            r.height = DH;
            XSetClipRectangles(xd,xgc,0,0,&r,1,Unsorted);
        }
    
        /* clear background */
        if (bg) {
            XSetForeground (xd,xgc,white.pixel);
            XFillRectangle(xd,xw,xgc,DM-1,DT-1,DW+2,DH+2);
        }

        /*
            when sliding or drawing bounding boxes instead of symbols,
            various visual features are switched off.
        */
        f = ((!sliding) && (*cm_d_bb == ' '));

        /* get the active class */
        if ((curr_mc >= 0) && ((id = mc[curr_mc].bm) >= 0)) {
            p = id2idx(id);
        }
        else {
            id = -1;
            p = -1;
        }

        /*
            Draw gray ellipses to enclose the symbols with
            unknown alignment.
        */
        if ((f) && (*cm_g_align != ' ')) {
            for (i=0; i<=topps; ++i) {
                if (mc[ps[i]].va < 0)
                    draw_symbol(mc+ps[i],1);
            }
        }

        /*
            Draw gray ellipses to enclose the symbols of the
            current class.
        */
        else if ((f) && (*cm_v_cc != ' ') && (p>=0)) {
            for (i=0; i<=topps; ++i) {
                if (mc[ps[i]].bm == id)
                    draw_symbol(mc+ps[i],1);
            }
        }

        /*
            Draw a black ellipse to enclose the current symbol.
        */
        if ((f) && (curr_mc >= 0)) {
            draw_symbol(mc+curr_mc,2);
        }

        /*
            Draw the symbols
        */
        list_s(X0,Y0,HR,VR);
        for (i=0; i<list_s_sz; ++i) {
            int c;

            c = list_s_r[i];

            /* the character under the cursor */
            if ((f) && (c == curr_mc)) {

                /* darkgray if UNDEF */
                if (uncertain(mc[c].tc))
                    draw_symbol(mc+c,-2);

                /* white */
                else
                    draw_symbol(mc+c,-1);
            }

            /*
                Symbol with unknown alignment or on the
                current class.
            */
            else if ((f) &&
                    (((*cm_g_align != ' ') && (mc[c].va < 0)) ||
                    ((p>=0) && (*cm_v_cc != ' ') && (mc[c].bm==id)))) {

                /* white if UNDEF */
                if (uncertain(mc[c].tc))
                    draw_symbol(mc+c,-1);

                /* black otherwise */
                else
                    draw_symbol(mc+c,0);
            }

            /* UNDEF symbols are darkgray */
            else if ((f) && (uncertain(mc[c].tc)))
                draw_symbol(mc+c,-2);

            /* others are black */
            else
                draw_symbol(mc+c,0);
        }

        /*
            Draw ellipses around words, if
            requested.
        */
        if ((sliding == 0) && (*cm_d_words != ' ')) {
            int i;

            XSetForeground(xd,xgc,black.pixel);

            /* draw ellipses and arrows */
            for (i=0; i<=topw; ++i) {
                wdesc *a,*b;
                int x,y;

                if ((word[i].F >= 0) &&
                    ((*cm_g_bo == ' ') || (word[i].f & F_BOLD)) &&
                    ((*cm_g_io == ' ') || (word[i].f & F_ITALIC))) {

                    a = word + i;

                    /* draw ellipse */
                    if (word[i].F != word[i].L)
                        XDrawArc(xd,xw,xgc,DM+(a->l-X0)/RF,DT+(a->t-Y0)/RF,
                                 (a->r-a->l)/RF,(a->b-a->t)/RF,0,23040);
                    else {
                        int dx,dy;

                        dx = (a->r - a->l) / 5;
                        dy = (a->b - a->t) / 5;
                        XDrawArc(xd,xw,xgc,DM+(a->l-X0-dx)/RF,
                                 DT+(a->t-Y0-dy)/RF,(a->r-a->l+2*dx)/RF,
                                 (a->b-a->t+2*dy)/RF,0,23040);
                    }

                    /* draw arrow to next word */
                    if (a->E >= 0) {

                        b = word + a->E;

                        x = (b->l - a->r);
                        y = (((a->b>b->b) ? a->b : b->b) -
                             ((a->t<b->t) ? a->t : b->t));

                        XDrawArc(xd,xw,xgc,DM+(a->r-X0)/RF,DT+(a->t-Y0)/RF,
                             x/RF,y/RF,1920,7680);
                    }
                }
            }
        }

        /*
            Draw ellipses around symbols,
            if requested.
        */
        else if ((sliding == 0) && (*cm_d_symbols != ' ')) {
            int i;

            XSetForeground(xd,xgc,black.pixel);

            /* draw ellipses */
            for (i=0; i<list_s_sz; ++i) {
                sdesc *a;

                a = mc + list_s_r[i];

                if ((a->r < X0+HR) &&
                    (a->l >= X0) &&
                    (a->t < Y0+VR) &&
                    (a->b >= Y0))

                    {


                    /* draw ellipse */
                    if (mc[i].ncl > 1)
                        XDrawArc(xd,xw,xgc,DM+(a->l-X0)/RF,DT+(a->t-Y0)/RF,
                                 (a->r-a->l)/RF,(a->b-a->t)/RF,0,23040);
                    else {
                        int dx,dy;

                        dx = (a->r - a->l) / 5;
                        dy = (a->b - a->t) / 5;
                        XDrawArc(xd,xw,xgc,DM+(a->l-X0-dx)/RF,
                                 DT+(a->t-Y0-dy)/RF,(a->r-a->l+2*dx)/RF,
                                 (a->b-a->t+2*dy)/RF,0,23040);
                    }

                    /* draw arrow to next symbol */
                    /*
                    if (a->E >= 0) {

                        b = mc + a->E;

                        x = (b->l - a->r);
                        y = (((a->b>b->b) ? a->b : b->b) -
                             ((a->t<b->t) ? a->t : b->t));

                        XDrawArc(xd,xw,xgc,DM+(a->r-X0)/RF,DT+(a->t-Y0)/RF,
                             x/RF,y/RF,1920,7680);
                    }
                    */
                }
            }
        }

        /*
            Draw ellipses around closures,
            if requested.
        */
        else if ((sliding == 0) &&
                 (*cm_d_closures != ' ')) {
            int i;

            XSetForeground(xd,xgc,black.pixel);

            /* draw ellipses */
            for (i=0; i<=list_cl_sz; ++i) {
                cldesc *a;

                a = cl + list_cl_r[i];

                if ((a->r < X0+HR) &&
                    (a->l >= X0) &&
                    (a->t < Y0+VR) &&
                    (a->b >= Y0))

                    {


                    /* draw ellipse */
                    if (mc[i].ncl > 1)
                        XDrawArc(xd,xw,xgc,DM+(a->l-X0)/RF,DT+(a->t-Y0)/RF,
                                 (a->r-a->l)/RF,(a->b-a->t)/RF,0,23040);
                    else {
                        int dx,dy;

                        dx = (a->r - a->l) / 5;
                        dy = (a->b - a->t) / 5;
                        XDrawArc(xd,xw,xgc,DM+(a->l-X0-dx)/RF,
                                 DT+(a->t-Y0-dy)/RF,(a->r-a->l+2*dx)/RF,
                                 (a->b-a->t+2*dy)/RF,0,23040);
                    }
                }
            }
        }

        /*
            Draw ellipses around lines (geometrical),
            if requested.
        */
        else if ((sliding == 0) && (*cm_g_glines != ' ')) {
            int a,l,r,t,b,lm,rm;

            if ((0 <= curr_mc) && (curr_mc <= tops)) {

                /* search leftmost symbol */
                for (lm=curr_mc; (a=lsymb(lm,1)) >= 0; lm=a);

                /* search rightmost symbol */
                for (rm=curr_mc; (a=rsymb(rm,1)) >= 0; rm=a);

                l = mc[lm].l;
                r = mc[rm].r;
                t = mc[curr_mc].t;
                b = mc[curr_mc].b;

                XDrawArc(xd,xw,xgc,DM+(l-X0)/RF,DT+(t-Y0)/RF,
                         (r-l)/RF,(b-t)/RF,0,23040);
            }
        }

        /* remove clipping */
        if (mclip == 0) {
            XRectangle r;

            r.x = 0;
            r.y = 0;
            r.width = WW;
            r.height = WH;
            XSetClipRectangles(xd,xgc,0,0,&r,1,Unsorted);
        }

        redraw_zone = 1;
    }

    /* the scanned page (pixmap) */
    else if ((CDW == PAGE) && (pixmap != NULL)) {

        draw_pm(pixmap,XRES,YRES,DM,DT);
        redraw_zone = 1;
    }

    /*
        Set the buttons accordingly to the current symbol.

        BUG: if the user change the status of the "italic"
        button, then resize the window the status of the
        "italic" button will change (really?).
    */
    if (CDW == PATTERN)
        set_buttons(0,cdfc);
    else if (CDW == PAGE)
        set_buttons(1,curr_mc);
    else if (!dw[PAGE].v) {
        button[balpha] = 0;
        button[btype] = 0;
    }

    /* restore default redraw area */
    if (mclip == 0) {
        clip_dw(0);
    }
}

/*

Free global resources allocated by graphic elements.

*/
void free_earray(void)
{
    int i;

    /* just a workaround by now */
    for (i=0; i<=topge; ++i)
        if (ge[i].type == GE_SELECT)
            --TOP_CM;
    topge = -1;
}

/*

Get image size.

*/
void img_size(char *img,int *w,int *h)
{
    if (strcmp(img,"internal") == 0) {
        *w = (RWX + 4 + 1) / plist_rf;
        *h = (RWY + 4 + 1) / plist_rf;
    }
    else if ((strncmp(img,"pattern/",8) == 0) || (strncmp(img,"symbol/",7) == 0)) {
        *w = FS/plist_rf;
        *h = FS/plist_rf;
    }
    else if (strncmp(img,"apatt/",6) == 0) {
        int i;

        i = id2idx(atoi(img+6));
        if ((0<=i) && (i<=topp))
            *w = pattern[i].bw/plist_rf;
        else
            *w = FS/plist_rf;
        *h = FS/plist_rf;
    }
    else if (strncmp(img,"word/",5) == 0) {
        int i;
        wdesc *W;

        i = id2idx(atoi(img+5));
        W = word + i;
        *w = (W->r - W->l + 1) / plist_rf;
        *h = (W->b - W->t + 1) / plist_rf;
    }
}

/*

Add text line to the welcome page.

*/
void add_text_welcome(char *s,int y)
{
    XTextItem xti;
    int t;

    xti.delta = 0;
    xti.font = dfont->fid;
    xti.chars = s;
    xti.nchars = t = strlen(xti.chars);
    XSetForeground(xd,xgc,black.pixel);
    if (((DT-PT-TH) > 2*DFH) && (DW > t*DFW)) {
        XDrawText(xd,xw,xgc,DM+(DW-t*DFW)/2,y,&xti,1);
    }
}

/*


*/
void move_closure(cldesc *c)
{
    c->l -= 10;
    if ((c->r -= 10) < 0) {
        c->l += 1200;
        c->r += 1200;
    }
}

/*

Draw the welcome window.

*/
void draw_welcome(void)
{
    char v[20];

    /* closures */
    XSetForeground(xd,xgc,white.pixel);
    XFillRectangle(xd,xw,xgc,DM,DT,DW,DH);
    draw_closure(&c12,0);
    draw_closure(&c13,0);
    draw_closure(&c14,0);
    draw_closure(&c15,0);
    draw_closure(&c16,0);
    draw_closure(&c17,0);
    draw_closure(&c18,0);
    draw_closure(&c19,0);

    /* text messages */
    add_text_welcome("W E L C O M E   T O",DT-DFH-VS);
    add_text_welcome("(  C  L  A  R  A     O  C  R  )",DT+DH+10+DFH+VS);
    snprintf(v,19,"version %s",version);
    v[19] = 0;
    add_text_welcome(v,PT+PH-6*(DFH+VS));
    add_text_welcome("(C) 1999-2002 Ricardo Ueda Karpischek and others",PT+PH-5*(DFH+VS));
    add_text_welcome("Clara OCR comes with ABSOLUTELY NO WARRANTY",PT+PH-4*(DFH+VS));
    add_text_welcome("type L to see the conditions",PT+PH-3*(DFH+VS));

    /* provision for one more line */
    /* add_text_welcome("new line",PT+PH-2*(DFH+VS)); */

#ifdef FUN_CODES
    /* move banner */
    if (fun_code == 1) {
        move_closure(&c12);
        move_closure(&c13);
        move_closure(&c14);
        move_closure(&c15);
        move_closure(&c16);
        move_closure(&c17);
        move_closure(&c18);
        move_closure(&c19);
    }
#endif
}

/*

Draw the alphabet map.

*/
void draw_map(void)
{
    int i,j,k,l,t;

    /* background */
    if (*cm_v_map != ' ') {
        XSetForeground(xd,xgc,darkgray.pixel);
        XFillRectangle(xd,xw,xgc,PM-52,PT-1,42,(max_sz*15+max_sz-1)+4);
    }

    /* draw the map */
    if ((*cm_v_map != ' ') && (alpha != NULL)) {

        /* cells */
        XSetForeground(xd,xgc,gray.pixel);
        for (i=0; i<alpha_sz; ++i) {
            for (j=0; j<alpha_cols; ++j)
                XFillRectangle(xd,xw,xgc,PM-50+j*13,PT+i*16+1,12,15);
        }

        /* letters */
        XSetForeground(xd,xgc,black.pixel);
        for (i=0; i<alpha_sz; ++i) {

            /* capital letters */
            for (j=0; j<16; ++j) {
                t = alpha[i].cbm[j];
                for (k=0; k<8; ++k) {
                    if (t & 1) {
                        XDrawPoint(xd,xw,xgc,PM-50+9-k,PT+i*16+j-1);
                    }
                    t >>= 1;
                }
            }

            /* small leters */
            if (alpha_cols >= 2)
                for (j=0; j<16; ++j) {
                    t = alpha[i].sbm[j];
                    for (k=0; k<8; ++k) {
                        if (t & 1) {
                            XDrawPoint(xd,xw,xgc,PM-37+9-k,PT+i*16+j-1);
                        }
                        t >>= 1;
                    }
                }

            /* latin keyboard mapping */
            if (alpha_cols >= 3)
                for (j=0; j<16; ++j) {
                    for (l=0; (l<latin_sz) && (alpha[i].l!=latin[l].l); ++l);
                    if (l < latin_sz) {
                        t = latin[l].cbm[j];
                        for (k=0; k<8; ++k) {
                            if (t & 1) {
                                XDrawPoint(xd,xw,xgc,PM-24+9-k,PT+i*16+j-1);
                            }
                            t >>= 1;
                        }
                    }
                }
        }

        /* frames */
        if (alpha != NULL)
            draw_dframe(PM-50,PT+1,alpha_cols*12+alpha_cols-1,alpha_sz*15+alpha_sz-1);
    }
}

/*

Redraw junction when resizing windows.

*/
void redraw_junction(int j,int oj,int tdw,int bdw)
{
    int cur_dw,sb,a,b;
    XRectangle r;

    cur_dw = CDW;

    if (*cm_v_hide == ' ')
        sb = 10+RW;
    else
        sb = 0;

    /* compute the bottom of the top window before the resize */
    if (tdw == PAGE)
        a = LAST_PAGE_DT + LAST_PAGE_DH - 1;
    else if (tdw == PAGE_OUTPUT)
        a = LAST_OUTPUT_DT + LAST_OUTPUT_DH - 1;
    else {
        db("invalid context for redraw_junction");
        return;
    }

    /* compute the top of the bottom window before the resize */
    if (bdw == PAGE_OUTPUT)
        b = LAST_OUTPUT_DT;
    else if (bdw == PAGE_SYMBOL)
        b = LAST_SYMBOL_DT;
    else {
        db("invalid context for redraw_junction");
        return;
    }

    /*
        Set private clip to go from the topmost of the old and
        new bottom of the top window to the bottommost of the old and
        new top of the bottom window.
    */
    r.x = PM+4;
    r.width = PW-8;
    if (j > oj) {
        if (tdw == PAGE) {
            r.y = dw[tdw].dt;
            r.height = dw[bdw].dt-r.y;
            PDT = a;
            PVR = (dw[PAGE].dt + dw[PAGE].dh - PDT) * dw[PAGE].rf;
        }
        else {
            r.y = a;
            r.height = dw[bdw].dt-a;
        }
    }
    else {
        r.y = dw[tdw].dt + dw[tdw].dh - 1;
        r.height = b - dw[tdw].dt + dw[tdw].dh - 1;
    }

    XSetClipRectangles(xd,xgc,0,0,&r,1,Unsorted);
    mclip = 1;

    /* repaint background avoiding to overwrite the components */
    {
        XSetForeground(xd,xgc,gray.pixel);

        /* clearance over the new top of the bottom window */
        XFillRectangle(xd,xw,xgc,PM+4,dw[bdw].dt-10+2,PW-8,6);

        /*
            When the PAGE window is narrower than PAGE_OUTPUT, the
            background must be redrawn over the left and right
            clearances.
        */
        if (tdw == PAGE) {
            int l,h;

            if (j > oj) {

                /* left clearance */
                l = dw[PAGE].dm - dw[PAGE_OUTPUT].dm + 3;
                h = b - dw[PAGE_OUTPUT].dt;
                XFillRectangle(xd,xw,xgc,PM+4,b-3,l,h);

                /* right clearance */
                XFillRectangle(xd,xw,xgc,dw[PAGE].dm+dw[PAGE].dw,b-3,l,h);
            }

            else if (sb > 0) {

                /* background on the corner of the two scrollbars */
                XFillRectangle(xd,xw,xgc,dw[PAGE].dm+dw[PAGE].dw+7,dw[PAGE_OUTPUT].dt-10-RW-3,RW+6,RW+6);
            }
        }

        if (sb > 0) {

        XFillRectangle(xd,xw,xgc,PM+4,dw[bdw].dt-10+2,PW-8,6);

            /* background between the top window and the scrollbar */
            XFillRectangle(xd,xw,xgc,PM+4,dw[bdw].dt-10-sb+2,PW-8,6);

            /* background on the corner of the two scrollbars */
            XFillRectangle(xd,xw,xgc,PM+PW-10-RW-10+2,dw[bdw].dt-sb-2,8+RW+4,RW+4);
        }
    }

    /* redraw top window */
    if (j > oj) {
        r.x = DM-1;
        r.width = DW+2;
        r.y = oj-5-sb;
        r.height = j-oj;
        XSetClipRectangles(xd,xgc,0,0,&r,1,Unsorted);
        mclip = 1;
        CDW = tdw;
        draw_dw(1,0);
    }

    /* unset private clip */
    set_mclip(0);

    if (tdw == PAGE) {
        PDT = dw[PAGE].dt;
        PVR = dw[PAGE].vr;
    }

    /* top window frames */
    CDW = tdw;
    draw_dw_frames();

    /* redraw bottom window */
    CDW = bdw;
    draw_dw(1,0);
    draw_dw_frames();

    /* top window scrollbars */
    CDW = tdw;
    draw_sbars();

    /* bottom window scrollbars */
    CDW = bdw;
    draw_sbars();

    /* restore CDW */
    CDW = cur_dw;
}

/*

Draw the zone limits[0..7].

*/
void draw_zone(int *limits)
{
    int i,j,l,r,t,b;
    int x,y,x0,y0,dx,dx2,dy,dy2,k,vx,vy,sy,sx;
    int f,fx,fy,lx,ly;

    /* line attributes and color to use */
    XSetLineAttributes(xd,xgc,1,LineSolid,CapNotLast,JoinRound);
    XSetForeground(xd,xgc,black.pixel);

    /*
        Rectangular code
        ----------------

        This code is currently unused. It draws only rectangular zones.
    */
    if (0) {

        /* horizontal */
        if (! (((limits[0]-X0)/RF > DW) || (limits[4] < X0))) {
            l = (limits[0] < X0) ? 0 : ((limits[0]-X0) / RF);
            if ((r = (limits[4]-X0) / RF) >= DW)
                r = DW - 1;
            for (i=1; i<=5; i+=4)
                if ((Y0 <= limits[i]) && ((t=(limits[i]-Y0)/RF) < DH))
                    XDrawLine(xd,xw,xgc,DM+l,DT+t,DM+r,DT+t);
        }

        /* vertical */
        if (! (((limits[1]-Y0)/RF > DH) || (limits[5] < Y0))) {
            t = (limits[1] < Y0) ? 0 : ((limits[1]-Y0) / RF);
            if ((b = (limits[5]-Y0) / RF) >= DH)
                b = DH - 1;
            for (i=0; i<=4; i+=4)
                if ((X0 <= limits[i]) && ((l=(limits[i]-X0)/RF) < DW))
                    XDrawLine(xd,xw,xgc,DM+l,DT+t,DM+l,DT+b);
        }
    }

    /*
        Non-rectangular
        ---------------

        This is a brute-force approach. This code draws each line using a
        Bresenham-like method, in order to detect the first and last visible
        pixels.
    */
    else {

        for (j=0; j<4; ++j) {

            /* draw (x0,y0) -> (x0+dx,y0+dy) */
            x0 = limits[2*j];
            y0 = limits[2*j+1];
            dx = limits[(2*(j+1))%8] - x0;
            dy = limits[(2*(j+1)+1)%8] - y0;

            /* variation */
            vx = (dx > 0) ? 1 : ((dx < 0) ? -1 : 0);
            vy = (dy > 0) ? 1 : ((dy < 0) ? -1 : 0);

            /* prepare */
            dx = abs(dx);
            dy = abs(dy);
            dx2 = 2*dx;
            dy2 = 2*dy;
            x = x0;
            y = y0;
            i = 0;
            f = 0;
            sx = sy = 0;

            /* just to avoid a compilation warning */
            fx = fy = lx = ly = 0;

            /* use x as reference */
            if (dx >= dy) {

                for (k=0; k<=dx; ++k) {

                    /* found visible pixel */
                    if ((X0<=x) && (x-X0<HR) && (Y0<=y) && (y-Y0<VR)) {

                        /* the first pixel inside */
                        if (f==0) {
                            fx = x;
                            fy = y;
                            f = 1;
                        }

                        /* remember because maybe this is the last one inside.. */
                        lx = x;
                        ly = y;
                    }

                    /* the first pixel outside, get out */
                    else if (f) {
                        k = dx + 1;
                    }

                    /* compute next pixel */
                    x += vx;
                    sy += dy2;
                    if ((i == 0) && (sy >= dx)) {
                        y += vy;
                        i = 1;
                    }
                    if (sy >= dx2) {
                        sy -= dx2;
                        i = 0;
                    }
                }
            }

            /* use y as reference */
            else {

                for (x=x0, y=y0, k=0, i=0, f=0, l=0, sx=0; k<=dy; ++k) {

                    /* found visible pixel */
                    if ((X0<=x) && (x-X0<HR) && (Y0<=y) && (y-Y0<VR)) {

                        /* the first pixel inside */
                        if (f==0) {
                            fx = x;
                            fy = y;
                            f = 1;
                        }

                        /* remember because maybe this is the last one inside.. */
                        lx = x;
                        ly = y;
                    }

                    /* the first pixel outside, get out */
                    else if (f) {
                        k = dy + 1;
                    }

                    /* compute next pixel */
                    y += vy;
                    sx += dx2;
                    if ((i == 0) && (sx >= dy)) {
                        x += vx;
                        i = 1;
                    }
                    if (sx >= dy2) {
                        sx -= dy2;
                        i = 0;
                    }
                }
            }

            if (f) {
                XDrawLine(xd,xw,xgc,DM+(fx-X0)/RF,DT+(fy-Y0)/RF,DM+(lx-X0)/RF,DT+(ly-Y0)/RF);
            }
        }

    }
}

/*

The function redraw
-------------------

The function "redraw" redraws all graphic components as requested
by the redraw flags (redraw_button, redraw_dw, etc). These flags
are set by the procedures that request display. For instance, if
a procedure must put a message on the status line, it will copy
the message to the buffer mb and set redraw_stline to 1. From
time to time the function redraw must be called to perform the
redraw as determined by the flags and the associated data
structures.

The function redraw can be called at anytime by anyone. The only
possible side effect will be wasting system resources. Currently
redraw is called only by xevents.

*/
void redraw(void)
{
    int i;
    XTextItem xti;

    /*
        From time to time we use this block for debugging..
    */
    if ((redraw_tab) ||
        (redraw_button) ||
        (redraw_inp) ||
        (redraw_dw) ||
        (redraw_j1) ||
        (redraw_j2) ||
        (redraw_zone) ||
        (redraw_stline) ||
        (redraw_pbar) ||
        (redraw_flea) ||
        (redraw_menu)) {

        /* db("updating screen"); */
    }
    else {
        set_mclip(0);
        return;
    }

#ifdef FUN_CODES
    /* menus stop the flea to avoid trashing the window */
    if ((cmenu != NULL) && (fun_code == 3)) {
        redraw_flea = 1;
        fun_code = 0;
    }
#endif

    /*

       Redraw background
       -----------------

    */
    if (redraw_bg) {
        int a;

        /* window background color */
        XSetForeground(xd,xgc,darkgray.pixel);

        /* an expensive method.. */
        if (1) {
            XFillRectangle(xd,xw,xgc,0,MH,WW,WH-MH);
        }

        /* a bad method.. */
        else {
            XFillRectangle(xd,xw,xgc,0,MH,WW,10+TH);
            XFillRectangle(xd,xw,xgc,0,MH,10,WH-MH);
            XFillRectangle(xd,xw,xgc,0,MH,10+BW,10);
            a = MH+10+BUTT_PER_COL*BH;
            XFillRectangle(xd,xw,xgc,0,a,10+BW+10,WH-a);
            a = MH+10+PH;
            XFillRectangle(xd,xw,xgc,0,a,WW,WH-a);
            XFillRectangle(xd,xw,xgc,WW-10,MH,10,WH-MH);
        }

        /* plate background */
        XSetForeground (xd,xgc,gray.pixel);
        XFillRectangle(xd,xw,xgc,PM,PT+TH,PW,PH-TH);

        /* plate frame */
        XSetLineAttributes(xd,xgc,2,LineSolid,CapNotLast,JoinRound);
        XSetForeground (xd,xgc,vdgray.pixel);
        XDrawLine(xd,xw,xgc,PM,PT+PH-1,PM+PW-1,PT+PH-1);
        XDrawLine(xd,xw,xgc,PM+PW-1,PT+TH,PM+PW-1,PT+PH-1);
        XSetForeground (xd,xgc,white.pixel);
        XDrawLine(xd,xw,xgc,PM,PT+TH,PM,PT+PH-1);
        XSetLineAttributes(xd,xgc,1,LineSolid,CapNotLast,JoinRound);
    }

    /*

       Redraw tabs
       -----------

    */
    if (redraw_tab) {
        XSetForeground (xd,xgc,darkgray.pixel);
        XFillRectangle(xd,xw,xgc,PM-2,PT,PW+4,TH);
        for (i=0; i<TABN; ++i) {
            draw_tab(PM+10+i*(10+TW+10),PT,i);
        }
    }

    /*

        Redraw windows
        --------------

    */
    if (redraw_dw) {
        int i,cur_dw;

        /* welcome page */
        if (dw[WELCOME].v) {
            draw_welcome();
            draw_dw_frames();
        }

        /* scanned page */
        if (dw[PAGE].v) {
            cur_dw = CDW;
            CDW = PAGE;
            draw_dw(1,0);
            draw_sbars();
            draw_dw_frames();
            CDW = cur_dw;
        }

        /* PAGE fat bits */
        if (dw[PAGE_FATBITS].v) {
            if (dw[PAGE_FATBITS].rg) {

#ifdef FUN_CODES
                /* stop the flea */
                if (fun_code == 3) {

                    /* stop it */
                    redraw_flea = 1;
                    fun_code = 0;
                }
#endif

                copychar();
            }
            draw_cfont(cfont);
            draw_sbars();
        }

        /* pattern in fat bit mode */
        if (dw[TUNE_PATTERN].v) {

            /* regenerate the fat bit image */
            if (dw[TUNE_PATTERN].rg) {

                /* compute the skeleton and copy to cfont */
                if ((0<=cdfc) && (cdfc<=topp)) {
                    pskel(cdfc);
                    p2cf();
                }

                /* no valid pattern: clear cfont */
                else
                    memset(cfont,WHITE,FS*FS);

                dw[TUNE_PATTERN].rg = 0;
            }
            cur_dw = CDW;
            CDW = TUNE_PATTERN;
            draw_cfont(cfont);
            CDW = cur_dw;
        }

        /* MATCHES */
        if (dw[PAGE_MATCHES].v) {
            draw_cfont(cfont);
        }

        /* HTML windows */
        for (i=0; i<=TOP_DW; ++i) {
            if ((dw[i].html) && (dw[i].v)) {
                cur_dw = CDW;
                CDW = i;

                if (RG) {
                    curr_hti = -1;
                    if (CDW == PAGE_OUTPUT)
                        mk_page_output(0);
                    else if (CDW == PATTERN_LIST)
                        mk_pattern_list();
                    else if (CDW == PATTERN)
                        mk_pattern_props();
                    else if (CDW == PATTERN_TYPES)
                        mk_pattern_types();
                    else if (CDW == PATTERN_ACTION)
                        mk_pattern_action();
                    else if (CDW == TUNE_SKEL)
                        mk_tune_skel();
                    else if (CDW == TUNE)
                        mk_tune();
                    else if (CDW == TUNE_ACTS)
                        mk_history(curr_act);
                    else if (CDW == PAGE_LIST)
                        mk_page_list();
                    else if (CDW == PAGE_SYMBOL)
                        mk_page_symbol(curr_mc);
                    else if (CDW == PAGE_DOUBTS)
                        mk_page_doubts();
                    else if (CDW == DEBUG)
                        mk_debug();

                    /* HTML renderization */
                    if ((HS) && (*cm_v_vhs == ' '))
                        html2ge(text,HM);

                    /* text renderization */
                    else
                        text2ge((CDW==GPL) ? gpl : text);

                    check_dlimits(0);
                    RG = 0;
                }
                draw_dw(1,0);

                if (dw[CDW].ds)
                    draw_sbars();
                draw_dw_frames();
                CDW = cur_dw;
            }
        }

        /* no need to redraw input fields if we redraw all windows */
        redraw_inp = 0;
    }

    /*

        Redraw the flea
        ---------------

    */
    if (redraw_flea) {

        if (CDW == PAGE_FATBITS) {

            /* someone stopped the flea: clear last and current positions */
            if (fun_code != 3) {
                if ((0<=last_fp) && (last_fp<=topfp)) {
                    draw_flea(fp[2*last_fp],fp[2*last_fp+1],0);
                    last_fp = -1;
                }
                if ((0<=curr_fp) && (curr_fp<=topfp))
                    draw_flea(fp[2*curr_fp],fp[2*curr_fp+1],0);
            }

            /* the flea moved */
            else {

                /* clear last */
                if ((0<=last_fp) && (last_fp<=topfp)) {
                    draw_flea(fp[2*last_fp],fp[2*last_fp+1],0);
                    last_fp = -1;
                }

                /* draw current */
                if ((0<=curr_fp) && (curr_fp<=topfp))
                    draw_flea(fp[2*curr_fp],fp[2*curr_fp+1],1);
            }
        }

        redraw_flea = 0;
    }

    /*

      Redraw buttons
      --------------

    */
    if (redraw_button) {

        /* draw buttonbars */
        draw_bb(10,PT,BL,0);

        /*
            Deprecated "arrow" buttons. Maybe this code will
            become useful in the future.
        */
        {
            /* prepare to draw arrows */
            /*
            XSetForeground(xd,xgc,black.pixel);
            XSetLineAttributes(xd,xgc,1,LineSolid,CapNotLast,JoinRound);
            */

            /* left */
            /*
            j = PT+bprev*(BH+VS)+BH/2;
            draw_arrow(2,10+(BW-20)/2,j,0);
            */

            /* right */
            /*
            j = PT+bnext*(BH+VS)+BH/2;
            draw_arrow(2,10+(BW-20)/2,j,1);
            */

            /* start and end of page */
            /*
            j = PT+sarrow*(BH+VS)+BH/2;
            draw_arrow(1,15,j,0);
            draw_arrow(1,10+BW-20,j,1);
            */

            /* top and bottom */
            /*
            j = PT+tarrow*(BH+VS)+(BH-12)/2;
            draw_arrow(1,20-2,j,2);
            draw_arrow(1,10+BW-10-2,j,3);
            */
        }
    }

    /*

      Redraw input fields
      -------------------

    */
    if (redraw_inp) {
        int cur_dw;

        /* redraw input fields on visible HTML windows */
        for (i=0; i<=TOP_DW; ++i) {
            if ((dw[i].html) && (dw[i].v)) {
                cur_dw = CDW;
                CDW = i;
                draw_dw(0,1);
                CDW = cur_dw;
            }
        }
    }

    /* redraw PAGE/PAGE_OUTPUT junction */
    if (redraw_j1) {
        redraw_junction(page_j1,opage_j1,PAGE,PAGE_OUTPUT);
    }

    /* redraw PAGE_OUTPUT/PAGE_SYMBOL junction */
    if (redraw_j2) {
        redraw_junction(page_j2,opage_j2,PAGE_OUTPUT,PAGE_SYMBOL);
    }

    /*

        Redraw the rectangle around the current zone
        --------------------------------------------

    */
    if (redraw_zone) {
        int cur_dw,t;

        /* redraw OCR zone */
        if ((dw[PAGE].v) && (zones > 0)) {

            cur_dw = CDW;
            CDW = PAGE;

            for (t=0; t<zones; ++t) {
                draw_zone(limits+t*8);
            }

            CDW = cur_dw;
        }
    }

    /*

       Redraw status line
       ------------------

    */
    if (redraw_stline) {

        /* clear message area */
        XSetForeground(xd,xgc,darkgray.pixel);
        XFillRectangle(xd,xw,xgc,10,WH-10-DFH-2,WW-20,DFH+4);

        /* message */
        XSetForeground(xd,xgc,black.pixel);
        xti.delta = 0;
        xti.font = dfont->fid;
        xti.chars = mb;
        if ((xti.nchars = strlen(xti.chars)) > 0) {
            XDrawText(xd,xw,xgc,10,WH-10-DFH+DFA,&xti,1);
        }
    }

    /*

       Redraw progress bar
       -------------------

    */
    if (redraw_pbar) {

        /* clear message area */
        XSetForeground(xd,xgc,darkgray.pixel);
        XFillRectangle(xd,xw,xgc,10,WH-10-DFH-2,WW-20,DFH+4);

        /* progress bar */
        XSetForeground(xd,xgc,black.pixel);
        xti.delta = 0;
        xti.font = dfont->fid;
        xti.chars = mb;
        if ((xti.nchars = strlen(xti.chars)) > 0) {
            XDrawText(xd,xw,xgc,10,WH-10-DFH+DFA,&xti,1);
        }

        XSetLineAttributes(xd,xgc,2,LineSolid,CapNotLast,JoinRound);
        XSetForeground (xd,xgc,black.pixel);
        XDrawLine(xd,xw,xgc,10,WH-10,10+redraw_pbar,WH-10);
        XSetLineAttributes(xd,xgc,1,LineSolid,CapNotLast,JoinRound);
    }

    /*

        Redraw alphabet mapping
        -----------------------

        remark: the menu must be the last component draw by the
        function redraw.
    */
    if (redraw_map) {
        draw_map();
    }

    /*

        Redraw context menu
        -------------------

    */
    if (redraw_menu) {

        /* redraw menu bar */
        if ((cmenu==NULL) || (CY0_orig < 0) || (redraw_menu==2)) {
            draw_mbar();
        }

        /* redraw menu */
        if (cmenu != NULL) {
            draw_menu();
        }
    }

    /*
        Copy updated areas
    */
    if (use_xb) {

        /* copy the entire window */
        if (redraw_bg) {
            XCopyArea(xd,xw,XW,xgc,0,0,WW,WH,0,0);
        }

        /* copy the menu */
        else if (mclip) {

            if (cmenu != NULL) {
                XCopyArea(xd,xw,XW,xgc,CX0-1,CY0-1,CW+2,CH+2,CX0-1,CY0-1);
            }
        }

        else {

            /* copy the menu bar and the menu (if any) */
            if (redraw_menu) {
                XCopyArea(xd,xw,XW,xgc,0,0,WW,MH,0,0);

                if (cmenu != NULL) {
                    XCopyArea(xd,xw,XW,xgc,CX0-1,CY0-1,CW+2,CH+2,CX0-1,CY0-1);
                }
            }

            /* copy the buttonbar */
            if (redraw_button) {
                XCopyArea(xd,xw,XW,xgc,9,PT-1,BW+2,BUTT_PER_COL*(BH+VS)-VS+2,9,PT-1);
            }

            /* copy the tabs */
            if (redraw_tab) {
                XCopyArea(xd,xw,XW,xgc,PM-2,PT,PW+4,TH,PM-2,PT);
            }

            /*
                Copy the plate. TODO: redraw_inp should not require
                redrawing the windows!
            */
            if ((redraw_dw) ||
                (redraw_j1) ||
                (redraw_j2) ||
                (redraw_inp)) {

                XCopyArea(xd,xw,XW,xgc,PM-2,PT+TH,PW+4,PH-TH,PM-2,PT+TH);
            }

            /* copy the status line */
            if (redraw_stline) {
                XCopyArea(xd,xw,XW,xgc,10,WH-10-DFH-2,WW-20,DFH+4,10,WH-10-DFH-2);
            }

            /* copy the progress bar */
            if (redraw_pbar) {
                XCopyArea(xd,xw,XW,xgc,10,WH-10-DFH-2,WW-20,DFH+4,10,WH-10-DFH-2);
            }
        }

    }

    /* done */
    redraw_bg = 0;
    redraw_tab = 0;
    redraw_dw = 0;
    redraw_button = 0;
    redraw_inp = 0;
    redraw_j1 = 0;
    redraw_j2 = 0;
    redraw_zone = 0;
    redraw_stline = 0;
    redraw_pbar = 0;
    redraw_map = 0;
    redraw_menu = 0;

    /* make sure that mclip is unset */
    set_mclip(0);

    /* flush outgoing events */
    XFlush(xd);
}

/*

Set the geometry of the PAGE tab.

*/
void set_pagetab(int y1,int y2,int u)
{
    int i,sb;
    int cur_dw;

    /*

        These are required to optimize window resize.

    */
    if (y1 != page_j1) {
        opage_j1 = page_j1;
        page_j1 = y1;
        LAST_PAGE_DT = dw[PAGE].dt;
        LAST_PAGE_DM = dw[PAGE].dm;
        LAST_PAGE_DH = dw[PAGE].dh;
        LAST_OUTPUT_DT = dw[PAGE_OUTPUT].dt;
        LAST_OUTPUT_DH = dw[PAGE_OUTPUT].dh;
    }
    else {
        opage_j2 = page_j2;
        page_j2 = y2;
        LAST_OUTPUT_DT = dw[PAGE_OUTPUT].dt;
        LAST_OUTPUT_DH = dw[PAGE_OUTPUT].dh;
        LAST_SYMBOL_DT = dw[PAGE_SYMBOL].dt;
        LAST_SYMBOL_DH = dw[PAGE_SYMBOL].dh;
    }

    if (*cm_v_hide == ' ')
        sb = 10 + RW;
    else
        sb = 0;

    /* remember CDW */
    cur_dw = CDW;

    /*

        PAGE
    */
    {

        /* current window */
        CDW = PAGE;

        /*
            PAGE bottom will be y1-5. Then subtract the horizontal
            scrollbar width and separation (if any), the tab height and
            the top separation (10) to obtain the PAGE height.
        */
        DH = y1-5-sb-10-TH-PT;
        DW = PW-20-sb;

        /* maximum reduction factor */
        {
            int b;
        
            MRF = YRES/DH + ((YRES % DH) != 0);
            b = XRES/DW + ((XRES % DW) != 0);
            if (b > MRF)
                MRF = b;
            if (RF > MRF)
                RF = MRF;
        }
    
        /* window size in document pixels */
        VR = DH * RF;
        if (VR > YRES)
            VR = YRES;
        HR = DW * RF;
        if (HR > XRES)
            HR = XRES;

        /* grid separation and width */
        GS = 1;
        GW = 0;

        /* recompute geometry from VR and HR */
        DW = HR/RF;
        DH = VR/RF;
        HR = DW*RF;
        VR = DH*RF;

        /* window margins */
        DM = PM+(PW-(DW+sb))/2;
        DT = PT+TH+(y1+5-PT-TH-(DH+sb))/2;

        /* current tab and visible windows */
        tab = PAGE;
        for (i=0; i<=TOP_DW; ++i)
            dw[i].v = 0;
        dw[PAGE].v = 1;

        /* document total size in document pixels */
        GRX = XRES;
        GRY = YRES;

        /* HTML mode */
        HTML = 0;

        /* restricted clip window */
        PHR = HR;
        PVR = VR;
        PDM = DM;
        PDT = DT;
    }

    /*

        PAGE (OUTPUT)

    */
    if (u == 0) {

        /* current window */
        CDW = PAGE_OUTPUT;

        /* window size, measured in document pixels */
        VR = y2-5-sb-y1-5;
        HR = PW-20-sb;
    
        /* grid separation and width */
        GS = 1;
        GW = 0;

        /* window size in display pixels */
        DW = HR;
        DH = VR;

        /* window margins */
        DM = PM+(PW-(DW+sb))/2;
        DT = y1 + 5;

        /* PAGE_OUTPUT is visible */
        dw[PAGE_OUTPUT].v = 1;

        /* reduction factor */
        RF = 1;
    
        /* HTML mode */
        HTML = 1;

        /* do not underline links */
        ULINK = 0;
    }
    else
        dw[PAGE_OUTPUT].v = 0;

    /*

        PAGE (SYMBOL)

    */
    if (u == 0) {

        /* current window */
        CDW = PAGE_SYMBOL;

        /* window size, measured in document pixels */
        VR = PT+PH-10-sb-y2-5;
        HR = PW-20-sb;

        /* grid separation and width */
        GS = 1;
        GW = 0;

        /* window size in display pixels */
        DW = HR;
        DH = VR;

        /* window margins */
        DM = PM+(PW-(DW+sb))/2;
        DT = y2+5;

        /* PAGE_SYMBOL is visible */
        dw[PAGE_SYMBOL].v = 1;

        /* reduction factor */
        RF = 1;

        /* position */
        X0 = Y0 = 0;

        /* HTML mode */
        HTML = 1;
    }
    else
        dw[PAGE_SYMBOL].v = 0;

    /* recover DW */
    CDW = cur_dw;
}

/*

The function force_redraw
-------------------------

This service just set all redraw flags to force the redraw of
all components (buttons, grid, context menu, etc) in the next
call of redraw. So the effets of force_redraw are
asynchronous. The function force_redraw receives no parameter.

*/
void force_redraw(void)
{
    /*db("force_redraw requested");*/

    /* flags */
    redraw_tab = 1;
    redraw_button = -1;
    redraw_bg = 1;
    redraw_grid = 1;
    redraw_stline = 1;
    redraw_dw = 1;
    redraw_menu = 2;
    redraw_map = 1;
}

/*

Sliding actions. These may be scrolling through the
scrollbar cursor or window resizing.

*/
void slide(void)
{
    static struct timeval mrt = {0,0};
    struct timeval t;
    int SL,CL;
    
    if ((0 <= CDW) && (CDW <= TOP_DW) && (sliding <= 2)) {
    
        /* sliding horizontally */
        if (sliding == 1) {
            unsigned d;
            int x,dx;
            float fx;
    
            x = xev.xbutton.x;
            dx = x - s0;
    
            /*
    
                When moving the cursor on the scrollbar, each display
                pixel moved will correspond to how many pixels to move
                the document? The scrollbar length is SL=DW-2*RW and
                the cursor length is CL=HR*(DW-2*RW)/GRX (if GRX>0)
                or CL=DW-2*RW if (GRX==0), both measured in display
                pixels. As difference SL-CL must correspond to GRX-HR
                pixels on the document, each display pixel moved will
                correspond to (GRX-HR)/(SL-CL) pixels to move the
                document.
    
            */
            SL = DW-2*RW;
            if (GRX > 0)
                CL = (HR*(DW-2*RW))/GRX;
            else
                CL = SL;
            if (SL > CL)
                fx = ((float)(GRX-HR)) / (SL-CL);
            else
                fx = 1;
            dx *= fx;
    
            gettimeofday(&t,NULL);
            d = (t.tv_sec - mrt.tv_sec) * 1000000;
            d += (t.tv_usec - mrt.tv_usec);
            if ((dx != 0) && (d > 200000)) {
                X0 += dx;
                check_dlimits(0);
                s0 = x;
                memcpy(&mrt,&t,sizeof(mrt));
                redraw_dw = 1;
                if (CDW == PAGE_FATBITS)
                    RG = 1;
            }
        }
    
        /* sliding vertically */
        else if (sliding == 2) {
            unsigned d;
            int y,dy;
            float fy;
    
            y = xev.xbutton.y;
            dy = y - s0;
    
            SL = DH-2*RW;
            if (GRY > 0)
                CL = (VR*(DH-2*RW))/GRY;
            else
                CL = SL;
            if (SL > CL)
                fy = ((float)(GRY-VR)) / (SL-CL);
            else
                fy = 1;
            dy *= fy;
    
            gettimeofday(&t,NULL);
            d = (t.tv_sec - mrt.tv_sec) * 1000000;
            d += (t.tv_usec - mrt.tv_usec);
            if ((dy != 0) && (d > 200000)) {
                Y0 += dy;
                check_dlimits(0);
                s0 = y;
                memcpy(&mrt,&t,sizeof(mrt));
                redraw_dw = 1;
                if (CDW == PAGE_FATBITS)
                    RG = 1;
            }
        }
    }
    
    /* resizing PAGE/PAGE_OUTPUT junction */
    else if (sliding == 3) {
        int x,y;
        int sb;
        int cur_dw;
    
        if (*cm_v_hide == ' ')
            sb = 10+RW;
        else
            sb = 0;
    
        x = xev.xbutton.x;
        y = xev.xbutton.y;
        if ((y > PT+TH+2*RW+20+10+RW) &&
            (y < page_j2-2*RW-20-10-RW)) {

            set_pagetab(y,page_j2,0);
            cur_dw = CDW;
            CDW = PAGE;
            if ((DT == LAST_PAGE_DT) &&
                (DM == LAST_PAGE_DM)) {
                redraw_j1 = 1;
                redraw_zone = 1;
            }
            else
                force_redraw();
            CDW = cur_dw;
        }
    }
    
    /* resizing PAGE_OUTPUT/PAGE_SYMBOL junction */
    else if (sliding == 4) {
        int x,y;
        int sb;
    
        if (*cm_v_hide == ' ')
            sb = 10+RW;
        else
            sb = 0;

        x = xev.xbutton.x;
        y = xev.xbutton.y;
        if ((y > page_j1+2*RW+20+10+RW) &&
            (y < PT+PH-2*RW-20-10-RW)) {

            set_pagetab(page_j1,y,0);
            redraw_j2 = 1;
            redraw_zone = 1;
        }
    }
}

/* (devel)

The function setview
--------------------

As each window is displayed on only one mode and each mode belongs
to only one tab, in order to set a given mode or a given tab,
just call setview informing one window present on that mode as
parameter. That is the only parameter received by setview.
The geometry of each window will be re-computed by setview, so
setview is not called only to change the current mode, but
also after operations that change the geometry of the windows,
just like resizing the application X window or hiding the
scrollbars, or resizing the PAGE window, etc.

*/
void setview(int mode)
{
    int a,i,sb;

    /* submit current form before changing mode */
    form_auto_submit();

    /* for geometric computations */
    if (*cm_v_hide == ' ')
        sb = 10 + RW;
    else
        sb = 0;

    /*
         To welcome. The window in this case contains only the
         typewriter "CLARA OCR" image. The text messages
         are displayed outside the window.
    */
    if (mode == WELCOME) {
        int w,h;

        /* current window */
        CDW = WELCOME;

        /* grid separation and width */
        GS = 1;
        GW = 0;

        /* window size in display pixels */
        DH = PH - TH - 10*(DFH+VS);
        if (DH < 0)
            DH = DFH;
        DW = PW - 40;

        /* reduction factor */
        RF = 1;
        w = wx1 - wx0 + (wx1-wx0)/4;
        h = wy1 - wy0 + (wy1-wy0)/2;
        a = w/DW + (w%DW != 0);
        if (a > RF)
            RF = a;
        a = h/DH + (h%DH != 0);
        if (a > RF)
            RF = a;

        /* resize window */
        DH = h / RF + 1;
        DW = w / RF + 1;

        /* window size in document pixels */
        HR = DW*RF;
        VR = DH*RF;

        /* top left */
        if ((X0 = wx0-(HR-wx1+wx0)/2) < 0)
            X0 = 0;
        if ((Y0 = wy0-(VR-wy1+wy0)/2) < 0)
            Y0 = 0;

        /* window margins */
        DM = PM+(PW-DW)/2;
        DT = PT+TH+(PH-TH-DH-8*(DFH+VS))/2+DFH+VS;

        /* current tab and visible windows */
        tab = -1;
        for (i=0; i<=TOP_DW; ++i)
            dw[i].v = 0;
        dw[WELCOME].v = 1;

        /* HTML mode */
        HTML = 0;

        /* restricted clip window */
        PHR = HR;
        PVR = VR;
        PDM = DM;
        PDT = DT;
    }

    /* to fat bits */
    else if (mode == PAGE_FATBITS) {

        /* current window */
        CDW = PAGE_FATBITS;
        PAGE_CM = PAGE_FATBITS;

        /* grid separation and width */
        GS = ZPS + 1;
        GW = 1;

        /* window size measured in document pixels */
        if ((*cm_b_skel == 'X') ||
            (*cm_b_border == 'X') ||
            (*cm_b_hs == 'X') ||
            (*cm_b_hb == 'X')) {

            HR = VR = FS;
        }
        else {
            VR = (PH-TH-20-sb) / GS;
            HR = (PW-20-sb) / GS;
            if (VR > 4*FS)
                VR = 4*FS;
            if (HR > 4*FS)
                HR = 4*FS;
        }

        /* window size in display pixels */
        DW = HR*GS;
        DH = VR*GS;

        /* horizontal and vertical margins */
        DM = PM+(PW-(DW+sb))/2;
        DT = PT+TH+(PH-TH-(DH+sb))/2;

        /* current tab and visible windows */
        tab = PAGE;
        for (i=0; i<=TOP_DW; ++i)
            dw[i].v = 0;
        dw[CDW].v = 1;

        /* reduction factor */
        RF = 1;

        /* document total size in document pixels */
        GRX = XRES;
        GRY = YRES;

        /* HTML mode */
        HTML = 0;
    }

    /* to matches */
    else if (mode == PAGE_MATCHES) {

        /* current window */
        CDW = PAGE_MATCHES;

        /* window size measured in document pixels */
        VR = FS;
        HR = FS;

        /* grid separation and width */
        GS = ZPS + 1;
        GW = 1;

        /* window size in display pixels */
        DW = HR*GS;
        DH = VR*GS;

        /* horizontal and vertical margins */
        DM = PM+(PW-DW)/2;
        DT = PT+TH+(PH-TH-DH)/2;

        /* current tab and visible windows */
        tab = PAGE;
        for (i=0; i<=TOP_DW; ++i)
            dw[i].v = 0;
        dw[CDW].v = 1;

        /* reduction factor */
        RF = 1;

        /* document total size in document pixels */
        GRX = FS;
        GRY = FS;

        /* HTML mode */
        HTML = 0;
    }

    /*

        Modes featuring just one big HTML window.

    */
    else if ((mode == DEBUG) ||
             (mode == GPL) ||
             (mode == PATTERN) ||
             (mode == PATTERN_LIST) ||
             (mode == PATTERN_TYPES) ||
             (mode == PATTERN_ACTION) ||
             (mode == PAGE_LIST) ||
             (mode == PAGE_DOUBTS) ||
             (mode == TUNE) ||
             (mode == TUNE_ACTS)) {

        /* current window and tab */
        CDW = mode;
        if ((mode == PATTERN) ||
            (mode==PATTERN_LIST) ||
            (mode == PATTERN_TYPES)) {

            PATTERN_CM = mode;
            tab = PATTERN;
        }
        else if (mode == PAGE_LIST) {
            PAGE_CM = mode;
            tab = PAGE;
        }
        else if ((mode == GPL) ||
                 (mode == PATTERN_ACTION) ||
                 (mode == DEBUG)) {

            tab = -1;
        }
        else if ((mode == TUNE) || (mode == TUNE_ACTS)) {
            TUNE_CM = mode;
            tab = TUNE;
        }

        /* window size, measured in document pixels */
        VR = PH-TH-20-sb;
        HR = PW-20-sb;

        /* grid separation and width */
        GS = 1;
        GW = 0;

        /* window size in display pixels */
        DW = HR;
        DH = VR;

        /* window margins */
        DM = PM+(PW-(DW+sb))/2;
        DT = PT+TH+(PH-TH-(DH+sb))/2;

        /* current tab and visible windows */
        for (i=0; i<=TOP_DW; ++i)
            dw[i].v = 0;
        dw[CDW].v = 1;

        /* reduction factor */
        RF = 1;

        /* HTML mode and source */
        HTML = 1;
        HS = ((mode != GPL) && (mode != DEBUG));

        /* has scrollbars */
        DS = 1;
    }

    /* to PAGE or PAGE_OUTPUT or PAGE_SYMBOL */
    else if ((mode == PAGE) ||
             (mode == PAGE_OUTPUT) ||
             (mode == PAGE_SYMBOL)) {

        CDW = mode;
        PAGE_CM = PAGE;
        if (*cm_o_pgo == ' ')
           set_pagetab(page_j1,page_j2,0);
        else
           set_pagetab(PT+PH-5,PT+PH-5,1);
    }

    /* To skeleton tuning */
    else if ((mode == TUNE_PATTERN) || (mode == TUNE_SKEL)) {

        /* current window */
        CDW = TUNE_SKEL;
        TUNE_CM = TUNE_SKEL;

        /* window size in display pixels */
        DW = PW - 10 - FS*(ZPS+1) - 10 - sb - 10 - 4;
        DH = PH-TH-20-sb;

        /* window size measured in document pixels */
        VR = DH;
        HR = DW;

        /* grid separation and width */
        GS = 1;
        GW = 1;

        /* horizontal and vertical margins */
        DM = PM+10+FS*(ZPS+1)+10+2;
        DT = PT+TH+(PH-TH-DH-sb)/2;

        /* reduction factor */
        RF = 1;

        /* HTML mode */
        HTML = 1;

        /* has scrollbars */
        DS = 1;

        /* current window */
        CDW = TUNE_PATTERN;

        /* window size measured in document pixels */
        VR = FS;
        HR = FS;

        /* grid separation and width */
        GS = ZPS + 1;
        GW = 1;

        /* window size in display pixels */
        DW = HR*GS;
        DH = VR*GS;

        /* horizontal and vertical margins */
        DM = PM+10; /*PM+(PW-DW)/2;*/
        DT = dw[TUNE_SKEL].dt;
        /* DT = PT+TH+(PH-TH-DH)/2; */

        /* current tab and visible windows */
        tab = TUNE;
        for (i=0; i<=TOP_DW; ++i)
            dw[i].v = 0;
        dw[TUNE_PATTERN].v = 1;
        dw[TUNE_SKEL].v = 1;

        /* reduction factor */
        RF = 1;

        /* HTML mode */
        HTML = 0;

        /* no scrollbars */
        DS = 0;

        /* mode requested */
        CDW = mode;
    }

    /* force redraw */
    if (batch_mode == 0) {
        force_redraw();
    }

    /* check_dlimits(NULL); */
}

/*

Display cb.

*/
void display_cb(void)
{
    int i,j;

    if (tab != PAGE_FATBITS)
        setview(PAGE_FATBITS);
    for (i=0; i<FS; ++i)
        for (j=0; j<FS; ++j)
            cfont[i+j*FS] = cb[i+j*LFS];
    redraw_dw = 1;
    redraw();
}

/*

Dismiss current menu.

*/
void dismiss(int f)
{
    int m,T;

    /* no current menu to dismiss */
    if (cmenu == NULL)
        return;

    /* conditional dismiss */
    if ((!f) /* && (cmenu->a == 1) */) {
        int x,y;

        /* get pointer position */
        get_pointer(&x,&y);

        /* tolerance */
        T = 5;

        /* become unconditional */
        m = mb_item(x,y);
        if ((m == -1) &&
            ((x < (CX0-T)) || ((CX0+CW+T) < x) || (y < (CY0-T)) || ((CY0+CH+T) < y)))
            f = 1;
    }

    /* unconditional dismiss */
    if (f) {
        cmenu = NULL;
        CX0_orig = CY0_orig = -1;
        force_redraw();
        set_mclip(1);
    }
}

/*

Build the list of labels available for the alphabets button. This
list is maintained in the local static variable b, and the balpha
button label is pointed to b, so the standard support for drawing
button labels is used.

*/
void set_bl_alpha(void)
{
    static char b[256];
    int n;

    b[255] = 0;
    nalpha = 0;

    strncpy(b,aname(OTHER),255);
    n = 255-strlen(b);
    BL[balpha] = b;
    inv_balpha[nalpha] = OTHER;
    ++nalpha;

    if (*cm_a_latin != ' ') {
        strncat(b,":",n);
        strncat(b,aname(LATIN),--n);
        n = 255-strlen(b);
        inv_balpha[nalpha] = LATIN;
        ++nalpha;
    }
    if (*cm_a_greek != ' ') {
        strncat(b,":",n);
        strncat(b,aname(GREEK),--n);
        n = 255-strlen(b);
        inv_balpha[nalpha] = GREEK;
        ++nalpha;
    }
    if (*cm_a_cyrillic != ' ') {
        strncat(b,":",n);
        strncat(b,aname(CYRILLIC),--n);
        n = 255-strlen(b);
        inv_balpha[nalpha] = CYRILLIC;
        ++nalpha;
    }
    if (*cm_a_hebrew != ' ') {
        strncat(b,":",n);
        strncat(b,aname(HEBREW),--n);
        n = 255-strlen(b);
        inv_balpha[nalpha] = HEBREW;
        ++nalpha;
    }
    if (*cm_a_arabic != ' ') {
        strncat(b,":",n);
        strncat(b,aname(ARABIC),--n);
        n = 255-strlen(b);
        inv_balpha[nalpha] = ARABIC;
        ++nalpha;
    }
    if (*cm_a_kana != ' ') {
        strncat(b,":",n);
        strncat(b,aname(KANA),n);
        n = 255-strlen(b);
        inv_balpha[nalpha] = KANA;
        ++nalpha;
    }
    if (*cm_a_number != ' ') {
        strncat(b,":",n);
        strncat(b,aname(NUMBER),--n);
        n = 255-strlen(b);
        inv_balpha[nalpha] = NUMBER;
        ++nalpha;
    }
    if (*cm_a_ideogram != ' ') {
        strncat(b,":",n);
        strncat(b,aname(IDEOGRAM),--n);
        n = 255-strlen(b);
        inv_balpha[nalpha] = IDEOGRAM;
        ++nalpha;
    }
    button[balpha] = (nalpha > 1) ? 1 : 0;
    set_alpha();
    redraw_button = balpha;
}

/*

Copy the region around the symbol mm of the current page to the
global buffer rw, and optionally dumps it as an asc pgm file.

*/
int wrmc8(int mm,char *s1,char *s2)
{
    int i0,j0,l,r,t,b,w,h;
    unsigned char g[200];
    int k;
    int a,rf;
    sdesc *m;
    cldesc *y;
    int cmap[5];

    memset(rw,WHITE,RWX*RWY);
    if ((mm < 0) || (tops < mm))
        return(0);

    /* symbol limits */
    m = mc + mm;
    l = m->l;
    r = m->r;
    t = m->t;
    b = m->b;
    w = r-l+1;
    h = b-t+1;

    /*
        Compute reduction factor to the symbol fit in
        the buffer confortably.
    */
    rf = 1;
    if ((a = (w / (2*RWX/3)) + ((w % (2*RWX/3)) != 0)) > rf)
        rf = a;

    if ((a = (h / (2*RWY/3)) + ((h % (2*RWY/3)) != 0)) > rf)
        rf = a;

/*
    if (mm==2284)
        ++rf;
*/

    /*

        The page region we're interested on has
        size (RWX*rf) x (RWY*rf). The sampled pixels will
        be (m->l+i*rf,m->t+j*rf), where

        i = i0, i0+1, ..., i0+RWX-1
        j = j0, j0+1, ..., j0+RWY-1

        and

        i0 = -(RWX-((w/rf)+(w%rf!=0))) / 2
        j0 = -(RWY-((h/rf)+(h%rf!=0))) / 2

        Note that (w/rf)+(w%rf!=0) is just the number of sampled
        pixels per line on mm. The number RWX-((w/rf)+(w%rf!=0))
        is the horizontal excess, so i0 is just half this excess
        (to centralize the symbol on the buffer).

    */
    i0 = -(RWX-((w/rf)+(w%rf!=0))) / 2;
    j0 = -(RWY-((h/rf)+(h%rf!=0))) / 2;

    /*

        Now we compute the absolute limits l, r, t and b of the
        region we are interested in. After that the sampled
        pixels will be (l+i*rf,t+j*rf) where

        i = 0, 1, ..., RWX-1
        j = 0, 1, ..., RWY-1

    */
    l = m->l + i0*rf;
    r = m->l + (i0+RWX-1)*rf;
    t = m->t + j0*rf;
    b = m->t + (j0+RWY-1)*rf;

    /* list the closures that intersect the rectangle (l,r,t,b) */
    list_cl(l,t,r-l+1,b-t+1,0);

    /* clip and copy each such closure */
    for (k=0; k<list_cl_sz; ++k) {
        int i,j;
        int ia,ib,ja,jb;
        int black;

        y = cl + list_cl_r[k];

        /* choose colors */
        black = GRAY;
        for (i=0; i<m->ncl; ++i) {
            if ((m->cl)[i] == list_cl_r[k]) {
                black = BLACK;
                i = m->ncl;
            }
        }
	
        /*
	
            Now we need to compute the values of i and j for which
            (l+i*rf,t+j*rf) is a point of the closure k and of
            the rectangle (l,r,t,b). The minimum and maximum such
            values will be ia, ib, ja and jb:
	
                i = ia, ia+1, ..., ib
                j = ja, ja+1, ..., jb
	
        */
        if (y->l <= l)
            ia = 0;
        else
            ia = ((y->l-l)/rf) + (((y->l-l) % rf) != 0);
        if (y->r >= r)
            ib = RWX-1;
        else
            ib = (y->r-l) / rf;
        if (y->t <= t)
            ja = 0;
        else
            ja = ((y->t-t)/rf) + (((y->t-t) % rf) != 0);
        if (y->b >= b)
            jb = RWY-1;
        else
            jb = (y->b-t) / rf;
	
        /* copy the pixels to the buffer */
        for (i=ia; i<=ib; ++i) {
            for (j=ja; j<=jb; ++j) {
                if (pixel(y,l+i*rf,t+j*rf) == BLACK)
                    rw[i+j*RWX] = black;
            }
        }
    }

    /* dump to file */
    if (s1 != NULL) {
        int F,i,j;

        /* colormap */
        cmap[WHITE] = 255;
        cmap[BLACK] = 0;
        cmap[GRAY] = 180;
        cmap[DARKGRAY] = 110;
        cmap[VDGRAY] = 40;

        F = open(s1,O_WRONLY|O_CREAT|O_TRUNC,0644);
        sprintf((char *)g,"P2\n%d %d\n255\n",RWX,RWY);
        write(F,g,strlen((char *)g));
        for (i=0; i < RWY; ++i) {
            for (j=0; j < RWX; ++j) {
                sprintf((char *)g,"%d\n",cmap[rw[j+i*RWX]]);
                write(F,g,strlen((char *)g));
            }
        }
        close(F);
    }

    return(1);
}

/*

Prepare inp_str to be displayed on the message area.

*/
void redraw_inp_str(void)
{
    strncpy(mb,inp_str,MMB);
    mb[MMB] = 0;
    strncat(mb,"_",MMB-strlen(mb));
    redraw_stline = 1;
}

#endif
void redraw_document_window() {
    UNIMPLEMENTED();
}
void redraw_flea() {
    UNIMPLEMENTED();
}
void setview(int mode) {
    UNIMPLEMENTED();
}
int wrmc8(int mm, char* s1, char* s2) {
    UNIMPLEMENTED();
    return 0;
}
void check_dlimits(int cursoron) {
    UNIMPLEMENTED();
}
void force_redraw() {
    UNIMPLEMENTED();
}
