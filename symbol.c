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

symbol.c: Symbol stuff

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include "common.h"
#include "gui.h"

/* (book)

Pixel number threshold. To bitmaps p and q match, the condition
(PNT*abs(np-nq) <= (np+nq)) must hold, where np and nq are the
number of pixels of p and q. The first pass uses PNT1, and, the
second, PNT2.

The default values for PNT1 and PNT2 may be changed through the -P
command-line switch. You want PNT2 < PNT1 to make the entire process
faster (so the first pass will recognize the "easier" characters and,
the second, the others).

*/
int MD = 6;
int PNT = 0, PNT1 = 40, PNT2 = 3;

/*

Thresholds for pixel distance computations. These thresholds are
used to rank the quality of the symbol match.

*/
int PD_T0 = 50, PD_T1 = 500, PD_T3 = 30;

/*

Number of axis to consider when testing if one pixel is tangent
to the border. Acceptable values are 1 and 3. The best choice seems
to be 1.

*/
int axis_th = 1;

/*

Classification result and diagnostic.

*/
int cp_result, cp_diag;

/*

Parameters and array for partial classification.

*/
char *P_TR = NULL;
int P_LL, P_RL, P_TR_SZ = 0;
int P_SPL[LFS];                 /* array for splitting points; see p_spl_comp() */
/*

Counter used by size_bc.

*/
int size_bc_acc;

/*

The lists of closures and symbols built by list_cl and list_s.

*/
int *list_cl_r = NULL, list_cl_sz;
int *list_s_r = NULL, list_s_sz;

/*

Minimum scores for strong and weak matches.

*/
int strong_match[CL_TOP + 1];
int weak_match[CL_TOP + 1];

/*

Argument buffer, used to pass to bmpcmp_skel an avulse bitmap
that's nor a document symbol nor a registered pattern.

*/
unsigned cmp_bmp[BMS / 4];
int cmp_bmp_w, cmp_bmp_h, cmp_bmp_dx, cmp_bmp_dy, cmp_bmp_x0, cmp_bmp_y0;

/*

Returns a symbol close to the pixel (x,y), where (x,y) are
document coordinates. Returns -1 if no symbol could be found "not
too far" from (x,y).

*/
int symbol_at(int x, int y) {
        int c, md = -1, a, b, d, e = -1;
        sdesc *m;

        list_s(x - FS, y - FS, 2 * FS, 2 * FS);

        for (c = 0; c < list_s_sz; ++c) {
                m = mc + list_s_r[c];

                /* the pointer is inside the symbol bounding box */
                if ((m->l <= x) && (x <= m->r) &&
                    (m->t <= y) && (y <= m->b)) {
                        int w, h;

                        /* return immediately if (x,y) is a black pixel in c */
                        w = (m->r - m->l + 1);
                        h = (m->b - m->t + 1);
                        if (spixel(m, x, y) == BLACK)
                                return (list_s_r[c]);

                        /* otherwise consider distance 0 for small symbols */
                        else if ((w < (XRES / 2)) || (h < (YRES / 2)))
                                d = 0;

                        /* or FS for gigantic symbols (page frames) */
                        else
                                d = FS * FS;
                }

                /* compute the (square of the) distance from (x,y) to c */
                else {
                        a = abs(x - (m->l + m->r) / 2);
                        b = abs(y - (m->t + m->b) / 2);
                        d = a * a + b * b;
                }

                /* new (square) minimum distance */
                if ((md < 0) || (d < md)) {
                        md = d;
                        e = list_s_r[c];
                }
        }

        /* the nearest */
        return (e);
}

/*

Returns a closure close to the pixel (x,y), where (x,y) are
document coordinates. Returns -1 if no closure could be found
"not too far" from (x,y).

If u is nonzero, searches a closure that contains (x,y) as
a black pixel. In most cases you'll want u==0.

*/
int closure_at(int x, int y, int u) {
        int c, md = -1, a, b, d = -1, e = -1;

        if (u) {
                list_cl(x, y, 1, 1, 0);
        } else
                list_cl(x - FS, y - FS, 2 * FS, 2 * FS, 0);

        for (c = 0; c < list_cl_sz; ++c) {

                cldesc* const m = closure_get(list_cl_r[c]);

                /* the pointer is inside the closure bounding box */
                if ((m->l <= x) && (x <= m->r) &&
                    (m->t <= y) && (y <= m->b)) {
                        int w, bpl;

                        /* return immediately if (x,y) is a black pixel in m */
                        w = (m->r - m->l + 1);
                        bpl = (w / 8) + ((w % 8) != 0);
                        if (pix(m->bm, bpl, x - m->l, y - m->t)) {
                                return (list_cl_r[c]);
                        }

                        /* otherwise consider distance 0 */
                        else
                                d = 0;
                }

                /* compute the (square of the) distance from (x,y) to c */
                else if (u == 0) {
                        a = abs(x - (m->l + m->r) / 2);
                        b = abs(y - (m->t + m->b) / 2);
                        d = a * a + b * b;
                }

                /* new (square) minimum distance */
                if ((u == 0) && ((md < 0) || (d < md))) {
                        md = d;
                        e = list_cl_r[c];
                }
        }

        /* the nearest */
        return (e);
}

/*

Computes the distance between the barycenters of symbols a and b.

Obs. comment by Berend Reitsma:

When trying to attach accents and dots to symbols it is
using bsymbol() to find the nearest symbol using bdist().

bdist() is using centers of the symbols to calculate the distance. Because
the letter j has a descent, its center is lower and in this case it
results in a longer distance from the j-dot to j-body then from j-dot to
i-body.

I changed bdist() to use top and bottom of respective symbols and that
does the trick but I still don't like it. I think we should have something
to calculate the shortest distance between the black parts of the symbols
with additional costs for horizontal distance.

*/
float bdist(int a, int b) {
        int xa, ya, xb, yb;
        float d;

        xa = (mc[a].l + mc[a].r) / 2;
        xb = (mc[b].l + mc[b].r) / 2;
        ya = (mc[a].t + mc[a].b) / 2;
        yb = (mc[b].t + mc[b].b) / 2;
        d = sqrt((xa - xb) * (xa - xb) + (ya - yb) * (ya - yb));
        return (d);
}

/*

Compute the distance between the bounding boxes of two
symbols. The distance between two symbols is defined as being 0
if they intersect, or the distance between their bounding boxes
otherwise.

If v is non-null, return on *v the value 1 if the symbols are
disjoint but they have horizontal intersection (cases 7 or 8), or
0 otherwise

Nine cases are considered. To make easier to debug this code we
depict pictures examplifying each case:

        l   r          l   r
        +---+ t      t +---+
        | p |          | q |                      l   r
  l   r +---+ b      b +---+ l   r          l   r +---+ t
t +---+                      +---+ t      t +---+ | p |
  | q |                      | p |          | q | +---+ b
b +---+                      +---+ b      b +---+

     case 1               case 2              case 3

        l   r          l   r
        +---+ t      t +---+
        | q |          | p |                      l   r
  l   r +---+ b      b +---+ l   r          l   r +---+ t
t +---+                      +---+ t      t +---+ | q |
  | p |                      | q |          | p | +---+ b
b +---+                      +---+ b      b +---+

     case 4               case 5              case 6

    l   r                 l   r
    +---+ t               +---+ t               +---+
    | q |                 | p |           	|p  |
    +---+ b               +---+ b         	|  ++--+
  l   r                 l   r             	|  ||  |
t +---+               t +---+             	+--++  |
  | p |                 | q |             	   |  q|
b +---+               b +---+             	   +---+

    case 7               case 8              case 9

*/
float box_dist(int a, int b, int *v) {
        sdesc *p, *q;

        p = mc + a;
        q = mc + b;
        if (v != NULL)
                *v = 0;

        if (p->l > q->r) {

                /* case 1 */
                if (p->b < q->t) {
                        return (sqrt
                                ((p->l - q->r) * (p->l - q->r) +
                                 (q->t - p->b) * (q->t - p->b)));
                }

                /* case 2 */
                else if (p->t > q->b) {
                        return (sqrt
                                ((p->l - q->r) * (p->l - q->r) +
                                 (p->t - q->b) * (p->t - q->b)));
                }

                /* case 3 */
                else
                        return (p->l - q->r);
        } else if (p->r < q->l) {

                /* case 4 */
                if (p->t > q->b) {
                        return (sqrt
                                ((q->l - p->r) * (q->l - p->r) +
                                 (p->t - q->b) * (p->t - q->b)));
                }

                /* case 5 */
                else if (p->b < q->t) {
                        return (sqrt
                                ((q->l - p->r) * (q->l - p->r) +
                                 (q->t - p->b) * (q->t - p->b)));
                }

                /* case 6 */
                else
                        return (q->l - p->r);
        }

        /* case 7 */
        else if (p->t > q->b) {
                if (v != NULL)
                        *v = 1;
                return (p->t - q->b);
        }

        /* case 8 */
        else if (p->b < q->t) {
                if (v != NULL)
                        *v = 1;
                return (q->t - p->b);
        }

        /* case 9 (the symbols intersect) */
        else
                return (0.0);
}

/*

Computes the intersection between two rectangles.

The intersection is computed from the projections of both
rectangles on the horizontal and vertical axis:

        ^
        |
        |       A
     at +      +-------+
        |      |       |
     bt *      |   +---|---+
        *      |   |   |   |
     ab *      +-------+   |
        |          |       |B
     bb +          +-------+
        |
        |
        +------+---*****---+---->
               al  bl  ar  br

The rectangles are defined by their left, right, top and bottom
limits.

Returns 0 if the intersection is empty or 1 otherwise. If the
intersection is non-empty, returns its coordinates on *l, *r, *t
and *b.

*/
int inter_rect(int al, int ar, int at, int ab,
               int bl, int br, int bt, int bb,
               int *l, int *r, int *t, int *b) {

        if ((intersize(al, ar, bl, br, l, r) <= 0) ||
            (intersize(at, ab, bt, bb, t, b) <= 0)) {

                return (0);
        }

        return (1);
}

/*

Decide if the distance between two symbols is larger than
t. Returns 1 if yes, 0 otherwise.

The distance between two symbols is defined as the minimum
distance between two pixels, one taken from the first symbol, and
another from the second.

*/
float s_dist(int a, int b, int t) {
        int i, j, k, l, x, y, r;
        int xl, xr, xt, xb;
        sdesc *c, *d;

        if (box_dist(a, b, NULL) > t)
                return (1);

        /*
           swap if the symbol a has more black pixels
           than the symbol b.
         */
        if (mc[a].nbp > mc[b].nbp) {
                int m;

                m = a;
                a = b;
                b = m;
        }

        /* copy the symbols to buffers cb and cb2 */
        pixel_mlist(b);
        memcpy(cb2, cb, LFS * FS);
        pixel_mlist(a);

        /* the symbols */
        c = mc + a;
        d = mc + b;

        /*
           for each pixel on the bounding box of symbol a..
         */
        for (i = 0; i < LFS; ++i) {
                for (j = 0; j < FS; ++j) {

                        /* ..if it's black.. */
                        if (cb[i + j * LFS] != BLACK)
                                continue;

                        /* ..get its coordinates */
                        x = c->l + i;
                        y = c->t + j;

                        /*
                           compute the minimal rectangle inside the
                           bounding box of symbol b that contains all
                           pixels at distance not larger than t from
                           (x,y).
                         */
                        r = inter_rect(x - t, x + t, y - t, y + t,
                                       d->l, d->r, d->t, d->b,
                                       &xl, &xr, &xt, &xb);

                        if (r == 0)
                                continue;

                        /*
                           for each black pixel on the rectangle,
                           check if its distance to (x,y) is
                           smaller than t.
                         */
                        xl -= d->l;
                        xr -= d->l;
                        xt -= d->t;
                        xb -= d->t;
                        for (k = xl; k <= xr; ++k) {
                                for (l = xt; l <= xb; ++l) {

#ifdef MEMCHECK
                                        checkidx(k + l * LFS, LFS * FS,
                                                 "s_dist");
#endif

                                        if (cb2[k + l * LFS] == BLACK) {
                                                int u, v;

                                                u = k + d->l - x;
                                                v = l + d->t - y;
                                                if ((abs(u) < t) &&
                                                    (abs(v) < t) &&
                                                    (sqrt(u * u + v * v) <
                                                     t)) {

                                                        return (0);
                                                }
                                        }
                                }
                        }
                }
        }

        /* no pair found */
        return (1);
}

/*

Test if symbols a and b are apt for geometric merging.

*/
void test_geo_merge(int a, int b) {

}

/*

Diagnose geometric merging between t and the current symbol and
inform the user about the result.

*/
void diag_geomerge(int t) {
        /* fragment is not inside the symbol bounding box */
        if (0) {
        }

        /* distance is larger than threshold */
        else if (s_dist(t, curr_mc, 4))
                show_hint(2, "too far");

        else
                show_hint(2, "that's ok!");
}

/*

Apply geometric merging heuristic to symbol c.

*/
void geo_merge(int c) {
/*
    int c,i;
    wdesc *w;

    for (i=0; i<=topw; ++i) {

        w = word + i;
        list_s(w->l - 2,w->t + 2,w->r - w->l + 3,w->b - w->t + 3);

        for (c=0; c<list_s_sz; ++c) {
            m = mc + list_s_r[c];
        }
    }
*/

}

/*

Tries to compute the vertical alignment from
the transliteration.

*/
int tr_align(char *a) {
        int i, c, n, o;

        if (a == NULL)
                return (-1);

        /* Improved: alignment for multichar transliteration (GL) */
        for (o = -1, i = 0; (c = a[i]); i++) {
                n = (('0' <= c) && (c <= '9')) ? n_align[c] : l_align[c];
                if (i == 0)
                        o = n;
                else
                        o = MIN(o / 10, n / 10) * 10 + MAX(o % 10, n % 10);
        }
        return (o);
}

/*

Get the well-defined alignment parameters for symbol c. All
coordinates are absolute (page-relative).

*/
void get_ap(int c, int *as, int *xh, int *bl, int *ds) {
        int a;

        /* alignment */
        a = mc[c].va;

        /* baseline */
        if ((a == 2) || (a == 12) || (a == 22))
                *bl = mc[c].b;
        else if (a == 23)
                *bl = mc[c].t;
        else
                *bl = -1;

        /* x_height */
        if (a == 1)
                *xh = mc[c].b;
        else if ((a == 11) || (a == 12) || (a == 13))
                *xh = mc[c].t;
        else
                *xh = -1;

        /* ascent */
        if ((a == 0) || (a == 1) || (a == 2) || (a == 3))
                *as = mc[c].t;
        else
                *as = -1;

        /* descent */
        if ((a == 3) || (a == 13) || (a == 23) || (a == 33))
                *ds = mc[c].b;
        else
                *ds = -1;
}

/* (book)

Alignment tuning
----------------

At this point, we can generate the output for all pages. The
output is already available if the classification was performed
clicking the OCR button with mouse button 1. If not, just select
the "Work on all pages" item on the "Options" menu, and click the
OCR button using the mouse button 1. The per-page output will be
saved to the files 5.html and 6.html.

Maybe the output will contain unknow symbols. Maybe the output
presents broken lines or broken words. If so, the numbers used to
perform symbol alignment must be changed. These numbers are
configured on the TUNE tab ("Magic numbers" section). They're
part of the session data, so they'll be saved to disk.

There are 7 such numbers:

    max word distance as percentage of x_height
    max symbol distance as percentage of x_height
    dot diameter measured in millimeters
    max alignment error as percentage of DD
    descent (relative to baseline) as percentage of DD
    ascent (relative to baseline) as percentage of DD
    x_height (relative to baseline) as percentage of DD
    steps required to complete the unity

In order to understand why these numbers are relevant, suppose,
for instance, that Clara OCR already knows that the "b" symbol
below is a letter "b", but does not know that the "p" symbol is a
letter "p". To decide if the "p" symbol seems to be a letter
instead of a blot, Clara OCR checks if it fits the the typical
dimensions of a letter. To do so, alignemnt hints are needed. On
the figure we can see the baseline-relative ascent (AS), descent
(DS) and x_height (XH), and the dot diameter (DD).


    XXX                          --\--
     XX                            |
     XX                            |
     XX                            |
     XX XXXXX   XX  XXXXX          |     --\--
     XXX     X   XXX     X         | AS    |  
     XX      XX  XX      XX        |       |  
     XX      XX  XX      XX        |       | XH  
     XX      XX  XX      XX        |       |  
     XX      XX  XX      XX  X     |       |     --\--
     XXX     X   XXX     X  XXX    |       |       | DD
     XX XXXXX    XX XXXXX    X   --\--   --\--   --\--
                 XX                |
                 XX                | DS
                 XX                |
                XXXX             --\--


The most relevant numbers to configure are the dot diameter, the
maximum alignment error, the descent, the ascent and the x_height.
They inform the baseline-relative ascent, descent and x_height, as
percentages of the dot diameter. The usage of these numbers is
expected to stop some day in the future, when the pattern types
implementation become more mature.

*/


