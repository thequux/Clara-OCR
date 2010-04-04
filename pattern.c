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

pattern.c: Book font stuff

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "common.h"
#include "gui.h"

/*

patterns

*/
int psz = 0, topp = -1, cdfc;
pdesc *pattern = NULL;
int s_id = -1;
int *slist = NULL, slistsz = 0;

/*

Pattern types.

*/
ptdesc *pt = NULL;
int ptsz = 0, toppt = -1, cpt = 1;

/*

Enlarge pattern types array.

*/
void enlarge_pt(int new_toppt) {
        int i;

        /* consist */
        if ((0 < new_toppt) && (new_toppt < 100)) {

                /* enlarge array */
                if (new_toppt >= ptsz) {
                        ptsz += (new_toppt - ptsz) + 10;
                        pt = g_renew(ptdesc, pt, ptsz);
                }

                for (i = toppt + 1; i <= new_toppt; ++i) {
                        init_pt(pt + i);
                }

                toppt = new_toppt;
        }

        else
                db("enlarge_pt called with invalid parameter %d",
                   new_toppt);
}

/*

Copy the FSxFS bitmap b to the buffer cb. Returns the number of
black pixels.

*/
int bm2cb(unsigned char *b, int dx, int dy) {
        int i, j, m, c;
        unsigned char *p;

        /* copy bitmap to buffer cb */
        memset(cb, WHITE, LFS * FS);
        for (c = 0, j = 0; j < FS - dy; ++j) {
                p = b + BLS * j;
                m = 128;
                for (i = 0; i < FS - dx; ++i) {
                        if (*p & m) {
                                cb[i + dx + (j + dy) * LFS] = BLACK;
                                ++c;
                        }
                        if ((m >>= 1) <= 0) {
                                ++p;
                                m = 128;
                        }
                }
        }
        return (c);
}

/*

Dump the FSxFS bitmap b to stdout.

*/
void dump_bm(unsigned char *b) {
        int i, j, m, lx, ly, f;
        unsigned char *p;

        ly = f = 0;
        for (j = 0; j < FS; ++j) {
                lx = 0;
                p = b + BLS * j;
                m = 128;
                for (i = 0; i < FS; ++i) {
                        if (*p & m) {
                                if (f) {
                                        for (; ly < j; ++ly)
                                                putchar('\n');
                                } else
                                        ly = j;
                                while (lx++ < i)
                                        putchar(' ');
                                putchar('X');
                                f = 1;
                        }
                        if ((m >>= 1) <= 0) {
                                ++p;
                                m = 128;
                        }
                }
        }
        if (f)
                putchar('\n');
}

/*

Copy the buffer cb to the FSxFS bitmap b.

*/
void cb2bm(unsigned char *b) {
        int i, j, m;
        unsigned char *p;

        /* copy skeleton to buffer s */
        memset(b, 0, BMS);
        for (j = 0; j < FS; ++j) {
                p = b + BLS * j;
                m = 128;
                for (i = 0; i < FS; ++i) {
                        if (cb[i + j * LFS] == BLACK) {
                                *p |= m;
                        }
                        if ((m >>= 1) <= 0) {
                                ++p;
                                m = 128;
                        }
                }
        }
}

/*

Compute the number of black pixels of the pattern s.

*/
int snbp(int s, int *bp, int *sp) {
        static int a = 0;
        int i, j, n, m;
        pdesc *d;
        unsigned char *p;

        /* number of bits of each byte */
        if (a == 0) {
                for (i = 0; i < 256; ++i) {
                        n = i;
                        np8[i] = 0;
                        for (j = 0; j < 8; ++j) {
                                np8[i] += (n & 1);
                                n >>= 1;
                        }
                }
                a = 1;
        }

        /*
           If the caller informed an invalid pattern index, return (that
           may happen to just initialize np8).
         */
        if (s < 0)
                return (0);

        /* compute the number of pixels of the pattern s */
        d = pattern + s;
        for (p = (unsigned char *) d->b, i = n = 0; i < FS * (FS / 8);
             ++p, ++i)
                n += np8[*p];
        if (bp != NULL)
                *bp = n;
        if (sp != NULL) {
                for (p = (unsigned char *) d->s, i = m = 0;
                     i < FS * (FS / 8); ++p, ++i)
                        m += np8[*p];
                *sp = m;
        }
        return (n);
}

