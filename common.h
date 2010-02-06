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

common.h: Shared declarations

*/

#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <glib.h>
/* version number */
extern char *version;

/*

Epsilon.

*/
extern float eps;

/*

Alphabets.

Remark: due to the pattern types accounting features, alphabets IDs
must be in the range [0..31].

*/
#define OTHER 0
#define LATIN 1
#define GREEK 2
#define CYRILLIC 3
#define HEBREW 4
#define ARABIC 5
#define KANA 6
#define NUMBER 7
#define IDEOGRAM 8

/*

Alphabet record

The alphabet record is the basic amount of data available for each
letter from known alphabets. The letter name in English,
like "alpha". The codes from the capital and the small letters are
token from ISO-8859-X, where X depends on the alphabet. The bitmaps
are used by the alphabet mapping display facility, in order to
allow entering non-latin transliteration from a latin keyboard.
For each letter we also store the mandatory and acceptable signs
(acents and others) for that letter.

*/
typedef struct {
        char *LN;               /* letter name in English (capital) */
        char *ln;               /* letter name in English (lower) */
        unsigned char cc;       /* ISO-8859-X code (capital) */
        unsigned char cbm[16];  /* 8x16 bitmap (capital) */
        unsigned char sc;       /* ISO-8859-X code (small) */
        unsigned char sbm[16];  /* 8x16 bitmap (small) */
        unsigned a;             /* acceptable signs */
        unsigned m;             /* mandatory signs */
        unsigned char l;        /* latin (ASCII) mapping for latin keyboards */
        char va;                /* vertical alignment */
} alpharec;

/* Records are grouped in arrays per alphabet */
extern alpharec latin[];
extern alpharec greek[];
extern alpharec cyrillic[];
extern alpharec hebrew[];
extern alpharec arabic[];
extern alpharec kana[];
extern alpharec number[];
extern alpharec *alpha;

/* size of each alphabet */
extern int latin_sz,
    greek_sz,
    cyrillic_sz,
    hebrew_sz, arabic_sz, kana_sz, number_sz, max_sz, alpha_sz, alpha_cols;

typedef struct _CFile CFile;

/*

Per-alphabet alignments.

*/
extern int l_align[256];
extern int g_align[256];
extern int n_align[256];

/* inverse mapping for the alphabet button */
#define MAX_ALPHA 100
extern char inv_balpha[MAX_ALPHA];

/*

Maximum filename length and X fontspec length.

*/
#define MAXFNL 256

/* (devel)

Return codes
------------

When Clara OCR exits, the exit code will diagnose the
finalization status:

  0 clean
  1 data inconsistency
  2 buffer overflow
  3 invalid field
  4 internal error
  5 memory exhausted
  6 X error
  7 I/O error
  8 bad input

*/
#define DI 1
#define BO 2
#define FD 3
#define IE 4
#define ME 5
#define XE 6
#define IO 7
#define BI 8

/* types for dump */
#define TNULL 0
#define TCHAR 1
#define TSHORT 2
#define TINT 3
#define TFLOAT 4
#define TSTR 5

/*

8-bit internal palette codes

Remark: the codes for BLACK, GRAY, WHITE, DARKGRAY and VDGRAY
must be the integers from 0 to 4, otherwise there will be
memory corrution when the colormap used to export PBM is
handled.

*/
#define BLACK 0
#define GRAY 1
#define WHITE 2
#define DARKGRAY 3
#define VDGRAY 4
#define C_MASK 3
#define D_MASK 32
extern char *cschema;

typedef enum {
        FL_ABAGAR_ACTIVE,       // CM_G_ABAGAR
        FL_ALPHABET_LATIN,      // CM_A_LATIN
        FL_ALPHABET_NUMBERS,    // CM_A_NUMBER
        FL_ATTACH_LOG,          // CM_G_LOG
        FL_ATTACH_VOCAB,        // CM_G_VOCAB
        FL_AUTO_CLASSIFY,       // CM_E_AC
        FL_CURRENT_ONLY,        // CM_O_CURR
        FL_OMIT_FRAGMENTS,      // CM_V_OF
        FL_ONLY_DOUBTS,         // CM_E_DOUBTS / cm_e_od
        FL_PAGE_ONLY,           // CM_O_PGO
        FL_PATSORT_HEIGHT,      // CM_E_SH
        FL_PATSORT_MATCHES,     // CM_E_SM
        FL_PATSORT_NPIXELS,     // CM_E_SN
        FL_PATSORT_PAGE,        // CM_E_SP
        FL_PATSORT_TRANSLIT,    // CM_E_ST
        FL_PATSORT_WIDTH,       // CM_E_SW
        FL_REPORT_SCALE,        // CM_D_RS
        FL_RESCAN,              // CM_E_RESCAN
        FL_SEARCH_UNEXPECTED_MISMATCHES,        // CM_G_SUM
        FL_SHOW_BORDER,         // CM_B_BORDER
        FL_SHOW_COMPARISONS,    // CM_V_CMP
        FL_SHOW_COMPARISONS_AND_WAIT,   // CM_V_CMP_K
        FL_SHOW_CLASS,          // CM_V_CC
        FL_SHOW_LOCALBIN_PROGRESS,      // CM_G_LB
        FL_SHOW_MATCHES,        // CM_V_MAT
        FL_SHOW_MATCHES_AND_WAIT,       // CM_V_MAT_K
        FL_SHOW_PATTERN_BORDER, // CM_B_HB
        FL_SHOW_PATTERN_SKEL,   // CM_B_HS
        FL_SHOW_SKELETON_TUNING,        // CM_V_ST
        FL_SHOW_SKELETONS,      // CM_B_SKEL
        FL_SHOW_WEBCLIP,        // CM_V_WCLIP
        FL_NFLAGS,
} flag_t;

/*

OCR steps.

After creating a new step, change LAST_STEP and add the new
step to the 

*/
#define OCR_SAVE 1
#define OCR_LOAD 2
#define OCR_PREPROC 3
#define OCR_BLOCK 4
#define OCR_SEG 5
#define OCR_CONS 6
#define OCR_OPT 7
#define OCR_REV 8
#define OCR_CLASS 9
#define OCR_GEOMERGE 10
#define OCR_BUILD 11
#define OCR_SPELL 12
#define OCR_OUTP 13
#define OCR_DOUBTS 14
#define OCR_OTHER 15
#define LAST_STEP 15

/*

Revision operations.

*/
#define REV_TR 1
#define REV_MERGE 2
#define REV_SLINK 3
#define REV_ALINK 4
#define REV_DIS 5
#define REV_PATT 6

/*

delta for progress warnings.

*/
#define DPROG 25