/*

Computes the dot (".") diameter in pixels from the (well-defined)
values of ascent, x_height, baseline and descent, and the symbol
alignment magic numbers.

*/
int get_dd(int as, int xh, int bl, int ds) {
        int dd;

        /* from x_height and baseline */
        if ((xh >= 0) && (bl >= 0))
                dd = (100 * (bl - xh)) / m_xh;

        /* from ascent and baseline */
        else if ((as >= 0) && (bl >= 0))
                dd = (100 * (bl - as)) / m_as;

        /* from ascent and descent */
        else if ((as >= 0) && (ds >= 0))
                dd = (100 * (ds - as)) / (m_as + m_ds);

        /* from x_height and descent */
        else if ((xh >= 0) && (ds >= 0))
                dd = (100 * (ds - xh)) / (m_ds + m_xh);

        /* from baseline and descent */
        else if ((bl >= 0) && (ds >= 0))
                dd = (100 * (ds - bl)) / m_ds;

        /* from the resolution */
        else
                dd = (15 * DENSITY) / 1000;

        /* undefined */
        /*
           else
           dd = -1;
         */

        return (dd);
}

/*

Compute and copy to *as, *xh, *bl and *ds the medium alignment
values for symbols a and b, estimating those not defined by a or
b (using the symbol alignment magic numbers). Returns the dot
diameter or -1 if could not obtain all data.

Obs. comment by Berend Reitsma:

I have a text where there are (at least) two ascents instead of one. This
is causing breaking words where it should merge the symbols.

At the moment I changed complete_align() to take the highest ascent
instead of the average and that works for me.

*/
int complete_align(int a, int b, int *as, int *xh, int *bl, int *ds) {
        int dd;
        int as1, as2, xh1, xh2, bl1, bl2, ds1, ds2;

        /* alignment parameters for a and b */
        get_ap(a, &as1, &xh1, &bl1, &ds1);
        get_ap(b, &as2, &xh2, &bl2, &ds2);

        /* medium ascent */
        if (as1 < 0)
                *as = as2;
        else if (as2 < 0)
                *as = as1;
        else
                *as = (as1 + as2) / 2;

        /* medium x_height */
        if (xh1 < 0)
                *xh = xh2;
        else if (xh2 < 0)
                *xh = xh1;
        else
                *xh = (xh1 + xh2) / 2;

        /* medium baseline */
        if (bl1 < 0)
                *bl = bl2;
        else if (bl2 < 0)
                *bl = bl1;
        else
                *bl = (bl1 + bl2) / 2;

        /* medium descent */
        if (ds1 < 0)
                *ds = ds2;
        else if (ds2 < 0)
                *ds = ds1;
        else
                *ds = (ds1 + ds2) / 2;

        /* dot diameter */
        dd = get_dd(*as, *xh, *bl, *ds);
        if (dd <= 0)
                return (-1);

        /* estimates the ascent */
        if (*as < 0) {

                /* estimates from x_height */
                if (*xh >= 0)
                        *as = *xh - ((m_as - m_xh) * dd) / 100;

                /* just a try */
                else if (*bl >= 0)
                        *as = *bl - (m_as * dd) / 100;
        }

        /* estimates the x_height */
        if (*xh < 0) {

                /* interpolates from ascent */
                if (*as >= 0) {
                        if (*bl >= 0)
                                *xh = *bl - (m_xh * (*bl - *as)) / m_as;
                        else if (*ds >= 0)
                                *xh =
                                    *ds - (m_xh * (*ds - *as)) / (m_as +
                                                                  m_ds);
                }

                /* just a try */
                else if (*bl >= 0)
                        *xh = *bl - (m_xh * dd) / 100;
        }

        /* estimates the baseline */
        if (*bl < 0) {

                /* interpolates from descent */
                if (*ds >= 0) {
                        if (*xh >= 0)
                                *bl =
                                    *ds - (m_ds * (*ds - *xh)) / (m_ds +
                                                                  m_xh);
                        else if (*as >= 0)
                                *bl =
                                    *ds - (m_ds * (*ds - *as)) / (m_ds +
                                                                  m_as);
                }

                /* just a try */
                else if (*xh >= 0)
                        *bl = *xh + (m_xh * dd) / 100;
        }

        /* estimates the descent */
        if (*ds < 0) {

                /* estimates from baseline */
                if (*bl >= 0)
                        *ds = *bl + (m_ds * dd) / 100;

                /* just a try */
                else if (*xh >= 0)
                        *ds = *xh + ((m_ds + m_xh) * dd) / 100;
        }

        /* incomplete data */
        if ((*as < 0) || (*xh < 0) || (*bl < 0) || (*ds < 0))
                return (-1);

        /* complete alignment data */
        else
                return (dd);
}

/*

Computes the alignment of symbol c from the completed values
for ascent, x_height, baseline and descent. Returns the
alignment or the following diagnostics:

    -1 symbol above ascent
    -2 symbol below descent

Ideally, this service is to be used for untransliterated symbols.
The alignment for transliterated symbols is obtained from the its
transliteration (see the function tr_align).

*/
int geo_align(int c, int dd, int as, int xh, int bl, int ds) {
        int t, b;

        /* symbol c above ascent */
        if ((mc[c].t + (m_mae * dd) / 100) < as)
                return (-1);

        /* symbol c below descent */
        if ((mc[c].b - (m_mae * dd) / 100) > ds)
                return (-2);

        /* align top */
        t = mc[c].t;
        if (t < (as + xh) / 2)
                t = 0;
        else if (t < (xh + bl) / 2)
                t = 1;
        else if (t < (bl + ds) / 2)
                t = 2;
        else
                t = 3;

        /* align bottom */
        b = mc[c].b;
        if (b < (as + xh) / 2)
                b = 0;
        else if (b < (xh + bl) / 2)
                b = 1;
        else if (b < (bl + ds) / 2)
                b = 2;
        else
                b = 3;

        /* alignment */
        return (t * 10 + b);
}

/* (devel)

Symbol pairing
--------------

Pairing applies to letters and digits. We say that the symbols a and b
(in this order) are paired if the symbol b follows the symbol a within
one same word. For instance, "h" and "a" are paired on the word
"that", "3" and "4" are paired on "12345", but "o" and "b" are not
paired on "to be" (because they're not on the same word).

The function s_pair tests symbol pairing, and returns
the following diagnostics:

0 .. the symbols are paired
1 .. insuficcient vertical intersection
2 .. one or both symbols above ascent
3 .. one or both symbols below descent
4 .. maximum horizontal distance exceeded
5 .. incomplete data
6 .. different zones

If p is nonzero, then store the inferred alignment for each symbol
(a and b) on the va field of these symbols, except when these
symbols have the va field already defined.

If rd is non-null, returns the dot diameter in *rd. If an
estimative for the dot diameter cannot be computed, does not
change *rd.

*/
int s_pair(int a, int b, int p, int *rd) {
        int as, xh, bl, ds, dd;
        int a1, a2;

        int r, t, d;

        /* the symbols belong to different zones: fail */
        if (mc[a].c != mc[b].c)
                return (6);

        /* size of the vertical intersection */
        t = intersize(mc[a].t, mc[a].b, mc[b].t, mc[b].b, NULL, NULL);

        /* trivial case */
        if (t <= 0)
                return (1);

        /*
           The horizontal distance is the distance between
           the leftmost coordinate of b and the rightmost
           coordinate of a. In some cases, even when the
           symbol a is on the left side of the symbol b,
           the horizontal distance may be 0 or negative
           (e.g. on italicized texts). In these case
           we define the distance as being 1.
         */
        if (mc[a].r < mc[b].l)
                d = mc[b].l - mc[a].r;
        else if (mc[a].r + mc[a].l < mc[b].r + mc[b].l)
                d = 1;
        else
                d = 0;

        /* obtain complete alignment data or fail */
        dd = complete_align(a, b, &as, &xh, &bl, &ds);
        if (dd < 0)
                return (5);
        if (rd != NULL)
                *rd = dd;

        /* inferred alignments for a and b from the complete parameters */
        a1 = geo_align(a, dd, as, xh, bl, ds);
        a2 = geo_align(b, dd, as, xh, bl, ds);

        /* one or both symbols above ascent */
        if ((a1 == -1) || (a2 == -1))
                r = 2;

        /* one or both symbols below descent */
        else if ((a1 == -2) || (a2 == -2))
                r = 3;

        /*
           the horizontal distance must not exceed
           (baseline - x_height) * m_msd/100.
         */
        else if (100 * d > m_msd * (bl - xh))
                r = 4;

        /* pairing successfull */
        else {

                r = 0;

                /* store the inferred alignment for symbol a */
                if (p && (mc[a].va < 0))
                        mc[a].va = a1;

                /* store the inferred alignment for symbol b */
                if (p && (mc[b].va < 0))
                        mc[b].va = a2;
        }

        return (r);
}

/*

Returns one letter above c, or -1.

If relax is nonzero, do not apply the discarding rules.

TODO: use and prefer LINK revision data.

*/
int tsymb(int c, int relax) {
        sdesc *m, *t;
        int i, a, s, n;

        m = mc + c;
        s = m->t + m->b;

        /* select the symbols above c */
        {
                int l, t, w, h;

                l = (m->l + m->r - FS) / 2;
                w = FS;
                if (l + w > XRES)
                        w = XRES - l;
                h = (relax) ? 3 * FS : FS;
                t = m->t - h;
                if (t < 0)
                        t = 0;
                if (t + h > YRES)
                        h = YRES - t;
                list_s(l, t, w, h);
        }

        /* criticize them */
        for (i = n = 0; i < list_s_sz; ++i) {

                t = mc + (a = list_s_r[i]);

                /*
                   Discard the following symbols:

                   - c itself
                   - those not strictly above c
                   - non-letters
                 */
                if ((a == c) ||
                    (t->b >= m->t) || ((!relax) && (t->tc != CHAR)))

                        list_s_r[i] = -1;

                /* count remaining candidates */
                else
                        ++n;
        }

        /* no solution */
        if (n <= 0) {
                return (-1);
        }

        /* choose the closest solution */
        {
                int a1 = -1, a2, k;

                for (s = -1, i = 0; i < list_s_sz; ++i) {
                        if ((k = list_s_r[i]) >= 0) {
                                a2 = bdist(k, c);
                                if ((s < 0) || (a1 > a2)) {
                                        s = k;
                                        a1 = a2;
                                }
                        }
                }
        }
        return (s);
}


/*

Returns one letter below c, or -1.

If relax is nonzero, do not apply the discarding rules.

TODO: use and prefer LINK revision data.

*/
int bsymb(int c, int relax) {
        sdesc *m, *t;
        int i, a, s, n;

        m = mc + c;
        s = m->t + m->b;

        /* select the symbols below c */
        {
                int l, t, w, h;

                l = (m->l + m->r - FS) / 2;
                t = m->b;
                w = FS;
                if (l + w > XRES)
                        w = XRES - l;
                h = (relax) ? 3 * FS : FS;
                if (t + h > YRES)
                        h = YRES - t;
                list_s(l, t, w, h);
        }

        /* criticize them */
        for (i = n = 0; i < list_s_sz; ++i) {

                t = mc + (a = list_s_r[i]);

                /*
                   Discard the following symbols:

                   - c itself
                   - those not strictly below c
                   - non-letters
                 */
                if ((a == c) ||
                    (t->t <= m->b) || ((!relax) && (t->tc != CHAR)))

                        list_s_r[i] = -1;

                /* count remaining candidates */
                else
                        ++n;
        }

        /* no solution */
        if (n <= 0) {
                return (-1);
        }

        /* choose the closest solution */
        {
                int a1 = -1, a2, k;

                for (s = -1, i = 0; i < list_s_sz; ++i) {
                        if ((k = list_s_r[i]) >= 0) {
                                a2 = bdist(k, c);
                                if ((s < 0) || (a1 > a2)) {
                                        s = k;
                                        a1 = a2;
                                }
                        }
                }
        }
        return (s);
}

/*

Returns one symbol at the right side of c, or -1. Assumes
that c has the typical dimension of a letter.

If relax is nonzero, do not apply the discarding rules.

TODO: use and prefer LINK revision data.

*/
int rsymb(int c, int relax) {
        sdesc *m, *t;
        int i, a, s, n, dd;

        m = mc + c;
        s = m->l + m->r;

        /* gross (initial) estimative for the dot diameter */
        if ((m->r - m->l) > (m->b - m->t))
                dd = (m->r - m->l) / 4;
        else
                dd = (m->b - m->t) / 4;

        /* select the symbols at the left side of c */
        {
                int l, t, w, h;

                l = m->l;
                t = m->t;
                w = (relax) ? 10 * FS : 4 * (m->r - m->l + 1);
                if (l + w > XRES)
                        w = XRES - l;
                h = m->b - m->t + 1;
                if (t + h > YRES)
                        h = YRES - t;
                list_s(l, t, w, h);
        }

        /* criticize them */
        for (i = n = 0; i < list_s_sz; ++i) {

                t = mc + (a = list_s_r[i]);

                /*
                   Discard the following symbols:

                   - c itself
                   - those not strictly at the right side of c
                   - DOTs, COMMAs and ACCENTs
                   - those non-paired with c
                   - small untransliterated fragments
                 */
                if ((a == c) ||
                    (t->l + t->r <= s) ||
                    ((!relax) &&
                     ((t->tc == DOT) || (t->tc == COMMA) ||
                      (t->tc == ACCENT) || (s_pair(c, a, 1, &dd) != 0) ||
                      ((t->tc == UNDEF) && (t->r - t->l < dd) &&
                       (t->b - t->t < dd)))) || (relax &&
                                                 (t->r - t->l < (dd / 3))
                                                 && (t->b - t->t <
                                                     (dd / 3))))

                        list_s_r[i] = -1;

                /* count remaining candidates */
                else
                        ++n;
        }

        /* no solution */
        if (n <= 0)
                return (-1);

        {
                int a1 = -1, a2, k;

                /* choose the largest */
                /*
                   for (s=-1, i=0; i<list_s_sz; ++i) {
                   if ((k=list_s_r[i]) >= 0) {
                   a2 = (mc[k].r-mc[k].l+1)*(mc[k].b-mc[k].t+1);
                   if ((s < 0) || (a1 < a2)) {
                   s = k;
                   a1 = a2;
                   }
                   }
                   }
                 */

                /* choose the closest */
                for (s = -1, i = 0; i < list_s_sz; ++i) {
                        if (((k = list_s_r[i]) >= 0) && (relax || sdim(k))) {

                                a2 = (mc[k].r + mc[k].l) / 2 - (m->l +
                                                                m->r) / 2;
                                if ((s < 0) || (a1 > a2)) {
                                        s = k;
                                        a1 = a2;
                                }
                        }
                }
        }
        return (s);
}