/*

Compute the skeleton of the pattern c.

*/
void pskel(int c) {
        pdesc *d;
        int sp;

        /* copy pattern bitmap to buffer cb */
        d = pattern + c;
        bm2cb(d->b, 0, 0);

        /* copy tuned parameters */
#ifdef PATT_SKEL
        spcpy(SP_GLOBAL, c);
#endif

        /* compute skeleton */
        skel(-1, -1, 0, 0);
#ifdef PATT_SKEL
        spcpy(SP_GLOBAL, SP_DEF);
#endif

        /* compute skeleton height and width */
        justify_pattern(&(d->sh), &(d->sw), &(d->sx), &(d->sy));

        /* copy skeleton to buffer s */
        cb2bm(d->s);

        /* update counter of black pixels */
        snbp(c, NULL, &sp);
        d->sp = sp;
}

/*

Initialize pattern type

*/
void init_pt(ptdesc *p) {
        int i;

        p->at = 1;
        p->f = 0;
        p->bd = 0;
        strcpy(p->n, "unknown");
        p->a = -1;
        p->d = -1;
        p->xh = -1;
        p->dd = -1;
        p->ls = -1;
        p->ss = -1;
        p->ws = -1;
        p->acc = 0;
        for (i = 0; i < 256; ++i) {
                (p->sc)[i] = ((1 << CL_TOP) - 1);
                (p->sa)[i] = -1;
        }
}

/*

Top- and left-justification of the symbol currently on buffer
cb. The symbol width and height are returned on *w and *h.

*/
void justify_pattern(short *h, short *w, short *dx, short *dy) {
        int b, i, j, k;
        int width, height;

        /* gray pixels are discarded */
        for (i = 0; i < FS; ++i)
                for (j = 0; j < FS; ++j)
                        if (cb[i + j * LFS] != BLACK)
                                cb[i + j * LFS] = WHITE;

        /* left justification */
        b = 0;
        for (i = 0; (b == 0) && (i < FS); ++i) {
                for (j = 0; (b == 0) && (j < FS); ++j)
                        b = (cb[i + j * LFS] == BLACK);
        }
        if (b == 0)
                i = 0;
        else
                --i;
        if (dx != NULL)
                *dx = i;
        if (i > 0) {
                k = 0;
                while (i < FS) {
                        for (j = 0; j < FS; ++j)
                                cb[k + j * LFS] = cb[i + j * LFS];
                        ++i, ++k;
                }
                while (k < FS) {
                        for (j = 0; j < FS; ++j)
                                cb[k + j * LFS] = WHITE;
                        ++k;
                }
        }

        /* top justification */
        b = 0;
        for (j = 0; (b == 0) && (j < FS); ++j) {
                for (i = 0; (b == 0) && (i < FS); ++i)
                        b = (cb[i + j * LFS] == BLACK);
        }
        if (b == 0)
                j = 0;
        else
                --j;
        if (dy != NULL)
                *dy = j;
        if (j > 0) {
                k = 0;
                while (j < FS) {
                        for (i = 0; i < FS; ++i)
                                cb[i + k * LFS] = cb[i + j * LFS];
                        ++j, ++k;
                }
                while (k < FS) {
                        for (i = 0; i < FS; ++i)
                                cb[i + k * LFS] = WHITE;
                        ++k;
                }
        }

        /* compute height */
        b = 0;
        for (height = FS; (b == 0) && (--height >= 0);) {
                for (i = 0; (b == 0) && (i < FS); ++i)
                        b = (cb[i + height * LFS] == BLACK);
        }

        /* compute pattern width */
        b = 0;
        for (width = FS; (b == 0) && (--width >= 0);) {
                for (j = 0; (b == 0) && (j < FS); ++j)
                        b = (cb[width + j * LFS] == BLACK);
        }

        /* return fields */
        *w = width + 1;
        *h = height + 1;
}