/*

FS ("font size") is the size of the pattern bitmap (the pattern
bitmap is a square FSxFS). FS must be a multiple of 32. If you
change FS, not only the OCR must be recompiled, but all existing
fonts will need to be converted.

BMS is the bitmap size measured in bytes, and BLS is the bitmap
line size measured in bytes.

*/
#define FS 96
#define BMS (FS*(FS/8))
#define BLS (FS/8)

/*

LINKMUL*FS is the total width that an horizontal link may have.

*/
#define LINKMUL 3
#define LFS (LINKMUL*FS)

/*

The pixmap

*/
extern unsigned char *pixmap;
extern int thresh_val, pm_t;
extern int graydist[];
extern int pp_deskew, pp_bal, pp_double, pp_deskew_accurate;
extern float pp_dark, pp_avoid_links;
extern int p_match;
extern int bin_method;

/*

Binarization methods

*/
#define h_thresh (bin_method == 2)
#define localbin ((bin_method == 3) || (bin_method == 4))
#define localbin_strong (bin_method == 4)

/* (devel)

Global variables
----------------

Clara OCR uses a lot of global variables. Large data structures,
flags, paths, etc, use stored on global variables. In some cases we
use a naming strategy to make the code more readable. The important
cases are:

a. The main data structures of Clara OCR are global arrays that grow
as required. The following a convention was created for the names
associated with these arrays:

    structure    type    array    top    size
   --------------------------------------------
    act          adesc   act      topa   actsz
    closure      cldesc  cl       topcl  clsz
    symbol       sdesc   mc       tops   ssz
    pattern      pdesc   pattern  topp   psz
    link         ldesc   lk       toplk  lksz
    ptype        ptdesc  pt       toppt  ptsz

The "top" is the last used entry (initial value -1). The "size"
is the total size of the allocate memory block for that array
(initial value 0). So the relation (top < size) must always be
true.

b. Menus are referred by their registration indexes. These indexes are
stored on variables named CM_X. The menu items registration indexes
are stored on variables named CM_X_SOMETHING (all capital). If the
item has an associated flag, the flag is named cm_x_something (all
small).

*/

/* (devel)

Acts and transliterations
-------------------------

The "acts" or "revision acts" are the human interventions for
training a symbol, merging a fragment to one symbol, etc.

As the human interventions are the more precious
source of information, Clara logs all revision acts, in
order to be able to reuse them.

The transliterations are obtained from the revision acts, so
each transliteration refers one (or more) revision acts, and
also inherits some properties from that act (or those acts).

The acts are on the book scope, and not on the page scope. The acts
are stored on the file "acts" on the work directory.

Each act stores some data about the reviewer and also the submission
date. As we plan to reuse revision data, each act also stores some
data about the "original reviewer" and the "original submission
date". These fields are meaningful only for reused acts.

*/

/*

Types

1 = symbol transliteration
2 = merge
3 = word definition (through its symbols)

*/
typedef struct {

        /* preamble */
        char t;                 /* type */
        char *d;                /* document */
        int f;                  /* flags */

        /* symbols */
        int mc;                 /* main symbol */
        int a1;                 /* auxiliar 1 */
        int a2;                 /* auxiliar 2 */

        /* these are relevant to type REV_TR */
        char a;                 /* alphabet */
        short pt;               /* pattern type */
        char *tr;               /* transliteration */

        /* reviewer data */
        char rt;                /* reviewer type (anon, trusted, arbiter) */
        char *r;                /* reviewer (email address or host fqdn) */
        time_t dt;              /* date of submission */
        char *sa;               /* internet address of originating computer */

        /* original reviewer data */
        char *or;               /* original reviewer (email address or host fqdn) */
        char *ob;               /* original book */
        int om;                 /* original symbol */
        time_t od;              /* original submission date */

} adesc;

/* the acts */
extern adesc *act;
extern int actsz, topa;
extern int curr_act;

/*

Vote types and structure.

*/
#define REVISION 0
#define SHAPE 1
#define SPELLING 2
#define COMPOSITION 3
#define SPECIAL 4
typedef struct {
        char orig;              /* REVISION, SHAPE or SPELLING */
        int act;                /* revision act */
        char q;                 /* quality */
        struct vdesc *nv;       /* next vote */
} vdesc;


/* (devel)

Symbol transliterations
-----------------------

Clara OCR maintains a list of 0 or more proposed or deduced
transliterations for each symbol. Along the OCR process, each
transliteration receives "votes" from reviewers (REVISION votes)
or from machine deduction heuristics, based on shape similarity
(SHAPE votes) or on spelling (SPELLING votes).

So the choice of the "best" transliteration is performed through
election. Votes are stored on structures of type vdesc, and
transliterations are stored on structures of type trdesc. Each
symbol stores a pointer for a (possibly empty) list of
transliterations and each transliteration stores a pointer
for a (possibly empty) list of votes.

So, for instance, when one classifier deduces that one symbol is
"a", it generates a "vote" for the transliteration of that symbol
to be "a". At the same time, another heuristic could generate
another vote for the transliteration to be, say, "o". The diagram
illustrates this situation:

   sdesc  ---> trdesc ("a")  ---> trdesc ("o")
                 |                  |
                 +-vdesc            + vdesc
                 |
                 +-vdesc

In this case, the transliteration "a" has two votes, one from the
classifier and another from, say, revision and the transliteration "o"
has one vote.

As the total stored information about one symbol may be large, Clara
maintains for each symbol its "transliteration class", used by the
heuristics to categorize each symbol and also to test the current
transliteration status (is it known? is it dubious?), frequently used
along the source code.

*/
typedef struct {
        int pr;                 /* preference */
        void *nt;               /* next transliteration */
        int n;                  /* number of confirmations */
        int f;                  /* flags */
        char a;                 /* alphabet (latin, greek, etc) */
        vdesc *v;               /* votes for this transliteration */
        char *t;                /* the transliteration */
} trdesc;


/* (devel)

Internal representation of pages
--------------------------------

Even for non-developers, a knowledge of the internal data
structures used by Clara OCR is required for fine tuning and to
make simple diagnostics.

The basic elements stored are the "closures". Sets of one or more
closures are called "symbols". Symbols are arranged in lists
forming "words". The words are arranged in lists forming "lines".


Closures
--------

Closures of black pixels by contiguity are a first attempt to
identify the atomic symbols of the document. The name "closure"
is of course due to the consideration of the contiguity as a
relation (in the mathematical sense of the word). Starting (for
instance) from (i,j), we compute the set of black pixels ("X" and
"*" in the figure). The limits (l,r,t,b) define the bounding box
of the closure.

          l i    r
      +---+-+----+---+
      |              |
    t +   XX XXXX    |
      |    XX   XX   |
    j +    X*   XX   |
      |    XX   XX   |
    b +    XX   XX   |
      |              |
      +--------------+

When loading a document, the OCR computes all its closures and
use an array to store them. When the session file is written, the
closures are stored in CML format. Note that, if required, the
closures may be recomputed from the document, because the
document and the closure computing algorithm determine the index
that each closure will have on the array.

*/

