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

revision.c: Revision procedures

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "common.h"
#include "gui.h"

/*

Reviewer data

*/
char *reviewer = "nobody";
revtype_t revtype = ANON;

/*

LINK revision data. These are pairs (l,r) of symbols submitted
as revision acts.

*/
lkdesc *lk = NULL;
int lksz = 0, toplk = -1;

/*

Type of action to perform, when the user trains an already
classified symbol. The user selects one from the following
codes:

1 - both the pattern and symbol equal the old transliteration
2 - the symbol transliteration is the new, the pattern is the old
3 - both the pattern and symbol equal the new transliteration

*/
int action_type = 1;
char *r_tr_old, *r_tr_new;
int r_tr_patt, r_tr_symb;

/*

Collect data to process the submission of transliteration and
symbol properties.

*/
int review_tr(adesc *a) {
        /*
           The id of the pattern of the class.
         */
        int b;

        /*
           "the class is untransliterated", "the symbol is classified",
           "the symbol is not the pattern of the class", "the symbol was bad"
           and "the symbol is bad" statuses
         */
        int u, c, np, wb, ib;

        /* the old transliteration and the old flags */
        char *old_tr;
        int old_f;

        /* the new transliteration */
        char *new_tr;

        /* other */
        int i, old_mc, old_cdfc, toolarge;

        /* step */
        static int st = 1;

        /* (devel)

           The function review_tr
           ----------------------

           Process the submission of transliterations. Also
           process the actions that change the properties of the
           current symbol. This is not a simple operation. In
           order to make the interface powerful, the submission of
           a transliteration may change the transliteration of the
           current symbol, and also the transliterations of all
           symbols on its class. Depending on the properties,
           other actions may be performed as well.

         */

        /*

           Decision table (partial)
           ------------------------

           symbol is classified       0 1 1 1 1
           tr. differs from class     - 0 1 1 1
           user input                 - - 1 2 3
           isnotbad                   X * 0 X 0

           train class                - 0 0 0 1
           add patt                   X 0 0 X 0

         */

        /*
           At this point we'll need to criticize the input
           in the future:

           a. If the input is not accented but the symbol seems
           to be accented, then change the input.

           b. If the input is accented but the symbol seems
           to be unaccented, than change the input.

           c. If the input is from anonymous source then
           apply likelihood tests.

         */

        /*
           Decide about asking the user or not.
         */
        if (st == 1) {
                int bm, p;
                char *bm_tr;

                /*
                   If the symbol is classified and the submitted
                   transliteration differ from the class transliteration,
                   then ask the user about what to do.
                 */
                if ((!batch_mode) &&
                    (a->tr != NULL) &&
                    ((bm = mc[a->mc].bm) >= 0) &&
                    ((p = id2idx(bm)) >= 0) &&
                    ((bm_tr = pattern[p].tr) != NULL) &&
                    (strcmp(a->tr, bm_tr) != 0)) {

                        setview(PATTERN_ACTION);
                        r_tr_old = bm_tr;
                        r_tr_new = a->tr;

                        /* fixed by Ben Wong */
                        r_tr_patt = pattern[p].id;
                        r_tr_symb = a->mc;
                        enter_wait("ENTER or OK to proceed, ESC to cancel",
                                   0, 2);
                        st = 2;
                        dw[PATTERN_ACTION].rg = 1;
                        return (1);
                }

                /* otherwise use the default action */
                action_type = 1;
        }

        /* must go back to PAGE */
        if (!batch_mode)
                setview(PAGE);

        st = 1;

        /*
           Save the current values. Maybe some service requires
           curr_mc and cdfc pointing to the current symbol/pattern,
           so we change both values.
         */
        old_mc = curr_mc;
        old_cdfc = cdfc;

        /*
           Make the symbol received from the message the current
           symbol.
         */
        curr_mc = a->mc;

        /* too large? */
        toolarge = (((mc[curr_mc].r - mc[curr_mc].l + 1) > FS) ||
                    ((mc[curr_mc].b - mc[curr_mc].t + 1) > FS));

        /*
           Check if the symbol is classified and also if the its
           class (if any) is untransliterated.
         */
        cdfc = id2idx(b = mc[curr_mc].bm);
        if ((0 <= cdfc) && (cdfc <= topp)) {
                u = (pattern[cdfc].tr == NULL);
                c = 1;
        } else {
                u = 0;
                c = 0;
        }

        /* check if the symbol is not the pattern of the class */
        if (c) {
                unsigned char bm[FS * FS];

                pixel_mlist(a->mc);
                cb2bm(bm);
                np = (memcmp(bm, pattern[cdfc].b, FS * FS) != 0);
        } else
                np = 1;

        /*
           Remember the old transliteration of curr_mc (if any)
           and the old flags.
         */
        if ((mc[curr_mc].tr == NULL) || ((mc[curr_mc].tr)->t == NULL)) {

                old_tr = NULL;
                old_f = 0;
        } else {
                old_tr = alloca(strlen((mc[curr_mc].tr)->t) + 1);
                strcpy(old_tr, (mc[curr_mc].tr)->t);
                old_f = (mc[curr_mc].tr)->f;
        }

        /* "was bad" and "is bad" statuses */
        wb = C_ISSET(old_f, F_BAD);
        ib = C_ISSET(a->f, F_BAD);

        /* (devel)

           0. Remove the current revision vote (if any).

         */
        {
                int obm;
                obm = mc[a->mc].bm;
                rmvotes(REVISION, curr_mc, -1, 0);
                mc[a->mc].bm = obm;
        }

        /* (devel)

           1. Add a REVISION vote for the current symbol,
           informing the submitted transliteration, and compute
           the preferred transliteration considering this new
           vote.

         */
        add_tr(curr_mc, a->tr, REVISION, curr_act, 0, a->a, a->f);
        new_tr = (mc[curr_mc].tr)->t;

        /* (devel)

           2. If the symbol is unclassified or if the transliteration
           differ from the class transliteration then add its bitmap
           as a pattern, unless it's bad or too large.
         */
        if (((!c) || (action_type == 2)) && (!ib) && (!toolarge)) {

                /*
                   This will add the current symbol as a
                   pattern.
                 */
                update_pattern(curr_mc, new_tr, curr_act, a->a, a->pt, -1,
                               a->f);

                /*
                   This will make the symbol its own best match.
                 */
                mc[curr_mc].bm = cdfc;
        }

        /* (devel)

           3. If the symbol is classified and the class is untransliterated
           or if the user asked to change the class transliteration, then
           propagate the submitted transliteration to the entire class.
         */
        else if (((c && u) || (action_type == 3)) && (!ib) && (!toolarge)) {

                /*
                   This will change the transliteration of
                   the pattern of the class.
                 */
                update_pattern(-1, new_tr, curr_act, a->a, a->pt, -1,
                               a->f);

                /*
                   This will resubmit the SHAPE vote of all
                   symbols on the class except curr_mc.
                 */
                for (i = 0; i <= tops; ++i) {
                        if ((i != curr_mc) && (mc[i].bm == b)) {
                                add_tr(i, a->tr, SHAPE, curr_act, mc[i].bq,
                                       a->a, a->f);
                        }
                }
        }

        /* (devel)

           4. If the symbol is classified, it is not the pattern of
           the class, and it was not bad, but it became bad now,
           then remove it from its class, and remove its SHAPE vote.
         */
        else if ((c) && (np) && (!wb) && (ib)) {
                mc[curr_mc].bm = -1;
                rmvotes(SHAPE, curr_mc, -1, 0);
        }

        /* (devel)

           5. If the symbol is the pattern of the class, and it
           was not bad, but it became bad now, then remove it from
           the patterns.
         */
        /*
           else if ((q) && (!wb) && (ib)) {

           rm_pattern(cdfc);
           }
         */

        /*
           6. In all other cases generate a debug message.
         */
        else {
                db("unexpected statuses at review_tr: c=%d,wb=%d,ib=%d", c,
                   wb, ib);
        }

        /* restore the old values */
        curr_mc = old_mc;
        cdfc = old_cdfc;

        return (0);
}