/*

Returns one symbol at the left side of c, or -1. Assumes
that c has the typical dimension of a letter.

If relax is nonzero, do not apply the discarding rules.

TODO: use and prefer LINK revision data.

*/
int lsymb(int c, int relax) {
        sdesc *m, *t;
        int i, a, s, n, dd;

        m = mc + c;
        s = m->l + m->r;

        /* gross (initial) estimative for the dot diameter */
        if ((m->r - m->l) > (m->b - m->t))
                dd = (m->r - m->l) / 4;
        else
                dd = (m->b - m->t) / 4;

        /* select the symbols at the left side of c */
        {
                int l, t, w, h;

                t = m->t;
                w = (relax) ? 10 * FS : 4 * (m->r - m->l + 1);
                l = m->r - w;
                if (l < 0) {
                        w = m->r;
                        l = 0;
                }
                h = m->b - m->t + 1;
                if (t + h > YRES)
                        h = YRES - t;
                list_s(l, t, w, h);
        }

        /* criticize them */
        for (i = n = 0; i < list_s_sz; ++i) {

                t = mc + (a = list_s_r[i]);

                /*
                   Discard the following symbols:

                   - c itself
                   - those not strictly at the left side of c
                   - DOTs, COMMAs and ACCENTs
                   - those non-paired with c
                   - small untransliterated fragments
                 */
                if ((a == c) ||
                    (t->l + t->r >= s) ||
                    ((!relax) &&
                     ((t->tc == DOT) || (t->tc == COMMA) ||
                      (t->tc == ACCENT) || (s_pair(a, c, 1, &dd) != 0) ||
                      ((t->tc == UNDEF) && (t->r - t->l < dd) &&
                       (t->b - t->t < dd)))) || (relax &&
                                                 (t->r - t->l < (dd / 3))
                                                 && (t->b - t->t <
                                                     (dd / 3))))

                        list_s_r[i] = -1;

                /* count remaining candidates */
                else
                        ++n;
        }

        /* no solution */
        if (n <= 0)
                return (-1);

        {
                int a1 = -1, a2, k;

                /* choose the largest solution */
                /*
                   for (s=-1, i=0; i<list_s_sz; ++i) {
                   if (((k=list_s_r[i]) >= 0) && (relax || sdim(k))) {
                   a2 = (mc[k].r-mc[k].l+1)*(mc[k].b-mc[k].t+1);
                   if ((s < 0) || (a1 < a2)) {
                   s = k;
                   a1 = a2;
                   }
                   }
                   }
                 */

                /* choose the closest solution */
                for (s = -1, i = 0; i < list_s_sz; ++i) {
                        if (((k = list_s_r[i]) >= 0) && (relax || sdim(k))) {

                                a2 = -(mc[k].r + mc[k].l) / 2 + (m->l +
                                                                 m->r) / 2;
                                if ((s < 0) || (a1 > a2)) {
                                        s = k;
                                        a1 = a2;
                                }
                        }
                }

        }
        return (s);
}

/*

Make sure that the mc array has at least ssz entries and
creates a new symbol. The parameter l is the list
of closures and cls its size.

*/
int new_mc(int *l, int cls) {
        int a, c, i, j, d, n;

        /* make sure that the list of closures in sorted */
        qss(l, 0, cls - 1);

        /* check if there is a symbol with list of closures equal to l */
        for (i = 0, d = 1; 
	     (d != 0) && ((c = closure_get(l[0])->sup[i]) >= 0);
	     ++i) {
                if (mc[c].ncl == cls) {
                        for (j = 0; (mc[c].cl[j] == l[j]) && (l[j] >= 0); ++j)
				;
                        if (mc[c].cl[j] == l[j])
                                d = 0;
                }
        }

        /* a symbol with this list of closures already exist */
        if (d == 0)
                return (c);

        /* enlarge memory area */
        if ((n = tops + 1) >= ssz)
                mc = g_renew(sdesc, mc, (ssz += 1000));

        /* initializes mc[n] */
	cldesc* const cla = closure_get(a = l[0]);
        mc[n].l = cla->l;
        mc[n].r = cla->r;
        mc[n].t = cla->t;
        mc[n].b = cla->b;
        mc[n].nbp = cla->nbp;
        mc[n].va = -1;
        mc[n].c = -1;
        mc[n].f = 0;
        mc[n].tc = UNDEF;
        mc[n].sw = -1;
        mc[n].tr = NULL;
        mc[n].N = -1;
        mc[n].S = -1;
        mc[n].E = -1;
        mc[n].W = -1;
        mc[n].pb = 0;
        mc[n].lfa = -1;
        mc[n].bm = -1;
        mc[n].sl = -1;
        mc[n].bs = -1;

        /* alloc memory and store the cl list */
        mc[n].cl = g_memdup(l, cls * sizeof(int));
        mc[n].ncl = cls;

        /* compute the geometric limits */
        for (i = 1; i < cls; ++i) {
		cldesc *const cli = closure_get(l[i]);
                if (cli->l < mc[n].l)
                        mc[n].l = cli->l;
                if (cli->r > mc[n].r)
                        mc[n].r = cli->r;
                if (cli->t < mc[n].t)
                        mc[n].t = cli->t;
                if (cli->b > mc[n].b)
                        mc[n].b = cli->b;
                mc[n].nbp += cli->nbp;
        }

        /* include n in the lists of symbols of all closures */
        for (i = 0; i < cls; ++i) {
                int b;
                cldesc *const c = closure_get(l[i]);

                for (b = 0; c->sup[b] >= 0; ++b);

                if ((b + 1) == c->supsz) {
                        c->sup = g_renew(int, c->sup, (c->supsz += 10));
                } else if ((b + 1) > closure_get(i)->supsz) { // BUG: inconsistent with snprintf
                        snprintf(mb, MMB, "unterminated symbol list for cl=%d\n", l[i]);
                        fatal(IE, mb);
                }
                c->sup[b] = n;
                c->sup[b + 1] = -1;
        }

        /* counter of symbols */
        tops = n;
        return (n);
}

/* (devel)


Transliteration preference
--------------------------

The election process used to choose the "best" transliteration for one
symbol (from those obtained through human revision or heuristics based
on shape similarity or spelling) consists in computing the
"preference" of each transliteration and choosing the one with maximum
preference.

The transliteration preference is the integer

    UTSEAN

where

U is 1 if the transliteration was confirmed by the arbiter,
or 0 otherwise.

T is 0 if this transliteration was confirmed by no trusted
source, 1 if it was confirmed by some trusted source.

S is 0 if this transliteration was not shape-deduced
from trusted input, or 1 if it was shape-deduced
from trusted input.

E is 1 if this transliteration was deduced from spelling,
or 0 otherwise.

A is 0 if this transliteration was confirmed by no anon
source, 1 if it was confirmed by some anon source.

N is 0 if this transliteration was not shape-deduced
from anon input, or 1 if it was shape-deduced from anon input.


Transliteration class computing
-------------------------------

Once we have computed the "best" transliteration, we can compute
its transliteration class, important for various heuristics. From
the transliteration class it's possible test things like "do we
know the transliteration of this symbol?" or "is it an
alphanumeric character?"  or "concerning dimension and vertical
alignment could it be an alphanumeric character?", and others.

There are two moments where the transliteration class is
computed. The first is when a transliteration is added to
the symbol, and the second is when the CHAR class is
propagated.

The first uses the following criteria to compute the
transliteration class:

1. If the symbol has no transliteration at all, its class is
UNDEF.

2. On all other cases, the transliteration with largest
preference will be classified as DOT, COMMA, NOISE, ACCENT and
others. This search is implemented by the classify_tr function in
a straightforward way.

Just before the distribution of all symbols on words we propagate
CHARs. All CHAR symbols are searched, and for each one we look
its neighbours that seem to compose with it one same word. Such
neighbours, if untransliterated, will be classified as SCHARs.

*/

/*

Extract the fields from the transliteration preference.

*/
void extr_tp(int *U, int *T, int *S, int *E, int *A, int *N, int p) {
        *U = (p / 100000);
        *T = (p / 10000) % 10;
        *S = (p / 1000) % 10;
        *E = (p / 100) % 10;
        *A = (p / 10) % 10;
        *N = (p % 10);
}

/*

Searches the transliteration t on the repertoire of known
(and classified) transliterations. This function should be
much more complete. Some TeX macros must be supported, like
{\Alpha}.

*/
int classify_tr(char *t) {
        int c;

        if (strcmp(t, ".") == 0)
                c = DOT;
        else if ((strcmp(t, "'") == 0) ||
                 (strcmp(t, "`") == 0) ||
                 (strcmp(t, "^") == 0) || (strcmp(t, "~") == 0))
                c = ACCENT;
        else if (strcmp(t, ",") == 0)
                c = COMMA;
        else if (strcmp(t, "") == 0)
                c = NOISE;
        else
                c = CHAR;
        return (c);
}

/*

Add a transliteration to the symbol m, recompute the
preference of each transliteration and summarizes the
transliteration information.

The parameter orig identifies the origin of the vote (REVISION,
SHAPE or SPELLING). The parameter q is the quality of the
vote. The quality is an integer between 1 and 10. It tries to
reflect the level of certainty on the shape or spelling deduction
(10 is maximum level). The parameter an is the act number for
revision input.

*/
void add_tr(int m, char *rt, int orig, int an, int q, char al, int f) {
        trdesc *tr, *a;
        vdesc *v;
        char *t;
        int bm, bq, lfa;

        /* just a workaround */
        t = (rt == NULL) ? "" : rt;

        /*
           For simplicity we're avoiding the maintenance of more
           than one vote per origin for each symbol, so
           when a SHAPE vote is submitted, we must remove all
           others (if any).
         */
        if (orig == SHAPE) {
                bm = mc[m].bm;
                bq = mc[m].bq;
                lfa = mc[m].lfa;
                rmvotes(SHAPE, m, -1, 0);
        }

        /* just to avoid a compilation warning */
        else
                bm = bq = lfa = -1;

        /* check if this transliteration is new */
        for (a = mc[m].tr;
             (a != NULL) && (al == a->a) && (strcmp((a->t), t) != 0);
             a = a->nt);

        /* add the transliteration to the chain */
        if (a == NULL) {
                tr = g_new(trdesc, 1);
                tr->pr = 0;
                tr->nt = NULL;
                tr->a = al;
                tr->f = f;
                tr->v = NULL;
                tr->t = g_strdup(t);

                if (mc[m].tr == NULL)
                        mc[m].tr = tr;
                else {
                        for (a = (trdesc *) mc[m].tr;
                             a->nt != NULL;
                             a = a->nt)
                                ;
                        a->nt = (void *) tr;
                }
        } else {
                tr = a;
        }

        /* the vote is new? */
        for (v = tr->v; v != NULL; v = (vdesc *) v->nv) {
                if ((v->orig == orig) && (v->act == an)) {
                        if (v->q < q)
                                v->q = q;
                        return;
                }
        }

        /* add the vote */
        v = g_new(vdesc, 1);
        v->orig = orig;
        v->act = an;
        v->q = q;
        v->nv = (struct vdesc *) tr->v;
        tr->v = v;

        /*
           when a transliteration is submitted by an arbiter,
           we remove any arbiter vote to any other
           transliteration, so at any moment there is at most
           one transliteration with an arbiter vote.
         */
        for (a = mc[m].tr; a != NULL; a = a->nt) {

                if (a == tr) {
                } else {
                }
        }

        /* summarize transliteration information */
        summarize(m);

        /* restore best match field and shape status */
        if ((orig == SHAPE) && (bm >= 0)) {
                mc[m].bm = bm;
                mc[m].bq = bq;
                mc[m].lfa = lfa;
                C_SET(mc[m].f, F_SS);
        }

        /* to refresh the display */
        dw[PAGE_OUTPUT].rg = 1;
}

/*

Dump cb to stdout.

*/
void dump_cb(void) {
        int f, lx, ly, x, y;

        ly = f = 0;
        for (y = 0; y < FS; ++y) {
                lx = 0;
                for (x = 0; x < LFS; ++x) {
                        if (cb[x + y * LFS] == BLACK) {
                                if (f) {
                                        for (; ly < y; ++ly)
                                                putchar('\n');
                                } else
                                        ly = y;

                                while (lx++ < x)
                                        putchar(' ');
                                putchar('X');
                                f = 1;
                        }
                }
        }
        if (f)
                putchar('\n');
}

/*

Add closure pixels to cb with displacements dx and dy. Return the
number of black pixels found on the closure.

*/
int add_closure(cldesc *d, int dx, int dy) {
        int i, j, T;
        unsigned char *p, m;
        int w, h;

        T = 0;
        w = d->r - d->l + 1;
        h = d->b - d->t + 1;

        /* outside limits */
        if ((w + dx > LFS) || (h + dy > FS))
                return (0);

        /* copy line-by-line */
        for (j = h - 1; j >= 0; --j) {
                p = d->bm + byteat(w) * j;
                m = 128;
                for (i = 0; i < w;) {

                        /* copy 64 bits at a time */
                        if ((i64_opt) && (w - i >= 8)) {
                                *((unsigned long long *) (cb + i + dx +
                                                          (j +
                                                           dy) * LFS)) =
                                    p64[*p];
                                T += s64[*p];
                                i += 8;
                                ++p;
                        }

                        /* copy one bit at a time */
                        else {
                                if ((*p & m) != 0) {
#ifdef MEMCHECK
                                        checkidx(i + dx + (j + dy) * LFS,
                                                 LFS * FS, "pixel_mlist");
#endif
                                        cb[i + dx + (j + dy) * LFS] =
                                            BLACK;
                                        ++T;
                                }
                                if ((m >>= 1) == 0) {
                                        ++p;
                                        m = 128;
                                }
                                ++i;
                        }
                }
        }

        /* number of black pixels found */
        return (T);
}

/*

Compute the list of pixels for symbol k. Write the list in cb. The
byte cb[x+y*LFS] stores the color of the pixel
(mc[k].l+x,mc[k].t+y). Returns 1 when successfull.

This service will reject symbols that do not fit into the buffer
cb. In this case, -1 is returned and a warning is sent to the
standard output. When successfull, returns the total number of
black pixels.

*/
int pixel_mlist(int k) {
        sdesc *c;
        cldesc *d;
        int l, r, t, b, n;
        int dx, dy, T;

        /* symbol limits */
        c = mc + k;
        l = c->l;
        r = c->r;
        t = c->t;
        b = c->b;

        /* consist limits */
        if ((r - l + 1 > LFS) || (b - t + 1 > FS)) {
                db("warning: symbol too large at pixel_mlist");
                return (0);
        }

        /*
           Copy closures.
         */
        memset(cb, WHITE, LFS * FS);
        for (T = 0, n = 0; n < c->ncl; ++n) {

                d = closure_get(c->cl[n]);

                dx = d->l - l;
                dy = d->t - t;

                T += add_closure(d, dx, dy);
        }
        return (T);
}

/*

Copy symbol c to the buffer mcbm (the buffer
mcbm must be a matrix FSxFS of bits).

*/
void copy_mc(void *mcbm, int c) {
        int x, y, h, w;
        unsigned char m, *p, *q;

        memset(mcbm, 0, BMS);

        h = mc[c].b - mc[c].t + 1;
        w = mc[c].r - mc[c].l + 1;
        if (w > FS)
                w = FS;
        pixel_mlist(c);
        for (y = 0; y < h; ++y) {
                p = ((unsigned char *) cb) + y * LFS;
                q = ((unsigned char *) mcbm) + y * (FS / 8);
                m = 128;
                for (x = 0; x < w; ++x) {
                        if (*p == BLACK) {
                                *q |= m;
                        }
                        if ((m >>= 1) <= 0) {
                                ++q;
                                m = 128;
                        }
                        ++p;
                }
        }
}

/* (devel)

    How to add a bitmap comparison method
    -------------------------------------

    It's not hard to add a bitmap comparison method to Clara
    OCR. This may become very important when the available
    heuristics are unable to classify the symbols of some
    book, so a new heuristic must be created. In order to exemplify
    that, we'll add a naive bitmap comparison method. It'll
    just compare the number of black pixels on each bitmap,
    and consider that the bitmaps are similar when these
    numbers do not differ too much.

    Please remember that the code added or linked to Clara
    OCR must be GPL.

    In order to add the new bitmap comparison method, we need
    to write a function that compares two bitmaps returning how
    similar they are, add this function as an alternative to
    the Options menu, and call it when classifying the page symbols.
    We'll perform all these steps adding a naive comparison
    method, step by step. The more difficult one is to write
    the bitmap comparison method. This step is covered on the
    subsection "How to write a bitmap comparison function".

    Let's present the other two steps. To add the new method
    to the Options menu, we need:

    a. Declare the macro CL_NBP:

      #define CL_BM 2

    b. Include the new classifier on the tune form (function
    mk_tune):

      C = (classifier==CL_NBP) ? "CHECKED" : "";
      ..

    Now add the call to
    this new method on the classifier. This is just a matter of
    adding one more item on the function selbc:

      else if (classifier == NBP)
          r = classify(c,bmpcmp_nbp,1);

    Where bmpcmp_nbp is the function that will be discussed
    on the subsection "How to write a bitmap comparison function".

    To use the new method, recompile the sources, start
    Clara OCR and select the new method on the tune tab.
*/

/*

Copy the FSxFS bitmap b to the LFSxFS byte buffer c.

*/
void bm2byte(unsigned char *c, unsigned char *b) {
        unsigned char m, *p;
        int i, j, h;

        memset(c, WHITE, LFS * FS);
        for (j = 0; j < FS; ++j) {
                p = b + BLS * j;
                m = 128;
                h = j * LFS;
                for (i = 0; i < FS; ++i) {
                        if ((*p & m) != 0)
                                c[i + h] = BLACK;
                        if ((m >>= 1) <= 0) {
                                ++p;
                                m = 128;
                        }
                }
        }
}

