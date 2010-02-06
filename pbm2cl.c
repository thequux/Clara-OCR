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

pbm2cl: Import PBM

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "common.h"

/*

Each time the active bitmaps array on pbm2bm function becomes
full, its size is increased by AB_INCR.

*/
#define AB_INCR 128

/*

Total memory allocated for bitmaps (to avoid crashes).

*/
int ab_mem;

/*

To avoid crashes, we limit the maximum amount of memory to be
allocated for bitmaps. The segmentation algorithm will alloc too
much memory when the page is too dark. This is not a "bug", but a
"naive method".

*/
void test_ab_mem(void) {
        if (ab_mem > XRES * YRES) {
                if (selthresh) {
                        printf("bookfont size is -1\n");
                }
                fatal(BI, "too dark image?");
        }
}

/* (devel)

Bitmaps
-------

Clara stores bitmaps in a linear array of bytes, following
closely the pbm raw format. The first line of a bitmap with width
w is stored on the first (w/8)+((w%8)!=0) bytes of the array. The
remaining bits (if any) are left blank, and so on. The leftmost
bit on each byte is the most significative one (black, or "on",
is 1, and white, or "off" is 0). An example follows:

       76543210765432
      +--------------+
      |              | 00000000 00000000 =  0   0
      |   XX XXXX    | 00011011 11000000 = 27 192
      |    XX   XX   | 00001100 01100000 = 12  96
      |    XX   XX   | 00001100 01100000 = 12  96
      |    XX   XX   | 00001100 01100000 = 12  96
      |    XX   XX   | 00001100 01100000 = 12  96
      |              | 00000000 00000000 =  0   0
      +--------------+

      stored as: 0 0 27 192 12 96 12 96 12 96 12 96 0 0

Note that the array of bytes that encodes one bitmap does not
contain the bitmap width nor the height. So bitmaps must be
stored together with other data. This is done by structures where
the bitmap is one field and the geometric information is stored
on other fields. There are two such structures: bdesc and
cldesc.

*/

/*

The bdesc structure
-------------------

The bdesc structure is used exclusively by the page.c modulus as
temporary storage to build the closures while reading each line
of the scanned document. The function pbm2bm alloc a temporary
array of these structures (the ab array).

The fields x and y of the bdesc structure store the bitmap
leftmost column and topmost line (document-relative
coordinates). The fields w and h store the width and heigth of
the bitmap. The field s stores the size of the memory buffer
point by the field b (used to store the bitmap itself).

When x<0, the bdesc strucuture is not being used, but the field b
may be pointing to a valid memory buffer. In this case, the field
y may be used to build a free list of descriptors, or to point to
the descritor to which the bitmap was merged (in both cases, the
y field stores an index of ab).

The "join" function invalidates one bitmap after merging it to some
other bitmap (assigning -1 to its x field). However, references
to the invalidated bitmap may remain on arrays ll or kl (defined
and used by pbm2bm), so it's necessary to use the y field to
point to that other bitmap.

*/
typedef struct {
        int x, y, w, h, s;
        unsigned char *b;
} bdesc;

/* mask-to-position */
unsigned char m2p[129];

/*

Computes maximum.

*/
int max(int a, int b) {
        return ((a > b) ? a : b);
}

/*

Computes minimum.

*/
int min(int a, int b) {
        return ((a < b) ? a : b);
}

/*

Computes the index of the byte that contains the pixel b
within a bitmap line.

*/
int byteat(int b) {
        return ((b / 8) + ((b % 8) != 0));
}


/*

Dump bitmap to asc.

The size of the memory area pointed by s must be at least
b->h * (b->w+1) + 1.

This service is a debugging facility.

*/
void bdump(char *s, bdesc *b) {
        int x, y, j;
        unsigned char *p, m;

        m = 128;
        p = b->b;
        j = 0;
        for (y = 0; y < b->h; ++y) {
                for (x = byteat(b->w) * 8; x > 0; --x) {

                        s[j++] = ((*p & m) == m) ? 'X' : '.';
                        if (m == 1) {
                                m = 128;
                                ++p;
                        } else
                                m >>= 1;
                }
                s[j++] = '\n';
        }
        s[j] = 0;
}