/*

Submit pattern transliteration.

*/
void review_patt(adesc *a) {
        /* other */
        int i, old_cdfc;

        /* (devel)

           The function review_patt
           ------------------------

         */
        old_cdfc = cdfc;

        /*
           Use the symbol information (if any) as pattern ID.
         */
        if (a->mc >= 0)
                cdfc = id2idx(a->mc);
        else
                cdfc = -1;

        /*
           Change the pattern transliteration.
         */
        update_pattern(-1, a->tr, curr_act, a->a, a->pt, -1, a->f);

        /*
           This will resubmit the SHAPE vote of all
           symbols on the class except curr_mc.
         */
        if (a->mc >= 0) {
                for (i = 0; i <= tops; ++i) {
                        if (mc[i].bm == a->mc)
                                add_tr(i, a->tr, SHAPE, curr_act, mc[i].bq,
                                       a->a, a->f);
                }
        }

        /* restore the old values */
        cdfc = old_cdfc;
}

/*

Process the submission of fragment merging.

*/
void review_merge(adesc *a) {
        int k, l;

        int ws, t;
        int i, j, n, m, *wc, *af;
        /*int f; */

        k = a->a1;
        l = a->a2;

        /* total number of closures */
        ws = mc[l].ncl + mc[k].ncl;

        /* build list of closures */
        wc = alloca(sizeof(int) * ws);
        af = alloca(sizeof(int) * ws);
        for (n = 0; n < mc[l].ncl; ++n)
                wc[n] = mc[l].cl[n];
        for (m = 0; m < mc[k].ncl; ++m)
                wc[n + m] = mc[k].cl[m];

        /* create the new symbol if it's not there */
        t = new_mc(wc, ws);

        /*
           Make the new symbol preferred,
           and the old symbols nonpreferred
         */
        C_UNSET(mc[k].f, F_ISP);
        C_UNSET(mc[l].f, F_ISP);
        C_SET(mc[t].f, F_ISP);

        /* update the ps array */
        for (i = 0, j = -1; i <= topps; ++i) {
                if ((ps[i] == k) || (ps[i] == l)) {
                        if (j < 0)
                                ps[j = i] = t;
                        else {
                                memmove(ps + i, ps + i + 1,
                                        (topps - i) * sizeof(int));
                                i = topps;
                                --topps;
                        }
                }
        }
        if (j < 0) {
                if (topps + 1 >= pssz)
                        ps = c_realloc(ps,
                                       (pssz =
                                        (topps + 512)) * sizeof(int),
                                       NULL);
                ps[++topps] = t;
        }

        /*
           For each of the symbols l and k, if it belongs to the
           bookfont and has more than one closure, then that entry
           is marked as a fragment.
         */
        /*
           if ((mc[l].ncl > 1) && ((f=s2p(l)) >= 0)) {
           C_SET(pattern[f].f,F_FRAG);
           }
           if ((mc[k].ncl > 1) && ((f=s2p(k)) >= 0)) {
           C_SET(pattern[f].f,F_FRAG);
           }
         */

        /*
           if the new symbol belongs to the bookfont, make it a
           complete pattern.
         */
        /*
           if ((f=s2p(k)) >= 0) {
           C_UNSET(pattern[f].f,F_FRAG);
           }
         */

        /* the merge result becomes the current symbol */
        curr_mc = t;

        /* must refresh the window */
        redraw_document_window();
}