/*

Copy the bitmap on buffer bm to bmp_cmp, performing top- and
left-justification. Fails if the width or height are larger than
FS (in this case returns 0). Returns 1 when successfull.

*/
int justify_bitmap(unsigned char *bm, int bw, int bh) {
        int b, i, j, bpl;

        bpl = (bw / 8) + ((bw % 8) != 0);

        /* compute left margin */
        b = 0;
        for (i = 0; (b == 0) && (i < bw); ++i) {
                for (j = 0; (b == 0) && (j < bh); ++j)
                        b = pix(bm, bpl, i, j);
        }
        if (b == 0)
                i = 0;
        else
                --i;
        cmp_bmp_dx = i;

        /* compute width */
        b = 0;
        for (cmp_bmp_w = bw; (b == 0) && (--cmp_bmp_w >= 0);) {
                for (j = 0; (b == 0) && (j < bh); ++j)
                        b = pix(bm, bpl, cmp_bmp_w, j);
        }
        cmp_bmp_w -= (cmp_bmp_dx - 1);
        if (cmp_bmp_w > FS)
                return (0);

        /* compute top margin */
        b = 0;
        for (j = 0; (b == 0) && (j < bh); ++j) {
                for (i = 0; (b == 0) && (i < bw); ++i)
                        b = pix(bm, bpl, i, j);
        }
        if (b == 0)
                j = 0;
        else
                --j;
        cmp_bmp_dy = j;

        /* compute height */
        b = 0;
        for (cmp_bmp_h = bh; (b == 0) && (--cmp_bmp_h >= 0);) {
                for (i = 0; (b == 0) && (i < bw); ++i)
                        b = pix(bm, bpl, i, cmp_bmp_h);
        }
        cmp_bmp_h -= (cmp_bmp_dy - 1);
        if (cmp_bmp_h > FS)
                return (0);

        /* copy to cmp_bmp (non-optimized) */
        memset(cmp_bmp, 0, BMS);
        for (i = 0; i < cmp_bmp_w; ++i) {
                for (j = 0; j < cmp_bmp_h; ++j) {
                        if (pix(bm, bpl, i + cmp_bmp_dx, j + cmp_bmp_dy))
                                setpix(cmp_bmp, BLS, i, j);
                }
        }

        /* success */
        return (1);
}

/*

Alloc new pattern.

*/
void new_pattern(void) {
        int i;
        pdesc *s;

        /* enlarge pattern array if necessary */
        if ((topp + 1) >= psz) {
                pattern = g_renew(pdesc, pattern, (psz = (topp + 100)));
        }

        /* alloc memory buffers */
        s = pattern + (++topp);
        s->bs = BMS;
        s->b = g_malloc(s->bs);
        s->s = g_malloc(s->bs);
        s->ts = 0;
        s->tr = NULL;
        s->ds = 0;
        s->d = NULL;
        s->e = -1;
        (s->p)[0] = -1;
        for (i = 1; i < 8; ++i)
                (s->p)[i] = 0;
        s->sw = -1;
        s->bb = s->sb = -1;
        s->pt = 0;
}

