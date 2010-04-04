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

cml.c: ClaraML dump and recover

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "common.h"
#include "gui.h"

/* current line on session dump */
int DLINE;

/* line size */
int DLS = 80;

/*

Dictionary entries

*/
ddesc *de = NULL;
int desz = 0, topde = -1, dict_op = 0;

/*

Returns true if s is a non-empty string.

*/
int nonempty(char *s) {

        if (s == NULL)
                return (0);
        else
                for (;
                     (*s == ' ') || (*s == '\r') || (*s == '\t') ||
                     (*s == '\n'); ++s);
        if (*s == 0)
                return (0);
        else
                return (1);
}

/*

Convert structure field to asc.

*/
void toasc(FILE *F, char *name, int type, int n, void *f) {
        int i, r, d;
        static char *s = NULL;
        static int max;
        char sep;

        /* silently ignore NULL strings */
        if ((type == TSTR) && (f == NULL))
                return;

        /* alloc initial buffer */
        if (s == NULL)
                s = g_malloc(max = 4096);

        /* dump field name */
        if ((d = snprintf(s, max, "%s", name)) < 0) {
                fatal(BO, "buffer overflow at toasc\n");
        }

        /* STR to CHAR */
        if (type == TSTR) {
                type = TCHAR;
                n = strlen((char *) f);
        }

        /* empty array */
        if ((n == 0) && (type != TNULL)) {
                r = snprintf(s + d, max - d, "=");
                if (r < 0) {
                        fatal(BO, "buffer overflow at toasc\n");
                }
                d += r;
        }

        /* dump array */
        else
                for (sep = '=', i = 0; i < n; ++i) {

                        if (d > max - 128) {
                                if (F == NULL)
                                        totext("%s", s);
                                else
                                        fprintf(F, "%s", s);
                                s[d = 0] = 0;
                        }

                        if (type == TCHAR)
                                r = snprintf(s + d, max - d, "%c%x", sep,
                                             *(((unsigned char *) f) + i));
                        else if (type == TSHORT)
                                r = snprintf(s + d, max - d, "%c%d", sep,
                                             *(((short *) f) + i));
                        else if (type == TFLOAT)
                                r = snprintf(s + d, max - d, "%c%f", sep,
                                             *(((float *) f) + i));
                        else if (type == TINT)
                                r = snprintf(s + d, max - d, "%c%d", sep,
                                             *(((int *) f) + i));
                        else {
                                r = 0;
                                db("unknown type %d at toasc", type);
                        }
                        if (r < 0) {
                                fatal(BO, "buffer overflow at toasc\n");
                        }
                        d += r;
                        sep = ',';
                }

        /* record delimiter */
        if ((snprintf(s + d, max - d, "\n")) < 0) {
                fatal(BO, "buffer overflow at toasc\n");
        }

        /* send to output */
        if (F == NULL)
                totext("%s", s);
        else
                zfwrite(s, strlen(s), 1, F);
}

/*

Read one line from dump.

Returns the address of the buffer where the line was put,
or NULL if the input is exhausted.

*/
char *recover_line(FILE *F) {
        static int psz = 0;
        static char *s;
        int i, t, end;

        /* alloc initial buffer */
        if (s == NULL)
                s = g_malloc(psz = 512);

        /* reading loop */
        for (t = 0, end = 0; end == 0;) {

                if (F != NULL) {
                        if ((fgets(s + t, psz - t - 1, F) == NULL) &&
                            (feof(F)))
                                end = 1;
                        else
                                t += strlen(s + t);
                } else {
                        int nl;

                        if (text[topt] == 0)
                                end = 1;
                        for (nl = 0; (t + 1 < psz) && (nl == 0);) {
                                nl = (text[topt] == 0) ||
                                    (text[topt] == '\n');
                                s[t++] = text[topt++];
                        }
                        s[t] = 0;
                }

                if (end == 0) {

                        /* return line unless it is empty */
                        if ((t > 0) && (s[t] == 0) && (s[t - 1] == '\n')) {
                                ++DLINE;

                                /* remove comment (if any) */
                                for (i = 0; i < t - 1; ++i)
                                        if (s[i] == '#') {
                                                s[i] = '\n';
                                                s[i + 1] = 0;
                                                t = i + 1;
                                        }

                                if (nonempty(s)) {
                                        /* printf("will return %s",s); */
                                        return (s);
                                }
                                t = 0;
                        }

                        /* enlarge memory buffer */
                        else if (t >= psz - 2) {
                                s = g_realloc(s, psz += 512);
                        }
                }
        }

        /* input exhausted */
        return (NULL);
}

/*

This is a complex service.

It reads the input F searching for a field name and a list of atoms,
like "f=1" or "cl=122,479". The type of the atoms is informed through
the "type" parameter. If type==TNULL, searches for a single field
name, like "a" or "blue".

Returns 0 if end-of-file is detected before reading all that stuff.

Returns -1 if the field name informed throug the "name" parameter did
not match the name read from input.

If the field name matched, reads all atoms and store them on an
internal buffer. If the parameter "size" is strictly positive, then
returns -1 if the number of atoms read is not equal to "size".

If the number of atoms read is equal to "size", or if "size" is 0 or
strictly negative, returns the number of atoms and exports the array
of atoms to another buffer.

This another buffer is that pointed to by f, if "size" is
nonzero. Otherwise, this buffer will be allocated and its address will
return in *f.

If "size" is strictly negative, assumes that -size is the maximum
number of atoms to be read.

*/
int fromasc(FILE *F, char *name, int type, int size, void *f) {
        static char *s = NULL;
        static char *b = NULL;
        static int bs = 0;
        char *e, *g;
        int n, t, ts;
        long r;
        double d;

        /* type size */
        if (type == TCHAR)
                ts = sizeof(char);
        else if (type == TSHORT)
                ts = sizeof(short);
        else if (type == TFLOAT)
                ts = sizeof(float);
        else if (type == TINT)
                ts = sizeof(int);
        else if (type == TSTR)
                ts = sizeof(char);
        else if (type == TNULL)
                ts = 0;
        else {
                ts = -1;
                db("unknown type %d at fromasc", type);
        }

        /* read one non-empty input line or fail */
        if ((s == NULL) && ((s = recover_line(F)) == NULL)) {
                return (0);
        }

        /* field name does not match */
        t = strlen(name);
        if ((strncmp(s, name, t) != 0) ||
            ((type != TNULL) && (s[t] != '='))) {

                /* if string, initialize to NULL */
                if ((size == 0) && (type == TSTR))
                        *((void **) f) = NULL;

                return (-1);
        }

        /* no array to be read */
        if (type == TNULL) {
                n = 0;
                e = s = NULL;
        } else
                e = s + t + 1;

        /* read array */
        for (n = 0; (e != NULL);) {

                /* just to avoid a compilation warning */
                d = r = 0;

                if ((type == TCHAR) || (type == TSTR))
                        r = strtol(e, &g, 16);
                else if (type == TSHORT)
                        r = strtol(e, &g, 10);
                else if (type == TFLOAT)
                        d = strtod(e, &g);
                else if (type == TINT)
                        r = strtol(e, &g, 10);

                /* no other element found */
                if (e == g) {
                        s = NULL;
                        e = NULL;
                }

                /* found element */
                else {

                        /* add it to b */
                        if (f != NULL) {

                                /* enlarge memory buffer */
                                if (bs < (n + 1) * ts)
                                        b = g_realloc(b, bs += 512);

                                /* exceeded size */
                                if (((size > 0) && (n >= size)) ||
                                    ((size < 0) && (n >= -size))) {

                                        return (0);
                                }

                                /* add */
                                else if ((type == TCHAR) || (type == TSTR))
                                        *(((char *) b) + n) = r;
                                else if (type == TSHORT)
                                        *(((short *) b) + n) = r;
                                else if (type == TFLOAT)
                                        *(((float *) b) + n) = d;
                                else if (type == TINT)
                                        *(((int *) b) + n) = r;
                        }

                        /* count and advance */
                        ++n;
                        e = g + 1;
                }
        }

        /* invalid size */
        if ((size > 0) && (n != size)) {

                return (0);
        }

        /*
           If f is not NULL, then store the array on *f. If
           size is 0, then alloc memory and make *f
           point to it.
         */
        if (((n > 0) || (type == TSTR)) && (f != NULL)) {

                /* alloc memory */
                if (size == 0) {
                        if (type == TSTR) {
                                g_assert(ts == 1);
                                char** fv = (char**)f;
                                *(char**)fv = g_strndup(b, n);
                                //memcpy(*fv, b, n * ts);
                                //*(*fv + n) = 0;
                        } else {
                                *((void **) f) = g_memdup(b, n * ts);
                        }
                }

                /* copy to the pre-allocated buffer */
                else
                        memcpy(f, b, n * ts);
        }

        /* return array size */
        return (n);
}