/*

Closure struct.

*/
#define MAXSUP 10
#define MDV 20
typedef struct {
        int id;
        char type;              /* graphic component type */
        int l, r, t, b;         /* {left,right,top,bottom}-most coordinate */
        unsigned char *bm;      /* bitmap */
        int nbp;                /* number of black pixels */
        short seed[2];          /* seed for closure computation */
        int *sup;               /* list of the symbols it belongs to */
        int supsz;              /* size of the memory buffer sup */
} cldesc;

/* the closures */
cldesc *cl;
extern int topcl, clsz;
extern int *clx, *cly;

/* (devel)

Symbols
-------

As one character of the document may be composed by two or more
closures (for instance when it's broken), it's convenient to work not
with closures, but with sets of closures. So we define the concept of
"symbol" as being a set of one or more closures. Initially, the OCR
generates one unitary symbol for each closure. Subsequent steps may
define new symbols composed by two or more closures.

For instance, let's present three closures that do not correspond
to atomic symbols: "a" and "i" linked (one closure) and a broken
"u" (two closures). As a principle, Clara OCR do not try to break
closures into smaller closures. Instead of that, the
classification heuristic try to compose various patterns to
resolve symbols like the "ai" in the figure. Concerning the "u",
the classification heuristic is expected to merge the two
closures into one symbol and apply a "u" pattern to resolve it.


            l            r     l r l    r
      +-----+------------+-----+-+-+----+--+
      |                                    |
    t +                XX                  |
      |                XX                  |
      |                                    |
      |      XXXXX    XXX      XXX   XXX   + t
      |     X     XX   XX       XX    XX   |
      |           XX   XX       XX    XX   |
      |      XXXXXXX   XX       XX    XX   |
      |     X     XX   XX       XX    XX   |
      |     X     XX   XX       XX    XX   |
    b +      XXXXX XXXXXXX       XX  XXXX  + b
      |                                    |
      +------------------------------------+


As a principle, Clara OCR won't merge dots and accents into
characters. So an "i" will generally be formed by two individual
symbols (the dot and the body). The heuristics that build the OCR
output are expected to compose these two symbols into one ASC
character. The same applies for "j" and the accents (acute,
grave, tilde, etc) found on various european languages.


          l  r
      +---+--+-------+
      |              |
    t +    XX        |
      |    XX        |
      |              |
      |   XXX        |
      |    XX        |
      |    XX        |
      |    XX        |
      |    XX        |
    b +   XXXX       |
      |              |
      +--------------+


The preferred symbols
---------------------

One same closure may belong to more than one symbol. This is
important in order to allow various heuristic trials. For
instance, the left closure of the "u" on the preceding section
could be identified as the body of an "i". In this case however
we would not find its dot. So the heuristic could try by chance
another solution, for instance to join it with the nearest
closure (in that case, the right closure of the "u") and try to
match it with some pattern of the font.

So the OCR will need to choose, from all symbols that contain a
given closure, the one to be preferred. In fact, Clara OCR
maintains dynamically a partition of the set of closures on
"preferred" symbols. This is the ps array. Some manual
operations, like fragment merging and symbol disassembling
(activated by the context menu on the page tab), change that
partition dinamically, as well as some automatic procedures, like
the merge step on the OCR run.


The sdesc structure and the mc array
------------------------------------

Each symbol is stored in a sdesc structure. Those structures form
the mc array. Once created, a symbol is never deleted. So it's
index on the mc array identifies it (this is important for the
web-based revision procedure). Note that closures and symbols are
numbered on a document-related basis. The set of closures that
define one symbol never changes. So the symbol bounding box and
the total number of black pixels also won't change either.

So two different entries of the mc array never have the same set
of closures. The entries of the mc array are created by the
new_mc service.  When some procedure tries to create a new
symbol informing a list of closures for which already
exists a symbol, the service new_mc detects it and returns
to the caller not the index of a newly created symbol, but
the index of that already created one.


Font size
---------

The font size is important for classifying all book symbols on
pattern "types". For instance, books generally use smaller
letters for footnotes. This classification is performed
automatically by Clara OCR and presented by the "PATTERN (types)"
window.

Clara OCR generally uses millimeters for presenting sizes, but
we'll soon express sizes in "points". Let's see an example. One
inch corresponds to 72.27 printer's point (pt) (The METAFONTBook
pg 21, note). So when using 600 dpi, each pt will correspond to
600/72.27 = 8.3 pixels. For 10 point roman characters, Knuth
defines the height of lowercase letters as being 155/36 pt, so
35.7 pixels for us. Therefore, to compute the font size (f) from
the height in pixels (h) of one lowercase letter, the formula is
f = 10*h/35.7.


Symbol alignment
----------------

The vertical alignment of symbols is important for various
heuristics. For instance, the vertical line from a broken "p"
matches an "l", but using alignement tests we're able to refuse
this match.

The current Clara OCR alignment support was developed for the Latin
alphabet, and is being adapted for other alphabets. Four vertical
alignemnt positions are considered. These positions are referred as
usual (ascent, baseline and descent). We use the Knuth's identifier
"x_height" to refer the height of lowercase letters without ascenders.


  A XXX                     XXXXXXXXX         
     XX                      XX      X	       
     XX                      XX      XX       
     XX                      XX      XX       
  X  XX XXXXX   XX  XXXXX    XX      X      XXXX
     XXX     X   XXX     X   XXXXXXXX     XX    XX
     XX      XX  XX      XX  XX      X   XX      XX
     XX      XX  XX      XX  XX      XX  XXXXXXXXXX
     XX      XX  XX      XX  XX      XX  XX   
     XX      XX  XX      XX  XX      XX  XX   
     XXX     X   XXX     X   XX      X    XX    XX  XX
  B  XX XXXXX    XX XXXXX   XXXXXXXXX       XXXX    XXX
                 XX                                   X
                 XX                                   X
                 XX                                  X
  D             XXXX                          


  A (0) .. ascent (Knuth asc_height)
  X (1) .. x_height
  B (2) .. baseline
  D (3) .. descent (Knuth desc_depth)

So in the figure we say that the alignment of "b" and "B" is 02, the
alignment of "p" is 13, the alignment of "e" is 12, and the alignment
of the comma is 23. A period has alignment 22. The dot of an "i" and
accents have alignment 00. In fact, the positions 1 and 2 use to be
well defined: all lowercase letters have the same height, and all
symbols use the same baseline. However, positions 0 and 3 are not so
well defined. For instance, on some printed books "t" and "l" have
different heights.

*/
#define MAXPS 100
typedef struct {
        int id;
        /* fundamental */
        int ncl;                /* number of closures */
        int *cl;                /* list of closures */

        /* geometric data */
        int l, r, t, b;         /* geometric limits */
        int nbp;                /* number of black pixels */
        char va;                /* vertical alignment */
        char c;                 /* column (zone) */

        /*
           Flags. Symbols have per-transliteration flags
           for per-transliteration statuses (F_BOLD,
           F_ITALIC,F_UNDERL,F_BAD).

           The per-symbol flag field is for those
           statuses unrelated to the transliterations
           (F_ISP,F_ISW,F_SS,F_SDIM).

           Of course this is a bit confusing... coders must
           take care to avoid using s->f instead of
           (s->tr)->f.
         */
        int f;

        /* neighborhood data */
        int N, S, E, W;         /* nearest symbols */
        int sw;                 /* surrounding word */
        int sl;                 /* list of signals (accents) */
        int bs;                 /* base symbol */

        /* transliterations */
        trdesc *tr;             /* first transliteration */
        char tc;                /* transliteration class */
        char bq;                /* quality of the best match */
        int pb;                 /* number of publishings */
        int bm;                 /* best match pattern ID */
        int lfa;                /* last pattern ID analysed */

} sdesc;

