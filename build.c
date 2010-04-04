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

build.c: The function build.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include "common.h"

/*

Magic numbers

m_mwd .. Maximum horizontal distance accepted in order to
consider two words aligned, specified as a percentage of
x_height. Values greater than 100 are accepted as well.

m_msd .. Maximum horizontal distance accepted in order to
consider two symbols aligned, specified as a percentage of
x_height. Values greater than 100 are accepted as well.

m_mae .. Acceptable error when testing fitting of one symbol on the
expected limits for the ascent and descent, specified as a
percentage of the dot diameter. Values greater than 100 are
accepted as well.

m_ds .. Descent (relative to baseline) as a percentage of the dot
diameter. Usually greater than 100 (for instance 200).

m_as ..  Ascent (relative to baseline) as a percentage of the dot
diameter. Usually greater than 100 (for instance 800).

m_xh ..  x_height (relative to baseline) as a percentage of the dot
diameter. Usually greater than 100 (for instance 600).

m_fs ..  number of steps to complete one unity on float steppers.

*/

#ifdef CF
/* Candido de Figueiredo */
int m_mwd = 1000;
int m_msd = 40;
int m_mae = 60;
int m_ds = 150;
int m_as = 450;
int m_xh = 300;
int m_fs = 20;
#else
int m_mwd = 1000;
int m_msd = 60;
int m_mae = 50;
int m_ds = 200;
int m_as = 800;
int m_xh = 600;
int m_fs = 20;
#endif

/*

Bold and italic distinguishers.

*/
int bw[256], bh[256], bp[256];
int iw[256], ih[256], ip[256];

/*

cmpln diagnostic.

*/
int cmpln_diag;

/*

Validation of the recognition of the symbol i.

*/
void recog_validation(int i) {
        int k, b;
        sdesc *m;

        /* the symbol we're validating */
        m = mc + (k = i);

        /*
           Ignore DOTS with invalid alignments. The rationale here
           is that noise is easily classified as dot. So we must ignore all
           dots without a typical dot context, like the top of "i" or
           "j", period, etc. An easy approximation for this criteria is
           considering the alignment. So we remove all SHAPE votes
           from dots with unknown alignment.

           Remark: this rule is temporarily out of the code because it
           drops a valid classification when the engine does not have
           enough information to deduce the dot alignment.
         */
        /*
           if ((m->tc == DOT) && (m->va < 0)) {
           rmvotes(SHAPE,k,-1,0);
           }
         */

        /*
         *   COMMA with align = 01 => apostrophe. (GL)
         */
        if ((m->tc == COMMA) && (m->va == 01)
            && (m->tr != NULL) && (strlen(m->tr->t) == 1)
            && (m->tr->t[0] == ',')) {
                m->tr->t[0] = '\'';
        }

        /*
           Apply add-hoc filtering rules.
         */
        {
                char *t;

                if ((m->tr != NULL) &&
                    (strlen(t = (char *) (m->tr->t)) == 1) &&
                    (avoid(k, t[0]))) {

                        rmvotes(SHAPE, k, -1, 0);

                        /*
                           BUG: We must reset lfa and bm only when the symbol
                           is not a pattern. By now we're using a simple but
                           broken test. Must change in the future.
                         */
                        if (m->tr == NULL) {
                                m->lfa = -1;
                                m->bm = -1;
                        }
                }
        }

        /* attach accents to their base letters */
        if (m->tc == ACCENT) {
                int *t;

                /*
                   strangely enough, it seems that some untransliterated symbols
                   are being classified as accents.
                 */
                if (m->tr == NULL) {
                        db("oops.. untransliterated symbol classified as accent");
                }

                else if (((b = bsymb(k, 0)) >= 0) &&
                         (box_dist(ps[i], b, NULL) <
                          ((mc[b].b - mc[b].t) / 2))) {

                        for (t = &(mc[b].sl); (*t >= 0); t = &(mc[*t].sl));
                        *t = i;
                        m->bs = i;
                }

                /*
                   double quote preliminar tests.
                 */
                else if ((strcmp(trans(i), "\'") == 0) &&
                         ((b = lsymb(k, 1)) >= 0) &&
                         (strcmp(trans(b), "\'") == 0)) {

                        for (t = &(mc[b].sl); (*t >= 0); t = &(mc[*t].sl));
                        *t = i;
                        m->bs = i;
                } else if ((strcmp(trans(i), "`") == 0) &&
                           ((b = lsymb(k, 1)) >= 0) &&
                           (strcmp(trans(b), "`") == 0)) {

                        for (t = &(mc[b].sl); (*t >= 0); t = &(mc[*t].sl));
                        *t = i;
                        m->bs = i;
                }
        }

        /* attach dot to dot/comma -> colon/semicolon (GL) */
        if (m->tc == DOT) {
                int *t;

                /*
                   strangely enough, it seems that some untransliterated symbols
                   are being classified as dots. (??? Copied from accents. GL)
                 */
                if (m->tr == NULL) {
                        db("oops.. untransliterated symbol classified as dot");
                }

                else if ((b = bsymb(k, 1)) >= 0) {
                        /* printf("\n<%d on %d>",i,b); */

                        if (((mc[b].tc == DOT) || (mc[b].tc == COMMA))
                            && (mc[b].l < mc[i].r) && (mc[i].l < mc[b].r)
                            ) {
                                /* printf("!%d-%d ; %d-%d",mc[i].l,mc[i].r,mc[b].l,mc[b].r); */
                                for (t = &(mc[b].sl); (*t >= 0);
                                     t = &(mc[*t].sl));
                                *t = i;
                                m->bs = i;
                        }

                        else if ((box_dist(ps[i], b, NULL) <
                                  ((mc[b].b - mc[b].t) / 2))) {

                                for (t = &(mc[b].sl); (*t >= 0);
                                     t = &(mc[*t].sl));
                                *t = i;
                                m->bs = i;
                        }

                }

        }
}