/*

Session Dumper.

*/
int dump_session(char *f, int reset) {
        static int i, st;
        int bms;
        static FILE *F;
        static int pio;

        if (reset) {
                if ((F = zfopen(f, "w", &pio)) == NULL)
                        return (0);
                else {
                        st = 0;
                        return (1);
                }
        }

        /* finished */
        if (st == 7) {
                fprintf(F, "%s", "</Clara>\n");
                zfclose(F, pio);
                sess_changed = 0;
                return (0);
        }

        /* one quantum more */
        switch (st) {

                /* magic */
        case 0:
                toasc(F, "<Clara>", TNULL, 0, NULL);
                fprintf(F, "\n");
                st = 1;
                i = 0;
                break;

                /* unstructured variables */
        case 1:
                toasc(F, "<unstruct", TNULL, 0, NULL);
                toasc(F, "XRES", TINT, 1, &XRES);
                toasc(F, "YRES", TINT, 1, &YRES);
                toasc(F, "zones", TINT, 1, &zones);
                if (zones > 0)
                        toasc(F, "limits", TINT, 8 * zones, limits);
                toasc(F, "curr_mc", TINT, 1, &curr_mc);
                toasc(F, "symbols", TINT, 1, &symbols);
                toasc(F, "doubts", TINT, 1, &doubts);
                toasc(F, "runs", TINT, 1, &runs);
                toasc(F, "ocr_time", TINT, 1, &ocr_time);
                toasc(F, "ocr_r", TINT, 1, &ocr_r);
                toasc(F, "words", TINT, 1, &words);
                toasc(F, "classes", TINT, 1, &classes);
                toasc(F, "></unstruct>\n", TNULL, 0, NULL);
                st = 2;
                i = 0;
                break;

                /* closures */
        case 2:
                if (i > topcl) {
                        st = 3;
                        i = 0;
                } else {
                        int topsup;

                        if ((batch_mode == 0) && ((i % DPROG) == 0)) {
                                snprintf(mba, MMB,
                                         "now saving closure %d/%d", i,
                                         topcl);
                                show_hint(0, mba);
                        }

                        toasc(F, "<cldesc idx", TINT, 1, &i);
                        toasc(F, "l", TINT, 1, &(cl[i].l));
                        toasc(F, "r", TINT, 1, &(cl[i].r));
                        toasc(F, "t", TINT, 1, &(cl[i].t));
                        toasc(F, "b", TINT, 1, &(cl[i].b));
                        bms =
                            byteat(cl[i].r - cl[i].l + 1) * (cl[i].b -
                                                             cl[i].t + 1);
                        toasc(F, "bm", TCHAR, bms, cl[i].bm);
                        toasc(F, "nbp", TINT, 1, &(cl[i].nbp));
                        toasc(F, "seed", TSHORT, 2, cl[i].seed);
                        toasc(F, "supsz", TINT, 1, &(cl[i].supsz));
                        for (topsup = 0; cl[i].sup[topsup] >= 0; ++topsup);
                        toasc(F, "sup", TINT, topsup, cl[i].sup);
                        toasc(F, "></cldesc>\n", TNULL, 0, NULL);
                        ++i;
                }
                break;

                /* symbols */
        case 3:
                if (i > tops) {
                        st = 4;
                        i = 0;
                } else {
                        trdesc *t;

                        if ((batch_mode == 0) && ((i % DPROG) == 0)) {
                                snprintf(mba, MMB,
                                         "now saving symbol %d/%d", i,
                                         tops);
                                show_hint(0, mba);
                        }

                        toasc(F, "<sdesc idx", TINT, 1, &i);
                        toasc(F, "ncl", TINT, 1, &(mc[i].ncl));
                        toasc(F, "cl", TINT, mc[i].ncl, mc[i].cl);
                        toasc(F, "l", TINT, 1, &(mc[i].l));
                        toasc(F, "r", TINT, 1, &(mc[i].r));
                        toasc(F, "t", TINT, 1, &(mc[i].t));
                        toasc(F, "b", TINT, 1, &(mc[i].b));
                        toasc(F, "nbp", TINT, 1, &(mc[i].nbp));
                        toasc(F, "va", TCHAR, 1, &(mc[i].va));
                        toasc(F, "c", TCHAR, 1, &(mc[i].c));
                        toasc(F, "f", TINT, 1, &(mc[i].f));
                        toasc(F, "N", TINT, 1, &(mc[i].N));
                        toasc(F, "S", TINT, 1, &(mc[i].S));
                        toasc(F, "E", TINT, 1, &(mc[i].E));
                        toasc(F, "W", TINT, 1, &(mc[i].W));
                        toasc(F, "sw", TINT, 1, &(mc[i].sw));
                        toasc(F, "sl", TINT, 1, &(mc[i].sl));
                        toasc(F, "bs", TINT, 1, &(mc[i].bs));
                        toasc(F, "tc", TCHAR, 1, &(mc[i].tc));
                        toasc(F, "pb", TINT, 1, &(mc[i].pb));
                        toasc(F, "lfa", TINT, 1, &(mc[i].lfa));
                        toasc(F, "bm", TINT, 1, &(mc[i].bm));
                        toasc(F, "></sdesc>\n", TNULL, 0, NULL);

                        /* transliterations */
                        for (t = mc[i].tr; t != NULL; t = t->nt) {
                                vdesc *v;

                                toasc(F, "<trdesc idx", TINT, 1, &i);
                                toasc(F, "pr", TINT, 1, &(t->pr));
                                toasc(F, "n", TINT, 1, &(t->n));
                                toasc(F, "f", TINT, 1, &(t->f));
                                toasc(F, "a", TCHAR, 1, &(t->a));
                                toasc(F, "t", TCHAR, strlen(t->t), t->t);
                                toasc(F, "></trdesc>\n", TNULL, 0, NULL);

                                /* votes */
                                for (v = t->v; v != NULL;
                                     v = (vdesc *) v->nv) {

                                        toasc(F, "<vdesc idx", TINT, 1,
                                              &i);
                                        toasc(F, "orig", TCHAR, 1,
                                              &(v->orig));
                                        toasc(F, "act", TINT, 1,
                                              &(v->act));
                                        toasc(F, "q", TCHAR, 1, &(v->q));
                                        toasc(F, "></vdesc>\n", TNULL, 0,
                                              NULL);
                                }
                        }

                        /* prepare next symbol */
                        ++i;
                }
                break;

                /* links */
        case 4:
                if (i > toplk) {
                        st = 5;
                        i = 0;
                } else {

                        if ((batch_mode == 0) && ((i % DPROG) == 0)) {
                                snprintf(mba, MMB, "now saving link %d/%d",
                                         i, toplk);
                                show_hint(0, mba);
                        }

                        toasc(F, "<lkdesc idx", TINT, 1, &i);
                        toasc(F, "t", TCHAR, 1, &(lk[i].t));
                        toasc(F, "p", TCHAR, 1, &(lk[i].p));
                        toasc(F, "a", TINT, 1, &(lk[i].a));
                        toasc(F, "l", TINT, 1, &(lk[i].l));
                        toasc(F, "r", TINT, 1, &(lk[i].r));
                        toasc(F, "></lkdesc>\n", TNULL, 0, NULL);
                        ++i;
                }
                break;

                /* lines */
        case 5:
                if (i > topln) {
                        st = 6;
                        i = 0;
                } else {

                        if ((batch_mode == 0) && ((i % DPROG) == 0)) {
                                snprintf(mba, MMB, "now saving line %d/%d",
                                         i, topln);
                                show_hint(0, mba);
                        }

                        toasc(F, "<lndesc idx", TINT, 1, &i);
                        toasc(F, "f", TINT, 1, &(line[i].f));
                        toasc(F, "l", TINT, 1, &(line[i].l));
                        toasc(F, "></lndesc>\n", TNULL, 0, NULL);
                        ++i;
                }
                break;

                /* words */
        case 6:
                if (i > topw) {
                        st = 7;
                        i = 0;
                } else {

                        if ((batch_mode == 0) && ((i % DPROG) == 0)) {
                                snprintf(mba, MMB, "now saving word %d/%d",
                                         i, topw);
                                show_hint(0, mba);
                        }

                        toasc(F, "<wdesc idx", TINT, 1, &i);
                        toasc(F, "F", TINT, 1, &(word[i].F));
                        toasc(F, "L", TINT, 1, &(word[i].L));
                        toasc(F, "l", TINT, 1, &(word[i].l));
                        toasc(F, "r", TINT, 1, &(word[i].r));
                        toasc(F, "t", TINT, 1, &(word[i].t));
                        toasc(F, "b", TINT, 1, &(word[i].b));
                        toasc(F, "E", TINT, 1, &(word[i].E));
                        toasc(F, "W", TINT, 1, &(word[i].W));
                        toasc(F, "tl", TINT, 1, &(word[i].tl));
                        toasc(F, "bl", TINT, 1, &(word[i].bl));
                        toasc(F, "br", TINT, 1, &(word[i].br));
                        toasc(F, "f", TINT, 1, &(word[i].f));
                        toasc(F, "a", TCHAR, 1, &(word[i].a));
                        toasc(F, "sh", TSHORT, 1, &(word[i].sh));
                        toasc(F, "as", TSHORT, 1, &(word[i].as));
                        toasc(F, "ds", TSHORT, 1, &(word[i].ds));
                        toasc(F, "sk", TINT, 1, &(word[i].sk));
                        toasc(F, "></wdesc>\n", TNULL, 0, NULL);
                        ++i;
                }
                break;
        }

        /* unfinished */
        return (1);
}