/*

Symbol transliteration shortcut.

This macro returns always a non-null pointer. If the symbol is
untransliterated, it returns a pointer to an empty string (""). Be
careful! this is for immediate usage only. Example:

    if (strcmp(trans(m),".") == 0)

Never use as follows:

    char *p;

    p = trans(m);
    (...)
    if (strcmp(p,".") == 0)

*/
#define trans(b) (((mc[b].tr==NULL) || ((mc[b].tr)->t==NULL)) ? "" : ((mc[b].tr)->t))

/*

General statuses, that apply for various structure fields.

*/
#define UNDEF 0
#define DUBIOUS 1
#define KNOWN 2

/*

Type of the transliteration source.

*/
typedef enum {
        ANON = 1,
        TRUSTED = 2,
        ARBITER = 3,
} revtype_t;

/*

The transliteration classes. The general statuses UNDEF
and DUBIOUS apply for transliterations.

*/
#define SCHAR 2
#define ACCENT 10
#define CHAR 11
#define PUNCT 12
#define FRAG 13
#define NOISE 14
#define DOT 16
#define COMMA 17

/*

Transliteration classes smaller than 10 are reserved
for the cases where we are not certain about the
transliteration.

*/
#define uncertain(x) (x < 10)

/* flags for messages generation */
extern int verbose, debug, trace, selthresh;

/* optimization flags */
extern int shift_opt;
extern int jump_opt;
extern int shape_opt;
extern int square_opt;
extern int copy_opt;
extern int i64_opt;
extern int disp_opt;

/* from skel.c */
extern int SA;
extern float RR;
extern float MA;
extern int MP;
extern float ML;
extern int MB;
extern int RX;
extern int BT;

/* geometric limits of the skeleton */
extern int fc_bp, lc_bp, fl_bp, ll_bp;

/* cb and cb2 are buffers for symbols, used to compute skeletons */
extern char cb[], cb2[];

/*

These are used to maintain a list of doubts of the
current page. The OCR allows browse and revise the
symbols on this list and/or dump their images to
files on the disk for web-based revision.

*/
extern int RWX, RWY, RCX, RCY;
extern unsigned char *rw;
/*
extern int editing_doubts;
extern int topdoubt,doubtsz,curr_doubt;
extern int *doubt;
*/
extern int max_doubts;

/* document size */
extern int XRES, YRES, DENSITY;

/* millimeters-to-pixels conversion */
#define m2p(x) (x*DENSITY/25.4)

/*

Common buffer for HTML generation

*/
extern char *text, *ptext;
extern int textsz, topt;
extern int ptextsz, ptopt;

/*

Debug messages.

*/
extern char *dbm;
extern int topdbm, dbmsz;

/*

Maximum lenght of pattern transliteration. When a pattern is
recognized on the document, its transliteration is sent to the
output. Generally the transliteration is a string of lenght 1 (as
"a", "D", ",", etc), but strings of lenght up to MFTL are
supported, to make possible codify characters unsupported by the
X fonts, just like composed patterns like "fi", "ff" and others.

MFTL is also used to limit the size of some other strings.

Note: ideally the OCR should support transliterations of any
size, however the GUI routines that handle input are not prepared
for that.

*/
#define MFTL 30

/*

              0  width
              |    |
             +-------------+
         0 - |  XXX        |
             |XX   X       |
             | X   X       |
  baseline - | XXXX        |
             | X           |
    height - | X           |
             |             |
             |             |
             +-------------+

Each pattern is stored on a "pdesc" structure, as defined
below.

*/
typedef struct {
        int id;                 /* unique identifier */
        char a;                 /* alphabet (latin, greek, etc) */
        short pt;               /* pattern type */
        short fs;               /* font size (10pt, 12pt, etc) */
        short bw;               /* bitmap width (pixels) */
        short bh;               /* bitmap height (pixels) */
        short bb;               /* bitmap baseline (pixels) */
        short bp;               /* number of bitmap black pixels */
        short sw;               /* skeleton width (pixels) */
        short sh;               /* skeleton height (pixels) */
        short sb;               /* skeleton baseline (pixels) */
        short sp;               /* number of skeleton black pixels */
        short sx, sy;           /* skeleton x and y margins */
        short bs;               /* size of the fields b and s */
        unsigned char *b;       /* the bitmap */
        unsigned char *s;       /* the skeleton */
        int f;                  /* flags */
        int act;                /* originating act */
        int cm;                 /* cumulative matches */
        short ts;               /* size of the field tr */
        short ds;               /* size of the field d */
        char *tr;               /* transliteration */
        char *d;                /* name of the originating page */
        int e;                  /* originating symbol (DEPRECATED) */
        float sl;               /* slope (skew) */
        short l, t;             /* fragment top-left position within symbol */
        short p[8];             /* parameters for skeleton computation (deprecated?) */
} pdesc;

/*

Default skeleton parameters.

*/
int DEF_SA;
float DEF_RR;
float DEF_MA;
int DEF_MP;
float DEF_ML;
int DEF_MB;
int DEF_RX;
int DEF_BT;