/*

Using the estimated baseline for each symbol, computes the baseline
geometry, top and bottom limits for line a.

          +-+        +-+
       +-+| |  +-++-+| |+-++-+
    a: | || |  | || || || || |
       +*++*+  |*|O*++*++*++*+
               +-+

For each symbol s on line a, estimates its baseline bl=bl(s). The
medium points at heigh bl(s) for each symbol s (stars, in the
figure) are used to interpolate a line. The slope of this line is
returned as *sl. The topmost and bottommost coordinates reached
by the line a symbols are returned on *at and *ab. The baseline
medium point ("O" in the figure) is returned on (*mx,*my). The
medium x_height is returned in *xh. The function value is 0 if
could not interpolate a baseline, 1 otherwise.

*/
int bl_geo(int a, float *sl, int *at, int *ab, int *mh, float *mx,
           float *my) {
        /* alignment */
        int as, xh, bl, ds;

        /* liner regression */
        int x, y, n;
        int sxh, nxh;
        long long sx, sy, sx2, sy2, sxy;
        float N;

        /* other */
        int s, w, ret, l, r;

        /* prepare */
        *at = YRES;
        *ab = 0;
        l = XRES;
        r = 0;
        sx = sy = sx2 = sy2 = sxy = 0;
        sxh = 0;
        n = nxh = 0;

        /* visit all symbols */
        for (w = line[a].f; w >= 0; w = word[w].E) {
                for (s = word[w].F; s >= 0; s = mc[s].E) {

                        /* extreme coordinates */
                        if (mc[s].b > *ab)
                                *ab = mc[s].b;
                        if (mc[s].t < *at)
                                *at = mc[s].t;
                        if (mc[s].l < l)
                                l = mc[s].l;
                        if (mc[s].r > r)
                                r = mc[s].r;

                        /* alignment parameters for a and b */
                        get_ap(s, &as, &xh, &bl, &ds);

                        /* accumulate */
                        if (bl > 0) {
                                x = (mc[s].l + mc[s].r) / 2;
                                y = bl;
                                sx += x;
                                sx2 += (x * x);
                                sy += y;
                                sy2 += (y * y);
                                sxy += (x * y);
                                ++n;
                        }

                        /* accumulate xh */
                        if (xh > 0) {
                                sxh += xh;
                                ++nxh;
                        }

                }
        }

        /* slope and medium point */
        if ((n > 1) && (nxh > 0)) {
                float y0;

                N = n;
                *sl = (sxy - sx * sy / N) / (sx2 - sx * sx / N);
                *mx = ((float) (l + r)) / 2;

                /*
                   If the absolute value of the slope is too large, our
                   dataset is strange and probably useless.
                 */
                if (fabs(*sl) > 0.5) {
                        db("what? slope %f?\n", *sl);
                        ret = 0;
                }

                /* finish computing the medium point */
                else {
                        y0 = (sy - *sl * sx) / N;
                        *my = *sl * *mx + y0;
                        *mh = (sxh / nxh);
                        ret = 1;
                }
        }

        else {
                ret = 0;
        }

        return (ret);
}