/*

Acts Dumper.

*/
int dump_acts(char *f, int reset) {
        static int i, st, pio;
        static FILE *F;

        if (reset) {
                if (f == NULL) {
                        F = NULL;
                        topt = -1;
                } else {
                        if ((F = zfopen(f, "w", &pio)) == NULL)
                                return (0);
                }
                st = 0;
                i = 0;
                return (1);
        }

        /* successfully finished */
        if (st == 3) {
                toasc(F, "</Clara>\n", TNULL, 0, NULL);
                if (F != NULL)
                        zfclose(F, pio);
                hist_changed = 0;
                return (0);
        }

        switch (st) {

                /* magic */
        case 0:
                toasc(F, "<Clara>", TNULL, 0, NULL);
                toasc(F, "\n", TNULL, 0, NULL);
                st = 1;
                i = 0;

                /* unstructured variables */
        case 1:
                st = 2;
                i = 0;
                break;

                /*
                   Dump all acts if output is a file, or only the
                   topmost act if the output is the array text.
                 */
        case 2:
                if (i > topa) {
                        st = 3;
                        i = 0;
                } else {
                        if (F == NULL)
                                i = topa;

                        if ((batch_mode == 0) &&
                            ((F != NULL) && ((i % DPROG) == 0))) {
                                snprintf(mba, MMB, "now saving act %d/%d",
                                         i, topa);
                                show_hint(0, mba);
                        }

                        /* preamble */
                        toasc(F, "<adesc idx", TINT, 1, &i);
                        toasc(F, "t", TCHAR, 1, &(act[i].t));
                        toasc(F, "d", TSTR, 0, act[i].d);
                        toasc(F, "f", TINT, 1, &(act[i].f));

                        /* symbols */
                        toasc(F, "mc", TINT, 1, &(act[i].mc));
                        toasc(F, "a1", TINT, 1, &(act[i].a1));
                        toasc(F, "a2", TINT, 1, &(act[i].a2));

                        /* transliteration */
                        toasc(F, "a", TCHAR, 1, &(act[i].a));
                        toasc(F, "pt", TSHORT, 1, &(act[i].pt));
                        if ((act[i].tr != NULL) && (act[i].tr[0] != 0))
                                toasc(F, "tr", TSTR, 0, act[i].tr);

                        /* reviewer data */
                        toasc(F, "r", TSTR, 0, act[i].r);
                        toasc(F, "rt", TCHAR, 1, &(act[i].rt));
                        toasc(F, "sa", TSTR, 0, act[i].sa);
                        toasc(F, "dt", TINT, 1, &(act[i].dt));

                        /* original revision data */
                        toasc(F, "or", TSTR, 0, act[i].or);
                        toasc(F, "ob", TSTR, 0, act[i].ob);
                        toasc(F, "om", TINT, 1, &(act[i].om));
                        toasc(F, "od", TINT, 1, &(act[i].od));
                        toasc(F, "></adesc>\n", TNULL, 0, NULL);

                        /* prepare next act */
                        ++i;
                }
        }

        /* unfinished */
        return (1);
}