/*

Pattern types.

*/
#define MAXSAMPLES 3
typedef struct {
        char at;                /* auto-tune flag */
        char n[MFTL];           /* font name */
        int f;                  /* flags */
        int a, d, xh, dd;       /* geometry (in pixels) */
        short bd;               /* baseline displacement (in pixels) */
        short ls;               /* line separation (in pixels) */
        short ss;               /* symbol separation (in pixels) */
        short ws;               /* word separation (in pixels) */
        unsigned int acc;       /* alphabet accounting flags */
        unsigned int sc[256];   /* symbol classification flags */
        int sa[256];            /* symbol alignment */
} ptdesc;

/*

Array of pattern types.

*/
extern ptdesc *pt;
extern int toppt, ptsz, cpt;

/*

Flags
-----

Various Clara structures have an "f" 32-bit field used to store
flags. Yet most flags are not meaningful to all structs, we've used
the same address space for all to keep the implementation simple. So
for instance the revision act statuses include "is nullificated", but
the symbol statuses no. On the other hand, the "is preferred" or the
"is start of word" statuses are symbol-specific.

The "f" field is currently present on the following structs:

    adesc  (revision act)
    trdesc (transliteration)
    sdesc  (symbol)
    pdesc  (pattern)
    wdesc  (word)

When one specific flag is set, its semantics is:

  BOLD    .. bold font           (a/tr/s/p/w)
  ITALIC  .. italic font         (a/tr/s/p/w)
  UNDERL  .. underlined font     (a/tr/s/p/w)
  FRAG    .. is fragment         (p)
  PROPAG  .. is propagable       (a)
  ISP     .. is preferred        (s)
  ISW     .. is start of word    (s)
  SS      .. shape status known  (s)
  ISNULL  .. is nullificated     (a)

The following macros are available to set, unset, toggle and test a
flag:

  C_SET(f,flag)
  C_UNSET(f,flag)
  C_TOGGLE(f,flag)
  C_ISSET(f,flag)

*/
#define F_BOLD 1
#define F_ITALIC 2
#define F_UNDERL 4
#define F_FRAG 8
#define F_PROPAG 16
#define F_ISP 32
#define F_ISW 64
#define F_SS 128
#define F_ISNULL 256
#define F_SDIM 512
#define F_BAD 1024
#define F_ATTACH 2048

/*

Remark: if more than 28 flags are defined, the mask C_AS must be
redefined.

*/
#define C_AS 0x0fffffff
#define C_SET(f,flag) (f |= flag)
#define C_UNSET(f,flag) (f &= (C_AS ^ flag))
#define C_TOGGLE(f,flag) (f ^= flag)
#define C_ISSET(f,flag) ((f & flag) != 0)

/*

The array of patterns, its parameters, and the mapping of pattern
indexes to pattern id's. As patterns may be deleted, we need to
identify them using an id that may be different from the position
where it is stored on the array of patterns.

*/
extern int psz, topp, cdfc, cdfc_inv;
extern pdesc *pattern;
extern int s_id;
extern int *slist, slistsz;

/*

OCR statuses.

*/
extern int ocring;
extern int justone, this_pattern;
extern int waiting_key, key_pressed;
extern char inp_str[];
extern int recomp_cl;
extern int ocr_all, starting, onlystep;
extern int stop_ocr, cannot_stop;
extern char to_tr[MFTL + 1];
extern int nopropag;
extern int to_rev;
extern int to_arg;
extern int ocr_other;
#ifdef FUN_CODES
extern int fun_code;
#endif
extern int text_switch;
extern int abagar;

/*

DEBUG viewable buffer.

*/
extern char *dv;
extern int dvsz, topdv, dvfl;

/*

Page geometric parameters.

*/
extern float LW, LH, DD, LS, LM, RM, TM, BM;

extern int PG_AUTO, TC, SL;

/*

Classifiers. After adding or removing one classifier, please
change the limits CL_BOT and CL_TOP.

remark: all values between CL_BOT and CL_TOP must refer a valid
classifier (i.e. no holes).

*/
#define CL_SKEL 1
#define CL_BM 2
#define CL_PD 3
#define CL_SHAPE 4
#define CL_BOT 1
#define CL_TOP 4

/*

Minimum scores for strong and weak matches.

*/
extern int strong_match[];
extern int weak_match[];

/*

Session flags.

*/
//extern int finish;
extern int sess_changed;
extern int font_changed;
extern int hist_changed;
extern int form_changed;
extern int cl_ready;
extern int tskel_ready;
extern int wnd_ready;
extern int web;
extern int batch_mode;
extern int nullifying;
extern int dkeys;
extern int zsession;
extern int report;
extern int searchb;
extern int djvu_char;

/*

Type of action to perform, when the user trains an already
classified symbol.

*/
extern int action_type;
extern char *r_tr_old, *r_tr_new;
extern int r_tr_patt, r_tr_symb;

/*

Magic Numbers

*/
extern int m_mwd, m_msd, m_mae, m_ds, m_as, m_xh, m_fs;

/*

TUNE form.

*/
extern int classifier;
extern int bf_auto, st_auto;
extern float st_bq;
extern int mf_i, mf_b, mf_c, mf_r, mf_l;

/* override color buttons */
extern int overr_cb;

/* paths */
extern char* pagename;
extern char* pagebase;
extern GPtrArray *page_list;
extern char *session_file,
        *acts_file,
        *patterns_file;
extern char* pagesdir;
extern char* workdir;
extern char urlpath[];
extern char host[];
extern char *doubtsdir;
extern int npages, cpage;

/* used for revision */
extern int curr_mc;

/* text lines */
typedef struct {
        int f, l;               /* first and last symbols on line */
        char hw;                /* have true words */
} lndesc;
extern lndesc *line;
extern int lnsz, topln;

/* (devel)

Words and lines
---------------

Clara OCR applies The concept of "symbol" to atomic symbols like
letters, digits or punctuation signs. Words (as "house" or
"peace"), are handled by Clara OCR as sequences of symbols.

It's very important to compute the words of the page. They
provide a context both to the OCR and to the reviewer. For
instance, if the known symbols of some word were identified as
bold, then Clara will automatically make the bold button on when
someone tries to review the unknown symbols of that word. The
same applies to prefer the recognition of one symbol as the digit
"1" instead of the character "l" if the known symbols of the
"word" are digits. Words are also the basis for revision based on
spelling. Each words is stored on a wdesc structure on the "word"
array.

When building the OCR output, Clara will combine words in
lines. Each line is a sequence of words (that is, wdesc
structures). The array "line" is the sequence of the heads of the
detected lines. Each entry of this array is a lndesc
structure. The left and right limits of words must be carefully
computed and compared in order to the OCR partitionate then in
columns, when dealing with multi-column pages.

*/