/*

Extending bitmaps
-----------------

The function "extend" extends the bitmap b to origin (x,y), width
w and heigth h.

To extend a bitmap means to make its memory buffer larger. The
extended bitmap will be the old bitmap plus a frame. Each pixel
on this frame is initialized to white. The extended bitmap is
defined by the parameters x, y, w and h passed to the function
"extend".

          x         x+w-1
          |           |
        y-+-----------+
          |           |
          |   +-----+ |
          |   |     | |
          |   |     | |
          |   +-----+ |
          |           |
    y+h-1-+-----------+

Note that the scanned document is read on a line-by-line basis,
and the closures are computed along that process. For each line
read, its black pixels that are neighbours of others from
closures that are being computed, are merged with those
closures. The first step to make that merge is to extend the
associated bitmap.

The extension is also the first step to merge two initially
separated closures that become joined along the scan of the
document lines. That happens (for instance) with the character
"u". All their pixels become joined on one closure only when we
read its bottom lines.

Remark: this function is currently not optimized. All operations are
performed on a bit-by-bit basis. They could be performed on a
word-by-word basis, but the code would be more complex.

*/
void extend(bdesc *b, int x, int y, int w, int h) {
        int bl, obl, dx, dy, i, l;
        unsigned char *p, *q;
        unsigned char m, n;

        /* bitmap cannot shrink */
        dx = b->x - x;
        dy = b->y - y;
        if ((dx < 0) || (dy < 0) || (w < b->w) || (h < b->h)) {
                fatal(IE,
                      "bad extend call dx=%d dy=%d w=%d h=%d ow=%d oh=%d",
                      dx, dy, w, h, b->w, b->h);
        }

        /* bytes per line */
        obl = byteat(b->w);
        bl = byteat(w);

        /* enlarge buffer */
        if (b->s < bl * h) {
                int a, ns;

                /* large bitmaps probably will grow more, so enlarge it by at least 10% */
                ns = bl * (h + 10);
                if (ns < (a = b->s + b->s / 10))
                        ns = a;

                /* enlarge */
                ab_mem += ns - b->s;
                test_ab_mem();
                b->s = ns;
                b->b = c_realloc(b->b, b->s, "extend");
        }

        /* clear bottom lines */
        memset(b->b + (dy + b->h) * bl, 0, (h - dy - b->h) * bl);

        /*
           Move each line to its new position. This is
           unnecessary when someone is just adding new
           lines to the bitmap, or when the bitmap
           was "empty" (width 0).
         */
        if ((x < b->x) || (bl > obl) || (b->w == 0)) {

                n = 1;
                q = b->b + (dy + b->h) * bl - 1;
                p = b->b + (b->h * obl - 1);

                if (copy_opt) {

                        for (l = b->h; l > 0; --l) {

                                /* clear additional right margin */
                                if ((i = (bl - obl) * 8 - dx) >= 0) {

                                        while (i >= 8) {
                                                *q = 0;
                                                --q;
                                                i -= 8;
                                        }
                                        n = (1 << i) - 1;
                                        *q &= ~n;
                                        n = (1 << i);
                                        i = 0;

                                        /* prepare mask to move line */
                                        m = 1;
                                }

                                /*
                                   Part of the old right margin will be lost. Prepare
                                   the mask in order to ignore those bits.
                                 */
                                else {
                                        i = -i;
                                        m = ((unsigned) 1) << i;
                                }

                                /* move line l */
                                for (i = obl * 8 - i; i > 0;) {
                                        int a, b, t;
                                        unsigned char mp, mq, v;

                                        if (n > m) {
                                                if ((t = m2p[n]) > i)
                                                        t = i;
                                                mp = ((a =
                                                       m << t) - 1) ^ (m -
                                                                       1);
                                                v = (*p & mp) << (m2p[m] -
                                                                  m2p[n]);
                                        } else {
                                                if ((t = m2p[m]) > i)
                                                        t = i;
                                                mp = ((a =
                                                       m << t) - 1) ^ (m -
                                                                       1);
                                                v = *p & mp;
                                                if (m > n)
                                                        v >>= (m2p[n] -
                                                               m2p[m]);
                                        }

                                        mq = ((b = n << t) - 1) ^ (n - 1);
                                        *q &= ~mq;
                                        *q |= v;

                                        i -= t;
                                        if (b == 256) {
                                                n = 1;
                                                --q;
                                        } else
                                                n = b;
                                        if (a == 256) {
                                                m = 1;
                                                --p;
                                        } else
                                                m = a;
                                }

                                /* clear left margin */
                                if (dx > 0) {

                                        i = dx - m2p[n];
                                        --n;
                                        *q &= n;
                                        --q;

                                        for (i = dx; i > 8; i -= 8) {
                                                *q = 0;
                                                --q;
                                        }
                                        n = 1;
                                }
                        }
                }

                /* bit-a-bit operation */
                else {

                        for (l = b->h; l > 0; --l) {

                                /* clear additional right margin */
                                if ((i = (bl - obl) * 8 - dx) >= 0) {
                                        for (; i > 0; --i) {
                                                *q &= ~n;
                                                if (n == 128) {
                                                        n = 1;
                                                        --q;
                                                } else
                                                        n <<= 1;
                                        }

                                        /* prepare mask to move line */
                                        m = 1;
                                }

                                /*
                                   Part of the old right margin will be lost. Prepare
                                   the mask in order to ignore those bits.
                                 */
                                else {
                                        i = -i;
                                        m = ((unsigned) 1) << i;
                                }

                                /* move line l */
                                for (i = obl * 8 - i; i > 0; --i) {
                                        if ((*p & m) == m)
                                                *q |= n;
                                        else
                                                *q &= ~n;
                                        if (n == 128) {
                                                n = 1;
                                                --q;
                                        } else
                                                n <<= 1;
                                        if (m == 128) {
                                                m = 1;
                                                --p;
                                        } else
                                                m <<= 1;
                                }

                                /* clear left margin */
                                for (i = dx; i > 0; --i) {
                                        *q &= ~n;
                                        if (n == 128) {
                                                n = 1;
                                                --q;
                                        } else
                                                n <<= 1;
                                }
                        }
                }
        }

        /*

           If new lines were added to the top, the bitmap must
           be moved down.

         */
        else if ((y < b->y) && (b->w > 0)) {

                memmove(b->b + dy * bl, b->b, b->h * obl);
        }

        /* clear top lines */
        if (dy > 0)
                memset(b->b, 0, dy * bl);

        /* new bitmap parameters */
        b->x = x;
        b->y = y;
        b->w = w;
        b->h = h;
}