/*

Process submission of symbol linking.

*/
void review_slink(adesc *a) {
        lkdesc *l;

        /* enlarge buffer */
        if (toplk + 1 >= lksz) {
                lk = c_realloc(lk, (lksz += 128) * (sizeof(lkdesc)), NULL);
        }

        /* store symbol link */
        l = lk + (++toplk);
        l->t = REV_SLINK;
        l->l = a->mc;
        l->r = a->a1;
        l->p = a->rt;

        /* oops.. we're assuming that "a" is an entry of array "act" */
        l->a = (a - act);
}

/*

Process submission of accent linking.

*/
void review_alink(adesc *a) {
        lkdesc *l;

        /* enlarge buffer */
        if (toplk + 1 >= lksz) {
                lk = c_realloc(lk, (lksz += 128) * (sizeof(lkdesc)), NULL);
        }

        /* store symbol link */
        l = lk + (++toplk);
        l->t = REV_ALINK;
        l->l = a->mc;
        l->r = a->a1;
        l->p = a->rt;

        /* oops.. we're assuming that a is an entry of array "act" */
        l->a = (a - act);
}

/*

Process submission of symbol disassemble.

*/
void review_dis(adesc *a) {
        int i, *p /*,f */ , k;

        k = a->mc;

        /* mark the symbol as non-preferred */
        C_UNSET(mc[k].f, F_ISP);

        /*
           Mark each unitary symbol as preferred.

           WARNING: this code assumes that, for each closure i,
           the symbol i is the one whose only closure is i.
         */
        for (i = 0; i < mc[k].ncl; ++i) {
                C_SET(mc[mc[k].cl[i]].f, F_ISP);
                p = alloca((1 + mc[k].ncl) * sizeof(int));

                /* the unitary pattern (if any) is unmarked as fragment */
                /*
                   f = s2p(mc[k].cl[i]);
                   if (f >= 0) {
                   pattern[f].f |= F_FRAG;
                   pattern[f].f ^= F_FRAG;
                   }
                 */
        }

        /* The symbol pattern (if any) is marked as fragment */
        /*
           f = s2p(k);
           pattern[f].f |= F_FRAG;
         */

        /*
           Must remove k from ps array and include each
           unitary symbol.
         */
        for (i = 0; i <= topps; ++i) {
                if (ps[i] == k)
                        ps[i] = mc[k].cl[0];
        }
        if ((pssz - topps - 1) < mc[k].ncl - 1)
                ps = c_realloc(ps,
                               (pssz =
                                (topps + mc[k].ncl + 512)) * sizeof(int),
                               NULL);
        for (i = 1; i < mc[k].ncl; ++i)
                ps[++topps] = mc[k].cl[i];

        redraw_document_window();

}