/*

Dump patterns to disk.

*/
int dump_patterns(char *f, int reset) {
        static FILE *F;
        pdesc *s;
        static int i, st, pio;

        if (reset) {
                if ((F = zfopen(f, "w", &pio)) == NULL)
                        return (0);
                i = 0;
                st = 0;
                return (1);
        }

        /* successfully finished */
        if (st == 4) {
                fprintf(F, "%s", "</Clara>\n");
                zfclose(F, pio);
                font_changed = 0;
                return (0);
        }

        switch (st) {

                /* magic */
        case 0:
                toasc(F, "<Clara>", TNULL, 0, NULL);
                fprintf(F, "\n");
                st = 1;
                i = 0;
                break;

                /* unstructured variables */
        case 1:

                toasc(F, "<unstruct", TNULL, 0, NULL);
                toasc(F, "SA", TINT, 1, &DEF_SA);
                toasc(F, "RR", TFLOAT, 1, &DEF_RR);
                toasc(F, "MA", TFLOAT, 1, &DEF_MA);
                toasc(F, "MP", TINT, 1, &DEF_MP);
                toasc(F, "ML", TFLOAT, 1, &DEF_ML);
                toasc(F, "MB", TINT, 1, &DEF_MB);
                toasc(F, "RX", TINT, 1, &DEF_RX);
                toasc(F, "BT", TINT, 1, &DEF_BT);
                toasc(F, "s_id", TINT, 1, &s_id);

                /* page geometry */
                toasc(F, "LW", TFLOAT, 1, &LW);
                toasc(F, "LH", TFLOAT, 1, &LH);
                toasc(F, "DD", TFLOAT, 1, &DD);
                toasc(F, "LS", TFLOAT, 1, &LS);
                toasc(F, "LM", TFLOAT, 1, &LM);
                toasc(F, "RM", TFLOAT, 1, &RM);
                toasc(F, "TM", TFLOAT, 1, &TM);
                toasc(F, "BM", TFLOAT, 1, &BM);
                toasc(F, "PG_AUTO", TINT, 1, &PG_AUTO);
                toasc(F, "TC", TINT, 1, &TC);
                toasc(F, "SL", TINT, 1, &SL);

                /* magic numbers */
                toasc(F, "m_mwd", TINT, 1, &m_mwd);
                toasc(F, "m_msd", TINT, 1, &m_msd);
                toasc(F, "m_mae", TINT, 1, &m_mae);
                toasc(F, "m_ds", TINT, 1, &m_ds);
                toasc(F, "m_as", TINT, 1, &m_as);
                toasc(F, "m_xh", TINT, 1, &m_xh);
                toasc(F, "m_fs", TINT, 1, &m_fs);
                toasc(F, "></unstruct>\n", TNULL, 0, NULL);
                st = 2;
                i = 0;
                break;

                /* patterns */
        case 2:
                if (i > topp) {
                        st = 3;
                        i = 0;
                } else {

                        if ((batch_mode == 0) && ((i % DPROG) == 0)) {
                                snprintf(mba, MMB,
                                         "now saving pattern %d/%d", i,
                                         topp);
                                show_hint(0, mba);
                        }

                        s = pattern + i;
                        toasc(F, "<pdesc idx", TINT, 1, &i);
                        /* toasc(F,"dt",TINT,1,&(s->dt)); */
                        toasc(F, "id", TINT, 1, &(s->id));
                        toasc(F, "bw", TSHORT, 1, &(s->bw));
                        toasc(F, "bh", TSHORT, 1, &(s->bh));
                        toasc(F, "bb", TSHORT, 1, &(s->bb));
                        toasc(F, "bp", TSHORT, 1, &(s->bp));
                        toasc(F, "sw", TSHORT, 1, &(s->sw));
                        toasc(F, "sh", TSHORT, 1, &(s->sh));
                        toasc(F, "sb", TSHORT, 1, &(s->sb));
                        toasc(F, "sp", TSHORT, 1, &(s->sp));
                        toasc(F, "sx", TSHORT, 1, &(s->sx));
                        toasc(F, "sy", TSHORT, 1, &(s->sy));
                        toasc(F, "bs", TSHORT, 1, &(s->bs));
                        toasc(F, "b", TCHAR, s->bs, s->b);
                        toasc(F, "a", TCHAR, 1, &(s->a));
                        toasc(F, "pt", TINT, 1, &(s->pt));
                        toasc(F, "fs", TSHORT, 1, &(s->fs));
                        toasc(F, "f", TINT, 1, &(s->f));
                        toasc(F, "ts", TSHORT, 1, &(s->ts));
                        if (s->ts > 0)
                                toasc(F, "tr", TCHAR, s->ts, s->tr);
                        toasc(F, "act", TINT, 1, &(s->act));
                        toasc(F, "cm", TINT, 1, &(s->cm));
                        toasc(F, "ds", TSHORT, 1, &(s->ds));
                        if (s->ds > 0)
                                toasc(F, "d", TCHAR, s->ds, s->d);
                        toasc(F, "e", TINT, 1, &(s->e));
                        toasc(F, "sl", TFLOAT, 1, &(s->sl));
                        toasc(F, "l", TSHORT, 1, &(s->l));
                        toasc(F, "t", TSHORT, 1, &(s->t));
                        toasc(F, "p", TSHORT, 8, s->p);
                        toasc(F, "></pdesc>\n", TNULL, 0, NULL);

                        /* prepare next pattern */
                        ++i;
                }
                break;


                /* patterns types */
        case 3:
                if (i > toppt) {
                        st = 4;
                        i = 0;
                } else {
                        ptdesc *s;

                        s = pt + i;
                        toasc(F, "<ptdesc idx", TINT, 1, &i);
                        toasc(F, "at", TCHAR, 1, &(s->at));
                        toasc(F, "n", TSTR, 0, s->n);
                        toasc(F, "f", TINT, 1, &(s->f));
                        toasc(F, "bd", TSHORT, 1, &(s->bd));
                        toasc(F, "a", TINT, 1, &(s->a));
                        toasc(F, "d", TINT, 1, &(s->d));
                        toasc(F, "xh", TINT, 1, &(s->xh));
                        toasc(F, "dd", TINT, 1, &(s->dd));
                        toasc(F, "ls", TSHORT, 1, &(s->ls));
                        toasc(F, "ss", TSHORT, 1, &(s->ss));
                        toasc(F, "ws", TSHORT, 1, &(s->ws));
                        toasc(F, "acc", TINT, 1, &(s->acc));
                        toasc(F, "sc", TINT, 256, &(s->sc));
                        toasc(F, "sa", TINT, 256, &(s->sa));
                        toasc(F, "></ptdesc>\n", TNULL, 0, NULL);

                        /* prepare next pattern */
                        ++i;
                }
                break;


        }

        /* unfinished */
        return (1);
}