/*

Apply add-hoc geometric tests to the bitmap b in order to avoid a
false positive when trying to recognize it as the character c.

The character c is by now assumed to be ISO-LATIN.

*/
int avoid_geo(int w, int h, int nbp, int c) {

        /*
           Refuse recognizing as acute, grave or circumflex accents
           those dense square symbols (dots).
         */
        if (((10 * abs(w - h)) < (w + h)) &&
            (nbp > (w * h / 2)) &&
            ((c == '\'') || (c == '`') || (c == '^'))) {

                return (1);
        }

        /* accept */
        return (0);
}

/*

Apply add-hoc contextual tests to symbol k in order to avoid a
false positive when trying to recognize it as the character c.

The character c is by now assumed to be ISO-LATIN.

*/
int avoid_context(int k, int c) {
        sdesc *m;
        int w, h;

        m = mc + k;
        w = m->r - m->l + 1;
        h = m->b - m->t + 1;

        /*
           Refuse recognizing as "I" or "!" a middle letter.
         */
        if (m->sw >= 0) {

                /* k is a middle letter */
                if ((word[m->sw].F != k) && (mc[k].E != k)) {

                        /* BUG: "I" should be accepted if all letters are capitals */
                        if ((c == 'I') || (c == '!'))
                                return (1);
                }
        }

        /* accept */
        return (0);
}

/*

Apply add-hoc geometrical and contextual tests to the symbol k in
order to avoid a false positive when trying to recognize it as
the character c.

The character c is by now assumed to be ISO-LATIN.

*/
int avoid(int k, int c) {

        if (avoid_context(k, c))
                return (1);
        return (avoid_geo
                (mc[k].r - mc[k].l + 1, mc[k].b - mc[k].t + 1, mc[k].nbp,
                 c));
}

/*

Compare bitmaps counting their black pixels. This is a naive
method included as an example easy to understand.

*/
int bmpcmp_nbp(int c, int st, int k, int mode) {

        static int nbp;
        int m;

        /* (devel)

           How to write a bitmap comparison function
           -----------------------------------------

           The bitmap comparison function required for the example we're
           presenting has the following prototype:

           int bmpcmp_nbp(int c,int st,int k,int d)

           The first parameter (c) is the symbol being compared, the
           second (st) is the current status, the third (k) is the current
           pattern and the fourth (d) will be discussed later.

         */

        /* (devel)

           Clara OCR will call bmpcmp_nbp once informing status 1
           every time a new symbol c is chosen, so bmpcmp_cp
           will be able to bufferize symbol data on static areas.
           Note that to classify each symbol, Clara will
           perform various calls to the bitmap
           comparison function, because it will check X events
           (like the STOP button), and, when visual modes are
           enabled, Clara will need to refresh the screen
           displaying the progress of the classification.

           The block of bmpcmp_nbp corresponding to status 1 will
           merely store on the static variable np the value of
           the nbp field of the symbol structure.

           nbp = mc[c].nbp;
         */
        if (st == 1) {
                nbp = mc[c].nbp;
        }

        /* (devel)

           Before trying to classify the symbol c as similar to the
           pattern k, Clara allows the bitmap comparison method to
           apply simple heuristics to filter bad candidates in order
           to save CPU cycles. This is done informing status 2. The
           bitmap comparison function is expected, in this caso, to
           return 1 if the pattern was accepted for further
           processing, or 0 if it was rejected. For simplicity, the
           bmpcmp_nbp function will return 1 in all cases.
         */
        if (st == 2) {
                return (1);
        }

        /* (devel)

           When Clara OCR wants to effectively ask if the pattern
           k matches the symbol c, it calls the bitmap comparison
           function informing status 3. The function must return
           a similarity index ranging from 0 (no similarity) to
           10 (identity). Now we must take care of the fourth
           parameter (mode). It informs if Clara is asking for a direct
           (mode == 1) comparison or indirect (mode == 0) comparison.
           This applies for asymmetric comparison methods. For
           instance, when using skeleton fitting, "direct" means
           that the pattern skeleton fits the symbol, and "indirect"
           means that the symbol skeleton fits the pattern. Clara
           will make both calls to avoid false positives.

           In our example we'll on both cases return 10 if the test
           5 * abs(nbp-m) <= (nbp+m) results true, where m is the
           number of black pixels of the pattern.

           m = pattern[k].nbp;
           if ((5*abs(nbp-)) <= (d->bp+mc[c].nbp))
           return(10);
           else
           return(0)
         */
        if (st == 3) {
                m = pattern[k].bp;
                if ((5 * abs(nbp - m)) <= (m + nbp))
                        return (10);
                else
                        return (0);
        }

        /* (devel)

           Finally, every bitmap comparison method is expected to
           produce a graphic image of the current status of the
           comparison when called with status 0. That
           image must be an FSxFS bitmap where each pixel may assume
           the color WHITE, BLACK or GRAY. This bitmap must be
           stored on the cfont array of bytes. The pixel on line i
           and column j must be put on cfont[i+j*FS]. In our case
           we'll just call the services copy_mc and bm2byte. The
           effect is to copy the symbol bitmap to the cfont array:

           unsigned char mcbm[BMS];
           [...]
           copy_mc(mcbm,c);
           bm2byte(cb,mcbm);

         */
        if (st == 0) {
                unsigned char mcbm[BMS];

                copy_mc(mcbm, c);
                bm2byte(cb, mcbm);
        }

        /* just to avoid a compilation warning */
        return (0);
}

/*

Compare bitmaps using standardized shapes.

By now this is a first (or second) experiment.

*/
int bmpcmp_shape(int c, int st, int k, int mode) {
        static int cc;
        static unsigned work[BMS / 4], aux[BMS / 4];
        static int *plx = NULL, *ply = NULL, *plz = NULL, plsz = 0, toppl =
            -1, topmap;
        static int skw, skh;
        static int *Bx = NULL, *By = NULL, topB = -1, Bsz = 0;

        /* prepare static areas */
        if (st == 1) {
                int mcw, mch;
                int x, y;
                unsigned char m, *p, *q;

                copy_mc(work, c);

                /* symbol size */
                mcw = mc[c].r - mc[c].l + 1;
                mch = mc[c].b - mc[c].t + 1;
                skel(-2, c, mcw, mch);

                skw = lc_bp - fc_bp + 1;
                skh = ll_bp - fl_bp + 1;

                /* copy the skeleton to work buffer */
                memset(work, 0, BMS);
                for (y = fl_bp; y <= ll_bp; ++y) {
                        p = ((unsigned char *) cb) + fc_bp + y * LFS;
                        q = ((unsigned char *) work) + (y -
                                                        fl_bp) * (FS / 8);
                        m = 128;
                        for (x = fc_bp; x <= lc_bp; ++x) {
                                if (*p == BLACK) {
                                        *q |= m;
                                }
                                if ((m >>= 1) <= 0) {
                                        ++q;
                                        m = 128;
                                }
                                ++p;
                        }
                }

                /* copy the skeleton to cb2 */

                return (0);
        }

        /* filter bad candidates */
        if (st == 2) {
                int i, j, l, r, s, ph, pw;
                float sx, sy;
                unsigned char m, n, *q, *b;

                /* pattern size */
                pw = pattern[k].bw;
                ph = pattern[k].bh;

                /* scale factors */
                cc = c;
                sx = ((mc[c].r - mc[c].l) + 1) / ((float) pw);
                sy = ((mc[c].b - mc[c].t) + 1) / ((float) ph);

                toppl = -1;
                topB = -1;
                topmap = -1;

                /* scale and copy the pattern to buffer aux */
                memset(aux, 0, BMS);
                for (i = 0; i < ph; ++i) {
                        int d;

                        r = (s = rint(i * sy)) * (FS / 8);
                        m = 128;
                        b = ((unsigned char *) (pattern[k].b)) +
                            i * (FS / 8);

                        for (j = d = 0; j < pw; ++j) {

                                /* (i,j) is a pattern pixel */
                                if (b[d] & m) {

                                        /* set on aux */
                                        l = rint(j * sx);
                                        n = (128 >> (l % 8));
                                        q = ((unsigned char *) aux) + r + (l / 8);
                                        *q |= n;

                                        /* remember coords */
                                        if (++toppl >= plsz) {
                                                plsz += 50;
                                                plx = g_renew(int, plx, plsz);
                                                ply = g_renew(int, ply, plsz);
                                                plz = g_renew(int, plz, plsz);
                                        }
                                        plx[toppl] = l;
                                        ply[toppl] = s;
                                }
                                if ((m >>= 1) == 0) {
                                        m = 128;
                                        ++d;
                                }
                        }
                }

                return (1);
        }

        /* compare */
        if (st == 3) {
                /* int l,lf */
                int c, f;

                /* "indirect" is currently meaningless to this classifier */
                if (mode == 0)
                        return (10);

                /*
                   compute skeleton pixels close to pattern pixels
                 */
                for (c = 0; c <= toppl; ++c) {
                        int n, i, x, y, t, r;

                        f = 0;
                        for (r = 1; r < 10; ++r) {
                                if ((c >= 0) && (0 < r) && (r < circ_sz)) {
                                        n = circ_np[r];
                                        t = circ[r];
                                        for (i = 0; i < n; ++i, ++t) {
                                                x = plx[c] + circpx[t];
                                                y = ply[c] + circpy[t];
                                                if (((0 <= x) && (x < FS) &&
                                                     (0 <= y) && (y < FS)) &&
                                                    (cb[x + y * LFS] == BLACK)) {

                                                        if (++topB >= Bsz) {
                                                                Bsz += 100;
                                                                Bx = g_renew(int, Bx, Bsz);
                                                                By = g_renew(int, By, Bsz);
                                                        }
                                                        Bx[topB] = x;
                                                        By[topB] = y;

                                                        if (f == 0) {
                                                                plz[c] = topB;
                                                                f = 1;
                                                                topmap = c;
                                                        }
                                                }
                                        }
                                }
                        }
                        if (f == 0)
                                return (0);
                }

                /* (old naive test, to be removed) */
                /*
                   lf = BMS/4;
                   for (l=0; (l<lf) && ((work[l]&aux[l])==aux[l]); ++l);

                   if (l >= lf) {
                   return(10);
                   }
                   else {
                   return(0);
                   }
                 */

                return (10);

        }

        /* photograph */
        if (st == 0) {
                int i, j;

                memset(cfont, WHITE, FS * FS);

/*
        bm2byte(cb,(char *)work);
*/
                for (i = 0; i < FS; ++i) {
                        for (j = 0; j < FS; ++j) {
                                if (cb[i + j * LFS] == BLACK)
                                        cfont[i + j * FS] = BLACK;
                        }
                }

                /* add the pattern */

                for (i = 0; i <= toppl; ++i) {
                        int x, y;

/*
            x = plx[i];
            y = ply[i];
            if ((0<=x) && (x<FS) && (0<=y) && (y<FS))
                cfont[x+y*FS] |= D_MASK;
*/
                        if (i <= topmap) {
                                x = Bx[plz[i]];
                                y = By[plz[i]];
                                if ((0 <= x) && (x < FS) && (0 <= y) &&
                                    (y < FS))
                                        cfont[x + y * FS] |= D_MASK;
                        }
                }

/*
        for (i=0; i<=topB; ++i) {
            int x,y;

            x = Bx[i];
            y = By[i];
            if ((0<=x) && (x<FS) && (0<=y) && (y<FS))
                cfont[x+y*FS] |= D_MASK;
        }
*/

                /* add some circumferences ... */
/*
        {
            int c,n,i,x,y,t,r;

            c = toppl/2;
            r = 6;
            if ((c >= 0) && (0 < r) && (r < circ_sz)) {
                n = circ_np[r];
                t = circ[r];
                for (i=0; i<n; ++i, ++t) {
                    x = plx[c] + circpx[t];
                    y = ply[c] + circpy[t];
                    if ((0<=x) && (x<FS) && (0<=y) && (y<FS)) {
                        cfont[x+y*FS] |= D_MASK;
                    }
                }
            }
        }
*/
        }

        /* just to avoid a compilation warning */
        return (0);
}

/*

Recursively account size of the current cb2 component (the
pixels on the component are those strictly larger than 3, i.e.
those pattern pixels (indirect case) at distance 4 or more from
the candidade - see the bmpcmp_pd code).

*/
void size_bc(int i, int j) {
        if ((i < 0) || (j < 0) || (i >= FS) || (j >= FS))
                return;
        if (cb2[i + j * LFS] > 3) {
                cb2[i + j * LFS] = 0;
                ++size_bc_acc;
                size_bc(i - 1, j - 1);
                size_bc(i - 1, j);
                size_bc(i - 1, j + 1);
                size_bc(i, j - 1);
                size_bc(i, j + 1);
                size_bc(i + 1, j - 1);
                size_bc(i + 1, j);
                size_bc(i + 1, j + 1);
        }
}

/*

Search cb for thin uncovered details.

This is an alternative implementation of the growing lines
heuristic found (see the function skel). This implementation is
currently unused, but it achieves good results, better than the
current skel code.

*/
int detect_thin() {
        /* results, centers and distances */
        int R[16];

        /* the steps */
        int dx1[16] =
            { 1, 1, 1, 0, 0, 0, -1, -1, -1, -1, -1, 0, 0, 0, 1, 1 };
        int dy1[16] =
            { 0, 0, -1, -1, -1, -1, -1, 0, 0, 0, 1, 1, 1, 1, 1, 0 };
        int dx2[16] =
            { 1, 1, 1, 1, 0, -1, -1, -1, -1, -1, -1, -1, 0, 1, 1, 1 };
        int dy2[16] =
            { 0, -1, -1, -1, -1, -1, -1, -1, 0, 1, 1, 1, 1, 1, 1, 1 };

        /* other */
        int i, j, g, r, s, u, v;

        /* maximum component size and number of components */
        int ms, nc;

        /* cb_border(0,0); */

        memset(cb2, WHITE, LFS * FS);

        /* test each pixel */
        for (i = 0; i < FS; ++i) {
                for (j = 0; j < FS; ++j) {

                        /* ignore non-BLACK pixels */
                        if (cb[i + j * LFS] != BLACK)
                                continue;

                        /* draw each line */
                        for (r = 0; r < 16; ++r) {

                                /* try 3 steps */
                                for (g = 1, s = 0, u = i, v = j;
                                     g && (s < 4); ++s) {

                                        /* next step */
                                        if (s & 1) {
                                                u += dx2[r];
                                                v += dy2[r];
                                        } else {
                                                u += dx1[r];
                                                v += dy1[r];
                                        }

                                        /* failure to extend */
                                        if ((u < 0) || (LFS <= u) ||
                                            (v < 0) || (FS <= v) ||
                                            ((s > 1) &&
                                             (cb[u + v * LFS] != WHITE))) {

                                                g = 0;
                                        }
                                }

                                /* result for angle r */
                                R[r] = g;
                        }

                        g = 0;
                        for (r = 0; (r < 8); ++r) {
                                int a1, a2, c;

                                if ((R[r]) && (R[r + 8])) {

                                        c = 1;

                                        /*
                                           pixels on extreme coordinates are considered
                                           tangent to the border.
                                         */
                                        if ((i == 0) || (i + 1 == FS) ||
                                            (j == 0) || (j + 1 == FS)) {
                                                a1 = 1;
                                                a2 = 0;
                                        }

                                        /* three axis method */
                                        else if (axis_th == 3) {
                                                int p, t;

                                                p = (r + 3) % 16;
                                                a1 = R[p];
                                                for (t = 0; c && (t < 2);
                                                     ++t) {
                                                        p = (p + 1) % 16;
                                                        if (a1 != R[p])
                                                                c = 0;
                                                }

                                                p = (r + 11) % 16;
                                                a2 = R[p];
                                                for (t = 0; c && (t < 2);
                                                     ++t) {
                                                        p = (p + 1) % 16;
                                                        if (a2 != R[p])
                                                                c = 0;
                                                }
                                        }

                                        /* one axis method */
                                        else if (axis_th == 1) {
                                                a1 = R[(r + 4) % 16];
                                                a2 = R[(r + 12) % 16];
                                        }

                                        /* unexpected value */
                                        else {
                                                db("unsupported value %d for axis_th", axis_th);
                                                c = a1 = a2 = 0;
                                        }

                                        if ((!c) || (a1 == a2)) {
                                                if (g == 0)
                                                        g = 1;
                                        } else if (g == 1)
                                                g = 2;
                                }
                        }

                        /* accept (i,j) as center of a thin uncovered region */
                        if (g == 1) {
                                cb2[i + j * LFS] = BLACK;
                        }
                }
        }

        /* for debugging */
        {
                for (i = 0; i < FS; ++i) {
                        for (j = 0; j < FS; ++j) {
                                if (cb2[i + j * LFS] == BLACK) {
                                        cb[i + j * LFS] |= D_MASK;
                                }
                        }
                }
        }

        nc = 0;
        ms = 0;

        for (i = 0; i < FS; ++i) {
                for (j = 0; j < FS; ++j) {
                        if (cb2[i + j * LFS] == BLACK) {
                                size_bc_acc = 0;
                                ++nc;
                                size_bc(i, j);
                                if (size_bc_acc > ms)
                                        ms = size_bc_acc;
                        }
                }
        }

        printf("ms=%d, nc=%d\n", ms, nc);

        return (ms);
}