/*

Updates a pattern or add a new one. This is the low-level
implementation of update_pattern.

Ideally, this service won't be called by services other than
update_pattern.

On inclusion, the caller must put the pattern bitmap on the cb
buffer. The update operation does not change the current bitmap.

If "new" is 0, do not create a new entry, but updates the cdfc
entry of the pattern array. If "new" is >0, allocate new
pattern. If "new" is <0, then make as when it's >0, but do not
increase topp.

The parameter "an" informs the act number from which the
transliteration comes. The act number on the pattern is
unconditionally overwritten by the update operation.

The parameter mc is the symbol whose associated bitmap is
entering the font. If mc<0, then the pre-existing e field of the
pattern won't be overwritten.

The string tb is the pattern transliteration. If NULL, then the
pattern will be untransliterated (this is not the same as
informing an empty string as transliteration).

*/
void update_pattern_raw(int new, int an, int mc, char *tb, char a,
                        short ft, short fs, int f) {
        pdesc *d;

        /*
           Enlarge array of patterns if necessary, but do not
           increase topp by now, just use the new entry as
           a temporary buffer.
         */
        new_pattern();
        d = pattern + (topp--);

        if (new) {

                /*
                   Copy pattern from cfont to cb. This code is not
                   being used, but it will be when bitmap edition
                   become available.
                 */
                /*
                   memset(cb,WHITE,LFS*FS);
                   for (i=0; i<FS; ++i)
                   for (j=0; j<FS; ++j)
                   cb[i+j*LFS] = cfont[i+j*FS];
                 */

                /* compute pattern height and width */
                justify_pattern(&(d->bh), &(d->bw), NULL, NULL);

                /* copy justified bitmap to buffer b */
                cb2bm(d->b);

                /* compute skeleton of the pattern (currently in cb) */
                skel(-1, -1, 0, 0);

                /* compute skeleton height and width */
                justify_pattern(&(d->sh), &(d->sw), &(d->sx), &(d->sy));

                /* copy skeleton to buffer s */
                cb2bm(d->s);
        }

        else {
                g_free(d->b);
                g_free(d->s);
        }

        /* font size (10pt, 12pt, etc) */
        d->fs = 0;

        /* transliteration */
        if (tb == NULL) {
                d->ts = 0;
                if (d->tr != NULL)
                        g_free(d->tr);
                d->tr = NULL;
        } else {
                d->ts = strlen(tb) + 1;
                g_free(d->tr);
                d->tr = g_strdup(tb);
        }

        /* flags */
        if (f >= 0)
                d->f = f;

        /* act number */
        d->act = an;

        /* alphabet */
        if (a >= 0)
                d->a = a;

        /* initialize other fields .. */
        if (new) {
                int bp, sp;

                /*
                   if (mc >= 0)
                   d->e = mc;
                 */

                /* fields d and ds are deprecated */
                /*
                   d->ds = strlen(pagename) + 1;
                   d->d = c_realloc(NULL,d->ds,NULL);
                   strcpy(d->d,pagename);
                 */

                d->cm = 0;
                snbp(topp + 1, &bp, &sp);
                d->bp = bp;
                d->sp = sp;
        }

        /* .. or copy them from the entry that will be overwritten */
        else {
                d->e = pattern[cdfc].e;
                d->d = pattern[cdfc].d;
                d->ds = pattern[cdfc].ds;
                d->id = pattern[cdfc].id;
                d->cm = pattern[cdfc].cm;
                d->b = pattern[cdfc].b;
                d->bh = pattern[cdfc].bh;
                d->bw = pattern[cdfc].bw;
                d->bp = pattern[cdfc].bp;
                d->s = pattern[cdfc].s;
                d->sh = pattern[cdfc].sh;
                d->sw = pattern[cdfc].sw;
                d->sx = pattern[cdfc].sx;
                d->sy = pattern[cdfc].sy;
                d->sp = pattern[cdfc].sp;
        }

        /* update: overwrite cdfc entry */
        if (new == 0) {
                d = pattern + cdfc;
                if (d->tr != NULL)
                        g_free(d->tr);
                memcpy(pattern + cdfc, pattern + topp + 1, sizeof(pdesc));
                snprintf(mba, MMB, "pattern %d updated", cdfc);
                show_hint(1, mba);
        } else if (new > 0) {
                d->id = ++s_id;
                ++topp;
                snprintf(mba, MMB, "pattern %d added to font", topp);
                tskel_ready = 0;
                show_hint(1, mba);
        }

        /* success message */
        font_changed = 1;
        dw[PATTERN_LIST].rg = 1;
}

/*

Get the index of the symbol k of the current (loaded) page on
the pattern array, or return -1 if this symbol does not belong
to the pattern array.

BUG: expensive search.

*/
/*
int s2p(int k)
{
    int c,j;

    c = -1;
    if (pattern != NULL) {
        for (j=0; j<=topp; ++j)
            if ((pattern[j].e == k) && (strcmp(pattern[j].d,pagename)==0)) {
                c = j;
            }
    }
    return(c);
}
*/

/*

Add or update one pattern. The bitmap is obtained from the symbol
k of the current page. If k<0, then update the cdfc entry.

This function is just a higher-level interface for the
update_pattern function. It obtains the current location of the
symbol on the font (if any), copies its bitmap to the cfont
buffer and calls update_pattern.

The parameter "an" is the act number from which this
transliteration came, or -1.

The parameters a, ft, fs and f are respectively the alphabet, the
font type, the font size and the flags. For any of them, when <0,
the current value of that field won't be touched.

*/
int update_pattern(int k, char *tr, int an, char a, short pt, short fs,
                   int f) {

        /*
           Prepare the symbol for inclusion.
         */
        if (k >= 0) {

                /* the link between pattern and symbol does not exist anymore */
                /*
                   cdfc = s2p(k);
                 */

                cdfc = -1;

                if (cdfc < 0) {

                        /* copy the symbol bitmap to cb */
                        pixel_mlist(k);
                }
        }

        /*
           Prepare the spyhole buffer for inclusion.
         */
        else if (cdfc < 0) {
                spyhole(0, 0, 2, 0);
        }

        /* add or update pattern */
        if (cdfc < 0) {
                update_pattern_raw(1, an, k, tr, a, pt, fs, f);
                cdfc = topp;
        }

        /* update pattern */
        else if (cdfc >= 0)
                update_pattern_raw(0, an, k, tr, a, pt, fs, f);

        /* update the other fields */
        if (cdfc >= 0) {
                if (a >= 0)
                        pattern[cdfc].a = a;
                if (pt >= 0)
                        pattern[cdfc].pt = pt;
                if (fs >= 0)
                        pattern[cdfc].fs = fs;
                if (f >= 0)
                        pattern[cdfc].f = f;
        }

        return (cdfc);
}