/*

Returns -1 if the line a precedes the line b, or +1 if the line b
precedes the line a, or 0 if cannot decide.

This is the service used to sort the lines when generating the
OCR output. Clara OCR uses the alignment heuristics to build
words from the symbols, and lines from the words. The alignment
heuristics do not deduce if one line a precedes one line b, this
is done by a subsequent build step.

       +---------------+
    a: |to be or not to|
       +---------------+

       +-----------------------+
    b: |be, that's the question|
       +-----------------------+

Due to skew, it's not that easy to deduce if one line precedes
another. Remember that not all lines are full lines. Some lines
are only partially filled. Also, some lines become broken due to
weaknesses on the current alignment heuristics.

Stores on cmpln_diag a diagnostic as follows:

  0 could not decide
  1 the lines belong to different zones
  2 decided by baseline analysis
  3 vertical disjunction
  4 extremities medium vertical position
  5 extremities medium horizontal position
  6 horizontal disjunction

*/
int cmpln(int a, int b) {
        sdesc *p, *q, *u, *v;
        int at, ab, ah, bt, bb, bh;
        int ag, bg;
        float as, bs, ax, ay, bx, by;

        /* get the first and last symbols on each line */
        p = mc + word[line[a].f].F;
        q = mc + word[line[b].f].F;
        u = mc + word[line[a].f].L;
        v = mc + word[line[b].f].L;

        /*
           When the lines belong to different zones, use
           the zone indexes to decide.
         */
        if (p->c < q->c) {
                cmpln_diag = 1;
                return (-1);
        } else if (p->c > q->c) {
                cmpln_diag = 1;
                return (1);
        }

        /* geometries of the baselines */
        ag = bl_geo(a, &as, &at, &ab, &ah, &ax, &ay);
        bg = bl_geo(b, &bs, &bt, &bb, &bh, &bx, &by);

        /*

           Analysis of the baselines
           -------------------------

           At this point, the baselines are interpolated for
           lines a and b, computing their slopes. Unless
           the computation fails, the relative position of
           the baselines is studied.


           Q
           ......*..........._____________ (a)
           |
           |
           ------*------- (b)
           M

           We proceed as follows: instead of the baseline of a, we'll
           consider the medium slope (between the two baselines) and
           draw the line with such slope and intercepting the medium
           point of the baseline of a. Now we consider the medium
           point M of the baseline of b and its vertical projection
           Q into that line. The relative position of M and Q is then
           studied.
         */
        if (ag && bg) {
                float s, A, B, C, y, a, b;

                /* medium slope */
                a = atan(as);
                b = atan(bs);
                if (as > bs) {
                        s = a - (a - b) / 2;
                } else {
                        s = b - (b - a) / 2;
                }

                /* equation of the baseline for line a (normal form Ax+By+C=0) */
                A = s;
                B = -1;
                C = -(A * ax + B * ay);

                /*
                   vertical coordinate of Q.
                 */
                y = C + A * ax;

                /* the vertical distance is large enough: decide */
                if (fabs(y - by) > ((ah + bh) / 2)) {

                        if (y < by) {
                                cmpln_diag = 2;
                                return (-1);
                        } else {
                                cmpln_diag = 2;
                                return (1);
                        }
                }
        }

        /*
           Try some simple criteria (BUG: hardcoded values).
         */
        if (ab < bt) {
                cmpln_diag = 3;
                return (-1);
        } else if (bb < at) {
                cmpln_diag = 3;
                return (1);
        } else if (u->r + 5 < q->l) {
                cmpln_diag = 6;
                return (-1);
        } else if (v->r + 5 < p->l) {
                cmpln_diag = 6;
                return (1);
        } else if ((p->b + p->t) / 2 + 15 < (q->b + q->t) / 2) {
                cmpln_diag = 4;
                return (-1);
        } else if ((q->b + q->t) / 2 + 15 < (p->b + p->t) / 2) {
                cmpln_diag = 4;
                return (1);
        } else if ((p->l + p->r) / 2 < (q->l + q->r) / 2) {
                cmpln_diag = 5;
                return (-1);
        } else if ((q->l + q->r) / 2 < (p->l + p->r) / 2) {
                cmpln_diag = 5;
                return (1);
        } else {
                cmpln_diag = 0;
                return (0);
        }
}

/*

Account bold and italic distinguisher symbols. This simplistic
code assumes only one bold font size along the book. If more than
one such size exists, detection of bold words will become
compromised (not a big problem, though).

*/
void acc_f(int *fw, int *fh, int *fp, int FF) {
        int c, i, f[256], o[256];
        int ow[256], oh[256], op[256];
        pdesc *p;

        /* prepare account */
        for (i = 0; i < 256; ++i) {
                fw[i] = fh[i] = fp[i] = 0;
                ow[i] = oh[i] = op[i] = 0;
                f[i] = o[i] = 0;
        }

        /* account featured symbols */
        for (i = 0; i <= topp; ++i) {

                p = pattern + i;
                if ((p->tr != NULL) && (strlen(p->tr) == 1)) {

                        c = ((unsigned char *) (p->tr))[0];

                        if (p->f & FF) {
                                fw[c] += p->bw;
                                fh[c] += p->bh;
                                fp[c] += p->bp;
                                ++(f[c]);
                        } else {
                                ow[c] += p->bw;
                                oh[c] += p->bh;
                                op[c] += p->bp;
                                ++(o[c]);
                        }
                }
        }

        /* search bold distinguishers */
        for (c = 0; c < 256; ++c) {

                if ((f[c] > 0) && (o[c] > 0)) {

                        fw[c] /= f[c];
                        fh[c] /= f[c];
                        fp[c] /= f[c];

                        ow[c] /= o[c];
                        oh[c] /= o[c];
                        op[c] /= o[c];

                        if (abs(fp[c] - op[c]) <= (fp[c] / 10))
                                fp[c] = 0;
                        if (abs(fh[c] - oh[c]) <= (fh[c] / 10))
                                fh[c] = 0;
                        if (abs(fw[c] - ow[c]) <= (fw[c] / 10))
                                fw[c] = 0;
/*
            if ((fp[c] > 0) || (fh[c] > 0) || (fw[c] > 0))
                printf("%d distinguisher: %d %c\n",FF,c,c);
*/
                }
        }
}