/*

Session recover. If the parameter st is nonzero, then finish just
after reading the unstruct element of the session.

*/
int recover_session(char *f, int st) {
        FILE *F;
        int k, pio;
        int cm = -1;
        trdesc *tr;


        /* open file */
        if (selthresh || ((F = zfopen(f, "r", &pio)) == NULL)) {
                if (verbose)
                        warn("session file \"%s\" not found (ok)",
                             f);
                return 0;
        }
        DLINE = 0;

        while (TRUE) {
                /* magic */
                if (fromasc(F, "<Clara>", TNULL, 0, NULL) == 0) {
                }

                /* unstructured variables */
                else if (fromasc(F, "<unstruct", TNULL, 0, NULL) == 0) {
                        fromasc(F, "XRES", TINT, 1, &XRES);
                        fromasc(F, "YRES", TINT, 1, &YRES);
                        fromasc(F, "zones", TINT, 1, &zones);
                        if (zones > 0)
                                fromasc(F, "limits", TINT, 8 * zones, limits);
                        fromasc(F, "curr_mc", TINT, 1, &curr_mc);
                        fromasc(F, "symbols", TINT, 1, &symbols);
                        fromasc(F, "doubts", TINT, 1, &doubts);
                        fromasc(F, "runs", TINT, 1, &runs);
                        fromasc(F, "ocr_time", TINT, 1, &ocr_time);
                        fromasc(F, "ocr_r", TINT, 1, &ocr_r);
                        fromasc(F, "words", TINT, 1, &words);
                        fromasc(F, "classes", TINT, 1, &classes);
                        fromasc(F, "></unstruct>", TNULL, 0, NULL);
                        if (st)
                                break;
                }

                /* start of symbol */
                else if (fromasc(F, "<sdesc idx", TINT, 1, &k) == 1) {
                        sdesc m;

                        if ((batch_mode == 0) && ((k % DPROG) == 0)) {
                                snprintf(mba, MMB, "now reading symbol %d", k);
                                show_hint(0, mba);
                        }

                        /* current symbol */
                        cm = k;

                        /* enlarge memory area */
                        if (k >= ssz) {
                                int i;

                                i = ssz;
                                mc = g_renew(sdesc, mc, (ssz += 1000));
                                for (; i < ssz; ++i) {
                                        mc[i].ncl = 0;
                                        mc[i].tr = NULL;
                                }
                        }

                        m.tr = NULL;

                        fromasc(F, "ncl", TINT, 1, &(m.ncl));
                        fromasc(F, "cl", TINT, 0, &(m.cl));
                        fromasc(F, "l", TINT, 1, &(m.l));
                        fromasc(F, "r", TINT, 1, &(m.r));
                        fromasc(F, "t", TINT, 1, &(m.t));
                        fromasc(F, "b", TINT, 1, &(m.b));
                        fromasc(F, "nbp", TINT, 1, &(m.nbp));
                        fromasc(F, "va", TCHAR, 1, &(m.va));
                        fromasc(F, "c", TCHAR, 1, &(m.c));
                        fromasc(F, "f", TINT, 1, &(m.f));
                        fromasc(F, "N", TINT, 1, &(m.N));
                        fromasc(F, "S", TINT, 1, &(m.S));
                        fromasc(F, "E", TINT, 1, &(m.E));
                        fromasc(F, "W", TINT, 1, &(m.W));
                        fromasc(F, "sw", TINT, 1, &(m.sw));
                        fromasc(F, "sl", TINT, 1, &(m.sl));
                        m.bs = -1;
                        fromasc(F, "bs", TINT, 1, &(m.bs));
                        fromasc(F, "tc", TCHAR, 1, &(m.tc));
                        fromasc(F, "pb", TINT, 1, &(m.pb));
                        fromasc(F, "lfa", TINT, 1, &(m.lfa));
                        fromasc(F, "bm", TINT, 1, &(m.bm));
                        fromasc(F, "></sdesc>", TNULL, 0, NULL);

                        /* copy */
                        memcpy(mc + k, &m, sizeof(sdesc));
                }

                /* start of closure */
                else if (fromasc(F, "<cldesc idx", TINT, 1, &k) == 1) {
                        cldesc m;
                        int topsup;

                        if ((batch_mode == 0) && ((k % DPROG) == 0)) {
                                snprintf(mba, MMB, "now reading cl %d", k);
                                show_hint(0, mba);
                        }

                        fromasc(F, "l", TINT, 1, &(m.l));
                        fromasc(F, "r", TINT, 1, &(m.r));
                        fromasc(F, "t", TINT, 1, &(m.t));
                        fromasc(F, "b", TINT, 1, &(m.b));
                        fromasc(F, "bm", TCHAR, 0, &(m.bm));
                        fromasc(F, "nbp", TINT, 1, &(m.nbp));
                        fromasc(F, "seed", TSHORT, 2, &(m.seed));
                        fromasc(F, "supsz", TINT, 1, &(m.supsz));
                        m.sup = g_new(int, m.supsz);
                        topsup = fromasc(F, "sup", TINT, -m.supsz, m.sup);
                        m.sup[topsup] = -1;
                        fromasc(F, "></cldesc>", TNULL, 0, NULL);

                        /* must enlarge allocated area for cl */
                        checkcl(++topcl);
                        memcpy(cl + topcl, &m, sizeof(cldesc));

                        /* create unitary symbol */
                        /* mc[new_mc(&topcl,1)].isp = 1; */

                        /* printf("l=%d\n",m.l); */
                }

                /* start of vote */
                else if (fromasc(F, "<vdesc idx", TINT, 1, &k) == 1) {
                        vdesc *v, *a;

                        v = g_new(vdesc, 1);
                        fromasc(F, "orig", TCHAR, 1, &(v->orig));
                        fromasc(F, "act", TINT, 1, &(v->act));
                        fromasc(F, "q", TCHAR, 1, &(v->q));
                        fromasc(F, "></vdesc>", TNULL, 0, NULL);

                        /* add to the current transliteration */
                        v->nv = NULL;
                        if (tr->v == NULL)
                                tr->v = v;
                        else {
                                for (a = tr->v; a->nv != NULL;
                                     a = (vdesc *) (a->nv));
                                a->nv = (void *) v;
                        }
                }

                /* start of transliteration */
                else if (fromasc(F, "<trdesc idx", TINT, 1, &k) == 1) {
                        trdesc *a;

                        tr = g_new(trdesc, 1);
                        fromasc(F, "pr", TINT, 1, &(tr->pr));
                        fromasc(F, "n", TINT, 1, &(tr->n));
                        fromasc(F, "f", TINT, 1, &(tr->f));
                        fromasc(F, "a", TCHAR, 1, &(tr->a));
                        fromasc(F, "t", TSTR, 0, &(tr->t));
                        fromasc(F, "></trdesc>", TNULL, 0, NULL);

                        /* no votes by now */
                        tr->v = NULL;

                        /* add to the current symbol */
                        tr->nt = NULL;
                        if (mc[cm].tr == NULL)
                                mc[cm].tr = tr;
                        else {
                                for (a = mc[cm].tr; a->nt != NULL; a = a->nt);
                                a->nt = tr;
                        }
                }

                /* recover link */
                else if (fromasc(F, "<lkdesc idx", TINT, 1, &k) == 1) {

                        if (k >= lksz) {
                                lk = g_renew(lkdesc, lk, lksz += 128);
                        }
                        if (k > toplk)
                                toplk = k;

                        fromasc(F, "t", TCHAR, 1, &(lk[k].t));
                        fromasc(F, "p", TCHAR, 1, &(lk[k].p));
                        fromasc(F, "a", TINT, 1, &(lk[k].a));
                        fromasc(F, "l", TINT, 1, &(lk[k].l));
                        fromasc(F, "r", TINT, 1, &(lk[k].r));
                        fromasc(F, "></lkdesc>\n", TNULL, 0, NULL);
                }

                /* recover line */
                else if (fromasc(F, "<lndesc idx", TINT, 1, &k) == 1) {
                        if (k >= lnsz) {
                                int i = lnsz;

                                lnsz = k + 128;
                                line = g_renew(lndesc, line, lnsz);
                                for (; i < lnsz; ++i)
                                        line[i].f = -1;
                        }
                        if (k > topln)
                                topln = k;
                        fromasc(F, "f", TINT, 1, &(line[k].f));
                        fromasc(F, "l", TINT, 1, &(line[k].l));
                        fromasc(F, "></lndesc>", TNULL, 0, NULL);
                }

                /* recover word */
                else if (fromasc(F, "<wdesc idx", TINT, 1, &k) == 1) {

                        if (k >= wsz) {
                                int i = wsz;

                                wsz = k + 128;
                                word = g_renew(wdesc, word, wsz);
                                for (; i < wsz; ++i)
                                        word[i].F = -1;
                        }
                        if (k > topw)
                                topw = k;

                        fromasc(F, "F", TINT, 1, &(word[k].F));
                        fromasc(F, "L", TINT, 1, &(word[k].L));
                        fromasc(F, "l", TINT, 1, &(word[k].l));
                        fromasc(F, "r", TINT, 1, &(word[k].r));
                        fromasc(F, "t", TINT, 1, &(word[k].t));
                        fromasc(F, "b", TINT, 1, &(word[k].b));
                        fromasc(F, "E", TINT, 1, &(word[k].E));
                        fromasc(F, "W", TINT, 1, &(word[k].W));
                        fromasc(F, "tl", TINT, 1, &(word[k].tl));
                        fromasc(F, "bl", TINT, 1, &(word[k].bl));
                        fromasc(F, "br", TINT, 1, &(word[k].br));
                        fromasc(F, "f", TINT, 1, &(word[k].f));
                        fromasc(F, "a", TCHAR, 1, &(word[k].a));
                        fromasc(F, "sh", TSHORT, 1, &(word[k].sh));
                        fromasc(F, "as", TSHORT, 1, &(word[k].as));
                        fromasc(F, "ds", TSHORT, 1, &(word[k].ds));
                        fromasc(F, "sk", TINT, 1, &(word[k].sk));
                        fromasc(F, "></wdesc>\n", TNULL, 0, NULL);
                }

                /* end of dump */
                else if (fromasc(F, "</Clara>", TNULL, 0, NULL) == 0) {
                        break;
                }

                else {
                        fatal(FD, "unexpected token found ar line %d of %s\n",
                              DLINE, f);
                }

        }

        // done:

        /* compute tops */
        if (!st) {
                int i;

                for (i = ssz - 1; (i >= 0) && (mc[i].ncl == 0);
                     --i);
                tops = i;
                for (; (i >= 0); --i) {
                        if (mc[i].ncl == 0)
                                db("oops.. hole in mc array index %d\n", i);
                }

                if (batch_mode == 0) {
                        snprintf(mba, MMB,
                                 "finished reading session (topcl=%d tops=%d)",
                                 topcl, tops);
                        show_hint(0, mba);
                }
        }

                /* finished */
        zfclose(F, pio);
        sess_changed = 0;
        return 1;

}