/*

Get pattern index from its id.

*/
int id2idx(int id) {
        int l, r, m;

        if (id < 0)
                return (-1);

        /*
           Each pattern is associated with an unique ID. The ID of
           a pattern is always equal or larger to its index in the
           pattern array. So given one ID, we must search the entries
           of the pattern array from 0 until this ID in order to
           find that one associated with the ID.
         */
        l = 0;
        r = (id > topp) ? topp : id;
        while (l <= r) {
                m = l + (r - l) / 2;
                if (pattern[m].id < id)
                        l = m + 1;
                else if (pattern[m].id > id)
                        r = m - 1;
                else
                        return (m);
        }

        db("could not find idx for id=%d (topp=%d)", id, topp);
        return (-1);
}

/*

Remove the pattern n.

Note that n is the index of the pattern, not its ID.

If the pattern has a transliteration, then this transliteration
was originated by a revision act. In this case, the interface
forces the pattern remotion to be preceded by the nullification of
thas act, so there is no need to nullify it here.

*/
void rm_pattern(int n) {
        int i, k, id;
        pdesc *d;

        if ((n < 0) || (topp < n))
                return;

        id = pattern[n].id;

        /*
           Clear the "best match" field for all symbols whose current
           best match is the pattern to remove, and remove their
           SHAPE votes.
         */
        for (k = 0; k <= tops; ++k) {
                if (mc[k].bm == id) {
                        mc[k].bm = -1;
                        mc[k].lfa = -1;
                        rmvotes(SHAPE, k, -1, 0);
                }
        }

        /* free buffers */
        d = pattern + n;
        if (d->b != NULL)
                g_free(d->b);
        if (d->s != NULL)
                g_free(d->s);
        if (d->tr != NULL)
                g_free(d->tr);
        if (d->d != NULL)
                g_free(d->d);
        
        for (i = n; i < topp; ++i) {
                memcpy(pattern + i, pattern + i + 1, sizeof(pdesc));
        }

        --topp;

        font_changed = 1;
        tskel_ready = 0;

        /* must regenerate the pattern-related windows */
        dw[PATTERN].rg = 1;
        dw[PATTERN_LIST].rg = 1;
}

/*

Remove from the pattern all untransliterated patterns.

*/
void rm_untrans(void) {
        int i;

        for (i = 0; i <= topp; ++i)
                if (pattern[i].tr == NULL) {
                        rm_pattern(i);
                        --i;
                }
}

/*

Clear the "cumulative matches" field of all patterns.

*/
void clear_cm(void) {
        int i;

        for (i = 0; i <= topp; ++i) {
                if (pattern[i].tr == NULL)
                        pattern[i].cm = 0;
        }
}

/*

paint pixel accordingly to the current brush

*/
void pp(int i, int j) {

        /* override color buttons */
        if (overr_cb != 0) {
                cfont[i + j * FS] = overr_cb & (C_MASK | D_MASK);
        }

        /* use color buttons */
        else {
                cfont[i + j * FS] = COLOR;
        }
}

/*

recursively fills a region

*/
void pr(int i, int j, int c) {
        if ((i < 0) || (j < 0) || (i >= FS) || (j >= FS))
                return;
        if (cfont[i + j * FS] == c) {
                pp(i, j);
                pr(i - 1, j - 1, c);
                pr(i - 1, j, c);
                pr(i - 1, j + 1, c);
                pr(i, j - 1, c);
                pr(i, j + 1, c);
                pr(i + 1, j - 1, c);
                pr(i + 1, j, c);
                pr(i + 1, j + 1, c);
        }
}