/*

Compute word properties.

*/
void wprops(int i) {
        /* compute baseline (TO BE DONE) */

        /* italic and bold flags, and the word type */
        {
                wdesc *w;
                int j, it, bd;

                w = word + i;
                it = 0;
                bd = 0;
                w->a = UNDEF;
                for (j = word[i].F; j >= 0; j = mc[j].E) {
                        if (!uncertain(mc[j].tc)) {
                                char *t;
                                int c, w, h, p;

                                t = ((char *) (mc[j].tr->t));
                                if (strlen(t) == 1) {

                                        c = t[0];
                                        w = mc[j].r - mc[j].l + 1;
                                        h = mc[j].b - mc[j].t + 1;
                                        p = mc[j].nbp;

                                        if (((mc[j].tr->f & F_ITALIC) != 0)
                                            &&
                                            (((iw[c] > 0) &&
                                              (abs(iw[c] - w) <
                                               iw[c] / 20)) || ((ih[c] > 0)
                                                                &&
                                                                (abs
                                                                 (ih[c] -
                                                                  h) <
                                                                 ih[c] /
                                                                 20)) ||
                                             ((ip[c] > 0) &&
                                              (abs(ip[c] - p) <
                                               ip[c] / 20)))) {

                                                it = 1;
                                        }

                                        if (((mc[j].tr->f & F_BOLD) != 0)
                                            &&
                                            (((bw[c] > 0) &&
                                              (abs(bw[c] - w) <
                                               bw[c] / 20)) || ((bh[c] > 0)
                                                                &&
                                                                (abs
                                                                 (bh[c] -
                                                                  h) <
                                                                 bh[c] /
                                                                 20)) ||
                                             ((bp[c] > 0) &&
                                              (abs(bp[c] - p) <
                                               bp[c] / 20)))) {

                                                bd = 1;
                                        }
                                }
                        }
                }

                if (it)
                        C_SET(w->f, F_ITALIC);
                else
                        C_UNSET(w->f, F_ITALIC);
                if (bd)
                        C_SET(w->f, F_BOLD);
                else
                        C_UNSET(w->f, F_BOLD);
        }

        /* compute bounding box */
        {
                int l, r, t, b;
                sdesc *s;

                s = mc + word[i].F;
                l = s->l;
                r = s->r;
                t = s->t;
                b = s->b;
                while (s->E >= 0) {
                        s = mc + s->E;
                        if (s->l < l)
                                l = s->l;
                        if (s->r > r)
                                r = s->r;
                        if (s->t < t)
                                t = s->t;
                        if (s->b > b)
                                b = s->b;
                }
                word[i].l = l;
                word[i].r = r;
                word[i].t = t;
                word[i].b = b;
        }
}

/*

Word pairing
------------

The word pairing test is used to build the text lines. The function
w_pair tests word pairing, and returns the following diagnostics:

0 .. the words are paired
1 .. insufficient vertical intersection
2 .. incomplete data
3 .. maximum horizontal distance exceeded
4 .. different zones

*/
int w_pair(int wa, int wb) {
        int a, b;
        int r, t, d, e;
        int dd, as, bl, xh, ds;
        int pa, pb;

        a = word[wa].L;
        b = word[wb].F;

        /* consist */
        if ((a < 0) || (tops < a) || (b < 0) || (tops < b)) {
                db("inconsistent input at w_pair");
                return (2);
        }

        if (mc[a].c != mc[b].c)
                return (4);

        /* compute horizontal distance */
        if (mc[a].r < mc[b].l)
                d = mc[b].l - mc[a].r;
        else if (mc[a].l + mc[a].r < mc[b].l + mc[b].r)
                d = 1;
        else
                d = 0;

        /* vertical intersection */
        t = intersize(mc[a].t, mc[a].b, mc[b].t, mc[b].b, NULL, NULL);

        /* check if wa or wb are pseudo-words */
        pa = ((mc[a].W < 0) && ((mc[a].tc == DOT) || (mc[a].tc == COMMA)));
        pb = ((mc[b].E < 0) && ((mc[b].tc == DOT) || (mc[b].tc == COMMA)));

        /* try to obtain x_height and baseline */
        dd = complete_align(a, b, &as, &xh, &bl, &ds);

        /* horizontal distance unacceptable (1st test) */
        if ((d <= 0) || (((m_mwd * FS) / 100) <= d))
                r = 3;

        /* pseudo-words special case */
        else if (pa || pb) {

                /* non-null intersection is the easy case */
                if (t > 0)
                        return (0);

                if (dd < 0) {
                        if (mc[a].tc == DOT)
                                dd = ((mc[a].r - mc[a].l) +
                                      (mc[a].b - mc[a].t) + 2) / 2;
                        else if (mc[b].tc == DOT)
                                dd = ((mc[b].r - mc[b].l) +
                                      (mc[b].b - mc[b].t) + 2) / 2;
                        else if (mc[a].tc == COMMA)
                                dd = (mc[a].r - mc[a].l) + 2 / 2;
                }

                /* could not estimate the dot diameter */
                if (dd < 0)
                        return (2);

                /* vertical distance between a and b */
                e = ldist(mc[a].b, mc[a].t, mc[b].b, mc[b].t);

                /* small vertical distance */
                if (e <= 2 * dd)
                        r = 0;

                /* large vertical distance */
                else
                        r = 1;
        }

        /* insufficient data */
        else if ((xh <= 0) || (bl <= 0))
                r = 2;

        /* horizontal distance unacceptable (2nd test) */
        else if ((d <= 0) || (((m_mwd * (bl - xh)) / 100) <= d))
                r = 3;

        /*
           BUG: hardcoded magic number
         */
        else if (t > 5) {

                r = 0;
        }

        /* no vertical intersection */
        else
                r = 1;

        return (r);
}