/*

The function review
-------------------

This function is the entry point for all revision data. Revision
data comes from the web or from user keystrokes when the PAGE
window is active, or when the user submits the form on the PAGE
(SYMBOL) or on the PATTERN (PROPS) windows after changing the
text input field. The "process revision data" OCR step calls the
services process_webdata or from_gui to collect the revision
data, prepare it for processing, and calling review().

If the parameter a is non-null, then it's assumed that it points
to an already initialized structure that will be processed and
won't be added to the log of acts. Otherwise, will read the XML
revision record currently in the "text" buffer.

*/
int review(int reset, adesc *b) {
        static adesc *a;
        int r = 0;

        if (reset) {

                /*

                   Read the XML message and use it to initialize the
                   fields of act[topa].

                 */
                if (b == NULL) {

                        /*
                           Increase topa by 1, extract the fields from the XML
                           message message currently on the buffer text, and use
                           them to initialize act[topa].
                         */
                        recover_acts(NULL);

                        a = act + topa;
                        curr_act = topa;
                } else {
                        a = b;
                }
        }

        /* transliteration handler */
        if (a->t == REV_TR)
                r = review_tr(a);

        /* merge handler */
        else if (a->t == REV_MERGE)
                review_merge(a);

        /* symbol linkage handler */
        else if (a->t == REV_SLINK)
                review_slink(a);

        /* accent linkage handler */
        else if (a->t == REV_ALINK)
                review_alink(a);

        /* disassemble handler */
        else if (a->t == REV_DIS)
                review_dis(a);

        /* pattern handler */
        if (a->t == REV_PATT)
                review_patt(a);

        if (r == 0)
                hist_changed = 1;

        return (r);
}

/*

Add to the revision act a the reviewer data, prints to the "text"
buffer the revision act as an XML message and dump it as a revision
file if web mode is active.

*/
void reviewer_data(adesc *a) {
        /* reviewer data */
        a->r = reviewer;
        a->rt = revtype;
        a->sa = g_get_host_name();
        a->dt = time(NULL);

        /* original revision data */
        a->or = NULL;
        a->ob = NULL;
        a->om = -1;
        a->od = 0;

        topt = -1;
        text[0] = 0;
        dump_acts(NULL, 1);
        while (dump_acts(NULL, 0));
        --topa;

        /* append to the acts dump */
        /*
           printf(text);
         */

        /* write it to web revision file */
        if (web)
                gen_wrf();
}

/*

Entry point for transliterations submitted from keystrokes on the
PAGE or PATTERN windows. The transliteration properties
(alphabet, italic and bold flags, etc) are read from the
interface buttons, and the full data is used to generate a XML
message containing a revision act informing the current symbol,
the submitted transliteration and the default reviewer info.

*/
void gen_tr(char *tr) {
        adesc *a;
        char *ttr;
        int s;

        /* alloc more space for acts */
        if (++topa >= actsz) {
                act =
                    c_realloc(act, (actsz = topa + 100) * sizeof(adesc),
                              NULL);
        }
        a = act + topa;

        /* symbol or pattern? */
        s = (to_rev == REV_TR);

        /* preamble */
        a->t = s ? REV_TR : REV_PATT;
        a->d = pagename;
        a->f = 0;

        /* symbol or pattern */
        a->mc = s ? curr_mc : ((cdfc < 0) ? -1 : pattern[cdfc].id);

        /* properties */
        // UNPATCHED: a->a  = inv_balpha[(int)(button[balpha])];
        a->f = 0;               // UNPATCHED: ((button[bbad]) ? F_BAD : 0);
        /* UNPATCHED:
           a->f |= ((button[bitalic]) ? F_ITALIC : 0);
           a->f |= ((button[bbold]) ? F_BOLD : 0);
         */

        /* UNPATCHED:
           a->pt = (*cm_o_ftro == ' ') ? button[btype] : 0;
         */
        a->pt = 0;

        /* convert the transliteration automatically */
        if ((a->a == GREEK) &&
            (strlen(tr) == 1) &&
            (l2g[(int) (*((unsigned char *) tr))] != NULL)) {

                ttr = l2g[(int) (*((unsigned char *) tr))];
        } else
                ttr = tr;

        /* transliteration */
        a->tr = ttr;

        /* set the alphabet automatically */
        if ((a->a == OTHER) && (strlen(ttr) == 1)) {

                /* digits */
                if (('0' <= ttr[0]) && (ttr[0] <= '9'))
                        a->a = NUMBER;

                /*
                   latin letters
                 */
                else if ((('a' <= tr[0]) && (ttr[0] <= 'z')) ||
                         (('A' <= tr[0]) && (ttr[0] <= 'Z')))

                        a->a = LATIN;
        }

        /* propagability */
        if (!nopropag)
                a->f |= F_PROPAG;

        /* reviewer data */
        reviewer_data(a);
        UNIMPLEMENTED();
}