/*

Compare bitmaps computing pixel distances.

*/
int bmpcmp_pd(int c, int st, int k, int mode) {
        /* control the comparison loop */
        static int u, lu, lv, v, du, dv;

        /* current pattern */
        pdesc *d;
        int i, j;

        /* maximum component size and number of components */
        int ms, nc;

        d = pattern + k;

        /* new symbol */
        if (st == 1) {
        }

        /* filter bad candidates */
        if (st == 2) {
                int dnbp = 0;

                d = pattern + k;

                /*
                   Check if this pattern must not be used by PD heuristic.
                 */
                if ((d->tr != NULL) && (strlen(d->tr) == 1) &&
                    (0 <= d->pt) && (d->pt <= toppt)) {
                        int p;

                        p = pt[d->pt].sc[((unsigned char *) (d->tr))[0]];
                        if ((p & (1 << (CL_PD - 1))) == 0)
                                return (0);
                }

                /* compute clearance on both axis */
                if ((mode == 1) || (mode == 0)) {
                        du = abs(mc[c].r - mc[c].l + 1 - d->bw);
                        dv = abs(mc[c].b - mc[c].t + 1 - d->bh);
                        dnbp =
                            ((PNT * abs(d->bp - mc[c].nbp)) >
                             (d->bp + mc[c].nbp));
                } else if (mode == 2) {
                        du = abs(pattern[c].bw - d->bw);
                        dv = abs(pattern[c].bh - d->bh);
                        dnbp =
                            ((PNT * abs(d->bp - pattern[c].bp)) >
                             (d->bp + pattern[c].bp));
                }

                /* clearance too big, fail */
                if ((((du > 2 * MD) || (dv > 2 * MD))) || (dnbp)) {

                        return (0);
                }

                else {

                        /* horizontal clearance cannot exceed MD */
                        if (du > MD)
                                du = MD;

                        /* prepare matches */
                        u = lu = 0;
                        v = lv = 0;
                }

                return (1);
        }

        /* compute similarity index */
        if (st == 3) {
                int D, t, f, bp;

                /*
                   direct mode: copy the pattern bitmap to cb2 and the
                   candidate bitmap to cb.
                 */
                if (mode == 1) {
                        bm2cb(d->b, 0, 0);
                        memcpy(cb2, cb, LFS * FS);
                        bp = pixel_mlist(c);
                }

                /*
                   indirect mode: copy the candidate bitmap to cb2 and the
                   pattern bitmap to cb.
                 */
                else if (mode == 0) {
                        pixel_mlist(c);
                        memcpy(cb2, cb, LFS * FS);
                        bp = bm2cb(d->b, 0, 0);
                }

                /*
                   pattern comparison mode: copy one pattern bitmap to cb2 and the
                   other to cb.
                 */
                else if (mode == 2) {
                        bm2cb(pattern[c].b, 0, 0);
                        memcpy(cb2, cb, LFS * FS);
                        bp = bm2cb(d->b, 0, 0);
                }

                /*
                   Pixels black on cb2 are scored 0, other cb
                   pixels are scored -1 and white pixels are scored -2.
                 */
                for (i = 0; i < FS; ++i) {
                        for (j = 0; j < FS; ++j) {
                                if (cb2[i + j * LFS] == BLACK)
                                        cb[i + j * LFS] = 0;
                                else if (cb[i + j * LFS] == BLACK)
                                        cb[i + j * LFS] = -1;
                                else
                                        cb[i + j * LFS] = -2;
                        }
                }

                /*
                   Computes the "distance" of each cb pixel to the
                   nearest cb2 pixel, for some definition of "distance".
                 */
                for (D = 0, t = 0, f = 1; f && (t < 127); ++t) {
                        f = 0;
                        for (i = 0; i < FS; ++i) {
                                for (j = 0; j < FS; ++j) {
                                        if (cb[i + j * LFS] == -1) {
                                                if (((i > 0) &&
                                                     (cb[(i - 1) + j * LFS]
                                                      == t)) ||
                                                    ((i + 1 < LFS) &&
                                                     (cb[(i + 1) + j * LFS]
                                                      == t)) || ((j > 0) &&
                                                                 (cb
                                                                  [i +
                                                                   (j -
                                                                    1) *
                                                                   LFS] ==
                                                                  t)) ||
                                                    ((j + 1 < LFS) &&
                                                     (cb[i + (j + 1) * LFS]
                                                      == t))) {

                                                        cb[i + j * LFS] =
                                                            t + 1;
                                                        if ((D +=
                                                             t + 1) >
                                                            PD_T1)
                                                                return (0);
                                                        f = 1;
                                                }
                                        }
                                }
                        }
                }

                /*
                   detect_thin requires the following colorization: pattern
                   pixels (indirect case) "far enough" (>4) from the candidate are
                   painted BLACK. Pixels not on the pattern are painted WHITE.
                   The others are painted GRAY.
                 */
                /*
                   for (i=0; i<FS; ++i) {
                   for (j=0; j<FS; ++j) {
                   if (cb[i+j*LFS] == -2)
                   cb[i+j*LFS] = WHITE;
                   else if (cb[i+j*LFS] < 4)
                   cb[i+j*LFS] = GRAY;
                   else
                   cb[i+j*LFS] = BLACK;
                   }
                   }
                 */

                /*
                   detect_thin tries to invalidate a match searching large
                   thin components on the pattern, but absent on the candidate
                   (indirect case). These components sometimes cannot be
                   detected just summing the distances, so an ad-hoc
                   heuristic is required (however, we're not using it by now).
                 */
                /* detect_thin(); */

                /*
                   detect_thick tries to invalidate a match searching large
                   thick components on the pattern, but absent on the candidate
                   (indirect case). These components sometimes cannot be
                   detected just summing the distances, so an ad-hoc
                   heuristic is required (however, it's not coded yet).
                 */
                /* detectthick(); */

                /*
                   This is a simple alternative to detect_thin and detect_thick.
                   Instead of checking if each component on the pattern absent on
                   the candidate (indirect case) seems to be a (large enough) thin
                   or thick component, we just compute the largest size in pixels
                   (ms) from all (nc) components.
                 */
                memcpy(cb2, cb, FS * LFS);
                nc = 0;
                ms = 0;
                for (i = 0; i < FS; ++i) {
                        for (j = 0; j < FS; ++j) {
                                if (cb2[i + j * LFS] == BLACK) {
                                        size_bc_acc = 0;
                                        ++nc;

                                        /* compute size of the black component */
                                        size_bc(i, j);
                                        if (size_bc_acc > ms) {
                                                if ((ms =
                                                     size_bc_acc) > PD_T3)
                                                        return (0);
                                        }
                                }
                        }
                }

                /* printf("ms=%d, nc=%d\n",ms,nc); */

                /* computes match score */
                if (D < PD_T0)
                        return (10);
                else
                        return (10 - (D - PD_T0) / (PD_T1 - PD_T0));
        }

        /* graphic image */
        if (st == 0) {
                int i, j;

                /* true code */
                /*
                   for (i=0; i<FS; ++i) {
                   for (j=0; j<FS; ++j) {
                   if (cb[i+j*LFS] == -2)
                   cfont[i+j*FS] = WHITE;
                   else if (cb[i+j*LFS] == 0)
                   cfont[i+j*FS] = GRAY;
                   else
                   cfont[i+j*FS] = BLACK;
                   }
                   }
                 */

                for (i = 0; i < FS; ++i) {
                        for (j = 0; j < FS; ++j) {
                                cfont[i + j * FS] = cb[i + j * LFS];
                        }
                }

        }

        /* just to avoid a compilation warning */
        return (0);
}

/*

Compare bitmaps using border mapping.

*/
int bmpcmp_map(int c, int st, int k, int mode) {

        /* auxiliar buffers */
        unsigned char mcbm[BMS];
        static int a[LFS * FS];

        int d, db, dp;
        trdesc *tr;
        int i, j, u, v, t;

        /* prepare to circulate all patterns */
        if (st == 1) {
                tr = mc[c].tr;
                copy_mc(mcbm, c);
                bm2byte(cb, mcbm);
                cb_border(0, 0);
                memcpy(a, cb, LFS * FS);
        }

        if (st == 2) {
                pdesc *d;

                d = pattern + k;
                if ((PNT * abs(d->bp - mc[c].nbp)) > (d->bp + mc[c].nbp)) {

                        return (0);
                }
                return (1);
        }

        /* visit current pattern */
        if (st == 3) {

                /* compute the pattern border */
                bm2byte(cb, pattern[k].b);
                cb_border(0, 0);

                /*

                   Compute border distance between the symbol
                   border and the pattern border.

                 */
                memcpy(cb2, a, LFS * FS);
                db = 0;
                for (i = 0; i < FS; ++i) {
                        for (j = 0; j < FS; ++j) {
                                if (cb[i + j * LFS] == GRAY) {

                                        dp = 64;
                                        for (t = 0; (t * t < dp); ++t) {
                                                int i0, i1, j0, j1;

                                                if ((i0 = i - t) < 0)
                                                        i0 = 0;
                                                if ((i1 = i + t) >= FS)
                                                        i1 = FS - 1;
                                                if ((j0 = j - t) < 0)
                                                        j0 = 0;
                                                if ((j1 = j + t) >= FS)
                                                        j1 = FS - 1;

                                                if (j >= t)
                                                        for (v = j - t, u =
                                                             i0; u <= i1;
                                                             ++u) {
                                                                if ((cb2
                                                                     [u +
                                                                      LFS *
                                                                      v] ==
                                                                     GRAY)
                                                                    &&
                                                                    ((d =
                                                                      (u -
                                                                       i) *
                                                                      (u -
                                                                       i) +
                                                                      (v -
                                                                       j) *
                                                                      (v -
                                                                       j))
                                                                     < dp))
                                                                        dp = d;
                                                        }
                                                if (j + t < FS)
                                                        for (v = j + t, u =
                                                             i0; u <= i1;
                                                             ++u) {
                                                                if ((cb2
                                                                     [u +
                                                                      LFS *
                                                                      v] ==
                                                                     GRAY)
                                                                    &&
                                                                    ((d =
                                                                      (u -
                                                                       i) *
                                                                      (u -
                                                                       i) +
                                                                      (v -
                                                                       j) *
                                                                      (v -
                                                                       j))
                                                                     < dp))
                                                                        dp = d;
                                                        }
                                                if (i >= t)
                                                        for (u = i - t, v =
                                                             j0; v <= j1;
                                                             ++v) {
                                                                if ((cb2
                                                                     [u +
                                                                      LFS *
                                                                      v] ==
                                                                     GRAY)
                                                                    &&
                                                                    ((d =
                                                                      (u -
                                                                       i) *
                                                                      (u -
                                                                       i) +
                                                                      (v -
                                                                       j) *
                                                                      (v -
                                                                       j))
                                                                     < dp))
                                                                        dp = d;
                                                        }
                                                if (i + t < FS)
                                                        for (u = i + t, v =
                                                             j0; v <= j1;
                                                             ++v) {
                                                                if ((cb2
                                                                     [u +
                                                                      LFS *
                                                                      v] ==
                                                                     GRAY)
                                                                    &&
                                                                    ((d =
                                                                      (u -
                                                                       i) *
                                                                      (u -
                                                                       i) +
                                                                      (v -
                                                                       j) *
                                                                      (v -
                                                                       j))
                                                                     < dp))
                                                                        dp = d;
                                                        }
                                        }

                                        if (dp > db)
                                                db = dp;
                                }
                        }
                }

                if (db <= 5)
                        return (10);
                else
                        return (0);
        }

        if (st == 0) {

                for (i = 0; i < FS; ++i)
                        for (j = 0; j < FS; ++j)
                                if (cb[i + LFS * j] == GRAY)
                                        cfont[i + FS * j] = BLACK;
                                else
                                        cfont[i + FS * j] = WHITE;

                for (i = 0; i < FS; ++i)
                        for (j = 0; j < FS; ++j)
                                if (cb2[i + LFS * j] == GRAY)
                                        cfont[i + FS * j] |= D_MASK;

        }

        /* just to avoid a compilation warning */
        return (0);
}

/*

Display the symbol and the skeleton we're trying to fit.

*/
void display_match(void *c, void *s, int r) {
        int i, j, m;
        unsigned char *p;

        /* copy symbol to cfont */
        for (j = 0; j < FS; ++j) {
                p = (unsigned char *) (((unsigned char *) c) +
                                       j * (FS / 8));
                m = 128;
                for (i = 0; i < FS; ++i) {
                        cfont[i + j * FS] =
                            ((*p & m) != 0) ? BLACK : WHITE;
                        if ((m >>= 1) <= 0) {
                                ++p;
                                m = 128;
                        }
                }
        }

        /* distinguish skeleton pixels */
        if (s != NULL)
                for (j = 0; j < FS; ++j) {
                        p = (unsigned char *) (((unsigned char *) s) +
                                               j * (FS / 8));
                        m = 128;
                        for (i = 0; i < FS; ++i) {
                                if ((*p & m) != 0)
                                        cfont[i + j * FS] |= D_MASK;
                                if ((m >>= 1) <= 0) {
                                        ++p;
                                        m = 128;
                                }
                        }
                }
}

/*

This is an auxiliar service to be used by bmpcmp_skel. It
computes the symbol bitmap (already copied to cb) restricted to
the partial matching parameters. So if cb contains two linked
symbols as "pe", and P_LL and P_RL are as in the figure, then
this service will clear the "p" letter, compute the width and
height of the "e" letter, left and top-justify it and copy it to
the FSxFS bitmap buffer b.

                    P_LL     P_RL
                     |        |
    +----------------+--------+-------+
    |                                 |
    |      XXXXXXX      XXXX          |
    |       XX    X   XX    XX        |
    |       XX    XXXXX      XX       |
    |       XX    XX XX  XXXXXX       |
    |       XX    XX XX               |
    |       XX    XX  XX              |
    |       XXXXXXX    XXXXXX         |
    |       XX                        |
    |       XX                        |
    |       XX                        |
    |                                 |
    +---------------------------------+

As P_LL and R_RL are absolute coordinates, the absolute leftmost
coordinate of the symbol must be informed as the parameter
l. Also, the total width and height must be informed as the
parameters w and h. The final width and height are returned on
*cw and *ch, and the final number of pixels is returned as the
value of the function. If no pixel is found on the restricted
area, 0 is returned and the contents of cb and b are undefined.

*/
int rlimits(int w, int h, int l, unsigned char *b, int *cw, int *ch) {
        int t;
        unsigned char *p;
        int i, j;
        int tm, lm, bm, rm, nbp;

        /* compute limits using cb */
        nbp = 0;
        tm = h;
        bm = -1;
        lm = w;
        rm = -1;
        t = P_RL - l;
        for (j = 0; j < h; ++j) {
                p = cb + j * LFS + (i = P_LL - l);
                for (; i < t; ++i, ++p) {
                        if (*p == BLACK) {

                                /* adjust limits as required */
                                if (i < lm)
                                        lm = i;
                                if (i > rm)
                                        rm = i;
                                if (j < tm)
                                        tm = j;
                                if (j > bm)
                                        bm = j;

                                ++nbp;
                        }
                }
        }

        /* shift cb and copy it to the bitmap buffer */
        if (nbp > 0) {
                int x, y, t;
                unsigned char m, *p, *q;

                /* shift cb and remove pixels outside the restricted area */
                *ch = bm - tm + 1;
                *cw = rm - lm + 1;

                t = tm * LFS + lm;
                for (i = 0; i < *ch; ++i) {
                        if (t > 0)
                                memmove(cb + i * LFS, cb + i * LFS + t,
                                        *cw);
                        if (*cw < LFS)
                                memset(cb + i * LFS + *cw, WHITE,
                                       LFS - *cw);
                }
                if (*ch < FS) {
                        t = FS - *ch;
                        memset(cb + (*ch) * LFS, WHITE, (FS - *ch) * LFS);
                }

                /* copy cb to the bitmap buffer */
                memset(b, 0, BMS);
                for (y = 0; y < *ch; ++y) {
                        p = ((unsigned char *) cb) + y * LFS;
                        q = ((unsigned char *) b) + y * (FS / 8);
                        m = 128;
                        for (x = 0; x <= *cw; ++x) {
                                if (*p == BLACK) {
                                        *q |= m;
                                }
                                if ((m >>= 1) <= 0) {
                                        ++q;
                                        m = 128;
                                }
                                ++p;
                        }
                }
        }

        /* return the number of pixels */
        return (nbp);
}