/*

Diagnose word pairing.

*/
void diag_wpairing(int t) {
        int r, w1, w2;

        if ((curr_mc < 0) ||
            (t < 0) ||
            ((w1 = mc[curr_mc].sw) < 0) || ((w2 = mc[t].sw) < 0)) {

                show_hint(2, "invalid parameters");
                return;
        }

        else if ((mc[curr_mc].l + mc[curr_mc].r) < (mc[t].l + mc[t].r))
                r = w_pair(w1, w2);
        else
                r = w_pair(w2, w1);

        if (r == 0)
                show_hint(2, "pairing successful");
        if (r == 1)
                show_hint(2, "insuficcient vertical intersection");
        if (r == 2)
                show_hint(2, "incomplete data");
        if (r == 3)
                show_hint(2, "maximum horizontal distance exceeded");
        if (r == 4)
                show_hint(2, "different zones");
}

/*

Diagnose symbol pairing.

*/
void diag_pairing(int t) {
        int r;

        if ((curr_mc < 0) || (t < 0)) {
                show_hint(2, "invalid parameters");
                return;
        }

        else if ((mc[curr_mc].l + mc[curr_mc].r) < (mc[t].l + mc[t].r))
                r = s_pair(curr_mc, t, 0, NULL);
        else
                r = s_pair(t, curr_mc, 0, NULL);

        if (r == 0)
                show_hint(2, "pairing successful");
        else if (r == 1)
                show_hint(2, "insuficcient vertical intersection");
        else if (r == 2)
                show_hint(2, "one or both symbols above ascent");
        else if (r == 3)
                show_hint(2, "one or both symbols below descent");
        else if (r == 4)
                show_hint(2, "maximum horizontal distance exceeded");
        else if (r == 5)
                show_hint(2, "incomplete data");
        else if (r == 6)
                show_hint(2, "different zones");
}

/*

Detect the word at the left side of the symbol t.

This routine was part of the "extend left word" feature. That
feature was removed from the OCR. This code is being mantained
here because it may be used again someday.

*/
int left_word(int t) {
        int i, w, a, d, l, x;

        /*
           To obtain the left word we use a
           straighforward method. Just search all words and, from those
           whose last symbol has vertical intersection with t, choose
           the one horizontally nearest to t.
         */
        w = -1;
        l = (mc[t].l + mc[t].r) / 2;
        for (i = 0, d = -1; i <= topw; ++i) {

                /* new best candidate */
                if ((word[i].F >= 0) &&
                    ((a = word[i].L) >= 0) &&
                    (intersize
                     (mc[a].t, mc[a].b, mc[t].t, mc[t].b, NULL, NULL) > 0)
                    && ((x = (mc[a].r + mc[a].l) / 2) < l) && ((w < 0) ||
                                                               (l - x <
                                                                d))) {

                        w = i;
                        d = l - x;
                }
        }

        /* return the left word (or -1) */
        return (w);
}

/*

Create and return an unitary word for symbol k.

*/
int new_word(int k) {
        int cw;

        /* enlarge buffer */
        if (++topw >= wsz) {
                wsz = topw + 512;
                word = g_renew(wdesc, word, wsz);
        }
        word[topw--].F = -1;

        /* find and use the next free entry */
        for (cw = 0; word[cw].F >= 0; ++cw);
        word[cw].F = word[cw].L = k;
        word[cw].bl = -1;
        word[cw].E = -1;
        word[cw].W = -1;
        word[cw].f = 0;
        if (cw > topw)
                topw = cw;
        mc[k].sw = cw;
        return (cw);
}