/*

Generates XML record for the merge of k with l creating t.

*/
void gen_merge(int k, int l) {
        adesc *a;

        /* alloc more space for acts */
        if (++topa >= actsz) {
                act =
                    c_realloc(act, (actsz = topa + 100) * sizeof(adesc),
                              NULL);
        }
        a = act + topa;

        /* preamble */
        a->t = REV_MERGE;
        a->d = pagename;
        a->f = 0;

        /* resulting symbol */
        a->mc = -1;

        /* transliteration */
        a->tr = NULL;
        a->f = 0;

        /* component symbols */
        a->a1 = k;
        a->a2 = l;

        /* reviewer data */
        reviewer_data(a);
}

/*

Generates XML record for disassembling symbol k.

*/
void gen_dis(int k) {
        adesc *a;

        /* alloc more space for acts */
        if (++topa >= actsz) {
                act =
                    c_realloc(act, (actsz = topa + 100) * sizeof(adesc),
                              NULL);
        }
        a = act + topa;

        /* preamble */
        a->t = REV_DIS;
        a->d = pagename;
        a->f = 0;

        /* symbol */
        a->mc = k;

        /* transliteration */
        a->tr = NULL;
        a->f = 0;

        /* reviewer data */
        reviewer_data(a);
}

/*

Generates XML record for symbol link from curr_mc to k.

*/
void gen_slink(int k) {
        adesc *a;

        /* alloc more space for acts */
        if (++topa >= actsz) {
                act =
                    c_realloc(act, (actsz = topa + 100) * sizeof(adesc),
                              NULL);
        }
        a = act + topa;

        /* preamble */
        a->t = REV_SLINK;
        a->d = pagename;
        a->f = 0;

        /* symbols */
        a->mc = curr_mc;
        a->a1 = k;

        /* transliteration */
        a->tr = NULL;

        /* reviewer data */
        reviewer_data(a);
}

/*

Generates XML record for accent link from curr_mc to k.

*/
void gen_alink(int k) {
        adesc *a;

        /* alloc more space for acts */
        if (++topa >= actsz) {
                act =
                    c_realloc(act, (actsz = topa + 100) * sizeof(adesc),
                              NULL);
        }
        a = act + topa;

        /* preamble */
        a->t = REV_ALINK;
        a->d = pagename;
        a->f = 0;

        /* symbols */
        a->mc = curr_mc;
        a->a1 = k;

        /* transliteration */
        a->tr = NULL;

        /* reviewer data */
        reviewer_data(a);
}

/*

Reset the book.

This service is intended to destroy all revision data and rebuild
it from the log of acts. That's a very important maintenance
operation, but it's currently unfinished. The code below is being
completed, it's able to rebuild the patterns file.

*/
void reset() {
        int i;
        adesc *a;

        /* destroy session files */

        /* destroy patterns */

        /* reprocess the acts */
        for (i = 0, a = act; i <= topa; ++i, ++a) {

                /* load document, if required */
                if (strcmp(a->d, pagename) != 0) {
                        int c;

                        printf("going to dump page %s\n", pagename);

                        if (dump_session(session_file, 1)) {
                                while (dump_session(session_file, 0));
                        }
                        free_page();

                        c = pagenbr(a->d);
                        if (c < 0)
                                fatal(DI, "page not found, cannot proceed");

                        printf("going to read page %s\n", a->d);

                        /* ugly! */
                        if (load_page(c, 1, 0)) {
                                while (load_page(c, 0, 0));
                        }
                }

                /* process act[i] (by now only revision acts) */
                if (act[i].t == REV_TR) {
                        printf("will process act %d\n", i);
                        curr_act = i;
                        review(1, act + i);
                        while (review(0, act + i));
                }

        }

        printf("reset finished\n");

}