/*

Acts recover.

*/
int recover_acts(char *f) {
        static FILE *F;
        static int pio;
        int e;

        /* prepare to read the buffer "text".. */
        if (f == NULL) {
                F = NULL;
                topt = 0;
        }

        /* ..or open file containing revision data */
        else {
                if ((F = zfopen(f, "r", &pio)) == NULL) {
                        if (verbose)
                                warn("acts file \"%s\" not found (ok)", f);
                        return (0);
                }
        }
        DLINE = 0;

        for (e = 0; e == 0;) {
                int i;

                /* magic */
                if (fromasc(F, "<Clara>", TNULL, 0, NULL) == 0) {
                }

                /* start of act */
                else if (fromasc(F, "<adesc idx", TINT, 1, &i) == 1) {

                        /*
                           if the XML record informs index -1, we use the
                           next available entry on the acts array.
                         */
                        if (i < 0)
                                i = topa + 1;

                        /* make sure that act[i] exist */
                        if (i >= actsz) {
                                int j;

                                j = actsz;
                                act = g_renew(adesc, act, (actsz = i + 100));
                                for (; j < actsz; ++j) {
                                        act[i].d = NULL;
                                        act[i].tr = NULL;
                                        act[i].r = NULL;
                                        act[i].sa = NULL;
                                        act[i].or = NULL;
                                        act[i].ob = NULL;
                                }
                        }

                        /* top */
                        if (i > topa)
                                topa = i;

                        /* preamble */
                        fromasc(F, "t", TCHAR, 1, &(act[i].t));
                        fromasc(F, "d", TSTR, 0, &(act[i].d));
                        fromasc(F, "f", TINT, 1, &(act[i].f));

                        /* symbols */
                        fromasc(F, "mc", TINT, 1, &(act[i].mc));
                        fromasc(F, "a1", TINT, 1, &(act[i].a1));
                        fromasc(F, "a2", TINT, 1, &(act[i].a2));

                        /* transliteration */
                        fromasc(F, "a", TCHAR, 1, &(act[i].a));
                        fromasc(F, "pt", TSHORT, 1, &(act[i].pt));
                        fromasc(F, "tr", TSTR, 0, &(act[i].tr));

                        /* reviewer data */
                        fromasc(F, "r", TSTR, 0, &(act[i].r));
                        fromasc(F, "rt", TCHAR, 1, &(act[i].rt));
                        fromasc(F, "sa", TSTR, 0, &(act[i].sa));
                        fromasc(F, "dt", TINT, 1, &(act[i].dt));

                        /* original revision data */
                        fromasc(F, "or", TSTR, 0, &(act[i].or));
                        fromasc(F, "ob", TSTR, 0, &(act[i].ob));
                        fromasc(F, "om", TINT, 1, &(act[i].om));
                        fromasc(F, "od", TINT, 1, &(act[i].od));

                        fromasc(F, "></adesc>", TNULL, 0, NULL);
                }

                /* end of dump */
                else if (fromasc(F, "</Clara>", TNULL, 0, NULL) == 0) {
                        e = 1;
                }

                else {
                        if (f != NULL)
                                fatal(FD,
                                      "unexpected token at line %d of file %s",
                                      DLINE, f);
                        else {
                                printf(text);
                                fatal(FD,
                                      "unexpected token found on internally generated act");
                        }
                }
        }

        /* success */
        if (F != NULL) {
                zfclose(F, pio);
        }
        hist_changed = 0;
        return (1);
}

/*

Recover patterns.

BUG: when reusing a pdesc structure, won't reuse the memory buffers
pointed by the fields b and s.

*/
int recover_patterns(char *f) {
        static FILE *F;
        static int pio;
        int e;

        /* open file */
        if ((F = zfopen(f, "r", &pio)) == NULL) {
                if (verbose)
                        warn("bookfont file \"%s\" not found (ok)", f);
                return (0);
        }

        for (e = 0; e == 0;) {
                int i;

                /* magic */
                if (fromasc(F, "<Clara>", TNULL, 0, NULL) == 0) {
                }

                /* unstructured variables */
                else if (fromasc(F, "<unstruct", TNULL, 0, NULL) == 0) {
                        fromasc(F, "SA", TINT, 1, &DEF_SA);
                        fromasc(F, "RR", TFLOAT, 1, &DEF_RR);
                        fromasc(F, "MA", TFLOAT, 1, &DEF_MA);
                        fromasc(F, "MP", TINT, 1, &DEF_MP);
                        fromasc(F, "ML", TFLOAT, 1, &DEF_ML);
                        fromasc(F, "MB", TINT, 1, &DEF_MB);
                        fromasc(F, "RX", TINT, 1, &DEF_RX);
                        fromasc(F, "BT", TINT, 1, &DEF_BT);
                        spcpy(SP_GLOBAL, SP_DEF);
                        fromasc(F, "s_id", TINT, 1, &s_id);

                        /* page geometry */
                        fromasc(F, "LW", TFLOAT, 1, &LW);
                        fromasc(F, "LH", TFLOAT, 1, &LH);
                        fromasc(F, "DD", TFLOAT, 1, &DD);
                        fromasc(F, "LS", TFLOAT, 1, &LS);
                        fromasc(F, "LM", TFLOAT, 1, &LM);
                        fromasc(F, "RM", TFLOAT, 1, &RM);
                        fromasc(F, "TM", TFLOAT, 1, &TM);
                        fromasc(F, "BM", TFLOAT, 1, &BM);
                        fromasc(F, "PG_AUTO", TINT, 1, &PG_AUTO);
                        fromasc(F, "TC", TINT, 1, &TC);
                        SL = 0;
                        fromasc(F, "SL", TINT, 1, &SL);

                        /* magic numbers */
                        fromasc(F, "m_mwd", TINT, 1, &m_mwd);
                        fromasc(F, "m_msd", TINT, 1, &m_msd);
                        fromasc(F, "m_mae", TINT, 1, &m_mae);
                        fromasc(F, "m_ds", TINT, 1, &m_ds);
                        fromasc(F, "m_as", TINT, 1, &m_as);
                        fromasc(F, "m_xh", TINT, 1, &m_xh);
                        fromasc(F, "m_fs", TINT, 1, &m_fs);
                        fromasc(F, "></unstruct>", TNULL, 0, NULL);
                }

                /* start of pattern */
                else if (fromasc(F, "<pdesc idx", TINT, 1, &i) == 1) {
                        pdesc *s;

                        if ((batch_mode == 0) && ((i % DPROG) == 0)) {
                                snprintf(mba, MMB,
                                         "now reading pattern %d", i);
                                show_hint(0, mba);
                        }

                        if (psz <= i) {
                                psz += i - psz + 100;
                                pattern = g_renew(pdesc, pattern, psz);
                        }
                        s = pattern + i;
                        s->tr = s->d = NULL;

                        if (i > topp)
                                topp = i;

                        /* fromasc(F,"dt",TINT,1,&(s->dt)); */
                        fromasc(F, "id", TINT, 1, &(s->id));
                        fromasc(F, "bw", TSHORT, 1, &(s->bw));
                        fromasc(F, "bh", TSHORT, 1, &(s->bh));
                        fromasc(F, "bb", TSHORT, 1, &(s->bb));
                        fromasc(F, "bp", TSHORT, 1, &(s->bp));
                        fromasc(F, "sw", TSHORT, 1, &(s->sw));
                        fromasc(F, "sh", TSHORT, 1, &(s->sh));
                        fromasc(F, "sb", TSHORT, 1, &(s->sb));
                        fromasc(F, "sp", TSHORT, 1, &(s->sp));
                        fromasc(F, "sx", TSHORT, 1, &(s->sx));
                        fromasc(F, "sy", TSHORT, 1, &(s->sy));
                        fromasc(F, "bs", TSHORT, 1, &(s->bs));
                        fromasc(F, "b", TCHAR, 0, &(s->b));
                        s->s = g_malloc(s->bs);
                        fromasc(F, "a", TCHAR, 1, &(s->a));
                        fromasc(F, "pt", TINT, 1, &(s->pt));
                        fromasc(F, "fs", TSHORT, 1, &(s->fs));
                        fromasc(F, "f", TINT, 1, &(s->f));
                        fromasc(F, "ts", TSHORT, 1, &(s->ts));
                        if (s->ts > 0)
                                fromasc(F, "tr", TSTR, 0, &(s->tr));
                        s->ts = (s->tr == NULL) ? 0 : (strlen(s->tr) + 1);
                        fromasc(F, "act", TINT, 1, &(s->act));
                        fromasc(F, "cm", TINT, 1, &(s->cm));
                        fromasc(F, "ds", TSHORT, 1, &(s->ds));
                        if (s->ds > 0)
                                fromasc(F, "d", TSTR, 0, &(s->d));
                        s->ds = (s->d == NULL) ? 0 : (strlen(s->d) + 1);
                        fromasc(F, "e", TINT, 1, &(s->e));
                        fromasc(F, "sl", TFLOAT, 1, &(s->sl));
                        fromasc(F, "l", TSHORT, 1, &(s->l));
                        fromasc(F, "t", TSHORT, 1, &(s->t));
                        fromasc(F, "p", TSHORT, 8, s->p);
                        fromasc(F, "></pdesc>", TNULL, 0, NULL);

#ifdef CF
                        /*
                           This ugly hack is a companion for that one at event.c
                           (search "Display absent" on that file to locate it).
                         */
                        if (s->pt == 2)
                                s->f |= F_BOLD;
                        else if (s->pt == 3)
                                s->f |= F_ITALIC;
#endif

                }

                /* start of pattern type */
                else if (fromasc(F, "<ptdesc idx", TINT, 1, &i) == 1) {
                        ptdesc *s;

                        if (ptsz <= i) {
                                ptsz += i - ptsz + 100;
                                pt = g_renew(ptdesc, pt, ptsz);
                        }
                        s = pt + i;

                        if (i > toppt)
                                toppt = i;

                        fromasc(F, "<ptdesc idx", TINT, 1, &i);
                        fromasc(F, "at", TCHAR, 1, &(s->at));
                        fromasc(F, "n", TSTR, -(MFTL - 1), s->n);
                        s->n[MFTL - 1] = 0;
                        s->f = 0;
                        fromasc(F, "f", TINT, 1, &(s->f));
                        s->bd = 0;
                        fromasc(F, "bd", TSHORT, 1, &(s->bd));
                        fromasc(F, "a", TINT, 1, &(s->a));
                        fromasc(F, "d", TINT, 1, &(s->d));
                        fromasc(F, "xh", TINT, 1, &(s->xh));
                        fromasc(F, "dd", TINT, 1, &(s->dd));
                        fromasc(F, "ls", TSHORT, 1, &(s->ls));
                        fromasc(F, "ss", TSHORT, 1, &(s->ss));
                        fromasc(F, "ws", TSHORT, 1, &(s->ws));
                        fromasc(F, "acc", TINT, 1, &(s->acc));
                        fromasc(F, "sc", TINT, 256, &(s->sc));
                        fromasc(F, "sa", TINT, 256, &(s->sa));
                        fromasc(F, "></ptdesc>", TNULL, 0, NULL);
                }

                /* end of dump */
                else if (fromasc(F, "</Clara>", TNULL, 0, NULL) == 0) {
                        e = 1;
                }

                else {
                        fatal(FD, "unexpected token at line %d of file %s\n", DLINE, f);
                }
        }

        /* success */
        snprintf(mba, MMB, "%d patterns read", topp + 1);
        show_hint(0, mba);
        zfclose(F, pio);
        font_changed = 0;
        return (1);
}