/*

Compare bitmaps using skeleton fitting.

Instead of calling this complex service, the developer should
prefer calling the service classify().

This service compares the "argument" with the pattern k. The
"argument" may be the symbol c, the pattern c with the pattern k,
or the bitmap cmp_bmp (cmp_bmp_w and cmp_bmp_h are assumed to
contain the bitmap width and height). This service is expected to
be called for all patterns (that is, for k=0, 1, ..., topp) in
sequence, however this is not mandatory. The caller is allowed to
call this service for any pattern k, or for any sequence of
patterns.

At least one and at most three calls to this service are required
to complete the comparison of the argument with the pattern
k. Each call informs the current comparison state using the
parameter st as follows:

    state 1 (prepare): copies the bitmaps and associated
    data to internal buffers. Expected to be called only
    once for each argument. Returns 1.

    state 2 (filter bad candidates): apply some simple
    tests to detect that the two bitmaps are quite
    different, to avoid burning CPU with comparisons that
    surely will fail. Returns 0 if the tests detected that
    the bitmaps are different, 1 otherwise. Expected to
    be called once for each pattern k.

    state 3 (compare): the bitmaps are compared. Returns
    the match quality (0 means "completely different", 10
    means "perfect match"). Expected to be called once for
    each pattern k accepted by the filter (state 2).

An additional state provides support to the GUI:

    state 0 (prepare display): copies the internal bitmap
    buffers to the display buffer, to permit the user to
    visualize the comparison, or the developer to debug the
    code. Returns 1.

On state 1, the mode informs about the argument. If mode==2, the
argument is the pattern c. If mode==3, the argument is the bitmap
cmp_bmp. Otherwise the argument is the symbol c.

On states 2 and 3, the parameter mode is interpreted as the
"direct" flag. When nonzero ("direct"), the fitting of the
skeleton of the pattern on the argument is tested. When zero
("indirect"), the fitting of the skeleton of the argument on the
pattern is tested.

*/
int bmpcmp_skel(int c, int st, int k, int mode) {
        /*
           The buffer "mcbm" contains the argument. The buffer
           "mcskel" contains its skeleton. The width and heigth of the
           bitmap on mcskel are stored on skw and skh.
         */
        static unsigned mcskel[BMS / 4], mcbm[BMS / 4];
        static int mcw, mch, skw, skh, mcbp;

        /*
           The buffers "work" and "aux" are used along the loop. The loop
           checks if the "aux" bitmap fits into the "work" bitmap. The
           buffer "aux" is always a skeleton (of the argument or of
           some pattern), and the buffer work is always a full bitmap (the
           argument or the pattern).
         */
        static unsigned work[BMS / 4], aux[BMS / 4];

        /* fit loop */
        int l, lf;

        /* control the loop when trying to fit "aux" into "work" */
        static int u, lu, lv, v, du, dv;

        /* maximum width and height */
        static int mw, mh;

        /* current pattern */
        static pdesc *d;

        /*
           prepare buffers
         */
        if (st == 1) {

                /* prepare to classify pattern c */
                if (mode == 2) {

                        /* copy the pattern c bitmap to mcbm */
                        memcpy(mcbm, pattern[c].b, BMS);
                        mcw = pattern[c].bw;
                        mch = pattern[c].bh;
                        mcbp = pattern[c].bp;

                        /* copy the pattern c skeleton to mcskel */
                        memcpy(mcskel, pattern[c].s, BMS);
                        skw = pattern[c].sw;
                        skh = pattern[c].sh;
                }

                /* prepare to classify symbol c */
                else {
                        int x, y;
                        unsigned char m, *p, *q;

                        /*
                           Copy the symbol c or the bitmap bmp_cmp both to mcbm
                           and cb.
                         */
                        if (mode == 3) {
                                memcpy(mcbm, cmp_bmp, BMS);
                                mcw = cmp_bmp_w;
                                mch = cmp_bmp_h;
                                mcbp = bm2cb((unsigned char *) mcbm, 0, 0);
                                skel(-1, -1, 0, 0);
                        } else {

                                /* symbol size */
                                mcw = mc[c].r - mc[c].l + 1;
                                mch = mc[c].b - mc[c].t + 1;

                                /* partial matching: cut the symbol */
                                if ((P_LL > mc[c].l) || (P_RL < mc[c].r)) {
                                        pixel_mlist(c);
                                        mcbp =
                                            rlimits(mcw, mch, mc[c].l,
                                                    (unsigned char *) mcbm,
                                                    &mcw, &mch);
                                        skel(-1, -1, mcw, mch);
                                }

                                else {

                                        /* copy the symbol c bitmap to buffer mcbm */
                                        copy_mc(mcbm, c);
                                        mcbp = mc[c].nbp;

                                        /*
                                           WARNING: for faster operation, the buffer cb is
                                           assumed to contain the bitmap of the argument.
                                           The call to copy_mc above already copied to cb
                                           the bitmap of symbol c, so it's not required to
                                           call pixel_mlist. Anyway, we're maintaining this
                                           warn and the pixel_mlist call for documentation
                                           purposes.
                                         */
                                        /* pixel_mlist(c); */
                                        skel(-2, c, mcw, mch);
                                }
                        }

                        /* memset(work,0,BMS); */

                        skw = lc_bp - fc_bp + 1;
                        skh = ll_bp - fl_bp + 1;

                        /* copy the skeleton to mcskel buffer */
                        memset(mcskel, 0, BMS);
                        for (y = fl_bp; y <= ll_bp; ++y) {
                                p = ((unsigned char *) cb) + fc_bp +
                                    y * LFS;
                                q = ((unsigned char *) mcskel) + (y -
                                                                  fl_bp) *
                                    (FS / 8);
                                m = 128;
                                for (x = fc_bp; x <= lc_bp; ++x) {
                                        if (*p == BLACK) {
                                                *q |= m;
                                        }
                                        if ((m >>= 1) <= 0) {
                                                ++q;
                                                m = 128;
                                        }
                                        ++p;
                                }
                        }
                }

                return (1);
        }

        /*
           Filter bad candidates using simple geometric tests.
         */
        if (st == 2) {
                int dnbp = 0;

                d = pattern + k;

                /*
                   Check if this pattern must not be used by SKEL heuristic.
                 */
                if ((d->tr != NULL) && (strlen(d->tr) == 1) &&
                    (0 <= d->pt) && (d->pt <= toppt)) {
                        int p;

                        p = pt[d->pt].sc[((unsigned char *) (d->tr))[0]];
                        if ((p & (1 << (CL_SKEL - 1))) == 0)
                                return (0);
                }

                /* apply geometrical avoidance tests */
                if ((d->tr != NULL) && (strlen(d->tr) == 1) &&
                    (avoid_geo
                     (mcw, mch, mcbp, ((unsigned char *) (d->tr))[0]))) {

                        return (0);
                }

                /* compute clearance on both axis */
                if (mode == 0) {
                        du = d->bw - skw;
                        dv = d->bh - skh;
                        dnbp =
                            ((PNT * abs(d->bp - mcbp)) > (d->bp + mcbp));
                } else {
                        du = mcw - d->sw;
                        dv = mch - d->sh;
                        dnbp =
                            ((PNT * abs(d->bp - mcbp)) > (d->bp + mcbp));
                }

                /* clearance too big, fail */
                if ((du < 0) || (dv < 0) ||
                    (((du > 2 * MD) || (dv > 2 * MD))) || (dnbp)) {

                        return (0);
                }

                /* the filter accepted the candidates */
                else {

                        /* compute maximum width and height */
                        if (d->bw > (mw = mcw))
                                mw = d->bw;
                        if (d->bh > (mh = mch))
                                mh = d->bh;

                        /* horizontal clearance cannot exceed MD */
                        if (du > MD)
                                du = MD;

                        /* prepare matches */
                        u = lu = 0;
                        v = lv = 0;
                        if (mode == 0)
                                memcpy(work, d->b, BMS);
                        else
                                memcpy(work, mcbm, BMS);
                        if (shift_opt)
                                memset(aux, 0, BMS);
                }

                return (1);
        }

        /*
           compare
         */
        while (st == 3) {

                /*
                   The aux buffer need not to be rebuilt, so
                   just shift it.

                   remark: On x86 CPUs we cannot shift Clara
                   bitmaps using 32-bit variables.
                 */
                if ((shift_opt != 0) && (lv == v) && (u > 0)) {
                        int i, j, l;
                        unsigned a;

                        /*
                           shift only the lines and columns filled by the skeleton.
                         */
                        if (shape_opt) {
                                int dx, h, w, m;

                                /* skeleton width and heigth plus displacements */
                                if (mode == 0) {
                                        w = (u + skw) / 8 + (u + skw) % 8 +
                                            1;
                                        h = v + skh;
                                } else {
                                        w = (u + d->sw) / 8 + (u +
                                                               d->sw) % 8 +
                                            1;
                                        h = v + d->sh;
                                }

                                /* avoid surpassing the memory limits */
                                if (w > (i = FS / 8 - 1))
                                        w = i;
                                if (h > FS)
                                        h = FS;

                                /* mask and delta to transport bits */
                                m = (1 << (u - lu)) - 1;
                                dx = 8 - (u - lu);

                                /* 8-bit shift */
                                for (j = v; j < h; ++j) {
                                        l = j * FS / 8;
                                        for (i = w; i >= 0; --i) {
                                                *(((unsigned char *) aux) +
                                                  l + i) >>= (u - lu);
                                                if (i > 0) {
                                                        a = (m &
                                                             *(((unsigned
                                                                 char *)
                                                                aux) + l +
                                                               i -
                                                               1)) << dx;
                                                        *(((unsigned char
                                                            *) aux) + l +
                                                          i) |= a;
                                                }
                                        }
                                }
                        }

                        /* shift all lines and columns */
                        else {

                                /* 8-bit shift */
                                for (j = 0; j < FS; ++j) {
                                        l = j * FS / 8;
                                        for (i = FS / 8 - 1; i >= 0; --i) {
                                                *(((unsigned char *) aux) +
                                                  l + i) >>= (u - lu);
                                                if (i > 0) {
                                                        a = (1 << (u - lu))
                                                            - 1;
                                                        a &= *(((unsigned
                                                                 char *)
                                                                aux) + l +
                                                               i - 1);
                                                        a <<= 8 - (u - lu);
                                                        *(((unsigned char
                                                            *) aux) + l +
                                                          i) |= a;
                                                }
                                        }
                                }
                        }
                }

                /*
                   When shift optmization is on, we know that (u==0),
                   so we just shift vertically.
                 */
                else if (shift_opt) {

                        memset(aux, 0, v * BLS);

                        if (mode == 0)
                                memcpy(((unsigned char *) aux) + v * BLS,
                                       mcskel, skh * BLS);
                        else
                                memcpy(((unsigned char *) aux) + v * BLS,
                                       d->s, d->sh * BLS);
                }

                /*
                   When shift optimization is off, we need to shift the
                   skeleton bit a bit.
                 */
                else {
                        int i, j, rj;
                        unsigned char m, n, *p, *q, *a;

                        if (mode == 0)
                                a = ((unsigned char *) mcskel);
                        else
                                a = ((unsigned char *) d->s);

                        memset(aux, 0, BMS);
                        for (j = 0; j < d->bh; ++j) {
                                p = a + BLS * j;
                                rj = j + v;
                                if ((0 <= rj) && (rj < FS)) {
                                        q = (unsigned char *) (aux +
                                                               rj * (FS /
                                                                     32));
                                        m = 128;
                                        n = 128 >> u;
                                        for (i = u; i < FS; ++i) {
                                                if ((*p & m) != 0)
                                                        *q |= n;
                                                if ((m >>= 1) <= 0) {
                                                        ++p;
                                                        m = 128;
                                                }
                                                if ((n >>= 1) <= 0) {
                                                        ++q;
                                                        n = 128;
                                                }
                                        }
                                }
                        }
                }

                /* remember u and v (required for shift optimization) */
                lv = v;
                lu = u;

                /* compare */
                if (shape_opt != 0) {

                        {
                                l = v * FS / 32;
                                lf = l +
                                    ((mode == 0) ? skh : d->bh) * FS / 32;
                                if (lf > BMS / 4)
                                        lf = BMS / 4;
                                if ((u + ((mode == 0) ? skw : d->bw)) <=
                                    32)
                                        for (;
                                             (l < lf) &&
                                             ((work[l] & aux[l]) ==
                                              aux[l]); l += FS / 32);
                                else
                                        for (;
                                             (l < lf) &&
                                             ((work[l] & aux[l]) ==
                                              aux[l]); ++l);
                        }

                        /*
                           Tests using relaxed comparison: bad results.
                         */
                        /*
                           {
                           int eq,i,n;
                           unsigned p;

                           for (eq=1; eq && (l<lf) ; ++l) {
                           if ((p = work[l] & aux[l]) != aux[l]) {
                           p ^= aux[l];
                           for (i=n=0; i<32; ++i)
                           n += p & 1;
                           if (n > 3)
                           eq = 0;
                           }
                           }
                         */
                } else {
                        lf = BMS / 4;
                        for (l = 0;
                             (l < lf) && ((work[l] & aux[l]) == aux[l]);
                             ++l);
                }

                /* (debugging code for severe classification errors) */
                /*
                   printf("work buffer %sdirect symbol %d pattern %d:\n",(mode ? "" : "in"),c,k);
                   dump_bm((unsigned char *)work);
                   printf("aux buffer, u=%d v=%d:\n",u,v);
                   dump_bm((unsigned char *)aux);
                   printf("result = %s\n",((l>=lf) ? "yes" : "no"));
                 */

                /*
                 ** MATCH **
                 */
                if (l >= lf) {

                        /* quality add */
                        /* return((PNT==PNT1) ? 10 : 1); */

                        return (10);
                }

                /* prepare next try with jump optimization */
                else if (jump_opt) {

                        /*
                           Find next viable (u,v) for aux[l] analysing
                           aux[l] and work[l] (we compute how
                           many shifts are required to aux[l] fit into
                           work[l]).
                         */
                        {
                                unsigned m;
                                unsigned char *p;
                                int i;

                                m = aux[l];
                                p = (unsigned char *) &m;
                                do {
                                        for (i = 3; i >= 0; --i) {
                                                p[i] >>= 1;
                                                if ((i > 0) &&
                                                    (p[i - 1] & 1))
                                                        p[i] |= 128;
                                        }
                                } while ((++u <= du) &&
                                         ((m & work[l]) != m));
                                if (u > du) {
                                        ++v;
                                        u = 0;
                                }
                                if (v > dv) {
                                        return (0);
                                }
                        }
                }

                /*
                   Prepare next step without jump optimization, that
                   is, the next (u,v) to try will be (u+1,v) or
                   (0,v+1).
                 */
                else {
                        if (++u > du) {
                                if (++v > dv)
                                        return (0);
                                else
                                        u = 0;
                        }
                }
        }

        /*
           prepare display
         */
        if (st == 0) {
                display_match(work, aux, 1);
                return (1);
        }

        /* just to avoid a compilation warning */
        return (0);
}

/*

find splitting points in mc[c] for partial matching
output: P_SPL[]=0 for splitting points, <>0 for others

*/