/*

Text words.

*/
typedef struct {
        int F, L;               /* first and last symbols on word */
        int E, W;               /* next and previous words on line */
        int l, r, t, b;         /* bounding box */
        int tl;                 /* line to which this word belongs */
        int bl, br;             /* baseline left and right ordinates */
        int f;                  /* flags */
        int a;                  /* word type */
        short sh, as, ds;       /* small height, ascent and descent */
        int sk;                 /* skew */
} wdesc;
extern wdesc *word;
extern int wsz, topw;

/* fat bits display buffer */
extern char cfont[];

/*

Document windows. PAGE, PATTERN and HISTORY are also tab
indexes and must be defined as 0, 1 and 2.

*/
#define WELCOME 7
#define GPL 8
#define PAGE 0
#define PAGE_OUTPUT 6
#define PAGE_SYMBOL 9
#define PAGE_LIST 5
#define PAGE_FATBITS 12
#define PAGE_MATCHES 10
#define PAGE_DOUBTS 17
#define PATTERN 1
#define TUNE_PATTERN 11
#define PATTERN_LIST 4
#define TUNE 2
#define TUNE_ACTS 3
#define TUNE_SKEL 13
#define PATTERN_TYPES 14
#define PATTERN_ACTION 15
#define DEBUG 16
#define TOP_DW 17

/* maximum size of revision record (without LF) */
#define MAXRRL 240

/*

Maximum lenght of messages to be put on the message area.

*/
#define MMB 80

/* buffer of messages */
extern char mb[], mba[];

/*

geometry.

*/
// extern int WH,WW,PH,PW;
// extern int BW,BH,MRF,TW,TH,PM,PT;

/* (devel)

How to add an application button
--------------------------------

These are the steps to add a new button:

1. Create a new button macro after those already existing (bzoom,
balpha, etc). Note that each button macro is defined as an unique
integer (0, 1, 2, etc).

  #define bzoom 0
  [...]
  #define bfoo 13

2. Register the new button at init_ds(), together with its
label. Multi-state buttons have multiple labels, specified as
"state1:state2:state3":

    register_button(bzoom,"zoom");
    [...]
    register_button(bfoo,"foo");

The current state of the new button is stored by
button[bfoo]. When the state is nonzero, the button is drawn
using a dark background.

3. Add a new block to attend this button on mactions_b1 and, if
desired, on mactions_b2 (just copy one existing block and adapt
it). It's mandatory to attend help requests. On/off and
multi-state buttons must circulate the acceptable values of the
respective entry of the array "button" in order to change the
current state, and set the redraw_button flag.

  if (i == bfoo) {
      if (help) {
          show_hint(0,"This is the FOO button");
          return;
      }
      show_hint("You pressed the FOO button");
  }

There is no need to inform the type of the button (on/off,
multi-state or event catcher). The behaviour is defined by the
label and by the attending block. If the attending block changes
the button state, it must request redraw. Example:

      button[bfoo] = 1 - button[bfoo];
      redraw_button = bfoo;

*/
//extern int BUTT_PER_COL,bl_sz;
//extern char *button,**BL;
//extern int nalpha;


/*

GUI buttons

*/
#define bzoom 0
#define bzone 1
#define bocr 2
#define bstop 3
#define balpha 4
#define btype 5
#define bbad 6
#define btest 7

/*

Pixel size.

*/
extern int ZPS;

/*

The current tab and the total number of tabs.

*/
extern int tab;
#define TABN 3

/* context menus */
//extern int CX0,CY0,CX0_orig,CY0_orig,CW,CH;

/* indexes of the menus */
extern int CM_F, CM_E, CM_T, CM_O, CM_V, CM_S, CM_D;

/*

Types of menu entries:

    use CM_TB for binary flags (on/off).
    use CM_TA for actions (performed by cma).
    use CM_TR for multiple choice.

Per-item flags:

    CM_NG this item starts a new group
    CM_H  this item is (currently) hidden
    CM_D  dismiss the menu when this item is selected

MAX_MT is the maximum number of characters accepted for menu titles
and item labels.

*/
#define CM_TB  1
#define CM_TA  2
#define CM_TR  3
#define CM_TM  7
#define CM_NG  8
#define CM_HI 16
#define CM_DI 32
#define MAX_MT 45

/*

Menu descriptor.

*/
typedef struct {
        char a;                 /* type (1=bar, 2=select) */
        int m;                  /* maximum number of labels (may grow) */
        int n;                  /* effective number of labels */
        char *l;                /* labels */
        char *t;                /* per-item types and flags */
        char **h;               /* per-item short help */
        int *f;                 /* per-item activation flags */
        int p;                  /* position on bar */
        int c;                  /* current item */
        char tt[MAX_MT + 1];    /* menu title */
} cmdesc;

/*

The currently active menu (cmenu), the array of menus (CM, TOP_CM
and CM_SZ), and the menu clip flag.

*/
extern int TOP_CM, CM_SZ;
extern int mclip;


/* input buffers */
extern char tb[];

/* OCR zone */
#define MAX_ZONES 127
#define LIMITS_SIZE (8*MAX_ZONES)
extern int zones;
extern int limits[];

/* list of pixels of an symbol */
typedef struct {
        short x, y;
} point;

/* symbols */
extern sdesc *mc;
extern int ssz, tops;

/* OCR statistics */
extern int symbols, words, doubts, runs, classes;
extern time_t ocr_time, ocr_start;
extern int ocr_r;

/* per-page statistics */
extern int *dl_ne, *dl_db, *dl_r, *dl_t, *dl_lr, *dl_w, *dl_c;

/* preferred symbols */
extern int *ps, pssz, topps;

/* some parameters for comparison of symbols */
extern int MD;
extern int PNT, PNT1, PNT2;

/* the GPL */
extern char gpl[];

/*

Reduction factor for the list of patterns.

*/
extern int plist_rf;

/* list_cl and list_s stuff */
extern int *list_cl_r, list_cl_sz;
extern int *list_s_r, list_s_sz;

/*

Links. There are two types of links: symbol links and accent
links. Symbol links are created from one "left" symbol to one "right"
symbol to inform that they're part of one same word, and the right
symbol follows immediately the left one. Accent links are created from
one base symbol to an accent. Links are submitted as revision acts.

Note that links are exclusive for submitted data. Deduced data is
stored on the mc array, and cleared each time the build step is
run again.

*/
typedef struct {
        char t;                 /* type (REV_SLINK or REV_ALINK) */
        char p;                 /* reviewer privilege */
        int a;                  /* originating act */
        int l;                  /* left (or base) symbol */
        int r;                  /* right symbol (or accent) */
} lkdesc;

extern lkdesc *lk;
extern int lksz, toplk;