/*

Merging bitmaps
---------------

The function "bitmerge" merges the bitmap c into the bitmap b.

To merge the bitmap c into b means extend b to include c, and
copy the pixels of c to the extended bitmap. Note that bits on
areas not on b or c were initialized to white by the procedure
that extended b. The function "bitmerge" implements the operation
of merge bitmaps.

       x1          x2
       |           |
    y1-+-----+-----+
       |     |     |
       |     |  c  |
       +-----+--+  |
       |     +--+--+
       |        |  |
       |   b    |  |
       |        |  |
    y2-+--------+--+

*/
void bitmerge(bdesc *b, bdesc *c) {
        int x1, x2, y1, y2, w, h, i, bl, pbl, l;
        unsigned char *p, *p0, *q, m, m0, n;

        /* Compute the coordinates of the merge */
        x1 = min(b->x, c->x);
        y1 = min(b->y, c->y);
        x2 = max(b->x + b->w - 1, c->x + c->w - 1);
        y2 = max(b->y + b->h - 1, c->y + c->h - 1);

        /* compute width and heigth of the merge */
        w = x2 - x1 + 1;
        h = y2 - y1 + 1;

        /* extend b */
        if ((w > b->w) || (h > b->h)) {

                /*
                   large bitmaps probably will continue growing, so add
                   FS bits on each side.
                 */
                if (w > 5 * FS) {
                        if (x1 > FS)
                                x1 -= FS;
                        else
                                x1 = 0;
                        if (x2 += FS >= XRES)
                                x2 = XRES - 1;
                        w = x2 - x1 + 1;
                }

                /* now extend */
                extend(b, x1, y1, w, h);
        }

        /* bytes per line */
        bl = byteat(b->w);
        pbl = byteat(c->x - x1);

        /* copy c to the extended bitmap */
        n = 128;
        q = c->b;
        m0 = ((unsigned) 1) << (7 - ((c->x - x1) % 8));
        p0 = b->b + (c->y - y1) * bl + (c->x - x1) / 8;

        for (l = 0; l < c->h; ++l) {

                m = m0;
                p = p0;
                p0 += bl;

                /* copy line l */
                for (i = byteat(c->w) * 8; i > 0; --i) {
                        if ((*q & n) == n)
                                *p |= m;
                        if (m == 1) {
                                m = 128;
                                ++p;
                        } else
                                m >>= 1;
                        if (n == 1) {
                                n = 128;
                                ++q;
                        } else
                                n >>= 1;
                }
        }
}

/*

Make *p and *q be the same active bitmap index, merging
*q to *p (or *p to *q) if necessary.

This function exists only to simplify a loop on pbm2bm (take a
look on the calls).

*/
void join(int *p, int *q, bdesc *ab) {

        /*
           A previous step on the current line may have
           invalidated the *q bitmap (may happen on "n").
         */
        while ((*q >= 0) && (ab[*q].x < 0))
                *q = ab[*q].y;

        if (*q >= 0) {
                if ((*p >= 0) && (*p != *q)) {
                        if (ab[*q].x >= 0) {
                                int old;

                                /*
                                   merge the smaller to the larger.
                                 */
                                if (ab[*p].s >= ab[*q].s) {
                                        bitmerge(ab + *p, ab + *q);
                                        ab[*q].x = -1;
                                        ab[*q].y = *p;
                                        old = *q;
                                        *q = *p;
                                } else {
                                        bitmerge(ab + *q, ab + *p);
                                        ab[*p].x = -1;
                                        ab[*p].y = *q;
                                        old = *p;
                                        *p = *q;
                                }

                                /* free large unused memory buffers */
                                if (ab[old].s > 64000) {
                                        c_free(ab[old].b);
                                        ab[old].b = NULL;
                                        ab[old].s = 0;
                                }
                        } else
                                *q = *p;
                } else
                        *p = *q;
        }
}

/*

Create a new closure and the associated symbol.

(this service was obsoleted by new_cl)

*/
void new_cl_global(int x, int y, int w, int h, void *b) {
        int t;
        unsigned char m;
        int i, k;

        /* must enlarge allocated area for cl */
        checkcl(++topcl);

        /* initialize the new entry */
        cl[topcl].nbp = 0;
        cl[topcl].l = x;
        cl[topcl].r = x + w - 1;
        cl[topcl].t = y;
        cl[topcl].b = y + h - 1;

        t = byteat(w) * h;
        cl[topcl].bm = c_realloc(NULL, t, "new_cl");
        memcpy(cl[topcl].bm, b, t);

        cl[topcl].sup = c_realloc(NULL, MAXSUP * sizeof(int), NULL);
        cl[topcl].sup[0] = -1;
        cl[topcl].supsz = MAXSUP;

        /* seed and number of black pixels */
        cl[topcl].nbp = 0;
        for (i = 0; i < t; ++i) {
                m = *(((unsigned char *) b) + i);
                for (k = 0; m > 0; ++k) {
                        if (m & 128) {
                                if (++(cl[topcl].nbp) == 1) {
                                        cl[topcl].seed[0] = i * 8 + k + x;
                                        cl[topcl].seed[1] = y;
                                }
                        }
                        m <<= 1;
                }
        }

        /* create unitary symbol */
        C_SET(mc[new_mc(&topcl, 1)].f, F_ISP);
}