/*

Try to move the graphic cursor to the next untransliterated
symbol.

*/
void next_symb(void) {
        int cur_dw;

        /*
           When in PATTERN window, go to next
           untransliterated pattern.
         */
        if (dw[PATTERN].v) {
                // UNPATCHED: right();
                return;
        }

        /*
           When the caller did not inform a transliteration,
           we avoid to try to advance the cursor to the next
           doubt because we're processing web submissions.
         */

        /*
           Try to move to the next untransliterated CHAR (TODO).
         */
        if (0) {
                int cw, cl;

                /* current word and line */
                if (curr_mc >= 0)
                        cw = mc[curr_mc].sw;
                if (cw >= 0)
                        cl = word[cw].tl;

                while ((cl >= 0) && (uncertain(mc[curr_mc].tc))) {

                        curr_mc = mc[curr_mc].E;
                        while ((cl >= 0) && (curr_mc < 0)) {

                                /* try next word */
                                if (cw >= 0) {
                                        cw = word[cw].E;
                                        if (cw >= 0)
                                                curr_mc = word[cw].F;
                                }

                                /* try next line */
                                else {
                                        if (++cl > topln)
                                                cl = -1;
                                        else {
                                                cw = line[cl].f;
                                                curr_mc = word[cw].F;
                                        }
                                }
                        }
                }
        }

        /*
           Make your best efforts to try to make curr_mc
           point to an untransliterated CHAR.
         */
        if ((curr_mc < 0) || (tops < curr_mc)) {
                for (curr_mc = 0; (curr_mc <= tops) &&
                     ((mc[curr_mc].tc != CHAR) ||
                      (uncertain(mc[curr_mc].tc))); ++curr_mc);
        }
        if (tops < curr_mc)
                curr_mc = -1;

        /*
           Put the newly selected symbol under edition.
         */
        dw[PAGE_SYMBOL].rg = 1;
        cur_dw = CDW;
        if (dw[PAGE].v) {
                CDW = PAGE;
                check_dlimits(1);
                CDW = cur_dw;
        }
        redraw_document_window();

}

/*

Summarizes the transliteration information for symbol m.

*/
void summarize(int m) {
        trdesc *a, *tr;
        int N, A, E, S, T, U;
        int N2, A2, E2, S2, T2, U2;
        int n;

        /* recompute the preference of each transliteration */
        for (a = mc[m].tr, n = 0; a != NULL; a = a->nt, ++n) {
                vdesc *v;

                N = A = E = S = T = U = 0;
                for (v = a->v; v != NULL; v = (vdesc *) (v->nv)) {
                        if (v->orig == REVISION) {
                                int t;

                                t = act[v->act].rt;
                                if (t == ANON)
                                        A = 1;
                                else if (t == TRUSTED)
                                        T = 1;
                                else if (t == ARBITER)
                                        U = 1;
                        } else if (v->orig == SHAPE)
                                N = 1;
                        else if ((v->orig == SPELLING) ||
                                 (v->orig == COMPOSITION))
                                E = 1;
                }
                a->pr =
                    N + A * 10 + E * 100 + S * 1000 + T * 10000 +
                    U * 100000;
        }

        /* sort the transliterations by preference */
        if (mc[m].tr != NULL) {
                int i;
                trdesc **t;

                t = alloca(n * sizeof(trdesc *));
                for (a = mc[m].tr, i = 0; a != NULL; a = a->nt, ++i) {
                        t[i] = a;
                }
                qs((char **) t, 0, n - 1,
                   ((char *) (&(t[0]->pr))) - ((char *) (t[0])), 1);
                mc[m].tr = t[0];
                for (i = 1; i < n; ++i) {
                        t[i - 1]->nt = t[i];
                }
                t[n - 1]->nt = NULL;
        }

        /*
           Compute the number of transliterations and extract
           the fields of the first two transliterations.
         */
        for (tr = NULL, n = 0, a = mc[m].tr; a != NULL; ++n, a = a->nt) {
                if (n == 0) {
                        tr = a;
                        extr_tp(&U, &T, &S, &E, &A, &N, a->pr);
                } else if (n == 1)
                        extr_tp(&U2, &T2, &S2, &E2, &A2, &N2, a->pr);
        }

        /* cases where the first transliteration prevails */
        if (((n == 1) && (tr->pr > 0)) ||
            ((n >= 2) && ((U > U2) ||
                          (T > T2) ||
                          (S > S2) || (E > E2) || (A > A2) || (N > N2)))) {

                mc[m].tc = classify_tr(mc[m].tr->t);

        } else if (n == 0)
                mc[m].tc = UNDEF;
        else
                mc[m].tc = DUBIOUS;

        /* compute the alignment of the first transliteration */
        if ((mc[m].tr != NULL)
            /* && (strlen(mc[m].tr->t) >= 1) Obsolete: alignment for multichar transliteration managed. (GL) */
            ) {

                mc[m].va = (mc[m].tr == NULL) ? -1 : tr_align(mc[m].tr->t);
        }
}