/*

Prepare the patterns.

*/
int prepare_patterns(int reset) {
        static int c, i, j, st;

        /*
           Reset
         */
        if (reset) {
                c = 0;
                st = 1;
                show_hint(3, NULL);
                show_hint(0, "tuning skels");
                if (!tskel_ready)
                        tune_skel_global(1, -1);
                return (1);
        }

        /*
         ** STATE 1 **
         tune skeletons
         */
        if (st == 1) {

                /* finished */
                if (c > topp) {
                        show_hint(0, "computing skeletons..");
                        st = 2;
                        c = 0;
                        return (1);
                }

                /* compute skeleton of pattern c */
                else if (st_auto && (!tskel_ready)) {
                        int r;

                        show_hint(0, "auto-tuning now at %d/%d", c,
                                  topp + 1);

                        /* invalidate current skeleton */
                        pattern[c].sw = -1;

                        /* accumulate per-candidate qualities for pattern c */
                        r = tune_skel_global(0, c);

                        if (r == 0) {
                                spcpy(SP_DEF, SP_GLOBAL);
                        }

                        /*
                           display the skeleton, if requested

                           OOPS.. each pattern is displayed once (using its
                           best parameters), not once per candidate.
                         */
                        if ((r == 0) || get_flag(FL_SHOW_SKELETON_TUNING)) {
                                if ((get_flag(FL_SHOW_SKELETON_TUNING)) &&
                                    (!dw[TUNE_SKEL].v)) {
                                        // UNPATCHED: setview(TUNE_SKEL);
                                }
                                cdfc = c;
                                dw[TUNE_SKEL].rg = 1;
                                dw[TUNE_PATTERN].rg = 1;
                                redraw_document_window();
                        }

                        /* prepare the next */
                        if (++c > topp)
                                tskel_ready = 1;
                }

                else
                        c = topp + 1;
        }

        /*
         ** STATE 2 **
         compute font skeletons
         */
        if (st == 2) {

                /* finished */
                if (c > topp) {
                        show_hint(0, "distributing into types..");
                        st = 3;
                        c = 0;
                        return (1);
                }

                else if (pattern[c].sw < 0) {
                        if (c % DPROG == 0)
                                show_hint(0, "computing skeleton %d/%d", c,
                                          topp);
                        pskel(c);
                }

                ++c;
        }

        /*
         ** STATE 3 **
         distribute into pattern types
         */
        else if (st == 3) {
                int k;
                pdesc *d;

                /*
                   NOT READY. WHEN READY, CHANGE k<=-1 to k<=topp
                 */

                /* compute the pattern types */
                for (k = 0; k <= -1; ++k) {
                        d = pattern + k;

                        if ((d->pt < 0) && (tr_align(d->tr) == 12)) {
                                ptdesc *p;

                                /*
                                   Ignore differences smaller than 20%.

                                   BUG: hardcoded magic number.
                                 */
                                for (i = 0; (i <= toppt) &&
                                     (abs(d->bh - pt[i].xh) >
                                      pt[i].xh / 5); ++i);

                                /* add new font type */
                                if (i > toppt) {

                                        /* enlarge array */
                                        if (++toppt >= ptsz) {
                                                ptsz += (toppt - ptsz) + 10;
                                                pt = g_renew(ptdesc, pt, ptsz);
                                        }

                                        /* initialize fields */
                                        p = pt + toppt;
                                        init_pt(p);
                                        p->xh = d->bh;

                                        /* classify pattern */
                                        d->pt = toppt;

                                }

                                /* classify pattern */
                                else
                                        d->pt = i;

                                font_changed = 1;
                                dw[PATTERN_LIST].rg = 1;
                                dw[PATTERN].rg = 1;
                                dw[PATTERN_TYPES].rg = 1;
                        }
                }

                /* sort the patterns (TODO) */

                st = 4;
                i = 0;
                j = 0;
                show_hint(0, "searching false positives..");
                return (1);
        }

        /*
         ** STATE 4 **
         search false positives
         */
        else if (st == 4) {

                /* always off by now */
                j = i = topp;

                if (++j > topp) {
                        if (++i > topp) {
                                i = 0;
                                j = 0;
                                st = 5;
                                show_hint(0,
                                          "searching unexpected mismatches..");
                                return (1);
                        } else
                                j = i + 1;
                }

                /*
                   compare_patterns(-1,0,bmpcmp_skel);
                   while (compare_patterns(i,j,bmpcmp_skel));
                 */

                {
                        int t;

                        classify(-1, NULL, 0);
                        t = this_pattern;
                        this_pattern = j;
                        while (classify(i, bmpcmp_skel, 2));
                        this_pattern = t;
                }

                /*
                   printf("patterns %d and %d: %d\n",i,j,cp_result);
                 */
        }

        /*
         ** STATE 5 **
         search unexpected mismatches
         */
        else if (st == 5) {
                pdesc *p, *q;

                if (!get_flag(FL_SEARCH_UNEXPECTED_MISMATCHES)) {
                        i = topp;
                }

                if (++j > topp) {
                        if (++i > topp) {
                                i = 0;
                                j = 0;
                                st = 0;
                                show_hint(3, NULL);
                                return (0);
                        } else
                                j = i + 1;
                }

                p = pattern + i;
                q = pattern + j;

                if ((p->pt == q->pt) &&
                    (p->tr != NULL) && (q->tr != NULL) &&
                    (strcmp(p->tr, q->tr) == 0)) {

                        int t;

                        /*
                           compare_patterns(-1,0,bmpcmp_skel);
                           while (compare_patterns(i,j,bmpcmp_skel));
                         */

                        classify(-1, NULL, 0);
                        t = this_pattern;
                        this_pattern = j;
                        while (classify(i, bmpcmp_skel, 2));
                        this_pattern = t;
                }

                /*
                   if (cp_result >= 0)
                   printf("patterns %d and %d: %d\n",i,j,cp_result);
                 */
        }

        /* did not complete */
        return (1);
}