/*

Create a new closure and the associated symbol.

*/
void new_cl(int x, int y, int w, int h, void *b) {
        int t, ct;
        unsigned char m, n, *p, *q;
        int i, j;
        int tm, lm, bm, rm, cw, ch;

        /* must enlarge allocated area for cl */
        checkcl(++topcl);

        /*
           Compute the topmost, bottommost, leftmost and rightmost
           coordinates, number of black pixels and locate a seed.
         */
        cl[topcl].nbp = 0;
        t = byteat(w);
        tm = h;
        bm = -1;
        lm = w;
        rm = -1;
        for (j = 0; j < h; ++j) {
                m = 128;
                p = ((unsigned char *) b) + j * t;
                for (i = 0; i < w; ++i) {
                        if (*p & m) {

                                /* adjust limits as required */
                                if (i < lm)
                                        lm = i;
                                if (i > rm)
                                        rm = i;
                                if (j < tm)
                                        tm = j;
                                if (j > bm)
                                        bm = j;

                                /* take (i,j) as seed if it's the first black pixel found */
                                if (++(cl[topcl].nbp) == 1) {
                                        cl[topcl].seed[0] = x + i;
                                        cl[topcl].seed[1] = y + j;
                                }
                        }
                        if ((m >>= 1) == 0) {
                                ++p;
                                m = 128;
                        }
                }
        }

        /* computed geometry */
        cw = rm - lm + 1;
        ch = bm - tm + 1;

        /* alloc memory buffer to the closure bitmap */
        ct = byteat(cw);
        cl[topcl].bm = c_realloc(NULL, ct * ch, "new_cl");

        /*
           In some cases, the bitmap must be copied line
           by line.
         */
        if ((lm > 0) || (tm > 0) || (cw < w) || (ch < h)) {
                memset(cl[topcl].bm, 0, ct * ch);
                for (j = 0; j < ch; ++j) {
                        p = ((unsigned char *) b) + (j + tm) * t +
                            (lm / 8);
                        m = 1 << (7 - (lm % 8));
                        q = ((unsigned char *) cl[topcl].bm) + j * ct;
                        n = 128;
                        for (i = 0; i < cw; ++i) {
                                if (*p & m) {
                                        *q |= n;
                                }
                                if ((m >>= 1) == 0) {
                                        ++p;
                                        m = 128;
                                }
                                if ((n >>= 1) == 0) {
                                        ++q;
                                        n = 128;
                                }
                        }
                }
        }

        /* simple case: copy the bitmap using memcpy */
        else {
                memcpy(cl[topcl].bm, b, ct * ch);
        }

        /* initialize the new entry */
        cl[topcl].l = x + lm;
        cl[topcl].r = x + rm;
        cl[topcl].t = y + tm;
        cl[topcl].b = y + bm;
        cl[topcl].sup = c_realloc(NULL, MAXSUP * sizeof(int), NULL);
        cl[topcl].sup[0] = -1;
        cl[topcl].supsz = MAXSUP;

        /* create unitary symbol */
        int nsym = new_mc(&topcl, 1);
        C_SET(mc[nsym].f, F_ISP);
}

/*

Search next interior pixel and apply the spyhole to extract the
closure.

*/
int find_thing(int reset, int x, int y) {
        static int i, j;
        int u, v, k, l, f, r;

        if (reset) {
                i = j = 0;
                return (1);
        }

        if (x < 0) {
                u = i;
                v = j;
        } else {
                u = x;
                v = y;
        }

        /* try to find and extract one closure */
        for (f = 0; (f == 0) && (v < YRES);) {

                /* (u,v) is not black enough */
                r = (pixmap[u + v * XRES] > thresh_val);

                if (!r) {

                        /* got one? yes! */
                        if (spyhole(u, v, 4, thresh_val)) {

                                /* copy the result to cmp_bmp */
                                spyhole(0, 0, 3, 0);

                                /* abagar: display the seed and dump the spyhole buffer */
                                if (abagar) {
                                        printf("seed of topcl: (%d,%d)\n", u, v);
                                        bm2cb((unsigned char *) cmp_bmp, 0, 0);
                                        dump_cb();
                                }

                                /* add as new closure */
                                new_cl(cmp_bmp_x0, cmp_bmp_y0, FS, FS, cmp_bmp);

                                /* remove from the pixmap */
                                spyhole(0, 0, 1, 0);
                                for (k = 0; k < FS; ++k) {
                                        for (l = 0; l < FS; ++l) {
                                                if (pix(cmp_bmp, BLS, k, l)) {
                                                        pixmap[(l + cmp_bmp_y0) * XRES + k + cmp_bmp_x0]
                                                            = 255;
                                                }
                                        }
                                }

                                /* get out from the loop */
                                f = 1;
                        }

                        /*
                           spyhole() disagreed with pbm2bm(). So one of them
                           is probably buggy.
                         */
                        else {
                                db("hey, what's running on at %d %d?\n", u, v);
                        }
                }

                /* clear it */
                else {
                        pixmap[u + v * XRES] = 255;
                }

                /* prepare the next position */
                if (++u >= XRES) {
                        ++v;
                        u = 0;
                }

                /*
                   the caller is supposed to inform a true closure seed.
                 */
                if ((!f) && (x > 0)) {
                        db("this caller got drunk!");
                }
        }

        show_hint(1, "now at line %d, total %d closures", j, topcl + 1);

        /* remember position */
        i = u;
        j = v;

        return (f);
}