void p_spl_comp(c) {

        unsigned char *p;
        int i, j, h, w, d;
        float s;

        /* fprintf(stderr,"p_spl_comp(%d)\n",c); *//*debug */
        w = mc[c].r - mc[c].l + 1;
        h = mc[c].b - mc[c].t + 1;

        pixel_mlist(c);

        /* write in P_SPL the scores for splitting points of cb:
           P_SPL[i] = num of white pixels in n-th column of cb */

        memset(P_SPL, 0, sizeof(P_SPL));

        for (j = 0; j < h; ++j) {
                p = cb + j * LFS;
                for (i = 0; i < w; ++i, ++p) {
                        if (*p == BLACK) {
                                P_SPL[i]++;
                        }
                }
        }

        d = DD * DENSITY / 25.4;        /* dot dimension in pixel */

        /*if (c==5) for (i=0; i<w; i++) printf("%d %d\n",i,P_SPL[i]); *//*debug */

        for (i = 0, s = 0; i < w; i++) {
                s += P_SPL[i];
        }
        s /= w;
        s = MIN(s, 3 * d / 2);  /* threshold for splitting */

        /* splitting points must have minimal score over radius = 1dot
           and must have score<threshold */
        for (i = 0; i < w - 1; i++)
                for (j = MAX(i - d, 0); j <= MIN(i + d, w - 1); j++)
                        if ((P_SPL[i] > s) || (abs(P_SPL[j]) < P_SPL[i])) {
                                P_SPL[i] = -P_SPL[i];
                                j = w;
                        }
        i = 0;
        j = -1;
        P_SPL[w - 1] = 0;
        /* sequences of splitting points -> one central point */
        while (i > j) {
                while (P_SPL[i] < 0)
                        i++;
                j = i;
                while (P_SPL[i] > 0)
                        i++;
                if (i > j) {
                        j = (j + i - 1) / 2;
                        P_SPL[j] = 0;
                }
        }

        P_SPL[0] = 0;

        /* for (i=1; i<w-1; i++) if (!P_SPL[i]) fprintf(stderr,"%d ~ ",i); fprintf(stderr,"\n"); *//*debug */
}

/*

Classify the argument, where the argument may be the symbol c,
the pattern c or the bitmap cmp_bmp.

This is a meta-service for bitmap classification. It will call
the chosen classification strategy (skeleton fitting, border
mapping, etc).

To classify the symbol c, inform mode 1. To classify the pattern
c, inform mode 2. To classify the bitmap bmp_cmp, inform mode 3.

This service must be called once for initializetion and many
times until completion. Example:

    classify(-1,NULL,0);
    while (classify(5,bmpcmp_skel,1));

On mode 3, a non-negative integer must be passed as first
argument on the after-initialization calls. Example:

    classify(-1,NULL,0);
    while (classify(0,bmpcmp_skel,3));

The classification result is stored on the variable cp_result. At
finish, a diagnostic is stored on cp_diag as follows:

    1 .. symbol is too small
    2 .. no patterns at all
    3 .. symbol is already classified
    4 .. symbol is too large
    5 .. no more patterns to try
    6 .. classification finished

*/
int classify(int c, int bmpcmp(int, int, int, int), int mode) {
        /* current state */
        static int st;

        /* block message */
        char stb[MMB + 1];
        int s;

        /* current pattern is k */
        static int k, topk;

        /* current and best match */
        static int bq, ts = 0, quality = 0;
        static char result[MFTL + 1];
        static int best = -1, ba = 0, bact, bal, bf;

        /*
           Operation mode and associated data. "direct" mode means
           that we're tryint to fit the skeleton of pattern[d] into
           the symbol c. "inverted" mode (when direct is 0) means
           that we're trying to fit the skeleton of the symbol c
           into pattern[k].
         */
        static int direct = 1;

        /* flags for display */
        int wait_key = (get_flag(FL_SHOW_MATCHES_AND_WAIT) ||
                        get_flag(FL_SHOW_COMPARISONS_AND_WAIT));
        int display_cmp = (get_flag(FL_SHOW_COMPARISONS) ||
                           get_flag(FL_SHOW_COMPARISONS_AND_WAIT));
        int display_mat = (get_flag(FL_SHOW_MATCHES) ||
                           get_flag(FL_SHOW_MATCHES_AND_WAIT));

        /* reset state */
        if (c < 0) {

                cp_result = -1;
                st = 9;
                direct = 1;
                return (1);
        }

        /* filters */
        if (st == 9) {

                /* ignore symbols too small */
                if ((mode == 1) && (!sdim(c))) {
                        st = 0;
                        cp_diag = 1;
                        return (0);
                }

                /* there are no patterns at all: abandon */
                if (topp < 0) {
                        if (mode == 0)
                                C_UNSET(mc[c].f, F_SS);
                        st = 0;
                        cp_diag = 2;
                        return (0);
                }

                /* ignore already classified symbols */
                if ((mode == 1) && (!justone) && (C_ISSET(mc[c].f, F_SS))) {
                        st = 0;
                        cp_diag = 3;
                        return (0);
                }

                /* ignore symbols too large */
                if ((mode == 1) &&
                    ((mc[c].r - mc[c].l + 1 > LFS) ||
                     (mc[c].b - mc[c].t + 1 > FS))) {

                        st = 0;
                        snprintf(mba, MMB, "%d/%d too large", c + 1,
                                 tops + 1);
                        show_hint(0, mba);
                        cp_diag = 4;
                        return (0);
                }

                /* initialize partial matching parameters and array */
                if (mode == 1) {
                        P_LL = mc[c].l;
                        P_RL = mc[c].r;
                        p_spl_comp(c);
                }
                if (P_TR == NULL) {
                        P_TR = g_malloc(64);
                }
                P_TR[0] = 0;
                st = 1;
        }

        /* prepare OCR (classification) of the argument */
        if (st == 1) {

                /* prepare the bmpcmp internal buffers */
                bmpcmp(c, st, 0, mode);

                /*
                   Prepare to scan patterns from k to topk. Both k and
                   topk are carefully chosen depending on the context.
                 */
                if (classifier == CL_SHAPE) {
                        k = 0;
                        topk = topp;
                } else if (mode == 3) {
                        k = 0;
                        topk = topp;
                } else if (mode == 2) {
                        k = topk = this_pattern;
                } else if (this_pattern >= 0) {
                        topk = k = id2idx(this_pattern);
                } else if ((justone) || get_flag(FL_RESCAN)) {
                        k = 0;
                        topk = topp;
                } else {
                        k = id2idx(mc[c].lfa) + 1;
                        topk = topp;
                }

                /* there are not patterns to try */
                if (k > topk) {
                        st = 0;
                        cp_diag = 5;
                        return (0);
                }

                /* initialize the best current match and its quality */
                if (mode > 1) {
                        best = -1;
                        bq = -1;
                } else {
                        if (justone || recomp_cl)
                                best = -1;
                        else
                                best = id2idx(mc[c].bm);
                        if (best >= 0)
                                bq = mc[c].bq;
                        else
                                bq = -1;
                }

                /* quality of the current match */
                quality = 0;

                /* next state */
                st = 2;
        }

        /*
           Filter bad candidates.
         */
        while (st == 2) {
                int good = 1;
                pdesc *d;

                /* the display prefix */
                s = snprintf(stb, MMB, "%d/%d ", c, k);
                if (PNT == PNT1) {
                        if (direct)
                                s += snprintf(stb + s, MMB - s,
                                              "strict direct ");
                        else
                                s += snprintf(stb + s, MMB - s,
                                              "strict indirect ");
                } else {
                        if (direct)
                                s += snprintf(stb + s, MMB - s,
                                              "relaxed direct ");
                        else
                                s += snprintf(stb + s, MMB - s,
                                              "relaxed indirect ");
                }
                stb[MMB] = 0;

                /* avoid comparing the pattern k with itself */
                if ((mode == 2) && (k == c))
                        good = 0;

                /* try the add-hoc context avoidance tests */
                else if (mode == 1) {
                        d = pattern + k;
                        if ((d->tr != NULL) && (strlen(d->tr) == 1) &&
                            (avoid_context
                             (c, ((unsigned char *) (d->tr))[0]))) {

                                good = 0;
                        }
                }

                if (good && (bmpcmp(c, st, k, direct)))
                        st = 3;
                else if (++k > topk)
                        st = 4;
                else {
                        direct = 1;
                        quality = 0;
                }

        }

        /*
           Test the argument against the current pattern (k)
         */
        if (st == 3) {
                int r;
                pdesc *d;

                r = bmpcmp(c, st, k, direct);
                d = pattern + k;

                if (r) {

                        /* display the match (if requested) */
                        if (display_mat || display_cmp) {

                                /* Enter MATCHES window if required */
                                if (CDW != PAGE_MATCHES) {
                                        // UNPATCHED: setview(PAGE_MATCHES);
                                        // UNPATCHED: redraw_grid = 1;
                                }

                                /* copy the work buffers to the display buffer */
                                bmpcmp(0, 0, 0, 0);
                                redraw_document_window();

                                /* must wait */
                                if (wait_key) {
                                        if (d->tr != NULL)
                                                snprintf(stb + s, MMB - s,
                                                         "found \"%s\" shape",
                                                         d->tr);
                                        else
                                                snprintf(stb + s, MMB - s,
                                                         "found untransliterated shape");
                                        enter_wait(stb, 0, 2);
                                        waiting_key = 1;
                                }
                        }

                        /*
                           Check if this match is the best got until now.

                           The current match will be considered the best
                           match if (tests performed on this order):

                           it is the first discovered match, or
                           it's quality is the largest one, or
                           the pattern area is the largest one (direct only), or
                           the pattern is transliterated but the current best isn't
                         */
                        if (quality == 0)
                                quality = r;
                        else
                                quality *= r;

                        if ((best == -1) || (quality > bq)      /* ||
                                                                   ((direct!=0) && ((d->bh * d->bw) > ba)) ||
                                                                   ((pattern[best].t == NULL) && (d->t != NULL)) */
                            ) {

                                best = k;
                                ba = d->bh * d->bw;
                                bact = d->act;
                                bq = quality;
                                bal = d->a;
                                bf = d->f;

                                /* count and skip remaining patterns if strong match */
                                if (bq >= strong_match[classifier]) {
                                        ++d->cm;
                                        k = topk;
                                        font_changed = 1;
                                }
                        }
                }

                else {

                        /* display the failure (if requested) */
                        if (display_cmp) {

                                /* Enter MATCHES window if required */
                                if (CDW != PAGE_MATCHES) {
                                        // UNPATCHED: setview(PAGE_MATCHES);
                                        // UNPATCHED: redraw_grid = 1;
                                }

                                /* copy the work buffers to the display buffer */
                                bmpcmp(0, 0, 0, 0);
                                redraw_document_window();

                                s = snprintf(stb, MMB, "%d/%d ", c, k);
                                s += snprintf(stb + s, MMB - s, "failed");
                                if (wait_key) {
                                        enter_wait(stb, 0, 2);
                                        waiting_key = 1;
                                } else
                                        show_hint(0, stb);
                        }
                }

                /* try to invert */
                if (r && direct) {
                        direct = 0;
                        st = 2;
                }

                /* try next pattern */
                else {
                        if (++k > topk)
                                st = 4;
                        else {
                                direct = 1;
                                st = 2;
                                quality = 0;
                        }
                }

                /*
                   In visual modes, the contents of cfont must be displayed
                   and the queue of events must be checked.
                 */
                if (display_cmp || display_mat)
                        return (1);
        }

        /*
           Try PNT2 or jump to 7.
         */
        if (st == 4) {

                /* try PNT2 */
                if ((bq < strong_match[classifier]) && (PNT == PNT1) &&
                    (PNT2 < PNT1)) {
                        PNT = PNT2;
                        st = 1;
                } else
                        st = 7;
        }

        /*
           At this point, the classification of the argument
           finished (with ot without success), and the output for that
           symbol will be generated.
         */
        if (st == 7) {
                pdesc *r;

                /* success */
                if (bq >= weak_match[classifier]) {

                        /* transliteration of the best match */
                        r = pattern + best;
                        ts = (r->tr != NULL);
                        strcpy(result, ts ? r->tr : "");

                        /* other */
                        snprintf(mba, MMB,
                                 "%d/%d transliteration is \"%s\"", c + 1,
                                 tops + 1, result);
                        show_hint(0, mba);
                        cp_result = pattern[best].id;
                }

                /* fail */
                else {

                        /* account DUBIOUS shape detections */
                        /*
                           if ((bq != 0) &&
                           ((mc[c].ss == UNDEF) || (mc[c].ss == FAIL))) {
                           mc[c].ss = DUBIOUS;
                           mc[c].bm = pattern[best].id;
                           }
                         */

                        snprintf(mba, MMB, "%d/%d", c + 1, tops + 1);
                        show_hint(0, mba);
                        if (mode == 1)
                                C_UNSET(mc[c].f, F_SS);

                        cp_result = -1;
                }

                /* finished */
                st = 8;
        }

        /*

           Partial classification handler.

           At this point we try to combine various patterns to match
           one symbol. This is intended to recognize horizontal merged
           letters.

           BUG: hardcoded pixel distances (constants 3, 10 and 15 below).
         */
        if (st == 8) {

                /* classification failed: try another width */
                if (cp_result < 0) {

                        /*
                           No other width to try: fail

                           Remark: the tests involving width and height are
                           to avoid recognizing dashes as a sequence
                           of dots.
                         */

                        if (p_match)
                                for (P_RL = MIN(P_RL - 1, P_LL + FS - 1);
                                     (P_SPL[P_RL - mc[c].l] != 0) &&
                                     P_RL > P_LL; P_RL--);

                        if ((mode != 1) ||
                            (classifier != CL_SKEL) ||
                            ((mc[c].r - mc[c].l) <= ((15 * DENSITY) / 600))
                            || ((mc[c].b - mc[c].t) <=
                                ((15 * DENSITY) / 600)) || (!p_match) ||
                            (P_RL < (P_LL + 10))) {

                                /* remember the ID of the last pattern analysed */
                                if ((mode == 1) && (this_pattern < 0))
                                        mc[c].lfa = pattern[topk].id;

                                st = 0;
                                cp_diag = 6;
                                return (0);
                        }

                        /* try P_LL..P_RL */
                        else {
                                st = 1;
                        }
                }

                /* classification succeeded: iterate or finish */
                else {
                        int t;

                        /* concatenate transliteration */
                        t = strlen(P_TR) + strlen(result) + 1;
                        if (P_TR_SZ < t) {
                                P_TR_SZ = t + 64;
                                P_TR = g_realloc(P_TR, t);
                        }
                        strcat(P_TR, result);

                        /* try P_LL..P_RL */
                        if ((mode == 1) && (P_RL < mc[c].r)) {
                                P_LL = P_RL + 1;
                                for (P_RL = MIN(mc[c].r, P_LL + FS - 1);
                                     (P_SPL[P_RL - mc[c].l] != 0) &&
                                     P_RL > P_LL; P_RL--);
                                st = 1;
                        }

                        /* finish with success */
                        else {

                                /* remember the ID of the last pattern analysed */
                                if ((mode == 1) && (this_pattern < 0)) {
                                        mc[c].lfa = pattern[topk].id;
                                }

                                /*
                                   The symbol was completely recognized:
                                   define the best match and best quality fields
                                   BUG: using data for last component.
                                 */
                                if (mode == 1) {
                                        mc[c].bm = pattern[best].id;
                                        mc[c].bq = bq / 10;
                                        C_SET(mc[c].f, F_SS);
                                }

                                /*
                                   Add transliteration to the current symbol
                                   BUG: using last act.
                                 */
                                if ((mode == 1) && (strlen(P_TR) > 0))
                                        add_tr(c, P_TR, SHAPE, bact,
                                               bq / 10, bal, bf);

                                /* finish */
                                st = 0;
                                cp_diag = 6;
                                return (0);
                        }
                }

                /* reset control variables */
                direct = 1;
                ts = 0;
                quality = bq = 0;
                best = -1;
                PNT = PNT1;
        }

        /* did not complete */
        return (1);
}