/*

Performs four different operations:

    remove votes of origin o on symbol k
    remove votes from act a on symbol k
    remove votes of origin o on all symbols
    remove votes from act a on all symbols

If k>=0 and o>=0, performs the first. If k>=0 and o<0 performs the
second. If k<0 and o>=0 performs the third. If k<0 and o<0 performs
the fourth.

If nd is nonzero, do not destroy the best match field after
removing votes. The nd parameter must be set to 0 always,
except when calling rmvotes from the synchronization
procedure.

*/
void rmvotes(int o, int k, int a, int nd) {
        int p, q, r;
        trdesc *t, *lt;
        vdesc *v, *lv;

        /* range */
        if (k < 0) {
                p = 0;
                q = tops;
        } else {
                p = k;
                q = k;
        }

        /* visit symbols within the range */
        for (k = p; k <= q; ++k) {

                /* visit all transliterations */
                for (r = 0, lt = NULL, t = mc[k].tr; t != NULL;) {

                        /* remove each vote with type t of referring act n */
                        for (lv = NULL, v = t->v; v != NULL;) {
                                if (((o >= 0) && (v->orig == o)) ||
                                    ((o < 0) && v->act == a)) {

                                        /*
                                           If the vote origin is SHAPE or REVISION, must clear the
                                           best match field. In the first case, because the bm field
                                           stores the pattern from which this vote (being removed)
                                           was deduced. In the second, because we're just visiting
                                           the symbol from which the pattern was derived (the bm
                                           field of such symbols is used to store the derived pattern).
                                         */
                                        if ((!nd) &&
                                            ((v->orig == SHAPE) ||
                                             (v->orig == REVISION)))
                                                mc[k].bm = -1;

                                        /*
                                           If the vote origin is SHAPE, must clear the F_SS
                                           status and reset lfa.
                                         */
                                        if ((!nd) && (v->orig == SHAPE)) {
                                                C_UNSET(mc[k].f, F_SS);
                                                mc[k].lfa = -1;
                                        }

                                        /* removing vote from symbol k */
                                        if (lv == NULL)
                                                t->v = (vdesc *) (v->nv);
                                        else
                                                lv->nv = v->nv;
                                        c_free(v);
                                        v = (lv ==
                                             NULL) ? t->v : ((vdesc
                                                              *) (lv->nv));
                                        r = 1;
                                } else {
                                        lv = v;
                                        v = (vdesc *) (v->nv);
                                }
                        }

                        /* no vote anymore: remove the transliteration */
                        if (t->v == NULL) {

                                /* removing transliteration from symbol k */
                                if (lt == NULL)
                                        mc[k].tr = t->nt;
                                else
                                        lt->nt = t->nt;
                                c_free(t);
                                t = (lt == NULL) ? mc[k].tr : lt->nt;
                                r = 1;
                        } else {
                                lt = t;
                                t = t->nt;
                        }
                }

                /* summarize again */
                if (r) {
                        summarize(k);
                        dw[PAGE_OUTPUT].rg = 1;
                }
        }
}