/*

Read pbm file and convert its closures to bitmaps.

Some folks may think this function is a stupid thing because it
makes very hard to add support to other graphic formats.

Anyway, it's design has two good properties. The first is that it
reads the bitmap only once, so one could run a format conversion
tool (something-to-pbm) on-the-fly and read its output from
standard input. The second is that it's very economic in terms of
memory allocation.

*/
int pbm2bm(char *f, int reset) {
        /* to read the file */
        static FILE *F;
        static int pio;

        /* buffer and mask for document lines */
        static unsigned char *s;

        /* width and heigth in pixels, line counter and lenght */
        static int w, h, n, bl;

        /*
           ab is the array of active bitmaps (the bitmaps currently
           being constructed by the loop that reads the document, line
           by line, and searches black pixels). The current size of the
           array ab is abs.
         */
        static bdesc *ab;
        static int abs;

        /*
           kl[i] points to the bitmap that contains the pixel on column
           i of the current line (if that pixel is white, then kl[i] is
           -1). ll[i] is the same, but for the previous line.
         */
        static int *kl, *ll;

        /* account pixels eaten by neighbours */
        static int eaten;

        if (reset) {

                int c, rc;

                /* no byte by now */
                ab_mem = 0;

                /* mask-to-position */
                m2p[1] = 8;
                m2p[2] = 7;
                m2p[4] = 6;
                m2p[8] = 5;
                m2p[16] = 4;
                m2p[32] = 3;
                m2p[64] = 2;
                m2p[128] = 1;

                F = zfopen(f, "r", &pio);

                /* P4 magic number */
                if ((zfgetc(F) != 'P') || (zfgetc(F) != '4')) {
                        fatal(DI, "%s is not a pbm raw file", f);
                }

                /*
                   Reading loop. PBM defines "whitespace" as any from space,
                   tab, CR or LF. Comments start with '#' and end on LF.
                 */
                for (n = rc = 0, w = h = 0;
                     (n < 4) && ((c = zfgetc(F)) != EOF);) {

                        /* reading comment */
                        if ((c == '#') || ((rc == 1) && (c != '\n'))) {
                                rc = 1;
                        }

                        /* non-"whitespace" */
                        else if ((c != ' ') && (c != '\t') && (c != '\r')
                                 && (c != '\n')) {

                                /* invalid char */
                                if ((c < '0') || ('9' < c)) {
                                        fatal(DI,
                                              "%s is not a pbm raw file",
                                              f);
                                }

                                /* reading width */
                                if (n <= 1) {
                                        n = 1;
                                        w = w * 10 + c - '0';
                                }

                                /* reading heigth */
                                else {
                                        n = 3;
                                        h = h * 10 + c - '0';
                                }
                        }

                        /* "whitespace" character */
                        else {

                                /* stop reading width */
                                if (n == 1)
                                        n = 2;

                                /* stop reading heigth */
                                else if (n == 3)
                                        n = 4;

                                /* stop reading comment */
                                rc = 0;
                        }
                }

                /* alloc line buffers */
                bl = byteat(w);
                s = c_realloc(NULL, bl, "pbm2bm");
                ll = c_realloc(NULL, (w + 2) * sizeof(int), "pbm2bm");
                for (n = 0; n < w + 2; ++n)
                        ll[n] = -1;
                kl = c_realloc(NULL, (w + 2) * sizeof(int), "pbm2bm");
                kl[0] = kl[w + 1] = -1;
                ab = NULL;
                abs = 0;

                /* resolution */
                XRES = w;
                YRES = h;

                /* check for minimum page size */
                /*
                   if ((XRES < MHR) || (YRES < MVR)) {
                   fatal(DI,"oops.. document %dx%d too small\n",XRES,YRES);
                   }
                 */

                /* prepare reading loop and return */
                n = 0;
                eaten = 0;
                return (1);
        }

        /*
           Loop to read lines and compute closures. After the
           last line, simulates a blank line to close all
           remaining bitmaps.
         */
        {
                unsigned char *q, m;
                int i, j, la, k;

                if ((batch_mode == 0) && ((n % DPROG) == 0)) {
                        snprintf(mba, MMB,
                                 "now reading line %d/%d of file %s", n, h,
                                 pagebase);
                        show_hint(0, mba);
                }

                /* read one line from the document */
                if (n < h) {
                        if (zfread(s, 1, bl, F) < bl) {
                                fatal(IO, "error reading file \"%s\"", f);
                        }
                } else
                        memset(s, 0, bl);

                /* build list of free bitmaps */
                for (i = j = 0, la = -1; i < abs; ++i) {
                        if (ab[i].x < 0) {
                                if (la < 0)
                                        la = j = i;
                                else {
                                        ab[j].y = i;
                                        ab[i].y = -1;
                                        j = i;
                                }
                        }
                }

                /* for each pixel on the current line */
                for (q = s, m = 128, i = 1; i <= w; ++i) {

                        /* current pixel points to no bitmap by now */
                        kl[i] = -1;

                        /* black pixels must be joined with neighbours */
                        if ((*q & m) == m) {

                                join(kl + i, kl + i - 1, ab);
                                join(kl + i, ll + i - 1, ab);
                                join(kl + i, ll + i, ab);
                                join(kl + i, ll + i + 1, ab);

                                /* no neighbour! must create a new bitmap */
                                if (kl[i] < 0) {

                                        /* must enlarge the array of bitmaps */
                                        if (la < 0) {
                                                ab_mem +=
                                                    AB_INCR *
                                                    sizeof(bdesc);
                                                test_ab_mem();
                                                ab = c_realloc(ab,
                                                               (abs +
                                                                AB_INCR) *
                                                               sizeof
                                                               (bdesc),
                                                               "pbm2bm");
                                                for (j = 0; j < AB_INCR;
                                                     ++j) {
                                                        ab[abs + j].x = -1;
                                                        ab[abs + j].y =
                                                            (j ==
                                                             AB_INCR -
                                                             1) ? -1 : abs
                                                            + j + 1;
                                                        ab[abs + j].b =
                                                            NULL;
                                                        ab[abs + j].s = 0;
                                                }
                                                la = abs;
                                                abs += AB_INCR;
                                        }

                                        /* get the next free bitmap */
                                        j = la;
                                        la = ab[la].y;

                                        /* create an "empty" bitmap */
                                        kl[i] = j;
                                        ab[j].x = i - 1;
                                        ab[j].y = n;
                                        ab[j].w = 0;
                                        ab[j].h = 0;
                                        if (ab[j].b == NULL) {
                                                ab[j].b =
                                                    c_realloc(NULL,
                                                              ab[j].s =
                                                              FS * FS /
                                                              (4 * 8),
                                                              "pbm2bm");
                                        }
                                        memset(ab[j].b, 0, ab[j].s);
                                }
                        }

                        /* prepare mask to next pixel on current line */
                        if (m == 1) {
                                m = 128;
                                ++q;
                        } else
                                m >>= 1;
                }

                /* change remaining references to merged bitmaps */
                for (i = 1; i <= w; ++i) {
                        while (((j = ll[i]) >= 0) && (ab[j].x < 0))
                                ll[i] = ab[j].y;
                        while (((j = kl[i]) >= 0) && (ab[j].x < 0))
                                kl[i] = ab[j].y;
                }

                /* mark valid bitmaps on the previous line */
                for (i = 1; i <= w; ++i) {
                        if (((j = ll[i]) >= 0) && (ab[j].x >= 0) &&
                            (ab[j].y >= 0)) {
                                ab[j].y = -ab[j].y - 1;
                        }
                }

                /*
                   Unmark bitmaps that reach the current line.
                 */
                for (i = 1; i <= w; ++i) {
                        if (((j = kl[i]) >= 0) && (ab[j].y < 0)) {
                                ab[j].y = -ab[j].y - 1;
                        }
                }

                /* now visit the exhausted bitmaps */
                for (i = 1; i <= w; ++i) {
                        if (((j = ll[i]) >= 0) && (ab[j].x >= 0) &&
                            (ab[j].y < 0)) {

                                /* remove mark */
                                ab[j].y = -ab[j].y - 1;

                                /* create the closure */
                                if (localbin &&
                                    (ab[j].w < FS) &&
                                    (ab[j].h < FS) &&
                                    (ab[j].x > FS) &&
                                    ((XRES - ab[j].x) > FS) &&
                                    (ab[j].y > FS) &&
                                    ((YRES - ab[j].y) > FS)) {

                                        int f, u, v, t, m, c;
                                        unsigned char *p;

                                        f = 0;
                                        c = 0;

                                        /*
                                           search a bitmap seed and call find_thing.

                                           BUG (dropped fragments): when find_thing() expands
                                           one closure from the seed, the closure may not
                                           cover the entire bitmap computed above. The reason
                                           seems to be the choice of a threshold lower than
                                           thresh_val by spyhole().

                                           In this case, the remaining portions should
                                           be used to create other closures, but this is
                                           not easy. It's easy to detect a
                                           remaining bit, but it's hard to detect a
                                           "large enough" remaining component. Calling
                                           find_thing() for all remaining bits is a
                                           too expensive operation.
                                         */
                                        t = byteat(ab[j].w);
                                        for (v = 0;
                                             (f == 0) && (v < ab[j].h);
                                             ++v) {
                                                m = 128;
                                                p = ((unsigned char *)
                                                     ab[j].b) + v * t;
                                                for (u = 0;
                                                     (f == 0) &&
                                                     (u < ab[j].w); ++u) {
                                                        if (*p & m) {
                                                                int x, y;

                                                                x = u + ab [j].x;
                                                                y = v + ab [j].y;

                                                                if (get_flag(FL_ABAGAR_ACTIVE) && (zones > 0) && (limits[0] <= x) && (x <= limits[6]) && (limits[1] <= y) && (y <= limits[3])) {

                                                                        warn("ABAGAR at (%d,%d)\n");
                                                                        abagar = 1;
                                                                } else
                                                                        abagar = 0;

                                                                if (pixmap [x + y * XRES] <= thresh_val) {
                                                                        find_thing(0, x, y);
                                                                        ++c;
                                                                } else {

                                                                        /*
                                                                           The pixel (x,y) is set in ab[j], but
                                                                           cleared in pixmap. The reason is: one
                                                                           bitmap from the line y-1 grow using
                                                                           a high threshold and ate (x,y). Just
                                                                           account.
                                                                         */
                                                                        eaten++;
                                                                }
                                                                f = 1;
                                                        } else if ((m >>= 1) == 0) {
                                                                ++p;
                                                                m = 128;
                                                        }
                                                }
                                        }

                                        /*
                                           Search (globally) a bitmap seed and call find_thing.
                                           This code is buggy because searching the entire page
                                           may get as seed a pixel from another symbol. But the
                                           code is being maintained for eventual tests.
                                         */
                                        /*
                                           for (u=ab[j].w; (f==0) && (u>0); --u) {
                                           for (v=ab[j].h; (f==0) && (v>0); --v) {
                                           if (pixmap[(u+ab[j].x)+(v+ab[j].y)*XRES] < thresh_val) {
                                           find_thing(NULL,0,u+ab[j].x,v+ab[j].y);
                                           f = 1;
                                           }
                                           }
                                           }
                                         */
                                }

                                /*
                                   BUG: when the page contains symbols close to the
                                   page boundaries, new_cl is called directly even if
                                   local binarizations was chosen. So simple
                                   thresholding will be applied and the pixels won't
                                   be removed from the pixmap.
                                 */
                                else {
                                        new_cl(ab[j].x, ab[j].y, ab[j].w,
                                               ab[j].h, ab[j].b);
                                }

                                /* mark ab[j] as unused */
                                ab[j].x = -1;
                                ab[j].y = 0;
                        }
                }

                /*
                   Add to each active bitmap its pixels that we found
                   on the current line.
                 */
                memcpy(ll, kl, (w + 2) * sizeof(int));
                for (i = 1; i <= w; ++i) {
                        if ((k = kl[i]) >= 0) {
                                unsigned char *p, m;
                                int l;

                                /*
                                   Search the rightmost pixel
                                   of the bitmap kl[i] on the current line.
                                 */
                                for (j = w; (j > i) && (kl[j] != k); --j);

                                /*
                                   Now we must extend the bitmap kl[i] before adding
                                   to it its pixels that we just found on the current
                                   line. That means to make sure that the bitmap
                                   may receive pixels from absolute columns i-1 to j-1.

                                   In the first two cases below, i-1 becames the new x
                                   leftmost coordinate. In the first and third cases,
                                   the bitmap is extended regarding its rightmost column.
                                   In all cases, the heigth is increased by 1.
                                 */
                                if (i - 1 < ab[k].x)
                                        if ((j - 1) - ab[k].x + 1 >
                                            ab[k].w)
                                                extend(ab + k, i - 1,
                                                       ab[k].y,
                                                       (j - 1) - (i - 1) +
                                                       1, ab[k].h + 1);
                                        else
                                                extend(ab + k, i - 1,
                                                       ab[k].y,
                                                       ab[k].w + ab[k].x -
                                                       (i - 1),
                                                       ab[k].h + 1);
                                else if ((j - 1) - ab[k].x + 1 > ab[k].w)
                                        extend(ab + k, ab[k].x, ab[k].y,
                                               (j - 1) - ab[k].x + 1,
                                               ab[k].h + 1);
                                else
                                        extend(ab + k, ab[k].x, ab[k].y,
                                               ab[k].w, ab[k].h + 1);

                                /*
                                   add to the bitmap kl[i] its pixels that we
                                   found on the current line.
                                 */
                                p = ab[k].b + (n -
                                               ab[k].y) * byteat(ab[k].w) +
                                    (i - 1 - ab[k].x) / 8;
                                m = ((unsigned) 1) << (7 -
                                                       ((i - 1 -
                                                         ab[k].x) % 8));
                                for (l = i; l <= j; ++l) {
                                        if (kl[l] == k) {
                                                *p |= m;
                                                kl[l] = -1;
                                        }
                                        if (m == 1) {
                                                m = 128;
                                                ++p;
                                        } else
                                                m >>= 1;
                                }
                        }
                }
        }

        /* finished */
        if (++n > h) {

                /* free all buffers */
                c_free(s);
                c_free(ll);
                c_free(kl);
                if (ab != NULL) {
                        while (--abs >= 0)
                                if (ab[abs].b != NULL) {
                                        free(ab[abs].b);
                                }
                        c_free(ab);
                }

                snprintf(mba, MMB, "finished reading file %s", pagename);
                show_hint(1, mba);

                zfclose(F, pio);

                return (0);

        }

        /* unfinished, must continue */
        return (1);
}