/*

Reviewer data

*/
extern char *reviewer;
extern revtype_t revtype;

/* host name with domain */
extern char fqdn[MAXFNL + 1];

/* for list_cl */
extern int *clx, *cly;
extern int csz, list_s_r_sz, nlx, nly;

/*

Latin to Greek keyboard mapping.

*/
extern char *l2g[256];

/*

Pattern comparison result and bitmap buffer.

*/
extern int cp_result, cp_diag;
extern unsigned cmp_bmp[];
extern int cmp_bmp_w, cmp_bmp_h, cmp_bmp_dx, cmp_bmp_dy, cmp_bmp_x0,
    cmp_bmp_y0;

/*

Macros used by spcpy.

*/
#define SP_GLOBAL -1
#define SP_DEF -2

/*

Spyhole stuff.

*/
extern int sh_tries;

/*

64-bit pixel mapping.

*/
extern unsigned long long p64[256];
extern int s64[256];

/*

Number of bits 1 on each 8-bit value.

*/
extern int np8[256];

/*

Pixel macros.

The service "pixel" can be used instead of the macro "pix".

*/
#define pix(bm,bpl,i,j) \
    (((((unsigned char *)bm)[(j)*bpl+((i)/8)]) & (1 << (7-((i)%8)))) != 0)
#define pix3(bm,bpl,i,j) \
    ((((((unsigned char *)bm)[(j)*bpl+((i)/8)]) >> (5-((i)%8))) & 7) == 7)
#define setpix(bm,bpl,i,j) \
    ((((unsigned char *)bm)[(j)*bpl+((i)/8)]) |= (1 << (7-((i)%8))))
#define togglepix(bm,bpl,i,j) \
    ((((unsigned char *)bm)[(j)*bpl+((i)/8)]) ^= (1 << (7-((i)%8))))
#define unsetpix(bm,bpl,i,j) \
    ((((unsigned char *)bm)[(j)*bpl+((i)/8)]) &= (~(1 << (7-((i)%8)))))

/*

This is the flea path.

*/
/* TODO: fp was originally unsigned. Why? */
extern short fp[];
extern float fsl[], fpp[];
extern int topfp, fpsz, curr_fp, last_fp;

/*

Clusterization result.

*/
extern int *clusterize_r;

/*

Types of ClaraML elements.

*/
#define CML_END 0
#define CML_TEXT 1
#define CML_SEP 2
#define CML_TAG 3

/*

Buffers for *ML parse.

*/
extern int tagsz;
extern char *tag;
extern int tagtxtsz;
extern char *tagtxt;
extern int attribc;
extern int listsz;
extern char **attrib;
extern char **val;
extern int *attribsz;
extern int *valsz;
extern char *href, *type, *src, *value, *name, *action, *bgcolor;
extern int checked, size, cellspacing, oneword;

/*

Dictionary entry.

*/
typedef struct {

        char *b;
        char *col;
        char *lang;
        char *key;
        char *dec;
        char *gender;
        char *tense;
        char *orig;
        char *parad;
        char *type;
        char *ver;
        char *val;

} ddesc;

/*

Dictionary entries

*/
extern ddesc *de;
extern int desz, topde, dict_op;

/*

cmpln diagnostic.

*/
extern int cmpln_diag;

/*

Circumference data.

*/
extern short *circpx, *circpy;
extern int *circ, *circ_np, topcircp, circp_sz, circ_sz;

/*

Platform-dependent services.

*/
#ifdef NO_RINTF
#define rintf(A) rint(A)
#endif

#ifdef __EMX__
#include <float.h>              /* because of PI */
#define rintf(A) rint(A)
float modff(float A, float *B);
#endif

/*

Services oferred by the modules.

This section must be carefully reorganized.

*/

/* boundary check */
#ifdef MEMCHECK
extern int dontcheck;
int check(int i, int s, char *w);
#define checkidx(i,s,w) ((void)((dontcheck) || (check(i,s,w))))
#endif

/* messages */
void tr(char *m, ...);
void db(char *m, ...);
void warn(char *m, ...);
void fatal(int code, char *m, ...);

/* memory allocation */
void *c_realloc(void *p, int m, const char *s);
void c_free(void *p);

/* I/O selector */
CFile* cfopen(const gchar* path, const gchar* mode);
gint cfread(CFile *f, gpointer buf, gsize len);
gint cfwrite(CFile *f, gpointer buf, gsize len);
void cfflush(CFile *f);
void cfclose(CFile* f);


FILE *zfopen(char *f, char *mode, int *pio);
void zfclose(FILE *F, int pio);
size_t zfread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t zfwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int zfgetc(FILE *stream);


/* bitmap comparison heuristics */
int bmpcmp_map(int c, int st, int k, int direct);
int bmpcmp_skel(int c, int st, int k, int direct);
int bmpcmp_pd(int c, int st, int k, int direct);
int bmpcmp_shape(int c, int st, int k, int direct);

/* redraw-related services */
void display_match(void *c, void *s, int r);
void force_redraw(void);
void set_mclip(int f);
void swn(char *);
void redraw(void);
void show_hint(int f, char *s, ...);
void enter_wait(char *s, int f, int m);
void slide(void);

/* symbol pairing and word-related services */
int s_pair(int a, int b, int p, int *rd);
int rsymb(int c, int relax);
int bsymb(int c, int relax);
int lsymb(int c, int relax);
void diag_wpairing(int t);
void diag_pairing(int t);

/* symbol services */
int classify(int c, int bmpcmp(int, int, int, int), int mode);
int sdim(int k);
int swh(int w, int h);
int avoid(int k, int c);

/* geometric services */
int intersize(int a, int b, int c, int d, int *e, int *f);
float bdist(int a, int b);
float box_dist(int a, int b, int *v);
int ldist(int a, int b, int c, int d);
int inside(int x, int y, int *pol, int npol);
void comp_circ(int max);

/* clipping services */
int symbol_at(int x, int y);
int closure_at(int x, int y, int u);
void list_cl(int x, int y, int w, int h, int reset);
void list_s(int x, int y, int w, int h);

/* bitmap services */
int add_closure(cldesc *d, int dx, int dy);
int pixel_mlist(int k);
int wrmc8(int mm, char *s1, char *s2);
int pixel(cldesc *c, int x, int y);
int spixel(sdesc *m, int x, int y);
int byteat(int b);
void bm2byte(char *c, unsigned char *b);
int wrzone(char *s1, int all);
int pbm2bm(char *f, int reset);
int find_thing(int reset, int x, int y);

/* OCR startup and steps */
void start_ocr(int p, int s, int r);
int build(int reset);
int load_page(int p, int reset, int bin);
int loadpgm(int reset, char *f, unsigned char **pb, int *w, int *h);
int unload_page(int reset);
int ocr_prep(int reset);