/*

Free page data structures.

*/
int free_page(void) {
        int k;

        /* free closures */
        for (k = 0; k <= topcl; ++k) {
                g_free(cl[k].bm);
                g_free(cl[k].sup);
        }
        topcl = -1;
        cl_ready = 0;

        g_free(cl);
        cl = NULL;
        clsz = 0;

        /* free symbols */
        for (k = 0; k <= tops; ++k) {
                trdesc *t, *nt;
                vdesc *v, *nv;

                /* free each transliteration and votes */
                for (t = mc[k].tr; t != NULL; t = nt) {
                        nt = t->nt;
                        for (v = t->v; v != NULL; v = nv) {
                                nv = (vdesc *) (v->nv);
                                g_free(v);
                        }
                        g_free(t);
                }
                mc[k].tr = NULL;

                /* free cl */
                g_free(mc[k].cl);
                mc[k].ncl = 0;
                mc[k].cl = NULL;
        }
        tops = -1;
        curr_mc = -1;

        g_free(mc);
        ssz = 0;
        mc = NULL;

        /* free lines and words */
        g_free(line);
        line = NULL;
        lnsz = 0;
        topln = -1;
        topw = -1;

        /* free links */
        toplk = -1;

        /* counters */
        symbols = 0;
        cpage = -1;

        /* finished */
        return (0);
}

/*

From the page name, recover its number.

*/
int pagenbr(char *p) {
        int i;

        if (p == NULL)
                return -1;

        for (i = 0; i < npages; ++i)
                if (strcmp(p,g_ptr_array_index(page_list,i)) == 0)
                        return i;
        return -1;
}

/*

Prepare the names to load the current doc.

Clara OCR maintains prepared various different names associated with
the page name. For instance, when the file "1.pgm" is loaded, the
variable "session" contains the string "1.session". All such strings
are prepared here in order to simplify opening files.

*/
void names_cpage(void) {
        int i;

        /* locate the position at pagelist of the page cpage */
        pagename = g_ptr_array_index(page_list,cpage);

        /* page base ("1", in our example) */
        if (pagebase != NULL)
                g_free(pagebase);
        pagebase = g_strdup(pagename);
        i = strlen(pagebase);
        if ((i > 4) &&
            ((strcmp(pagebase + i - 4, ".pbm") == 0) ||
             (strcmp(pagebase + i - 4, ".pgm") == 0)))
                pagebase[i - 4] = 0;
        if ((i > 7) &&
            ((strcmp(pagebase + i - 7, ".pbm.gz") == 0) ||
             (strcmp(pagebase + i - 7, ".pgm.gz") == 0)))
                pagebase[i - 7] = 0;

        /* session (workdir path plus "1.session" in our example) */
        if (session_file != NULL) {
                g_free(session_file);
        }
        char* sessionbase = g_strconcat(pagebase,"session",zsession?".gz":"",NULL);
        session_file = g_build_filename(workdir, sessionbase, NULL);
        g_free(sessionbase);
}

/*

Load specified page.

*/
int load_page(int p, int reset, int bin) {
        int x0, y0;
        static int mode;

        /* prepare */
        if (reset) {

                /* no need to read the current page */
                if ((p == cpage) && (pixmap != NULL) && (!bin))
                        return (0);

                /* set the new current page */
                cpage = p;
                names_cpage();

                /* reset stats */
                ocr_time = 0;
                ocr_r = 0;
                runs = 0;
                symbols = 0;
                doubts = 0;
                words = 0;

                /* Try to start recovering the session file */
                if (recover_session(session_file, 0)) {
                        cl_ready = 1;
                        mode = 1;
                        synchronize();
                /* Try to start reading the bitmap */
                } else {
                        char *f = NULL;

                        /* filename */
                        f = g_build_filename(pagesdir, pagename, NULL);

                        /* start binarization/segmentation */
                        if ((pixmap != NULL) && (bin)) {

                                /* use the local thresholder */
                                if (localbin_strong) {
                                        find_thing(1, -1, -1);
                                        mode = 2;
                                }

                                /* use the global thresholder */
                                else {
                                        sh_tries = 0;
                                        pbm2bm((char*)pixmap, 1);
                                        mode = 2;
                                }
                        }

                        /* start reading pixmap from file */
                        else {
                                loadpgm(1, f, &pixmap, &XRES, &YRES);
                                mode = 3;
                        }
                        g_free(f);
                }

                return (1);
        }

        /* continue recovering the session file */
        if (mode == 1) {
                // at some point I need to get rid of this; it's just here so recoversing a session file can continue.
                //g_warn_if_reached();
        }

        /* continue reading the bitmap */
        else if (mode == 2) {

                /* not finished yet */
                if (localbin_strong) {
                        if (find_thing(0, -1, -1))
                                return (1);
                }

                /* global thresholding */
                else {
                        if (pbm2bm(NULL, 0))
                                return (1);
                }

                /* finished reading the bitmap */
                cl_ready = 1;
        }

        /* continue reading the pixmap */
        else {

                /* continue reading */
                if (loadpgm(0, NULL, &pixmap, &XRES, &YRES))
                        return (1);

                /* PAGE only is mandatory for graymaps */
                if (pm_t == 8)
                        set_flag(FL_PAGE_ONLY, TRUE);

                /* no predefined zone */
                zones = 0;

                /* finished */
                sess_changed = 0;
                return (0);
        }

        /* set the window title */
        if (batch_mode == 0) {
                // UNPATCHED: swn(pagebase);
        }

        /* list the preferred symbols */
        make_pmc();

        /* consist curr_mc */
        if ((curr_mc < 0) || (tops < curr_mc))
                curr_mc = 0;
        if (curr_mc > tops)
                curr_mc = -1;

        /*
           Initialize windows avoiding to lost the
           current position on PAGE_LIST because
           this window is the one being exhibited during
           a full OCR.
         */
        x0 = dw[PAGE_LIST].x0;
        y0 = dw[PAGE_LIST].y0;
        // UNPATCHED: init_dws(0);
        dw[PAGE_LIST].x0 = y0;
        dw[PAGE_LIST].y0 = y0;

        /* prepare data structures for list_cl */
        list_cl(0, 0, 0, 0, 1);

        /* parameters for page list display */
        // TODO: Make this not suck
        dl_ne[p] = symbols;
        dl_db[p] = doubts;
        dl_r[p] = runs;
        dl_t[p] = ocr_time;
        dl_lr[p] = ocr_r;
        dl_w[p] = words;
        dl_c[p] = classes;
        resync_pagelist(p);
        /* finished */
        sess_changed = 0;
        return (0);
}