/*

Reset per-pattern skeleton parameters.

*/
void reset_skel_parms(void) {
        int i;

        for (i = 0; i <= topp; ++i)
                pattern[i].p[0] = -1;
}

/*

Add pre-built patterns.

By now this is a first (or second) experiment.

*/
void build_internal_patterns(void) {
        int i, i0, j, l, ph, pw;
        char *p, tr[2];
        

        char *a[] = {

                "    XXXX   ",
                "  XX    X  ",
                " X       X ",
                "          X",
                "          X",
                "    XXXX  X",
                "  XX    X X",
                " X       XX",
                "X         X",
                "X         X",
                " X       XX",
                "  XX    X X",
                "    XXXX  X",
                NULL,

                "X          ",
                "X          ",
                "X          ",
                "X          ",
                "X          ",
                "X          ",
                "X  XXXX    ",
                "X X    XX  ",
                "XX       X ",
                "X        X ",
                "X         X",
                "X         X",
                "X         X",
                "X         X",
                "X         X",
                "X        X ",
                "XX       X ",
                "X X    XX  ",
                "X  XXXX    ",
                NULL,

                "    XXXX   ",
                "  XX    X  ",
                " X       X ",
                " X         ",
                "X          ",
                "X          ",
                "X          ",
                "X          ",
                "X         X",
                " X        X",
                " X       X ",
                "  XX    X  ",
                "    XXXX   ",
                NULL,

                "          X",
                "          X",
                "          X",
                "          X",
                "          X",
                "          X",
                "    XXXX  X",
                "  XX    X X",
                " X       XX",
                " X        X",
                "X         X",
                "X         X",
                "X         X",
                "X         X",
                "X         X",
                " X        X",
                " X       XX",
                "  XX    X X",
                "    XXXX  X",
                NULL,

                "    XXXX   ",
                "  XX    X  ",
                " X       X ",
                " X        X",
                "X         X",
                "XXXXXXXXXXX",
                "X          ",
                "X          ",
                "X         X",
                " X        X",
                " X       X ",
                "  XX    X  ",
                "    XXXX   ",
                NULL,

                NULL
        };

        i = 0;
        tr[0] = 'a';
        tr[1] = 0;

        while (a[i] != NULL) {

                /* compute dimension */
                ph = pw = 0;
                for (j = i; a[j] != NULL; ++j) {
                        l = strlen(a[j]);
                        if (l > pw)
                                pw = l;
                        ++ph;
                }

                /* copy shape to cfont */
                memset(cb, WHITE, FS * LFS);
                for (i0 = i; a[i] != NULL; ++i) {
                        p = a[i];
                        j = 0;
                        for (p = a[i]; *p != 0; ++p, ++j) {
                                if (*p == 'X') {
                                        cb[(i - i0) * LFS + j] = BLACK;
                                }
                        }
                }
                ++i;

                update_pattern_raw(1, -1, -1, tr, LATIN, 0, 10, 0);
                ++(tr[0]);
        }

}