/*

Compute current failure.

This function is a first try to locate pairs of symbols
whose match is falsely negative.

This code is here just to guide the future development
of a new implementation.

*/
#ifdef OLD_FAIL
int comp_fail(int new_a) {
        static int a = 0, b = 1, sm;
        static int st = 1;
        char *ta, *tb;
        int fp;

        if (new_a >= 0) {
                a = new_a;
                b = a + 1;
                st = 0;
        }

        /* make sure that a and b are consistent */
        if (a < 0)
                a = 0;
        if (b < a)
                b = a + 1;

        /* continue the last comparison */
        if (st > 1)
                fp = 1;

        /* locate new pair */
        else {

                /*
                   Look for the next pair (a,b) of symbols with
                   the same transliteration but in different classes.
                 */
                for (fp = 0; (fp == 0) && (a <= tops) && (b <= tops);) {

                        /* transliteration of a */
                        ta = (mc[a].tr == NULL) ? NULL : ((mc[a].tr)->t);

                        if ((ta != NULL) &&
                            ((cdfc = id2idx(mc[a].bm)) >= 0)) {

                                for (fp = 0; (fp == 0) && (b <= tops);) {

                                        /* transliteration of b */
                                        tb = (mc[b].tr ==
                                              NULL) ? NULL : ((mc[b].
                                                               tr)->t);

                                        /* found a pair */
                                        if ((tb != NULL) &&
                                            (strcmp(ta, tb) == 0) &&
                                            (mc[a].bm != mc[b].bm)) {

                                                fp = 1;
                                        } else {
                                                /*
                                                   printf("candidates %d %d failed:\n",a,b);
                                                   printf("    transliteration of a: %s\n",ta);
                                                   printf("    transliteration of b: %s\n",tb);
                                                   printf("    pattern of a: %d\n",mc[a].bm);
                                                   printf("    pattern of b: %d\n",mc[b].bm);
                                                   printf("    cdfc = %d\n",cdfc);
                                                 */
                                                ++b;
                                        }
                                }
                        }

                        if (fp == 0) {
                                ++a;
                                b = a + 1;
                        }
                }
        }

        /* reset a and b */
        if (fp == 0) {
                a = 0;
                b = 1;
                memset(cfont, WHITE, FS * FS);
                show_hint(1, "No more pairs");
                return;
        }

        /*
           Display the comparisons between b and the skeleton
           of the pattern of the class of a.
         */
        else {
                int lfa, bm, bq, true_topp;

                lfa = mc[b].lfa;
                bm = mc[b].bm;
                bq = mc[b].bq;
                mc[b].lfa = -1;
                mc[b].bm = -1;
                mc[b].bq = 0;
                ocr_st = st;
                true_topp = topp;
                topp = cdfc;
                classify_symbol(b);
                cdfc = true_topp;
                st = (ocr_st > 7) ? 1 : ocr_st;
                if (st == 1) {
                        memset(cfont, WHITE, FS * FS);
                        snprintf(mba, MMB, "Finished comparing %d and %d",
                                 a, b);
                        show_hint(1, mba);
                }
                mc[b].lfa = -1;
                mc[b].bm = bm;
                mc[b].bq = bq;
        }
}
#endif

/*

Auxiliar for locating intervals within clx or cly (see list_cl).

Some comments assume xaxis!=0. If xaxis==0, then the change the
"l" field on the comments by "t".

*/
void locate_cl(int *c, int t, int s, int w, int *a, int *b, int xaxis) {
        int l, r, m;

        /*
           Binary search to locate the smallest clx index "a"
           satisfying cl[clx[a]].l >= s. The loop hypothesis
           is cl[clx[i]].l < s for 0<=i<l and cl[clx[i]].l >= x
           for i >= r.
         */
        l = 0;
        r = t + 1;
        while (l < r) {
                m = (l + r) / 2;
                if ((xaxis ? closure_get(c[m])->l : closure_get(c[m])->t) < s)
                        l = m + 1;
                else
                        r = m;
        }
        *a = l;

        /*
           Binary search to locate the greatest clx index "b" 
           satisfying c[clx[b]].l < s+w. The loop hypothesis
           is cl[clx[i]].l >= s+w for i>=r and cl[clx[i]].l < s+w
           for 0<=i<l.
         */
        l = 0;
        r = t + 1;
        while (l < r) {
                m = (l + r) / 2;
                if ((xaxis 
		     ? closure_get(c[m])->l 
		     : closure_get(c[m])->t) < s + w)
                        l = m + 1;
                else
                        r = m;
        }
        *b = l - 1;
}

/* Auxiliar for sorting clx (see list_cl) */
int cmp_clx(int a, int b) {
        return closure_get(a)->l - closure_get(b)->l;
}

/* Auxiliar for sorting cly (see list_cl) */
int cmp_cly(int a, int b) {
        return closure_get(a)->t - closure_get(b)->t;
}

int *clx = NULL, *cly = NULL;
int csz = 0, list_s_r_sz, nlx, nly;

/* (devel)

The function list_cl
--------------------

The function list_cl lists all closures that intersect the rectangle
of height h and width w with top left (x,y). The result will be
available on the global array list_cl_r, on indexes
0..list_cl_sz-1. This service is used to clip the closures or symbols
(see list_s) currently visible on the PAGE window. It's also used by
OCR operations that require locating the neighbours of one closure or
symbol (see list_s).

The parameter reset must be zero on all calls, except on the very
first call of this function after loading one page.

Every time a new page is loaded, this service must be called
informing a nozero value for the reset parameter. In this case,
the other parameters (x, y, w and h) are ignored, and the effect
will be preparing the page-specific static data structures used
to speed up the operation.

Closures are located by list_cl from the static lists of closures
clx and cly, ordered by leftmost and topmost coordinates. Small
and large closures are handled separately. The number of closures
with width larger than FS is counted on nlx. The number of
closures with height larger than FS is counted on nly.

The clx array is divided in two parts. The left one contains
(topcl+1)-nlx indexes for the closures with width not larger than
FS, sorted by the leftmost coordinate. The right one contains the
other indexes, in descending order.

The cly array is divided in two parts. The left one contains
(topcl+1)-nly indexes for the closures with height not larger
than FS, sorted by the topmost coordinate. The right one contains
the other indexes, in descending order.

So the small closures on the rectangle (x,y,w,h) may be located
through a combination of bynary searches on both axis. The large
closures are located by a brute-force linear loop. As nlx and nly
are expected to be very small, this brute force loop won't waste
CPU time.

*/
void list_cl(int x, int y, int w, int h, int reset) {
        int i, a, b;
        int *cx, *cy, tx, ty;

        if (closure_count() <= 0) {
                if (reset) {
                        fatal(DI,
                              "resetting without closures (blank page?)");
                }
                list_cl_sz = 0;
                return;
        }

        if (x < 0)
                x = 0;
        if (x >= XRES)
                x = XRES - 1;
        if (x + w > XRES)
                w = XRES - x;
        if (y < 0)
                y = 0;
        if (y >= YRES)
                y = YRES - 1;
        if (y + h > YRES)
                h = YRES - y;

        /*
           Just prepare the data structures for locating closures
           on the current page.
         */
        if (reset) {

                /*
                   Make the buffers larger depending on the number of
                   closures on the loaded page.
                 */
                if (csz < closure_count()) {
                        csz = closure_count();
                        clx = g_renew(int, clx, csz);
                        cly = g_renew(int, cly, csz);
                        list_cl_r = g_renew(int, list_cl_r, csz);
                }

                /* This loop builds the clx array and computes nlx */
                for (nlx = 0, i = 0; i < closure_count(); ++i) {
                        if (closure_get(i)->r - closure_get(i)->l >= FS) {

#ifdef MEMCHECK
                                checkidx(closure_count() - nlx - 1, csz, "list_cl 1");
#endif
                                clx[closure_count() - 1 - nlx] = i;
                                ++nlx;
                        } else {

#ifdef MEMCHECK
                                checkidx(i - nlx, csz, "list_cl 2");
#endif
                                clx[i - nlx] = i;
                        }
                }
                qsf(clx, 0, closure_count() - 1 - nlx, 0, cmp_clx);

                /* This loop builds the cly array and computes nly */
                for (nly = 0, i = 0; i < closure_count(); ++i) {
                        if (closure_get(i)->b - closure_get(i)->t > FS) {

#ifdef MEMCHECK
                                checkidx(closure_count() - 1 - nly, csz, "list_cl 3");
#endif
                                cly[closure_count() - 1 - nly] = i;
                                ++nly;
                        } else {
#ifdef MEMCHECK
                                checkidx(i - nly, csz, "list_cl 3");
#endif
                                cly[i - nly] = i;
                        }
                }
                qsf(cly, 0, closure_count() - 1 - nly, 0, cmp_cly);

                /* for debugging */
                /*
                   for (i=0; i<topcl-nlx; ++i)
                   printf(" %d",cl[clx[i]].l);
                   printf("\n");
                   for (i=0; i<topcl-nly; ++i)
                   printf(" %d",cl[cly[i]].t);
                 */

                /* finished building the data structures */
                return;
        }

        /*
           The non-static array cx is allocated on the stack, and
           used as temporary space for computing the list of closures
           that intersect the horizontal coordinates [x..x+w[.
         */
        if ((cx = g_newa(int, closure_count())) == NULL) {
                fatal(ME, "at list_cl");
        }
        tx = -1;

        /* search the small closures with leftmost coordinate within [x..x+w[ */
        locate_cl(clx, closure_count() - 1 - nlx, x, w, &a, &b, 1);
        for (i = a; i <= b; ++i) {
#ifdef MEMCHECK
                checkidx(tx + 1, closure_count(), "list_cl 4");
#endif
                cx[++tx] = clx[i];
        }

        /*
           Examine the small closures with leftmost coordinate
           within [x-FS+1,x[
         */
        locate_cl(clx, closure_count() - 1 - nlx, x - FS + 1, FS - 1, &a, &b, 1);
        for (i = a; i <= b; ++i) {
                if (closure_get(clx[i])->r >= x) {

#ifdef MEMCHECK
                        checkidx(tx + 1, closure_count(), "list_cl 5");
#endif
                        cx[++tx] = clx[i];
                }
        }

        /* search the large closures that intersect [x..x+w[ */
        for (i = 0; i < nlx; ++i) {
                a = clx[closure_count() - 1 - i];
                if (intersize(closure_get(a)->l, closure_get(a)->r, x, x + w - 1, NULL, NULL) >
                    0) {

#ifdef MEMCHECK
                        checkidx(tx + 1, closure_count(), "list_cl 6");
#endif
                        cx[++tx] = a;
                }
        }

        /*
           The non-static array cy is allocated on the stack, and
           used as temporary space for computing the list of closures
           that intersect the vertical coordinates [y..y+w[.
         */
        if ((cy = g_newa(int, closure_count())) == NULL) {
                fatal(ME, "at list_cl");
        }
        ty = -1;

        /* search the small closures with topmost coordinate within [y..y+h[ */
        locate_cl(cly, closure_count() - 1 - nly, y, h, &a, &b, 0);
        for (i = a; i <= b; ++i) {

#ifdef MEMCHECK
                checkidx(ty + 1, closure_count(), "list_cl 7");
#endif
                cy[++ty] = cly[i];
        }

        /*
           examine the small closures with topmost coordinate
           within [y-FS+1,y[
         */
        locate_cl(cly, closure_count() - 1 - nly, y - FS + 1, FS - 1, &a, &b, 0);
        for (i = a; i <= b; ++i) {
                if (closure_get(cly[i])->b >= y) {
#ifdef MEMCHECK
                        checkidx(ty + 1, closure_count(), "list_cl 8");
#endif
                        cy[++ty] = cly[i];
                }
        }

        /* search the large closures that intersect [y..y+h[ */
        for (i = 0; i < nly; ++i) {
                a = cly[closure_count() - 1 - i];
                if (intersize(closure_get(a)->t, closure_get(a)->b, y, y + h - 1, NULL, NULL) >
                    0) {
#ifdef MEMCHECK
                        checkidx(ty + 1, closure_count(), "list_cl 9");
#endif
                        cy[++ty] = a;
                }
        }

        /* Compute the intersection of cx and cy */
        if ((tx >= 0) && (ty >= 0)) {
                qss(cx, 0, tx);
                qss(cy, 0, ty);
        }

        for (i = a = b = 0; (a <= tx) && (b <= ty);) {

                if (cx[a] < cy[b])
                        ++a;
                else if (cx[a] > cy[b])
                        ++b;
                else {
#ifdef MEMCHECK
                        checkidx(i, csz, "list_cl 10");
#endif
                        list_cl_r[i++] = cy[b++];
                        a++;
                }
        }
        list_cl_sz = i;
}

/*

The function list_s lists all preferred symbols that intersect
the rectangle of height h and width w with top left (x,y). The
result will be available on the global array list_s, on indexes
0..list_s_sz-1. This service is used to clip the symbols
currently being exhibited on the PAGE window. It's also used by
OCR operations that require locating the neighbours of one
symbol.

The function list_s relies on list_cl. A call to list_s will also
build list_cl_r as explained before.

*/
void list_s(int x, int y, int w, int h) {
        int *c, i, j;

        if (x < 0)
                x = 0;
        if (x >= XRES)
                x = XRES - 1;
        if (x + w > XRES)
                w = XRES - x;
        if (y < 0)
                y = 0;
        if (y >= YRES)
                y = YRES - 1;
        if (y + h > YRES)
                h = YRES - y;

        /* enlarge the buffer */
        if (list_s_r_sz <= tops) {
                list_s_r_sz = tops + 128;
                list_s_r = g_renew(int, list_s_r, list_s_r_sz);
        }

        /* list the closures */
        list_cl(x, y, w, h, 0);

        /* convert closures to preferred symbols */
        for (i = 0; i < list_cl_sz; ++i) {
                int k;

                c = closure_get(k = list_cl_r[i])->sup;
                for (j = 0; (*c >= 0) && (!C_ISSET(mc[*c].f, F_ISP));
                     ++j, ++c);

                if (*c >= 0)
                        list_s_r[i] = *c;
        }

        /* remove duplications */
        if (list_cl_sz > 0)
                qss(list_cl_r, 0, list_cl_sz - 1);
        for (i = -1, j = 0; j < list_cl_sz; ++j)
                if ((i < 0) || (list_s_r[i] != list_s_r[j]))
                        list_s_r[++i] = list_s_r[j];

        /* new size */
        list_s_sz = i + 1;
}

/*

Candido de Figueiredo block separation heuristic.

*/
void cf_block(void) {
        int i, l, r, t, b, tm, bm, m;
        sdesc *s;

        t = YRES;
        b = 0;
        tm = bm = XRES / 2;
        l = XRES;
        r = 0;

        for (i = 0; i <= tops; ++i) {
                if (mc[i].b - mc[i].t > 900) {
                        s = mc + i;

/*
            printf("symbol %d: l=%d r=%d t=%d b=%d\n",i,s->l,s->r,s->t,s->b);
*/

                        if (s->t < t) {
                                t = s->t;
                                tm = (s->l + s->r) / 2;
                        }
                        if (s->b > b) {
                                b = s->b;
                                bm = (s->l + s->r) / 2;
                        }

                        if (s->l < l)
                                l = s->l;
                        if (s->r > r)
                                r = s->r;
                }
        }

        m = (tm + bm) / 2;

        printf("%s left block: l=%d t=%d r=%d b=%d\n", pagebase, 0, t - FS,
               r, b + FS);

        limits[0] = limits[4] = 0;
        limits[1] = limits[7] = t - FS;
        limits[2] = limits[6] = r;
        limits[3] = limits[5] = b + FS;

        /*
           {
           char p[MAXFNL+1];

           snprintf(p,MAXFNL,"%s-l.pbm",pagebase);
           p[MAXFNL] = 0;
           wrzone(p,0);
           }
         */

        printf("%s right block: l=%d t=%d r=%d b=%d\n", pagebase, l,
               t - FS, XRES - 1, b + FS);

        limits[8] = limits[12] = l;
        limits[9] = limits[15] = t - FS;
        limits[10] = limits[14] = XRES - 1;
        limits[11] = limits[13] = b + FS;

        /*
           {
           char p[MAXFNL+1];

           snprintf(p,MAXFNL,"%s-r.pbm",pagebase);
           p[MAXFNL] = 0;
           wrzone(p,0);
           }
         */

        zones = 2;
}

/*

Macros used by the service inside().

*/
#define IntProj(a,u,x) ((a<u) ? (a>=x) : ((a==u) ? 1 : (a<=x)))
#define IntSeg(a,b,u,v,x,y) (IntProj(a,u,x)) && (IntProj(b,v,y))

/*

Returns 1 if the pixel (x,y) is inside the polygon pol, 0 if outside
or 2 if on border. The polygon pol is defined by the points (pol[0];
pol[1]), (pol[2]; pol[3]), ... ,(pol[2*(npol-1)]; pol[2*(npol-1)+1]).

This service is based on a standard vectorial method.

*/
int inside(int x, int y, int *pol, int npol) {
        int p = 0, *q = pol, i, r;

        for (i = npol; i > 0; --i) {

                /* vectorial dot */
                r = (q[0] - x) * (q[3] - q[1]) -
                    (q[1] - y) * (q[2] - q[0]);

                /* outside */
                if (((p > 0) && (r < 0)) ||
                    ((p < 0) && (r > 0)))
                        return (0);

                else if (r == 0) {

                        /* on border */
                        if (IntSeg(x, y, q[0], q[1], q[2], q[3]))
                                return (2);

                        /* outside */
                        else
                                return (0);
                } else
                        p = r;

                /* prepare next vertice */
                q += 2;
        }

        /* inside */
        return (1);
}