/*

Nullify the revision act n.

This service removes all permanent data that refer the revision
act n as its origin. Discardable data, like the current words or
OCR output, are not changed.

The concept of nullification is currently being reworked. All GUI
features that call this service were made inactive.

*/
void nullify(int n) {
        int k, i;

        /*
           Remove all votes that refer the revision act n. These
           may be REVISION or SHAPE votes.
         */
        rmvotes(-1, -1, n, 0);

        /*
           Patterns that refer the act n (there should be at most
           one such pattern) become untransliterated.
         */
        if (n >= 0) {
                for (k = 0; k <= topp; ++k) {

                        if (pattern[k].act == n) {
                                pattern[k].act = -1;
                                if (pattern[k].tr != NULL) {
                                        c_free(pattern[k].tr);
                                        pattern[k].tr = NULL;
                                }
                                pattern[k].ts = 0;
                        }
                }
        }

        /*
           Remove links that refer the act n (there should be at
           most one such link).
         */
        for (i = 0; i <= toplk; ++i) {
                if (lk[i].a == n) {
                        memmove(lk, lk + 1,
                                sizeof(lkdesc) * (toplk - i - 1));
                        --toplk;
                }
        }

        /* mark act n as nullified */
        if (n >= 0)
                C_SET(act[n].f, F_ISNULL);
}

/*

Process the revision operation requested by the user through the
gui. The operation is defined by the code to_rev and the
parameters to_tr and to_arg. Additional data (italic flag,
propagability, etc) is taken from the buttons.

*/
int from_gui(void) {
        int r = 1;

        /* prepare to submit transliteration */
        if (to_rev == REV_TR) {

                /*
                   Generates a revision record for the current symbol
                   using the transliteration to_tr, the statuses of the
                   buttons and the nopropag flag.
                 */
                gen_tr(to_tr);

                /* reset nopropag */
                if (nopropag) {
                        nopropag = 0;
                        mb[0] = 0;
                        // UNPATCHED: redraw_inp = 1;
                }

                /*
                   try to move the graphic cursor to the next
                   untransliterated symbol.
                 */
                next_symb();
        }

        /* prepare to submit pattern transliteration */
        else if (to_rev == REV_PATT) {

                /*
                   Generates a revision record for the current symbol
                   using the transliteration to_tr, the statuses of the
                   buttons and the nopropag flag.
                 */
                gen_tr(to_tr);

                /* reset nopropag */
                /*
                   if (nopropag) {
                   nopropag = 0;
                   mb[0] = 0;
                   redraw_inp = 1;
                   }
                 */

                /*
                   try to move to the next pattern.
                 */
                if (cdfc < 0)
                        cdfc = topp;
                else;
                // UNPATCHED: right();
        }

        /* prepare to merge fragment to symbol */
        else if (to_rev == REV_MERGE) {
                int k, l;

                k = curr_mc;
                l = to_arg;

                /* generate revision record */
                gen_merge(k, l);
        }

        /* prepare to disassemble symbol */
        else if (to_rev == REV_DIS) {
                int k;

                k = curr_mc;

                /* ignore symbols without at least two components */
                if (mc[k].ncl <= 1)
                        r = 0;

                else {
                        gen_dis(curr_mc);
                        curr_mc = mc[k].cl[0];
                }
        }

        /* prepare to link symbols */
        else if (to_rev == REV_SLINK) {
                gen_slink(to_arg);
        }

        /* prepare to link accent */
        else if (to_rev == REV_ALINK) {
                gen_alink(to_arg);
        }

        /* no operation at all */
        else {
                r = 0;
        }

        return (r);
}

/*

Synchronize the just-loaded page with the revision status.

To synchronize the page mean:

1. All SHAPE votes must be updated to the current transliteration
of each pattern. The SHAPE votes that refer removed patterns must
be deleted.

2. Remove all REVISION votes that refer nullificated acts.

3. Remove all links that refer nullificated acts.

*/
void synchronize(void) {
        int i, k, id;

        /* Remove all shape votes */
        rmvotes(SHAPE, -1, -1, 1);

        /*
           If one best match field points to a non-existing
           ID, reset it and the lfa too.

           Regenerate SHAPE votes for all symbols whose
           best match field is well defined.
         */
        for (k = 0; k <= tops; ++k) {
                id = mc[k].bm;
                if (id >= 0) {
                        pdesc *p;

                        /* reset bm and lfa */
                        if ((i = id2idx(id)) < 0) {
                                mc[k].lfa = -1;
                                mc[k].bm = -1;
                        }

                        /* regenerate SHAPE votes */
                        else if ((p = pattern + i)->tr != NULL) {
                                add_tr(k, p->tr, SHAPE, p->act, mc[k].bq,
                                       p->a, p->f);
                        }
                }
        }

        /*
           Remove all REVISION votes that refer nullificated
           acts.

           Currently this is unnecessary because the interface
           only permit to nullify acts that refer the currently
           loaded page.
         */
}
