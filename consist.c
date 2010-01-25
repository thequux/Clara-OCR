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

consist.c: Consistency tests for all data structures.

*/

#include <stdio.h>
#include "common.h"

/*

Consist closure c.

*/
int cl_cons(int c) {
        cldesc *m;
        int i, f;

        /* invalid closure index */
        if ((c < 0) || (topcl < c)) {
                show_hint(2, "closure %d does not exist", c);
                return (1);
        }

        /* consist closure parameters */
        m = cl + c;
        if ((m->l < 0) || (m->r >= XRES) || (m->t < 0) || (m->b >= YRES) ||
            (m->nbp < 0) ||
            (m->nbp > (m->r - m->l + 1) * (m->b - m->t + 1)) ||
            (m->seed[0] < m->l) || (m->seed[0] > m->r) ||
            (m->seed[1] < m->t) || (m->seed[1] > m->b)) {

                show_hint(2, "closure %d inconsistent", c);
                return (1);
        }

        /* consist indexes of symbols */
        for (i = f = 0; m->sup[i] >= 0; ++i) {
                if (tops < m->sup[i])
                        f = 1;
        }
        if (f != 0) {
                show_hint(2, "closure %d points to inexistent symbol", c);
                return (1);
        }

        return (0);
}

/*

Consist symbol c.

*/
int s_cons(int c) {
        sdesc *s;
        int i;

        s = mc + c;

        if (s->ncl <= 0) {
                show_hint(2, "invalid number of closures on symbol %d", c);
                return (1);
        }

        else if (s->cl == NULL) {
                show_hint(2, "absent list of closures on symbol %d", c);
                return (1);
        }

        else {
                for (i = 0; i < s->ncl; ++i)
                        if ((s->cl[i] < 0) || (s->cl[i] > topcl)) {
                                show_hint(2,
                                          "invalid closures on symbol %d",
                                          c);
                                return (1);
                        }
        }

        if ((s->l < 0) || (s->l >= XRES) ||
            (s->r < 0) || (s->l >= XRES) ||
            (s->t < 0) || (s->t >= YRES) ||
            (s->b < 0) || (s->b >= YRES) ||
            (s->l > s->r) || (s->t > s->b) ||
            (s->nbp > (s->r - s->l + 1) * (s->b - s->t + 1))) {

                show_hint(2, "symbol %d has invalid geometry\n", c);
                return (1);
        }

        if ((s->f & C_AS) != s->f) {
                show_hint(2, "symbol %d has invalid flags\n", c);
                return (1);
        }

        /* TO BE COMPLETED */

        return (0);
}

/*

Run all consistencies.

*/
int cons(int reset) {
        static int st = 0, i = 0;
        int r = 0;

        if (reset) {
                st = 0;
                i = 0;
                return (1);
        }

        /* consist closures */
        if (st == 0) {
                if (i > topcl) {
                        st = 1;
                        i = 0;
                        r = 0;
                } else {
                        if (i == 0)
                                show_hint(1, "checking closures");
                        r = cl_cons(i++);
                }
        }

        /* consist symbols */
        else if (st == 1) {
                if (i > tops) {
                        st = 2;
                        i = 0;
                        r = 0;
                } else {
                        if (i == 0)
                                show_hint(1, "checking symbols");
                        r = s_cons(i++);
                }
        }

        else if (st == 2) {
                show_hint(1, "tests finished successfully");
                return (0);
        }

        /* fatal */
        if (r) {
                return (2);
        }

        return (1);
}

/*

Consist page geometry

*/
void consist_pg(void) {
        float md;

        /* maximum acceptable width or height, in millimeters */
        md = ((float) FS * 25.4) / DENSITY;

        /* letter width and height */
        if (LW > md)
                LW = md;
        if (LW < 1)
                LW = 1;
        if (LH > md)
                LH = md;
        if (LH < 1)
                LH = 1;

        /* dot diameter */
        if (DD > md)
                DD = md;
        if (DD < 1)
                DD = 1;

        /* vertical separation between text lines */
        if (LS > 3 * md)
                LS = 3 * md;
        if (LS < 3)
                LS = 3;

        /* text columns */
        if (TC > 2)
                TC = 2;
        if (TC < 1)
                TC = 1;

        /* separation line */
        SL = (SL != 0);

        /* margins */
        if (LM < 1)
                LM = 1;
        if (RM < 1)
                RM = 1;
        if (TM < 1)
                TM = 1;
        if (BM < 1)
                BM = 1;
}

/*

Consist density

*/
void consist_density(void) {
        if (DENSITY <= 0)
                DENSITY = 600;
        if (DENSITY > 1200)
                DENSITY = 1200;
}

/*

Consist magic numbers

*/
void consist_magic(void) {
        if (m_mwd < 1)
                m_mwd = 1;
        if (m_mwd > 5000)
                m_mwd = 5000;
        if (m_msd < 1)
                m_msd = 1;
        if (m_msd > 500)
                m_msd = 500;
        if (m_mae < 0)
                m_mae = 0;
        if (m_mae > 500)
                m_mae = 500;
        if (m_ds < 10)
                m_ds = 10;
        if (m_ds > 2000)
                m_ds = 2000;
        if (m_as < 10)
                m_as = 10;
        if (m_as > 2000)
                m_as = 2000;
        if (m_xh < 10)
                m_xh = 10;
        if (m_xh > 2000)
                m_xh = 2000;
        if (m_fs < 1)
                m_fs = 1;
        if (m_fs > 100)
                m_fs = 100;
}

/*

Consist preprocessing variables.

*/
void consist_pp(void) {
        if ((pp_avoid_links < 0.0) || (1.0 < pp_avoid_links))
                pp_avoid_links = 0.0;
}

#ifdef TEST

/*

Run all internal tests.

*/
void internal_tests(void) {
        test_clusterize();
}

#endif