/*

The build function
------------------

*/
int build(int reset) {
        static int m, k, st;
        static int i;
        int j;

        /* reset state */
        if (reset) {
                st = 0;
                topw = -1;
                topln = -1;
                show_hint(0, "preparing");
                return (1);
        }

        /*
         ** STATE 0 **
         Preparation
         */
        if (st == 0) {

                /* (devel)

                   The build step
                   --------------

                   The "build" OCR step, implemented by the "build"
                   function, distributes the symbols on words
                   (analysing the distance, heights and relative
                   position for each pair of symbols), and the words
                   on lines (analysing the distance, heights and
                   relative position for each pair of words). Various
                   important heuristics take effect here.

                   0. Preparation

                   The first step of build is to distribute the symbols
                   on words. This is achieved by:

                   a. Undefining the next-symbol ("E" field) and previous-symbol
                   ("W" field) links for each symbol, the surrounding word ("sw"
                   field) of each symbol, and the next signal ("sl" field) for
                   each symbol.

                   Remark: The next-symbol and previous symbol links are used
                   to build the list of symbols of each word. For instance,
                   on the word "goal", "o" is the next for "g" and
                   the previous for "a", "g" has no previous and "l"
                   has no next).

                 */
                for (m = 0; m <= tops; ++m) {
                        mc[m].E = mc[m].W = mc[m].sw = mc[m].sl =
                            mc[m].bs = -1;
                }

                /* (devel)

                   b. Undefining the transliteration class of SCHARs and
                   the uncertain alignment information.

                 */
                for (m = 0; m <= tops; ++m) {
                        if (mc[m].sw < 0) {
                                if (mc[m].tc == SCHAR)
                                        mc[m].tc = UNDEF;
                                if (uncertain(mc[m].tc))
                                        mc[m].va = -1;
                        }
                }

                /* prepare next state */
                show_hint(0, "detecting words");
                m = 0;
                st = 1;
        }

        /*
         ** STATE 1 **
         distributing symbols on words
         */
        else if (st == 1) {

                /* (devel)

                   2. Distributing symbols on words

                   The second step is, for each CHAR not in any word, define
                   a unitary word around it, and extend it to right
                   and left applying the symbol pairing test.

                 */
                if (m <= topps) {
                        k = ps[m];
                        if ((mc[k].tc == CHAR) && (mc[k].sw < 0)) {
                                int a, b, cw, ow, f, t;

                                /* create unitary word */
                                cw = new_word(k);

                                /* extend to right */
                                for (a = k; ((b = rsymb(a, 0)) >= 0);
                                     a = b) {

                                        /* classify as SCHAR */
                                        if (mc[b].tc == UNDEF)
                                                mc[b].tc = SCHAR;

                                        /* extend word or... */
                                        if ((ow = mc[b].sw) < 0) {
                                                mc[a].E = b;
                                                mc[b].W = a;
                                                mc[b].sw = cw;
                                                word[cw].L = b;
                                        }

                                        /*
                                           ... merge words

                                           WARNING: this code creates unused entries on the
                                           array word below topw. Some of them will be
                                           reused by the word creation code, though.

                                         */
                                        else {

                                                /* medium vertical line */
                                                t = mc[a].l + mc[a].r;

                                                /* consist and merge */
                                                if (((f = word[ow].F) == b)
                                                    && (t <
                                                        mc[f].l +
                                                        mc[f].r)) {

                                                        for (f =
                                                             word[ow].F;
                                                             f >= 0;
                                                             f = mc[f].E)
                                                                mc[f].sw =
                                                                    cw;
                                                        mc[a].E = b;
                                                        mc[b].W = a;
                                                        mc[b].sw = cw;
                                                        word[cw].L =
                                                            word[ow].L;
                                                        word[ow].F = -1;
                                                }
                                        }
                                }

                                /* extend to left */
                                for (a = k; (b = lsymb(a, 0)) >= 0; a = b) {

                                        /* classify as SCHAR */
                                        if (mc[b].tc == UNDEF)
                                                mc[b].tc = SCHAR;

                                        /* extend word or... */
                                        if ((ow = mc[b].sw) < 0) {
                                                mc[b].E = a;
                                                mc[a].W = b;
                                                mc[b].sw = cw;
                                                word[cw].F = b;
                                        }

                                        /*
                                           ... merge words

                                           WARNING: this code creates unused entries on the
                                           array word below topw. Some of them will be
                                           reused by the word creation code, though.

                                         */
                                        else {

                                                /* medium vertical line */
                                                t = mc[a].l + mc[a].r;

                                                /* consist and merge */
                                                if (((f = word[ow].L) == b)
                                                    && (mc[f].l + mc[f].r <
                                                        t)) {

                                                        for (f =
                                                             word[ow].F;
                                                             f >= 0;
                                                             f = mc[f].E)
                                                                mc[f].sw =
                                                                    cw;
                                                        mc[b].E = a;
                                                        mc[a].W = b;
                                                        mc[b].sw = cw;
                                                        word[cw].F =
                                                            word[ow].F;
                                                        word[ow].F = -1;
                                                }
                                        }
                                }
                        }

                        /* prepare next symbol */
                        if ((++m % DPROG) == 0)
                                show_hint(0, "detecting words %d/%d", m,
                                          topps);
                }

                /* prepare next state */
                else {
                        show_hint(0, "merging");
                        st = 2;
                        i = 0;
                }
        }

        /*
         ** STATE 2 **
         Currently empty
         */
        else if (st == 2) {

                /*

                   (Currently empty)

                 */

                /* prepare next state */
                show_hint(0, "computing alignment");
                i = 0;
                st = 3;
        }

        /*
         ** STATE 3 **
         Computing the alignment using the words
         */
        else if (st == 3) {
                int k, l, dd, as, xh, bl, ds;
                sdesc *m, *t;

                /* (devel)

                   3. Computing the alignment using the words

                   Some symbols do not have a well-defined alignment by
                   themselves. For instance, a dot may be baseline-aligned
                   (a final dot) or 0-aligned (the "i" dot). So when
                   computing their alignments, we need to analyse their
                   neighborhoods. This is performed in this step.

                 */

                /* compute the alignment of dots and accents */
                if (i <= topps) {

                        m = mc + (k = ps[i]);

#ifdef MEMCHECK
                        checkidx(i, topps + 1, "build state 3.1");
#endif

                        /*
                           Alignment for dots may be 0 ("i", "j"),
                           11 (the top one in ":") or 22 (period).

                           Alignment for commas may be 22 (comma)
                           or 0 (apostrophe).

                           We expect alignment 0 for latin accents.
                         */
                        if ((m->tc == DOT) || (m->tc == COMMA) ||
                            (m->tc == ACCENT)) {
                                int x, y, w, h;

                                /* reset alignment */
                                m->va = -1;

                                /* compute neighbours */
                                x = (m->l + m->r) / 2 - FS;
                                y = (m->t + m->b) / 2 - FS;
                                w = h = 2 * FS;

                                list_s(x, y, w, h);

#ifdef MEMCHECK
                                checkidx(list_s_sz, tops + 1,
                                         "build state 3.2");
#endif

                                /*
                                   Try to compute the alignment using
                                   some known neighbour.
                                 */
                                for (l = 0; (l < list_s_sz) && (m->va < 0);
                                     ++l) {

#ifdef MEMCHECK
                                        checkidx(list_s_r[l], tops + 1,
                                                 "build state 3.3");
#endif

                                        t = mc + list_s_r[l];

                                        if ((intersize
                                             (m->t, m->b, t->t, t->b, NULL,
                                              NULL) > 0) &&
                                            ((t->tc == CHAR) ||
                                             (t->tc == SCHAR))) {

                                                /* obtain complete alignment data */
                                                dd = complete_align(k,
                                                                    list_s_r
                                                                    [l],
                                                                    &as,
                                                                    &xh,
                                                                    &bl,
                                                                    &ds);

                                                /* infer the alignment for k */
                                                if (dd > 0)
                                                        m->va =
                                                            geo_align(k,
                                                                      dd,
                                                                      as,
                                                                      xh,
                                                                      bl,
                                                                      ds);
                                        }
                                }
                        }

                        /* prepare next symbol */
                        if ((++i % DPROG) == 0)
                                show_hint(0, "computing alignment %d/%d",
                                          i, topps);
                }

                /* prepare next state */
                else {
                        show_hint(0, "validating");
                        st = 4;
                }
        }

        /*
         ** STATE 4 **
         Validating the recognition
         */
        else if (st == 4) {

                /* (devel)

                   4. Validating the recognition

                   Shape-based recognitions must be validated by special
                   heuristics. For instance, the left column of a broken 
                   "u" may be recognized as the body of an "i" letter. A
                   validation heuristic may refuse this recognition for
                   instance because the dot was not found. These heuristics
                   are per-alphabet.

                 */
                for (i = 0; i <= topps; ++i) {
                        recog_validation(ps[i]);
                }

                /* prepare next state */
                show_hint(0, "handling punctuation signs");
                st = 5;
        }

        /*
         ** STATE 5 **
         Creating fake words for punctuation signs
         */
        else if (st == 5) {
                sdesc *m;

                /* (devel)

                   5. Creating fake words for punctuation signs

                   To produce a clean output, symbols that do not belong to
                   any word are not included on the OCR output. So we need
                   to create fake words for punctuation signs like commas
                   of final dots.

                 */

                /*
                   Build unitary words for baseline-aligned dots and commas
                   and other things.
                 */
                for (i = 0; i <= topps; ++i) {

                        m = mc + (k = ps[i]);

                        if (((m->sw < 0) &&
                             (m->tc == DOT) && (m->va == 22))
                            || ((m->sw < 0) && (m->tc == COMMA) && ((m->va == 1)        /* apostrophe. (GL) */
                                                                    ||
                                                                    (m->va
                                                                     ==
                                                                     23)))
                            || ((m->sw < 0) && (m->tc == ACCENT) &&
                                (m->bs < 0) && (strcmp(trans(k), "'") == 0)
                                && (m->va == 1))
                            || ((m->sw < 0) && (m->tc == ACCENT) &&
                                (m->bs < 0) && (strcmp(trans(k), "`") == 0)
                                && (m->va == 1))
                            ) {
                                new_word(k);
                        }
                }

                /* prepare next state */
                show_hint(0, "aligning words");
                st = 6;
        }

        /*
         ** STATE 6 **
         Aligning words
         */
        else if (st == 6) {
                int a, b, d1, d2, k, l;

                /* (devel)

                   6. Aligning words

                   Words need to be aligned in order to detect the
                   page text lines. This is perfomed as follows:

                 */

                /* Mark the start of words */
                for (i = 0; i <= tops; ++i)
                        C_UNSET(mc[i].f, F_ISW);
                for (i = 0; i <= topw; ++i)
                        C_SET(mc[word[i].F].f, F_ISW);

                /* (devel)

                   a. Undefine the next-word and previous-word
                   links for each word. These are links for the
                   previous and next word within lines. For instance,
                   on the line "our goal is", "goal" is the next
                   for "our" and the previous for "is", "our" has
                   no previous and "is" has no next.
                 */
                for (i = 0; i <= topw; ++i) {
                        word[i].E = -1;
                        word[i].W = -1;
                }

                /* (devel)

                   b. Distribution of the words on lines. This is just
                   a matter of computing, for each word, its "next" word.
                   So for each pair of words, we test if they're "paired"
                   in the sense of the function w_pair. In affirmative
                   case, we make the left word point to the right word
                   as its "next" and the rigth point to the left as its
                   "previous".

                   The function w_pair does not test the existence of
                   intermediary words. So on the line "our goal is" that
                   function will report pairing between "our" and "is".
                   So after detecting pairing, our loop also checks if the
                   detected pairing is eventually "better" than those
                   already detected.

                 */
                for (i = 0; i <= topw; ++i) {
                        for (j = i + 1; j <= topw; ++j) {

                                /* ignore empty words */
                                if ((word[i].F < 0) || (word[j].F < 0))
                                        a = b = -1;

                                /* i-j or j-i pairing detected */
                                else if (w_pair(i, j) == 0) {
                                        a = i;
                                        b = j;
                                } else if (w_pair(j, i) == 0) {
                                        a = j;
                                        b = i;
                                } else
                                        a = b = -1;

                                /*
                                   Compare the pairings a-b and a-k where k=word[a].E,
                                   that is, a-k is a perhaps non-optimal previously
                                   detected pairing.
                                 */
                                if ((a >= 0) && (word[a].E >= 0)) {
                                        k = word[a].E;
                                        d1 = mc[word[b].F].l -
                                            mc[word[a].L].r;
                                        d2 = mc[word[k].F].l -
                                            mc[word[a].L].r;

                                        /* a-k is preferred */
                                        if (d1 >= d2) {
                                                a = -1;
                                        }
                                } else
                                        k = -1;

                                /*
                                   Compare the pairings a-b and l-b where l=word[b].W,
                                   that is, l-b is a perhaps non-optimal previously
                                   detected pairing.
                                 */
                                if ((a >= 0) && (word[b].W >= 0)) {

                                        l = word[b].W;
                                        d1 = mc[word[b].F].l -
                                            mc[word[a].L].r;
                                        d2 = mc[word[b].F].l -
                                            mc[word[l].L].r;

                                        /* l-b is preferred */
                                        if (d1 >= d2) {
                                                a = -1;
                                        }
                                } else
                                        l = -1;

                                /* pairing a-b is preferred */
                                if (a >= 0) {

                                        /* break link a-k */
                                        if (k >= 0) {
                                                word[k].W = -1;
                                        }

                                        /* break link l-b */
                                        if (l >= 0) {
                                                word[l].E = -1;
                                        }

                                        /* create link a-b */
                                        word[a].E = b;
                                        word[b].W = a;
                                }
                        }
                }

                /* count lines */
                for (topln = -1, k = 0; k <= topw; ++k) {
                        if ((word[k].W < 0) && (word[k].F >= 0))
                                ++topln;
                }

                /* enlarge memory area for lines */
                if (lnsz <= topln) {
                        lnsz = topln + 512;
                        line = g_renew(lndesc, line, lnsz);
                }

                /* line heads */
                for (topln = -1, k = 0; k <= topw; ++k) {
                        if ((word[k].W < 0) && (word[k].F >= 0)) {
                                line[++topln].f = k;
                                line[topln].hw = 0;
                        }
                }

                /* (devel)

                   c. Sort the lines. The lines are sorted based on the
                   comparison performed by the function "cmpln".

                 */
                {
                        int *a;
                        lndesc *l;

                        a = g_newa(int, topln + 1);
                        l = g_newa(lndesc, topln + 1);
                        for (i = 0; i <= topln; ++i)
                                a[i] = i;
                        qsf(a, 0, topln, 0, cmpln);
                        for (i = 0; i <= topln; ++i) {
                                memcpy(l + i, line + a[i], sizeof(lndesc));
                        }
                        memcpy(line, l, (topln + 1) * sizeof(lndesc));
                }

                /* word[i].tl must be the line where the word i was placed */
                for (i = 0; i <= topw; ++i)
                        word[i].tl = -1;
                for (i = 0; i <= topln; ++i)
                        for (j = line[i].f; j >= 0; j = word[j].E)
                                word[j].tl = i;

                /* prepare next state */
                show_hint(0, "computing word properties");
                st = 7;
        }

        /*
         ** STATE 7 **
         Computing word properties
         */
        else if (st == 7) {
                int i;

                /* account featured symbols */
                acc_f(bw, bh, bp, F_BOLD);
                acc_f(iw, ih, ip, F_ITALIC);

                /* (devel)

                   7. Computing word properties

                   Finally, word properties can be computed once we
                   have detected the words. Some of these properties are
                   applied to untransliterated symbols. The properties are:

                   1. The baseline left and right ordinates.

                   2. The italic and bold flags.

                   3. The alphabet.

                   4. The word bounding box.

                   All these properties are computed by the
                   function wprops.
                 */
                for (i = 0; i <= topw; ++i) {
                        if (word[i].F >= 0)
                                wprops(i);
                }

                /* finished */
                st = 0;
                return (0);
        }

        /* invalid state */
        else {
                st = 0;
                return (0);
        }

        /* did not complete */
        return (1);
}