/*

This is a more general version of add_closure.

*/
void clip_cl(unsigned char *rw, int bpl, cldesc *y, int l, int r, int t,
             int b, int RWX, int RWY) {
        int ia, ib, ja, jb;
        int i, j;
        unsigned char *p;
        unsigned n;

        /*

           Now we need to compute the values of i and j for which
           (l+i,t+j) is a point of the closure k and of
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
                ib = RWX - 1;
        else
                ib = y->r - l;
        if (y->t <= t)
                ja = 0;
        else
                ja = y->t - t;
        if (y->b >= b)
                jb = RWY - 1;
        else
                jb = y->b - t;

        /* copy the sampled pixels to the buffer */
        for (i = ia; i <= ib; ++i) {
                for (j = ja; j <= jb; ++j) {

                        p = rw + j * bpl + i / 8;
                        n = ((unsigned) 1) << (7 - i % 8);
                        if (pixel(y, l + i, t + j) == BLACK)
                                *p |= n;
                }
        }
}

/*

Write the first zone (or the entire page) to a raw pbm or pgm file.

*/
int wrzone(char *s1, int fz) {
        int l, r, t, b, w, h;
        int k, bpl;
        unsigned char g[200];
        unsigned char *rw;
        int RWX, RWY;

        if (s1 != NULL) {
                struct stat buf;

                if (stat(s1, &buf) == 0) {
                        show_hint(2,
                                  "write zone failure: a file %s already exists",
                                  s1);
                        return (0);
                }
        }

        /* compute the smallest rectangle that contains the first zone */
        if (fz) {
                l = limits[0];
                if (l > limits[2])
                        l = limits[2];
                r = limits[6];
                if (r < limits[4])
                        r = limits[4];
                t = limits[1];
                if (t > limits[7])
                        r = limits[7];
                b = limits[5];
                if (b < limits[3])
                        b = limits[3];
        }

        /* get the limits of the entire page */
        else {
                l = t = 0;
                r = XRES - 1;
                b = YRES - 1;
        }

        w = r - l + 1;
        h = b - t + 1;

        /* save rectangle directly as PGM */
        if ((pixmap != NULL) && (pm_t == 8) && (cl_ready == 0)) {
                int F, j;

                F = open(s1, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                sprintf((char *) g, "P5\n%d %d\n255\n", w, h);
                write(F, g, strlen((char *) g));
                for (j = 0; j < h; ++j) {
                        write(F, pixmap + (t + j) * XRES + l, w);
                }
                close(F);
                show_hint(2, "%s successfully written", s1);

                return (1);
        }

        /* alloc memory */
        RWX = r - l + 1;
        RWY = b - t + 1;
        bpl = (RWX / 8) + ((RWX % 8) != 0);
        rw = c_realloc(NULL, RWY * bpl, NULL);
        memset(rw, 0, RWY * bpl);

        /*
           By-closure strategy.
         */
        if (0) {
                cldesc *y;

                /* list the closures that intersect the rectangle (l,r,t,b) */
                list_cl(l, t, r - l + 1, b - t + 1, 0);

                /* clip and copy each such closure */
                for (k = 0; k < list_cl_sz; ++k) {

                        y = cl + list_cl_r[k];
                        clip_cl(rw, bpl, y, l, r, t, b, RWX, RWY);
                }
        }

        /*
           By-symbol strategy. Look for symbols that intersect
           the rectangle (l,r,t,b)
         */
        else {
                int i;
                int ia, ib, ja, jb;
                sdesc *y;

                /* list the symbols that intersect the rectangle (l,r,t,b) */
                list_s(l, t, r - l + 1, b - t + 1);

                /* clip and copy each such symbol */
                for (k = 0; k < list_s_sz; ++k) {
                        int j;

                        y = mc + (j = list_s_r[k]);

                        /* prefer the pattern */
                        if (mc[j].bm >= 0) {

                                /*

                                   Now we need to compute the values of i and j for which
                                   (l+i,t+j) is a point of the closure k and of
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
                                        ib = RWX - 1;
                                else
                                        ib = y->r - l;
                                if (y->t <= t)
                                        ja = 0;
                                else
                                        ja = y->t - t;
                                if (y->b >= b)
                                        jb = RWY - 1;
                                else
                                        jb = y->b - t;

                                /* copy the sampled pixels to the buffer */
                                for (i = ia; i <= ib; ++i) {
                                        for (j = ja; j <= jb; ++j) {
                                                int li, lj;
                                                unsigned char *p;
                                                unsigned n;

                                                p = rw + j * bpl + i / 8;
                                                n = ((unsigned) 1) << (7 -
                                                                       i %
                                                                       8);

                                                li = l + i - mc[k].l;
                                                lj = t + j - mc[k].t;
                                                if ((0 < li) && (li < FS)
                                                    && (0 < lj) &&
                                                    (lj < FS)) {
                                                        if (pix
                                                            (pattern
                                                             [mc[k].bm].b,
                                                             FS / 8, li,
                                                             lj))
                                                                *p |= n;
                                                }
                                        }
                                }
                        }

                        /* dump the symbol on a per-closure basis */
                        else {
                                int n;

                                printf("symbol %d: closures\n", j);

                                for (n = 0; n < y->ncl; ++n) {
                                        clip_cl(rw, bpl, cl + (y->cl)[n],
                                                l, r, t, b, RWX, RWY);
                                }
                        }
                }
        }

        /* create pbm file */
        if (s1 != NULL) {
                int F;

                F = open(s1, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                sprintf((char *) g, "P4\n%d %d\n", RWX, RWY);
                write(F, g, strlen((char *) g));
                write(F, rw, RWY * bpl);
                close(F);
                show_hint(2, "%s successfully written", s1);
        }

        c_free(rw);
        return (1);
}