/*

Load vocabulary into text array.

*/
void load_vocab(char *f) {
        int r, F;
        char b[8193];

        /* clear the array */
        topt = -1;
        text[0] = 0;

        /* read it by block */
        if ((F = open(f, O_RDONLY)) < 0) {
                db("could not open %s (ok)");
                return;
        }
        while ((r = read(F, b, 8192)) > 0) {
                b[r] = 0;
                totext(b);
        }
        close(F);
}

/*

Load all dictionary entries.

*/
void dict_load(void) {
        char *t;
        int el;
        load_vocab("vocab.txt");

        /* inform the ML parser to return full text instead of word */
        oneword = 0;

        /* loop all dictionary elements */
        for (t = text; (el = get_tag(&t)) != CML_END;) {

                /* a piece of text */
                if (el == CML_TEXT) {

                        /* printf("\n** found text:\n\n---\n%s\n---\n",tagtxt); */
                        g_free(de[topde].val);
                        de[topde].val = g_strdup(tagtxt);
                }

                /* start entry */
                else if ((el == CML_TAG) && (strcmp(tag, "entry") == 0)) {
                        int i, l;

                        /* enlarge buffer de */
                        if (++topde >= desz) {
                                int oldsz = desz;

                                desz = topde + 128;
                                de = g_renew(ddesc, de, desz);
                                for (; oldsz < desz; ++oldsz) {
                                        de[oldsz].b = NULL;
                                        de[oldsz].val = NULL;
                                }
                        }

                        /* default attribute values */
                        de[topde].col = NULL;
                        de[topde].lang = NULL;
                        de[topde].key = NULL;
                        de[topde].dec = NULL;
                        de[topde].gender = NULL;
                        de[topde].tense = NULL;
                        de[topde].orig = NULL;
                        de[topde].parad = NULL;
                        de[topde].type = NULL;
                        de[topde].ver = NULL;

                        /* sum all attribute lenghts */
                        for (i = l = 0; i < attribc; ++i) {

                                /* printf("  %s = %s\n",attrib[i],val[i]); */

                                l += strlen(val[i]) + 1;
                        }

                        /* alloc buffer for attributes */
                        de[topde].b = g_realloc(de[topde].b, l);

                        /* copy attributes */
                        for (i = l = 0; i < attribc; ++i) {

                                strcpy(de[topde].b + l, val[i]);

                                if (strcmp(attrib[i], "col") == 0)
                                        de[topde].col = de[topde].b + l;
                                else if (strcmp(attrib[i], "lang") == 0)
                                        de[topde].lang = de[topde].b + l;
                                else if (strcmp(attrib[i], "key") == 0)
                                        de[topde].key = de[topde].b + l;
                                else if (strcmp(attrib[i], "dec") == 0)
                                        de[topde].dec = de[topde].b + l;
                                else if (strcmp(attrib[i], "gender") == 0)
                                        de[topde].gender = de[topde].b + l;
                                else if (strcmp(attrib[i], "tense") == 0)
                                        de[topde].tense = de[topde].b + l;
                                else if (strcmp(attrib[i], "orig") == 0)
                                        de[topde].orig = de[topde].b + l;
                                else if (strcmp(attrib[i], "parad") == 0)
                                        de[topde].parad = de[topde].b + l;
                                else if (strcmp(attrib[i], "type") == 0)
                                        de[topde].type = de[topde].b + l;
                                else if (strcmp(attrib[i], "ver") == 0)
                                        de[topde].ver = de[topde].b + l;
                                else if (strcmp(attrib[i], "val") == 0)
                                        de[topde].val = de[topde].b + l;

                                l += strlen(val[i]) + 1;
                        }
                }

                /* entry end */
                else if ((el == CML_TAG) && (strcmp(tag, "/entry") == 0)) {

                }

        }

        /* return the ML parser to default behaviour */
        oneword = 1;
}

/*

Select dictionary entry.

*/
int dict_sel(ddesc *e) {
        int i;

        for (i = 0; i <= topde; ++i) {

                if ((de[i].key != NULL) &&
                    (e->key != NULL) && (strcmp(de[i].key, e->key) == 0)) {

                        printf("found %s\n\n%s\n", e->key, de[i].val);
                }
        }

        return (1);
}

/*

Store dictionary entry.

*/
int dict_update(ddesc *e) {
        /* replace */

        /* add */

        return (0);
}

/*

Dump entry field.

*/
void dump_field(char *q, char *n) {
        char *p;

        if (q != NULL) {
                for (p = q; (*p != ' ') && (*p != 0); ++p);
                if (*p)
                        totext(" %s=\"%s\"", n, q);
                else
                        totext(" %s=%s", n, q);
        }
}

/*

Dump all dictionary entries.

*/
int dump_dict(int reset, char *f) {
        static int i, st, pio;
        static FILE *F;

        if (reset) {
                if (f == NULL) {
                        F = NULL;
                        topt = -1;
                } else {
                        if ((F = zfopen(f, "w", &pio)) == NULL)
                                return (0);
                }
                st = 0;
                i = 0;
                return (1);
        }

        /* successfully finished */
        if (st == 3) {
                toasc(F, "</Clara>\n", TNULL, 0, NULL);
                if (F != NULL)
                        zfclose(F, pio);
                hist_changed = 0;
                return (0);
        }

        switch (st) {

                /* magic */
        case 0:
                toasc(F, "<Clara>", TNULL, 0, NULL);
                toasc(F, "\n", TNULL, 0, NULL);
                st = 1;
                i = 0;

                /* unstructured variables */
        case 1:
                st = 2;
                i = 0;
                break;

                /*
                   Dump all entries if output is a file, or only the
                   topmost entry if the output is the array text.
                 */
        case 2:
                if (i > topde) {
                        st = 3;
                        i = 0;
                } else {

                        if ((batch_mode == 0) &&
                            ((F != NULL) && ((i % DPROG) == 0))) {
                                snprintf(mba, MMB,
                                         "now saving entry %d/%d", i,
                                         topde);
                                show_hint(0, mba);
                        }

                        /* preamble */
                        totext("<entry");
                        dump_field(de[i].lang, "lang");
                        dump_field(de[i].key, "key");
                        dump_field(de[i].dec, "dec");
                        dump_field(de[i].gender, "gender");
                        dump_field(de[i].tense, "tense");
                        dump_field(de[i].orig, "orig");
                        dump_field(de[i].parad, "parad");
                        dump_field(de[i].type, "type");
                        dump_field(de[i].ver, "ver");

                        totext(">\n\n");

                        if (de[i].val != NULL)
                                totext(de[i].val);

                        totext("</entry>\n", TNULL, 0, NULL);

                        /* prepare next entry */
                        ++i;
                }
        }

        /* unfinished */
        return (1);

}

/*

Perform the dict operation requested from the command line.

*/
void dict_behaviour(void) {
        /* select */
        if (dict_op == 1) {
                ddesc e;

                e.key = "recusa";
                dict_load();
                dict_sel(&e);
                dump_dict(1, NULL);
                while (dump_dict(0, NULL));
                printf(text);
        }

        /* store with backup */
        else if (dict_op == 2) {
        }

        /* delete last version */
        else if (dict_op == 3) {
        }
}