/* code-to-string services */
char *statusname(int s);
char *dwname(int s);
char *aname(int a);

/* transliteration-related services */
void add_tr(int m, char *t, int orig, int an, int q, char al, int f);
void rmvotes(int o, int k, int a, int nd);

/* production of window contents */
//void mk_page_output(int encap);
void mk_pattern_list(void);
void mk_pattern_props(void);
void mk_page_list(void);
void mk_history(int n);
void mk_page_symbol(int c);
void mk_pattern_action(void);
void mk_debug(void);
void mk_page_doubts(void);

/* path-related services */
void names_cpage(void);

/* allocation and of structs */
int new_mc(int *l, int cls);

/* OCR thread */
void continue_ocr(void);

/* sorting services */
void qss(int *a, int l, int r);
void qsf(int *a, int l, int r, int inv, int cmpf(int, int));
void qsi(int *a, int l, int r, char *t, int sz, int p, int inv);
void qs(char *a[], int l, int r, int p, int inv);
void true_qsi(int *a, int l, int r, char *t, int sz, int p, int inv);

/* revision services */
int review(int reset, adesc *a);
void summarize(int m);
int from_gui(void);
void synchronize(void);

/* load and dump services */
void totext(const char *fmt, ...);
void to(char **t, int *top, int *sz, const char *fmt, ...);
void push_text(void);
void pop_text(void);
int dump_session(char *f, int reset);
int dump_acts(char *f, int reset);
int dump_patterns(char *f, int reset);
int recover_session(char *f, int st, int reset);
int recover_acts(char *f);
int recover_patterns(char *f);
int free_page(void);
void load_vocab(char *f);

/* consistency services */
int cl_cons(int c);
int s_cons(int c);
int cons(int reset);
void consist_pg(void);
void consist_density(void);
void consist_magic(void);
void consist_pp(void);

/* alphabet services */
void init_alphabet(void);
int compose(int *l, int c);

/* skeleton services */
void consist_skel(void);
void skel_parms(char *s);
int border(int i, int j);
void cb_border(int W, int H);
void skel(int i0, int j0, int MX, int MY);
int skel_quality(int p);
int tune_skel(int p);
int tune_skel_global(int reset, int p);
void spcpy(int to, int from);

/* pattern services */
int id2idx(int id);
void new_pattern(void);
void justify_pattern(short *h, short *w, short *dx, short *dy);
int opt_font();
void pskel(int c);
void new_pattern(void);
int update_pattern(int k, char *tr, int an, char a, short ft, short fs,
                   int f);
void rm_pattern(int n);
void rm_untrans(void);
void clear_cm(void);
int prepare_patterns(int reset);
void pp(int i, int j);
void pr(int i, int j, int c);
int compare_patterns(int p1, int p2, int bmpcmp(int, int, int, int));
int bm2cb(unsigned char *b, int dx, int dy);
void reset_skel_parms(void);
void enlarge_pt(int new_toppt);
int snbp(int s, int *bp, int *sp);

/* major gui services */
void xevents(void);
void set_alpha(void);
void xpreamble();
void comp_wnd_size(int ww, int wh);
void cmi(void) __attribute__ ((deprecated));
int mb_item(int x, int y);
void init_welcome(void);
void get_pointer(int *x, int *y) __attribute__ ((deprecated));
void setview(int mode) G_GNUC_DEPRECATED;
void check_dlimits(int cursoron);
void set_buttons(int s, int p);
void symb2buttons(int s);
void set_mclip(int f);
void draw_dw(int bg, int inp);

/* auxiliar gui services */
void copychar(void);
void dismiss(int f);
void set_bl_alpha(void);
void comp_menu_size(cmdesc *c);
void form_auto_submit(void);
void show_htinfo(int cx, int cy);

/* merging */
void diag_geomerge(int t);
void geo_merge(int c);

/* menu-related services */
int additem(cmdesc *m, char *t, int p, int g, int d, char *h, int f);

/* preferences stuff */
void make_pmc(void);
void extr_tp(int *U, int *T, int *S, int *E, int *A, int *N, int p);

/* HTML services */
void free_earray(void);
void img_size(char *img, int *w, int *h);
int get_tag(char **ht);

/* destroy data */
void reset();
void nullify(int n);

/* alignment */
int tr_align(char *a);
int complete_align(int a, int b, int *as, int *xh, int *bl, int *ds);
int geo_align(int c, int dd, int as, int xh, int bl, int ds);

/* barcode stuff */
int closure_border_slines(int e, int t, int crit, float val, short *res,
                          int mr, int bar);
int closure_border_path(int t);
int border_path(unsigned char *b, int w, int h, short *bp, int m, int u0,
                int v0, int relax);
int dist_bar(int i, int j);
int search_barcode(void);
int isbar(int k, float *sk, float *bl);
float s2a(float slxy, float sly, float slx, short *bp, int i, int j);

/* blockfinders */
void cf_block(void);
int blockfind(int reset, int pc);

/* clusterization */
int clusterize(int N, int T, int dist(int, int));
int test_clusterize(void);

/* preprocessors */
int deskew(int reset);
int pp_thresh(void);
void balance(void);
void avoid_links(float C);
void hqbin(void);
void test(void);

/* tests */
void internal_tests(void);

/* dictionary services */
int dump_dict(int reset, char *f);
int dict_sel(ddesc *e);
void dict_load(void);
void dict_behaviour(void);

/* other */
void checkcl(int m);
void gen_wrf();
void process_cl(int argc, char *argv[]);
void process_webdata(void);
int pagenbr(char *p);
int s2p(int k);
int tsymb(int c, int relax);
int classify_tr(char *t);
void new_alrm(unsigned d, int who);
void dump_cb(void);
void dump_bm(unsigned char *b);
void init_pt(ptdesc *p);
int dx(int k, int i0, int j0);
int cma(int it, int check);
int spyhole(int x, int y, int op, int et);
int justify_bitmap(unsigned char *bm, int bw, int bh);
void get_ap(int c, int *as, int *xh, int *bl, int *ds);
void cb2bm(unsigned char *b);
int cmpln(int a, int b);
void write_report(char *fn);
char *mkpath(char *d, char *a, char *b);
void build_internal_patterns(void);
int obd_main(int argc, char **argv);
char *ht(int secs);
void set_flag(flag_t flag, gboolean value);
gboolean get_flag(flag_t flag);
void resync_pagelist(int pageno);


#define UNIMPLEMENTED() real_UNIMPLEMENTED(__FILE__,__FUNCTION__)
void real_UNIMPLEMENTED();
