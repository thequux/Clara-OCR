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

clara.c: Startup and OCR run control

*/

/* version number */
char *version = "20031214";
int finish = 0;
/* system headers */
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <math.h>
#include <time.h>
#include <sys/utsname.h>
#include <stdarg.h>
#include <Imlib2.h>
#ifdef HAVE_SIGNAL
#include <signal.h>
#endif

/* Clara headers */
#include "common.h"
#include "gui.h"

GMutex* imlib_lock;

/*

The pixmap

The pixmap is the map of pixels of the loaded page. By now, maybe
a 1bpp bitmap or a 8bpp graymap (in fact, only 8bpp is currently
maintained, because PBMs are being automatically converted to PGM
on loading. The code that handles 1bpp is not being maintained,
may be broken and may be removed in the future):

pm_t .. type (1=black-and-white, 8=graymap).

These variables are used when reading the pixmap buffer as if it
were a file through the I/O selector (services z*):

pm_p .. current position to be read on the PBM header.
pm_b .. current position to be read on the graymap.
pm_w .. graymap width
pm_h .. graymap height

*/
unsigned char *pixmap = NULL;
int thresh_val = 128;
int pm_p, pm_b, pm_w, pm_h, pm_t;
int graydist[256];
int pp_deskew = 0, pp_bal = 0, pp_double = 0, pp_deskew_accurate =
    0, pp_test = 0, pp_save = 0, pp_only = 0;
float pp_dark = 1.0, pp_avoid_links = 0.0;
int p_match = 1;
int bin_method = 2;

/*

The closures.

*/
int clsz = 0, topcl = -1;
cldesc *cl = NULL;

/* text lines */
int lnsz = 0, topln = -1;
lndesc *line = NULL;

/* text words */
int wsz = 0, topw = -1;
wdesc *word = NULL;

/* common buffer for HTML generation */
char *text = NULL, *ptext = NULL;
int textsz = 0, topt = -1;
int ptextsz = 0, ptopt = -1;
output_encap_t outp_format = OE_ENCAP_HTML;

/* per-page statistics */
int *dl_ne = NULL,
    *dl_db = NULL,
    *dl_r = NULL, *dl_t = NULL, *dl_lr = NULL, *dl_c = NULL, *dl_w = NULL;

/* optimization flags */
int shift_opt = 1;
int jump_opt = 1;
int shape_opt = 1;
int square_opt = 1;
int copy_opt = 1;
int i64_opt = 1;
int disp_opt = 1;

/*

64-bit pixel mapping.

*/
unsigned long long p64[256];
int s64[256];

/*

Number of bits 1 on each 8-bit value.

*/
int np8[256];

/*

Page size and image resolution (density).

*/
int XRES, YRES, DENSITY = 600;

/*

Buffers for revision. The function wrmc8 writes on these buffers
the symbol to be revised and its context (the word that surrounds
it), in order to dumped to disk or exhibit it on the display.

*/
//int RWX=200,RWY=50,RCX=50,RCY=50;
//unsigned char *rw=NULL,*rc=NULL;


/* (book)


A first OCR project
-------------------

Clara OCR is intended to OCR a relatively large collection of pages at
once, typically a book. So we will refer the material that we are
OCRing as "the book".

Let's describe a small but real project as an example on how to use
Clara to OCR one "book". This section is in fact an in-depth tutorial
on using Clara OCR. In order to try all techniques explained along
this section, please download and uncompress the file referred as
"page 143" of Manuel Bernardes Branco Dictionary (Lisbon, 1879),
available at http://www.claraocr.org. It's a tarball containing the
two text columns (one per file) of that page.

Just to make the things easier, we will assume that the files
143-l.pgm and 143-r.pgm were downloaded to the directory
/home/clara/books/MBB/pgm/. We will assume also that the programs
"clara", and "selthresh" are on the PATH. Some programs
required to handle PBM files (pgmtopbm, pnmrotate and others, by
Jef Poskanzer) are also required. These programs can be easily
found around there, and are included on most free operating
systems.


The work directory
------------------

Clara OCR expects to find on one same directory one or more
images of scanned pages. In our case, this directory is assumed
to be /home/clara/books/BC/pbm. By default, on this same
directory, various files will be created to store the OCR data
structures. So, if 143-l.pbm and 143-r.pbm are the pages to OCR, then
after processing all pages at least once (not done yet) the work
directory will contain the following files:

    143-l.pbm
    143-l.html
    143-l.session
    143-r.pbm
    143-r.html
    143-r.session
    acts
    patterns

The files "*.pbm" are the PBM images, the files "*.html" are the
current OCR output, the file "patterns" is the current
"bookfont", the file "143-l.session" contains the OCR data structures
for the page 143-l.pbm, and the file "acts" stores the human revision
effort already spent.

When Clara OCR is processing the page x.pbm, the files
"x.session", "acts" and "patterns" are in memory. These three
files together are generally referred as "the section". So the
menu option "save session" means saving all three files.


Using two directories
---------------------

In order to make easier the usage of read-only media, Clara OCR
allows splitting the files in two directories, one for images and
other for work files. The path of the first is stored on
pagesdir, and the second, on workdir. For instance:

  (pagesdir)

    |
    +- 1.pbm
    |
    +- 2.pbm


  (workdir)

    |
    +- 1.session
    |
    +- 1.html
    |
    +- 2.session
    |
    +- 2.html
    |
    +- acts
    |
    +- font


In this example, there are 2 pages (files "1.pbm" and
"2.pbm"). The current font is the file "pattern". The files
1.session and 2.session are the dumps of the data structures
built when processing the pages 1 and 2. The files 1.html and
2.html contain the current OCR output generated for pages 1 and 2.


Multiple books
--------------

A somewhat rigid directory structure is recommended for
high-volume digitalization projects based on Clara and using the
web interface. In this case, there will be multiple "pagesdir"
directories ("book1" and "book2" from the docsroot in the figure)
and, for each one, a corresponding "workdir" ("book1" and "book2"
from the workroot in the figure).

  (booksroot)

    |
    +- book1/
    |       +- 1.pbm
    |       |
    |       +- 2.pbm
    |
    |
    +- book2/
            +- 1.pbm
            |
            +- 2.pbm


  (workroot)

    |
    +- book1/
    |       +- 1.session
    |       |
    |       +- 1.html
    |       |
    |       +- 2.session
    |       |
    |       +- 2.html
    |       |
    |       +- acts
    |       |
    |       +- doubts/
    |       |        +- s.1.319.pbm
    |       |        |
    |       |        +- u.2.7015.pbm
    |       |        |
    |       |        +- 1.958225189.17423.hal
    |       |
    |       +- pattern
    |
    +- book2/
    |       +- 1.session
    |       |


For each book subdirectory on the workroot subtree, there will be
a "doubts" directory, used to communicate with the web
server. Each OCR run on some page of this book will generate
files of the form "u.page.symbol.pbm", that contains a pbm image
of one symbol. Once the CGI is
claimed to produce a revision page, it will choose one of these
files and rename it to s.page.symbol.pbm. This procedure is
performed without using locks, so two simultaneous revision
acesses may access the same symbol. The revision submission
generates a qmail-style file doc.time.pid.host.

*/

/* (devel)

Path variables
--------------

Most path variables are computed from the path given through
the -f command line option. The variable "pagename" is the filename
of the PBM image of the page being processed, not
including the path eventually specified through the -f
switch. For instance, if the OCR is started with

    clara -f mydocs/test.pbm

Then the value of the variable "pagename" will be just
"test.pbm". The variable pagebase is pagename without the
suffix ".pbm" ("test", in the example).

Clara stores on the variable pagelist the null-separated list of
all names of pbm files found on this directory. Even in this
case, the variable pagename will store the filename of the page
being processed (at any moment Clara will be processing one and
only one page).

The directory that contains the pbm files that Clara will
process is stored on the variable pagesdir. In the example
above, the value of the variable pagesdir is "mydocs/".

The variable workdir stores the path of the directory where Clara
will create the files *.html, *.session, "patterns" and
"acts". This path is assumed to be equal to pagesdir, unless
another path is given through the -w switch. The variable
doubtsdir will be the concatenation of workdir with the string
"doubts/" (doubtsdir is ignored if -W is not used).

*/
char book[MAXFNL + 1];
char *page_dir = NULL;
char *pagename = NULL;
char *pagebase = NULL;
char *pagesdir = NULL;
char *workdir = NULL;
char *patterns_file = NULL;
char *acts_file = NULL;
char *session_file = NULL;
char urlpath[MAXFNL + 1];
gchar *doubtsdir = NULL;
GPtrArray *page_list = NULL;
GStringChunk *pagelist_store = NULL;
int npages, cpage;

/* session flags */
int dontblock = 1;
int sess_changed = 0;
int font_changed = 0;
int hist_changed = 0;
int form_changed = 0;
int cl_ready = 0;
int tskel_ready = 0;
int wnd_ready;
int web = 0;
int batch_mode = 0;
int complexdir = 0;
int dkeys = 0;
int reset_book = 0;
int zsession = 0;
int report = 1;
int searchb = 0;
int djvu_char = 0;

/* page restrictors */
int start_at = -1;
int stop_at = -1;

/*

TUNE form

bf_auto: build bookfont automatically.

st_auto: tune skeletons automatically (globally).

mf_*: flags for merging fragments.

*/
int classifier = CL_SKEL;
int bf_auto = 0, st_auto = 0;
float st_bq = -1.0;
int mf_i = 0, mf_b = 0, mf_c = 0, mf_r, mf_l;

/*

Override color buttons when painting pixel. This status is not
being used anymore, perhaps it'll be removed soon.

*/
int overr_cb = 0;

/* page input formats */
#define PBM 1
#define PGM 2
int inp_format = PBM;

/* (devel)

OCR statuses
------------

The OCR run in course (if any) stores various statuses on global
variables. For instance, the ocring macro will be nonzero if one
OCR run is in course. The GUI informs the OCR control routines
about what to do along the OCR run using various global
variables. Some of them drive the classification procedures:

  justone      .. Classify only one symbol
  this_pattern .. Use only one pattern to classify
  recomp_cl    .. Ignore current classes

The first two are used for testing purposes, for instance when
checking why the classification routines classified some symbol
unexpected way.

The stop_ocr variable is set by the GUI when the STOP button is
pressed. Its status will be tested by the routines that control
the OCR run in course. Note that the variable cannot_stop may be
set by the current OCR step in course. It's effect is to inhibit
the GUI setting the stop_ocr status. It's used by routines that
cannot be stopped, otherwise the data structures they're handling
would rest in a irrecuperable inconsistency.

The OCR control routines handle the following statuses:

  ocr_all  .. OCR all pages
  starting .. continue_ocr was not called until now
  onlystep .. run only this OCR step

The buttons CLASSIFY, BUILD, etc, start one specific OCR
step. The OCR step to be executed is stored on onlystep. The
to_ocr variable stores the page where the OCR run will be
executed.

The other to_* variables together with nopropag store information
about the revision operation requested from the GUI:

  to_tr    .. the transliteration to submit to the current symbol
  to_rev   .. the type of revision
  nopropag .. propagation flag for the result
  to_arg   .. integer argument to revision operation

The types of revision are: transliteration submission (1),
fragment merging (2), symbol disassemble (3) and word extension
(4).

The to_arg variable stores the flagment to merge to the current
symbol or the symbol to add to the current word.

The variable ocr_other stores which operation to perform by the
OCR_OTHER step. This step is reserved to operations that are outside
the OCR run main stream, but require the control provided by the
continue_ocr function.

The variable text_switch redirects (if nonzero) the DEBUG window
output to an internal array.

*/
int ocring = 0;
int justone = 0, this_pattern = -1;
int recomp_cl = 0;
int ocr_all, starting = 0, onlystep = -1;
int stop_ocr = 0, cannot_stop = 0;
int to_ocr;
char to_tr[MFTL + 1] = "";
int nopropag = 0;
int to_rev = 0;
int to_arg;
int ocr_other;
int text_switch = 0;
int abagar = 0;

/*

Automatic presentation of the OCR features. If allow_pres==1,
just allow to start the presentation using the "View" menu. If
allow_pres==2, start the presentation automatically, ignore the
delays and exits just after finishing the presentation.

The presentation is under development, but it already offers a
small tour. It expects to find the file "imre.pbm" on the current
directory (or that one informed through -f).

*/
#ifdef FUN_CODES
int fun_code = 0;
#endif

/*

Output generation flags.

*/
int verbose = 0, trace = 0, debug = 0, selthresh = 0;

/*

symbols

mc is the array of symbols, and ssz the size of the
memory area currently reserved for mc.

curr_mc is the current symbol (the one under the
cursor).

*/
int ssz = 0;
sdesc *mc = NULL;
int curr_mc = -1;

/* OCR statistics */
int tops = -1, symbols, words, doubts, runs, classes, links;
time_t ocr_time, ocr_start;
int ocr_r;

/* (book)

Page geometry parameters
------------------------

For best results, when OCRing an entire book, it's convenient to
spend some time gathering geometric parameters common to all
pages. These parameters will be useful to drive the OCR
heuristics. Currently there are 9 such parameters: LW, LH, DD,
LS, TC, SL, LM, RM, TM and BM, all floats except TC and SL. Not
all them are currently used by the engine. Each one can be
informed using the var=value syntax. For example:

    TC=2 SL=1

For documentation purposes we divide these 9 parameters into 4
groups:

1. Symbol typical dimensions.

LW .. letter width measured in millimeters.
LH .. letter height measured in millimeters.
DD .. dot diameter measured in millimeters.

2. Vertical separation between lines.

LS .. vertical line separation measured in millimeters.

3. Number of text columns.

TC .. number of text columns. Use "1" for just one text column,
"2" or "3" for two or three text columns.

SL .. separation line. Use "1" if there exists a vertical line
separating text columns, "0" otherwise.

4. Page margins.

LM .. left margin measured in millimeters.
RM .. right margin measured in millimeters.
TM .. top margin measured in millimeters.
BM .. bottom margin measured in millimeters.

Useful remarks:

a) 1 inch = 25.4 milimetres

b) scanning resolution is a linear measure. So when scanning at
600 dpi, there will be 600/25.4=23.6 lines per millimeter. So, for
instance, 3 millimeters will correspond to 3*23.6=71 pixels.

*/

#define DEF_LW (((float)40)/23.6)
#define DEF_LH (((float)60)/23.6)
#define DEF_DD (((float)12)/23.6)
#define DEF_LS (((float)80)/23.6)
#define DEF_TC 1
#define DEF_LM 20
#define DEF_RM 20
#define DEF_TM 20
#define DEF_BM 20
#define DEF_SL 0

float LW = DEF_LW,
    LH = DEF_LH,
    DD = DEF_DD,
    LS = DEF_LS, LM = DEF_LM, RM = DEF_RM, TM = DEF_TM, BM = DEF_BM;

int PG_AUTO = 1, TC = DEF_TC, SL = DEF_SL;

/*

Doubts.

*/

/* deprecated */
/*
int doubtsz=30,topdoubt=-1;
int *doubt=NULL;
int curr_doubt,editing_doubts=0;
char **revstr,*rbuffer;
*/

int max_doubts = 30;

/*

The list of all preferred symbols is built by the make_pmc
function and put into the pmc array.

*/
int *ps = NULL, pssz, topps = -1;

/*

The alarm queue

*/
int *alrmq = NULL, topalrmq = -1, alrmqsz = 0;

/*

The acts

*/
adesc *act = NULL;
int actsz = 0, topa = -1;
int curr_act = 0;

/*

Reduction factor for the list of patterns.

*/
int plist_rf = 2;

/*

Debug messages.

*/
char *dbm = NULL;
int topdbm = -1, dbmsz = 0;

/*

Fatal error handler.

*/
void fatal(int code, char *m, ...) {
        va_list args;

        va_start(args, m);

        fprintf(stderr, "Clara fatal error type %d (", code);
        if (code == DI)
                fprintf(stderr, "data inconsistency)\n");
        else if (code == BO)
                fprintf(stderr, "buffer overflow)\n");
        else if (code == FD)
                fprintf(stderr, "invalid field)\n");
        else if (code == IE)
                fprintf(stderr, "internal error)\n");
        else if (code == ME)
                fprintf(stderr, "memory exhausted)\n");
        else if (code == XE)
                fprintf(stderr, "X error)\n");
        else if (code == IO)
                fprintf(stderr, "I/O error)\n");
        else if (code == BI)
                fprintf(stderr, "bad input)\n");
        if (m != NULL) {
                vfprintf(stderr, m, args);
                fprintf(stderr, "\n");

                if (code == FD) {
                        fprintf(stderr,
                                "It seems that some session file (*.session,\n");
                        fprintf(stderr,
                                "patterns or acts) contains bad data.\n");
                }
        }
        exit(code);
}

/*

Add message to internal log.

*/
void logmsg(char *m) {
        /* concatenate */
        to(&dbm, &topdbm, &dbmsz, "%s\n", m);

        /* truncate log if too large */
        if (topdbm > 64 * 1024) {
                int i;

                for (i = 16 * 1024; dbm[i] != '\n'; ++i);
                memmove(dbm, dbm + i + 1, topdbm - i);
                topdbm -= (i + 1);
        }

        /* must regenerate */
        dw[DEBUG].rg = 1;
}

/*

Print trace message.

*/
void tr(char *m, ...) {
        va_list args;
        char s[129];

        /* add to history */
        va_start(args, m);
        vsnprintf(s, 128, m, args);
        s[128] = 0;
        logmsg(s);

        if (trace) {
                vfprintf(stderr, m, args);
                fprintf(stderr, "\n");
        }

        va_end(args);
}

/*

Print debug message.

*/
void db(char *m, ...) {
        va_list args;
        char s[129];

        /* add to history */
        va_start(args, m);
        vsnprintf(s, 128, m, args);
        s[128] = 0;
        logmsg(s);

        /* send to stderr if requested */
        if (debug) {
                vfprintf(stderr, m, args);
                fprintf(stderr, "\n");
        }

        va_end(args);
}

/*

Print warning.

*/
void warn(char *m, ...) {
        va_list args;
        char s[129];

        /* add to history */
        va_start(args, m);
        vsnprintf(s, 128, m, args);
        s[128] = 0;
        logmsg(s);

        vfprintf(stderr, m, args);
        fprintf(stderr, "\n");

        va_end(args);
}


/*

Check invalid index (valid range is [0,s[).

*/
#ifdef MEMCHECK
int dontcheck = 1;

int check(int i, int s, char *w) {
        int r = 1;

        if ((i < 0) || (s <= i)) {
                warn("invalid index %d at %s (size %d)\n", i, w, s);
                r = 0;
        }
        return (r);
}
#endif

/*

Open a (perhaps compressed) I/O stream to read or write file f.

*/
FILE *zfopen(char *f, char *mode, int *pio) {
        FILE *F = NULL;

        /* "open" the pixmap file */
        if (f == ((char *) pixmap)) {
                if (pixmap != NULL) {
                        pm_p = -1;
                        pm_b = 0;
                        pm_w = XRES;
                        pm_h = YRES;
                        return ((FILE *) pixmap);
                } else
                        return (NULL);
        }

        else {
#ifdef HAVE_POPEN
                int t;
                char *a;

                t = strlen(f);
                if ((t >= 4) && (strcmp(f + t - 3, ".gz") == 0)) {
                        if ((a = g_alloca(t + 20)) == NULL) {
                                fatal(ME, "at zfopen");
                        }
                        if (strcmp(mode, "r") == 0) {
                                struct stat b;

                                if (stat(f, &b) < 0)
                                        return (NULL);
                                sprintf(a, "gunzip -c %s", f);
                                F = popen(a, "r");
                        } else if (strcmp(mode, "w") == 0) {
                                sprintf(a, "gzip -c >%s", f);
                                F = popen(a, "w");
                        }
                        *pio = 1;
                } else {
                        F = fopen(f, mode);
                        *pio = 0;
                }
#else
                F = fopen(f, mode);
                *pio = 0;
#endif
        }

        return (F);
}

/*

Read a (perhaps compressed) I/O stream.

*/
int zfgetc(FILE *stream) {
        static char h[50];
        int r = -1;

        /*
           Reading the pixmap. This code assumes that the caller reads the
           entire header through zfgetc (see pbm2bm).
         */
        if ((pixmap != NULL) && (stream == ((FILE *) pixmap))) {
                if (pm_p < 0) {
                        snprintf(h, 49, "P4\n%d %d\n", XRES, YRES);
                        h[49] = 0;
                        pm_p = 0;
                }
                if (h[pm_p] == 0)
                        fatal(IE,
                              "not prepared for binarizing from zfgetc");
                else
                        r = h[pm_p++];
        }

        /*
           reading from file.
         */
        else
                r = fgetc(stream);

        /* return the char */
        return (r);
}

/*

Read a (perhaps compressed) I/O stream.

*/
size_t zfread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
        unsigned char *p, *q;
        int i, m, t, bpl;

        /* reading the pixmap */
        if ((pixmap != NULL) && (stream == ((FILE *) pixmap))) {

                /* number of bytes per bitmap line */
                bpl = (pm_w / 8) + ((pm_w % 8) != 0);

                /*
                   Convert the graymap (8bpp) to bitmap (1bpp). This simplistic
                   code assumes that the caller reads one bitmap line at a time.
                 */
                if (pm_t == 8) {

                        /* finished reading */
                        if (pm_b > (pm_w * pm_h))
                                return (0);

                        /* the number of bytes to read must equal the bitmap line width */
                        t = size * nmemb;
                        if (t != bpl)
                                fatal(IE,
                                      "trying to read the pixmap outside line boundaries");

                        /* prepare */
                        memset(ptr, 0, t);
                        q = ptr;
                        p = pixmap + pm_b;

                        /* binarize one line */
                        for (i = 0, m = 128; i < pm_w; ++i, ++p) {
                                if (*p <= thresh_val) {
                                        *q |= m;
                                }
                                if ((m >>= 1) == 0) {
                                        m = 128;
                                        ++q;
                                }
                        }

                        /* next position to read */
                        pm_b += pm_w;
                }

                /* just copying the pixmap */
                else {

                        /* finished reading */
                        if (pm_b > (bpl * pm_h))
                                return (0);

                        /* the number of bytes to read must equal the bitmap line width */
                        t = size * nmemb;
                        if (t != bpl)
                                fatal(IE,
                                      "trying to read the pixmap outside line boundaries");

                        /* just copy */
                        memcpy(ptr, pixmap + pm_b, t);

                        /* next position to read */
                        pm_b += bpl;
                }

                /* number of items */
                return (nmemb);
        }

        /* reading file */
        else
                return (fread(ptr, size, nmemb, stream));
}

/*

Write a (perhaps compressed) I/O stream.

*/
size_t zfwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
        return (fwrite(ptr, size, nmemb, stream));
}

/*

Close a (perhaps compressed) I/O stream.

*/
void zfclose(FILE *F, int pio) {
        /* reading the pixmap */
        if ((pixmap != NULL) && (F == ((FILE *) pixmap))) {

                /*
                   c_free(pixmap);
                   pixmap = NULL;
                 */

                return;
        }
#ifdef HAVE_POPEN
        if (pio)
                pclose(F);
        else
                fclose(F);
#else
        fclose(F);
#endif
}

/*

Sort the array a[l..r] of integers.

*/
static void true_qss(int *a, int l, int r) {
        int b, i, j, m;

        m = a[l];
        i = l;
        j = r;
        while (i <= j) {
                for (; (i <= r) && (a[i] <= m); ++i);
                for (; (j >= l) && (a[j] > m); --j);
                if (i < j) {
                        b = a[i];
                        a[i] = a[j];
                        a[j] = b;
                        ++i;
                        --j;
                }
        }
        if (i > r) {
                a[l] = a[r];
                a[r] = m;
                if (l < r - 1)
                        true_qss(a, l, r - 1);
        } else {
                if (l < i - 1)
                        true_qss(a, l, i - 1);
                if (j + 1 < r)
                        true_qss(a, j + 1, r);
        }
}

/*

Entry point for true_qss.

*/
void qss(int *a, int l, int r) {
        if (r >= l)
                true_qss(a, l, r);
}

/*

Sort the array a[l..r] of integers using as
criterion of comparison the function cmpf.

*/
static void true_qsf(int *a, int l, int r, int inv, int cmpf(int, int)) {
        int i, j, m, b;

        m = a[l];
        i = l;
        j = r;
        while (i <= j) {

                if (inv == 0) {
                        for (; (i <= r) && (cmpf(a[i], m) <= 0); ++i);
                        for (; (j >= l) && (cmpf(a[j], m) > 0); --j);
                } else {
                        for (; (i <= r) && (cmpf(a[i], m) >= 0); ++i);
                        for (; (j >= l) && (cmpf(a[j], m) < 0); --j);
                }
                if (i < j) {
                        b = a[i];
                        a[i] = a[j];
                        a[j] = b;
                        ++i;
                        --j;
                }
        }
        if (i > r) {
                a[l] = a[r];
                a[r] = m;

                if (l < r - 1)
                        true_qsf(a, l, r - 1, inv, cmpf);
        } else {
                if (l < i - 1)
                        true_qsf(a, l, i - 1, inv, cmpf);
                if (j + 1 < r)
                        true_qsf(a, j + 1, r, inv, cmpf);
        }
}

/*

Entry point for true_qsf.

*/
void qsf(int *a, int l, int r, int inv, int cmpf(int, int)) {
        if (r >= l)
                true_qsf(a, l, r, inv, cmpf);
}

/*

Given an array a[l..r] of integers and an array t of structures,
sort the array a using the "p" field of the indexed (by the
entries of a) entries of t.

*/
#define qsie(x) (*((int *)(t+x*sz+p)))
static void true_qsi(int *a, int l, int r, char *t, int sz, int p, int inv) {
        int i, j, m, b, me;

        m = a[l];
        me = qsie(m);
        i = l;
        j = r;
        while (i <= j) {
                if (inv == 0) {
                        for (; (i <= r) && (qsie(a[i]) <= me); ++i);
                        for (; (j >= l) && (qsie(a[j]) > me); --j);
                } else {
                        for (; (i <= r) && (qsie(a[i]) >= me); ++i);
                        for (; (j >= l) && (qsie(a[j]) < me); --j);
                }
                if (i < j) {
                        b = a[i];
                        a[i] = a[j];
                        a[j] = b;
                        ++i;
                        --j;
                }
        }
        if (i > r) {
                a[l] = a[r];
                a[r] = m;

                if (l < r - 1)
                        true_qsi(a, l, r - 1, t, sz, p, inv);
        } else {
                if (l < i - 1)
                        true_qsi(a, l, i - 1, t, sz, p, inv);
                if (j + 1 < r)
                        true_qsi(a, j + 1, r, t, sz, p, inv);
        }
}

/*

Entry point for true_qsi.

*/
#define qsie(x) (*((int *)(t+x*sz+p)))
void qsi(int *a, int l, int r, char *t, int sz, int p, int inv) {
        if (r >= l)
                true_qsi(a, l, r, t, sz, p, inv);
}

/*

Given an array a[l..r] of pointers to structures, sort it
using the "p" field of the structure.

If inv is nonzero, then sort in reverse order.

*/
#define qse(x) (*((int *)((x)+p)))
void true_qs(char *a[], int l, int r, int p, int inv) {
        int i, j, me;
        char *m, *b;

        m = a[l];
        me = qse(m);
        i = l;
        j = r;
        while (i <= j) {
                if (inv == 0) {
                        for (; (i <= r) && (qse(a[i]) <= me); ++i);
                        for (; (j >= l) && (qse(a[j]) > me); --j);
                } else {
                        for (; (i <= r) && (qse(a[i]) >= me); ++i);
                        for (; (j >= l) && (qse(a[j]) < me); --j);
                }
                if (i < j) {
                        b = a[i];
                        a[i] = a[j];
                        a[j] = b;
                        ++i;
                        --j;
                }
        }
        if (i > r) {
                a[l] = a[r];
                a[r] = m;
                if (l < r - 1)
                        true_qs(a, l, r - 1, p, inv);
        } else {
                if (l < i - 1)
                        true_qs(a, l, i - 1, p, inv);
                if (j + 1 < r)
                        true_qs(a, j + 1, r, p, inv);
        }
}

/*

Entry point for true_qs.

*/
void qs(char *a[], int l, int r, int p, int inv) {
        if (r >= l)
                true_qs(a, l, r, p, inv);
}

/*

Compute the size of the intersection between segments (a,b) and
(c,d). Note that the segments (a,b) and (c,d) are seen as the
discrete lists of points (a,a+1,...,b) and (c,c+1,...,d), and not
as the segments [a,b] and [c,d] of the Real Line, so the size of
the intersection of (4,8) and (8,12) is 1, not 0.

If e and f are non-null, returns on them the coordinates of the
intersection (leftmost and rightmost).

*/
int intersize(int a, int b, int c, int d, int *e, int *f) {
        int x, y;
        x = MAX(a,c);
        y = MIN(b,d);
        if (x > y)
                return 0;
        else {
                if (e != NULL) *e = x;
                if (f != NULL) *f = y;
                return (y  - x + 1);
        }
}

/*

Compute the linear distance between segments [a,b] and [c,d].

*/
int ldist(int a, int b, int c, int d) {
        int r = MAX(a,c);
        int l = MIN(b,d);
        return MAX(r-l, 0);
}


/*

List preferred symbols.

*/
void make_pmc(void) {
        int i;

        /* count preferred symbols */
        for (i = 0, topps = -1; i <= tops; ++i)
                if (C_ISSET(mc[i].f, F_ISP))
                        ++topps;

        /* enlarge area for the list */
        if (pssz <= tops) {
                ps = g_renew(int, ps, (pssz = (topps + 512)));
        }

        /* copy */
        for (i = 0, topps = -1; i <= tops; ++i)
                if (C_ISSET(mc[i].f, F_ISP))
                        ps[++topps] = i;
}


/*

Return the color of pixel of absolute coordinates (x,y)
on the closure c. It's assumed that the closure bounding
box contains the pixel with absolute coordinates (x,y).

The macro "pix" can be used instead of this service.

*/
int pixel(cldesc *c, int x, int y) {
        int k;
        unsigned m;
        unsigned char *p;

        x -= c->l;
        y -= c->t;
        k = (c->r - c->l + 1) / 8 + (((c->r - c->l + 1) % 8) != 0);
        p = c->bm + k * y + x / 8;
        m = ((unsigned) 1) << (7 - (x % 8));
        if ((*p & m) == m)
                return (BLACK);
        return (WHITE);
}

/*

Return the color of the pixel value at absolute coordinates
(x,y) in regard of symbol m.

*/
int spixel(sdesc *m, int x, int y) {
        int n;
        cldesc *c;

        for (n = 0; n < m->ncl; ++n) {
                c = closure_get(m->cl[n]);
                if ((c->l <= x) && (x <= c->r) && (c->t <= y) &&
                    (y <= c->b)) {
                        return (pixel(c, x, y));
                }
        }
        return (WHITE);
}

/*

Generates a web revision file.

*/
void gen_wrf() {
        int F;
        char *n;

        /* revision filename */
        n = g_strdup_printf("%s%s.%d.%d.%s", doubtsdir, pagebase, (int) time(NULL),
                            getpid(), g_get_host_name());

        /* write new record */
        F = open(n, O_WRONLY | O_CREAT, 0660);
        write(F, text, strlen(text));
        close(F);
        g_free(n);
}

/*

Process revision file.

*/
int prf(char *f) {
        int F;
        int n;

        if ((F = open(f, O_RDONLY)) < 0) {
                if (verbose)
                        warn("could not open revision file \"%s\"", f);
                return (0);
        }

        /* for each record on the revision file */
        /*
           for (n=1; fgets(s,MAXRRL+1,F) != NULL; ++n) {
           prs(s,f,n);
           }
         */

        if (textsz < MAXRRL + 1)
                text = g_realloc(text, textsz = MAXRRL + 1024);

        /* process the file contents as a revision stream */
        n = read(F, text, MAXRRL);
        text[n] = 0;
        review(1, NULL);
        while (review(0, NULL));
        return (1);
}




/*

Read the bookfont handling options from the argument to the
command-line option -a.

*/
void read_bf(char *s) {
        int i, j, n;
        char b[21];

        for (n = i = j = 0; j <= strlen(s); ++j) {
                if ((s[j] == ',') || (s[j] == 0)) {

                        /* copy next argument to b */
                        if (j - i > 20) {
                                fatal(DI, "argument to -a too long");
                        }
                        if (j > i)
                                strncpy(b, s + i, j - i);
                        b[j - i] = 0;

                        /* bf_auto */
                        if (n == 0) {
                                if (strlen(b) > 0)
                                        bf_auto = atoi(b);
                                if (bf_auto)
                                        bf_auto = 1;
                        }

                        /* st_auto */
                        else if (n == 1) {
                                if (strlen(b) > 0)
                                        st_auto = (atoi(b) != 0);
                        }

                        /* black square threshold */
                        else if (n == 2) {
                                if (strlen(b) > 0)
                                        classifier = atoi(b);
                                if ((classifier < CL_BOT) ||
                                    (CL_TOP < classifier))
                                        fatal(DI,
                                              "invalid classifier %s\n",
                                              b);
                        }

                        else {
                                fatal(DI, "too many arguments to -a");
                        }

                        /* prepare next iteration */
                        i = j + 1;
                        ++n;
                }
        }

        if (n < 3) {
                fatal(DI, "too few arguments to -a (%d)", n);
        }
}

/*

Read the pixel number thresholds from the argument to the
command-line option -P (received as parameter s).

*/
void read_pnt(char *s) {
        int i, j, n;
        char b[21];

        for (n = i = j = 0; j <= strlen(s); ++j) {
                if ((s[j] == ',') || (s[j] == 0)) {

                        /* copy next argument to b */
                        if (j - i > 20) {
                                fatal(DI, "argument to -P too long");
                        }
                        if (j > i)
                                strncpy(b, s + i, j - i);
                        b[j - i] = 0;

                        /* PNT1 */
                        if (n == 0) {
                                if (strlen(b) > 0)
                                        PNT1 = atoi(b);
                                if (PNT1 > 100)
                                        PNT1 = 100;
                                if (PNT1 < 1)
                                        PNT1 = 1;
                        }

                        /* PNT2 */
                        else if (n == 1) {
                                if (strlen(b) > 0)
                                        PNT2 = atoi(b);
                                if (PNT2 > 100)
                                        PNT2 = 100;
                                if (PNT2 < 1)
                                        PNT2 = 1;
                        }

                        /* black square threshold */
                        else if (n == 2) {
                                if (strlen(b) > 0)
                                        MD = atoi(b);
                                if (MD < 4)
                                        MD = 4;
                                if (30 < MD)
                                        MD = 30;
                        }

                        else {
                                fatal(DI, "too many arguments to -P");
                        }

                        /* prepare next iteration */
                        i = j + 1;
                        ++n;
                }
        }

        if (n < 3) {
                fatal(DI, "too few arguments to -P (%d)", n);
        }
}

/*

Returns +1 if one of the dimensions w or h is larger than
FS. Otherwise (and in this order), returns -1 if one of the dimensions
w or h is smaller than the expected value (in pixels) of the dot
diameter. Otherwise returns 0.

Dots have a typical diameter of 0.02 inches.

see also sdim()

*/
int swh(int w, int h) {
        int d;

        d = 0.01 * DENSITY;
        if ((w > FS) || (h > FS))
                return (1);
        else if ((w < d) || (h < d))
                return (-1);
        else
                return (0);
}

/*

Returns 1 if k is not too large and seems to have at least the
dimension of a dot. Returns 0 otherwise.

Dots have a typical diameter of 0.02 inches.

see also swh()

*/
int sdim(int k) {
        sdesc *t;
        int d;

        d = 0.01 * DENSITY;
        t = mc + k;
        if ((t->r - t->l > d) && (t->b - t->t > d) &&
            (t->r - t->l < LFS) && (t->b - t->t < FS)) {
                return (1);
        } else
                return (0);
}

/*

Compares two filenames as numbers.

*/
int strnatcmp(const gchar* a, const gchar* b) {
        gchar *ua, *ub; // a and b in utf-8
        ua = g_filename_to_utf8(a,-1,NULL,NULL,NULL);
        ub = g_filename_to_utf8(a,-1,NULL,NULL,NULL);
        if (ua == NULL || ub == NULL ||
            !g_utf8_validate(ua, -1, NULL) ||
            !g_utf8_validate(ub, -1, NULL)) {
                if (ua != NULL)
                        g_free(ua);
                if (ub == NULL)
                        g_free(ub);
                return strcmp(a,b);
        } else {
                gchar *ca, *cb;
                assert(ua != NULL && ub != NULL);
                ca = g_utf8_collate_key_for_filename(ua, -1);
                cb = g_utf8_collate_key_for_filename(ub, -1);
                g_free(ua); g_free(ub);
                int r = strcmp(ca,cb);
                g_free(ca); g_free(cb);
                return r;
        }
}

/*

Build list of pages.

*/
static gboolean try_load_page(const gchar* filename) {
        Imlib_Image* img;
        gchar* path = g_build_filename(pagesdir, filename, NULL);
        g_mutex_lock(imlib_lock);
        img = imlib_load_image(path);
        g_mutex_unlock(imlib_lock);
        if (img != NULL) {
                g_mutex_lock(imlib_lock);
                imlib_context_set_image(img);
                imlib_free_image();
                g_mutex_unlock(imlib_lock);
                g_free(path);
                g_ptr_array_add(page_list,(gpointer)g_string_chunk_insert(pagelist_store, filename));
                npages++;
                return TRUE;
        } else
                return FALSE;
}
static int ptr_nat_strcmp(gconstpointer a, gconstpointer b) {
        return strnatcmp(*(const gchar**)a, *(const gchar**)b);
}
void build_plist(const gchar* source) {
        struct stat statbuf;

        assert(source != NULL);

        if (page_list == NULL)
                page_list = g_ptr_array_new();
        else
                g_ptr_array_set_size(page_list, 0);

        if (pagelist_store == NULL)
                pagelist_store = g_string_chunk_new(256);
        else
                g_string_chunk_clear(pagelist_store);

        if (pagesdir != NULL)
                g_free(pagesdir);
        pagesdir = NULL;
        npages = 0;
        book[0] = 0;

        /* bad argument */
        if (stat(source, &statbuf) != 0) {
                snprintf(mba, MMB + 1, "could not stat %s", source);
                show_hint(2, mba);
        }

        /* -f argument is a file */
        if (S_ISREG(statbuf.st_mode)) {
                gchar* basename;
                pagesdir = g_path_get_dirname(source);
                basename = g_path_get_basename(source);
                try_load_page(basename);
                g_free(basename);
        }

        /* argument to -f is a directory */
        else if (S_ISDIR(statbuf.st_mode)) {
                GDir *dir;
                GError *err;
                char const* fname;

                pagesdir = g_strdup(source);

                /* search all pages on the pagesdir directory */
                if ((dir = g_dir_open(pagesdir, 0, &err)) == NULL) {
                        g_critical("could not open directory \"%s\": %s", pagesdir, err->message);
                }
                while ((fname = g_dir_read_name(dir)) != NULL) {
                        try_load_page(fname);
                }
                g_dir_close(dir);

                /* sort pages */
                g_ptr_array_sort(page_list, ptr_nat_strcmp);
                if (npages <= 0) {
                        show_hint(2, "no pages found");
                }
        }

        /* argument is nor a regular file nor a directory */
        else {
                snprintf(mba, MMB, "%s is not a file nor a directory",
                         page_dir);
                show_hint(2, mba);
        }


        /* book name unknown */
        if (book[0] == 0)
                strncpy(book, "unknown", MAXFNL);

        /* per-page counters for symbols and doubts */
        // TODO: make this not suck.
        if (npages > 0) {
                int i;

                dl_ne = g_renew(int, dl_ne, npages);
                dl_db = g_renew(int, dl_db, npages);
                dl_r = g_renew(int, dl_r, npages);
                dl_t = g_renew(int, dl_t, npages);
                dl_lr = g_renew(int, dl_lr, npages);
                dl_w = g_renew(int, dl_w, npages);
                dl_c = g_renew(int, dl_c, npages);
                for (i = 0; i < npages; ++i) {
                        dl_ne[i] = 0;
                        dl_db[i] = 0;
                        dl_r[i] = 0;
                        dl_t[i] = 0;
                        dl_lr[i] = 0;
                        dl_w[i] = 0;
                        dl_c[i] = 0;
                }
                for (cpage = 0; cpage < npages; ++cpage) {
                        names_cpage();
                        if (recover_session(session_file, 1)) {
                                dl_ne[cpage] = symbols;
                                dl_db[cpage] = doubts;
                                dl_r[cpage] = runs;
                                dl_t[cpage] = ocr_time;
                                dl_lr[cpage] = ocr_r;
                                dl_w[cpage] = words;
                                dl_c[cpage] = classes;
                        }
                }
                cpage = -1;
                symbols = doubts = 0;
                runs = 0;
        }
}
/*

Print short help to stderr.

*/
void print_help(void) {
        warn("Thanks for trying Clara OCR!");
        warn("Usage: clara [options]");
        warn("Available options:");
        warn("  -a list     bookfont handling options");
        warn("  -c colors   colors (N|c|l|b|b,g,w,dg,vdg)");
        warn("  -k list     skeleton parms SA,RR,MA,MP,ML,MB,RX,BT");
        warn("  -N sjaqcxd  switch off s,j,a,q,c,x or d optimizations");
        warn("  -o t|h      select output format (text or html)");
        warn("  -P n1,n2,MD parameters for filtering symbol comparison");
        warn("  -R n        maximum number of web doubts to generate");
        /*
           warn("  -r          reset (destroy all data)");
         */
        warn("  -t          trace on");
        warn("  -T          don't read nor write session files");
        /*
           warn("  -U path     URL revision path (implies -W)");
         */
        warn("  -u          unbuffered X I/O");
        warn("  -v          verbose mode");
        warn("  -V          display version and compile options");
        warn("  -W          read and write web files");
        warn("  -w path     workdir path");
        warn("  -X 0|1      don't|do check indexes");
        warn("  -y r        resolution in dots per inch (dpi)");
        warn("  -z          read and write compressed session files");
        warn("  -Z n        initial zoomed pixel size");
}

/*

Copy string. The caller must make sure that the area prm is
at least max+1 bytes long.

*/
int criticize_str(char *val, char *prm, int min, int max) {
        int l;

        l = strlen(val);
        if ((min <= l) && (l <= max)) {
                strcpy(prm, val);
        }
        return (0);
}

/*

Convert integer if within informed range.

*/
int criticize_float(char *val, float *prm, float min, float max) {
        int i, l, nd;
        float v;

        if (val == NULL)
                v = 1;
        else {
                nd = 0;
                l = strlen(val);
                for (i = 0; i < l; ++i) {
                        if (((val[i] == '.') && (++nd > 1)) ||
                            (((val[i] < '0') || ('9' < val[i])) &&
                             (val[i] != '.'))) {

                                warn("invalid float specification %s (ignored)", val);
                                return (0);
                        }
                }
                v = atof(val);
        }
        if ((v < min) || (max < v)) {
                warn("parameter %d outside range %d..%d (ignored)", v, min,
                     max);
                return (0);
        }
        *prm = v;
        return (1);
}

/*

Convert string to integer if within informed range.

*/
int criticize_int(char *val, int *prm, int min, int max) {
        int i, l, v;

        if (val == NULL)
                v = 1;
        else {
                l = strlen(val);
                for (i = 0; i < l; ++i) {
                        if ((val[i] < '0') || ('9' < val[i])) {
                                warn("invalid integer specification %s (ignored)", val);
                                return (0);
                        }
                }
                v = atoi(val);
        }
        if ((v < min) || (max < v)) {
                warn("parameter %d outside range %d..%d (ignored)", v, min,
                     max);
                return (0);
        }
        *prm = v;
        return (1);
}

/*

Check if argument is a variable definition. If yes, initialize as
requested.

*/
int checkvar(char *c) {
        int i, l, r;
        char *id, *val;

        /* trivial case */
        if (c == NULL)
                return (0);

        /* separate identifier from value */
        id = val = NULL;
        l = strlen(c);
        for (i = 0; i < l; ++i) {
                if (c[i] == '=') {

                        /* copy varname dropping '-' */
                        if (c[0] == '-') {
                                id = g_alloca(i);
                                strncpy(id, c + 1, i - 1);
                                id[i - 1] = 0;
                        }

                        /* copy varname as is */
                        else {
                                id = g_alloca(i + 1);
                                strncpy(id, c, i);
                                id[i] = 0;
                        }

                        /* remember initial position of value */
                        val = c + i + 1;
                        i = l;
                }
        }

        /* didn't find separator */
        if (id == NULL) {
                if (c[0] == '-')
                        id = c + 1;
                else
                        id = c;
        }

        /* default return value (succes) */
        r = 1;

        /*
           search known identifiers
           ------------------------
         */

        /* engine major options */
        if (strcmp(id, "classifier") == 0)
                criticize_int(val, &classifier, CL_BOT, CL_TOP);
        else if (strcmp(id, "bin_method") == 0)
                criticize_int(val, &bin_method, 0, 3);
        else if (strcmp(id, "MD") == 0)
                criticize_int(val, &MD, 4, 30);
        else if (strcmp(id, "bf_auto") == 0)
                criticize_int(val, &bf_auto, 0, 1);

        /* preprocessor variables */
        else if (strcmp(id, "pp_deskew") == 0)
                criticize_int(val, &pp_deskew, 0, 1);
        else if (strcmp(id, "pp_double") == 0)
                criticize_int(val, &pp_double, 0, 1);
        else if (strcmp(id, "p_match") == 0)
                criticize_int(val, &p_match, 0, 1);
        else if (strcmp(id, "pp_bal") == 0)
                criticize_int(val, &pp_bal, 0, 1);
        else if (strcmp(id, "pp_test") == 0)
                criticize_int(val, &pp_test, 0, 1);
        else if (strcmp(id, "pp_dark") == 0)
                criticize_float(val, &pp_dark, 0, 1);
        else if (strcmp(id, "pp_avoid_links") == 0)
                criticize_float(val, &pp_avoid_links, 0, 1);
        else if (strcmp(id, "pp_save") == 0)
                criticize_int(val, &pp_save, 0, 1);
        else if (strcmp(id, "pp_only") == 0)
                criticize_int(val, &pp_only, 0, 1);

        /*
           session flags

           searchb = 1: search barcode and try to decode
           searchb = 2: search barcode and output raw bits
         */
        else if (strcmp(id, "dontblock") == 0)
                criticize_int(val, &dontblock, 0, 1);
        else if (strcmp(id, "report") == 0)
                criticize_int(val, &report, 0, 1);
        else if (strcmp(id, "searchb") == 0)
                criticize_int(val, &searchb, 0, 2);
        else if (strcmp(id, "djvu_char") == 0)
                criticize_int(val, &djvu_char, 0, 1);

        /* book geometry */
        else if (strcmp(id, "LW") == 0)
                criticize_float(val, &LW, 0, 50.0);
        else if (strcmp(id, "LH") == 0)
                criticize_float(val, &LH, 0, 50.0);
        else if (strcmp(id, "DD") == 0)
                criticize_float(val, &DD, 0, 50.0);
        else if (strcmp(id, "LS") == 0)
                criticize_float(val, &LS, 0, 50.0);
        else if (strcmp(id, "TC") == 0)
                criticize_int(val, &TC, 0, 50.0);
        else if (strcmp(id, "SL") == 0)
                criticize_int(val, &SL, 0, 50.0);
        else if (strcmp(id, "LM") == 0)
                criticize_float(val, &LM, 0, 50.0);
        else if (strcmp(id, "RM") == 0)
                criticize_float(val, &RM, 0, 50.0);
        else if (strcmp(id, "TM") == 0)
                criticize_float(val, &TM, 0, 50.0);
        else if (strcmp(id, "BM") == 0)
                criticize_float(val, &BM, 0, 50.0);

        /* dictionary */
        else if (strcmp(id, "dict_op") == 0)
                criticize_int(val, &dict_op, 0, 5);

        /* page restrictors */
        else if (strcmp(id, "start_at") == 0)
                criticize_int(val, &start_at, 0, 1000000000);
        else if (strcmp(id, "stop_at") == 0)
                criticize_int(val, &stop_at, 0, 1000000000);

        /* geometry */
        else if (strcmp(id, "m_msd") == 0)
                criticize_int(val, &m_msd, 0, 1000);

        else
                r = 0;

        return (r);
}


/*

Process command-line parameters.

*/
void process_cl(int argc, char *argv[]) {
        /* -k read from cmdline */
        int havek = 0, i;

        /* local buffer for options */
        char **largv, *argb;
        int largc, topl, lsz, topb, bsz, *disp;

        /* other */
        int c, e;

        /*
           Some default values not specified elsewhere.
         */

        /* use the default skeleton parameters */

        /* filter out/translate long options */
        {
                int i, l;
                char *a;

                largc = 0;
                largv = NULL;
                disp = NULL;
                argb = NULL;
                lsz = 0;
                bsz = 0;
                topb = -1;
                topl = -1;

                for (i = 0; i < argc; ++i) {

                        /* check if it's a variable definition */
                        if (checkvar(argv[i])) {
                                a = NULL;
                        }

                        /* leave as is to getopt */
                        else {
                                a = argv[i];
                        }

                        if (a != NULL) {
                                if (largc >= lsz) {
                                        disp = g_renew(int, disp, (lsz += 32));
                                }
                                l = strlen(a);
                                if ((l + 1 + topb + 1) > bsz)
                                        argb = g_realloc(argb, (bsz += l + 256));
                                disp[largc] = topb + 1;
                                strcpy(argb + topb + 1, a);
                                topb += l + 1;
                                ++largc;
                        }
                }

                largv = g_new(char*, (largc + 2));
                for (i = 0; i < largc; ++i) {
                        largv[i] = argb + disp[i];
                }
                largv[largc] = NULL;
        }

        /* parse command-line parameters */
        e = 0;
        while (e == 0) {
                c = getopt(largc, largv, "a:bde:f:hik:m:N:o:p:P:R:rTtuU:vVw:WX:y:zZ:");

                /* (book)

                   Reference of command-line switches
                   ----------------------------------

                   A number of internal variables now can be defined on
                   the command-line. Variable names can be optionally preceded
                   by '-'. If a value is absent, the default is 1. Examples:

                   clara -pp_deskew
                   clara pp_deskew
                   clara pp_deskew=1
                   clara -pp_deskew=1

                   (apply deskewer)

                   clara bin_method=3

                   (use the classification-based, local thresholder)

                   To known about the variables that can be defined on the
                   command line, see the source code, file clara.c,
                   function checkvar().

                 */

                /* (book)

                   -a bf_auto,st_auto,st_auto_global,classifier

                   Bookfont handling options

                 */
                if (c == 'a') {
                        read_bf(optarg);
                }


                /* (book)

                   -k list

                   Parameters SA,RR,MA,MP,ML,MB,RX,BT used to compute
                   skeletons.

                   BUG: these parameters are ignored when a "patterns"
                   already exists. In this case, Clara will read the
                   parameters from the "patterns" file.
                 */
                else if (c == 'k') {
                        skel_parms(optarg);

                        /* change defaults */
                        spcpy(SP_DEF, SP_GLOBAL);

                        /* no need to call skel_parms anymore */
                        havek = 1;
                }

                /* (book)

                   -N list

                   Switch off optimizations. Generally useful only for debug
                   purposes. Non-supported displays depths (if any) may require
                   '-N d'. The argument is the list of the (one or more)
                   optimizations to switch off (s, a, j, q, c, x or d).
                   Examples:

                   -N s
                   -N aq
                   -N jq
                 */
                else if (c == 'N') {
                        if (optarg) {
                                for (i = 0; i < strlen(optarg); ++i) {
                                        if (optarg[i] == 's')
                                                shift_opt = 0;
                                        else if (optarg[i] == 'a')
                                                shape_opt = 0;
                                        else if (optarg[i] == 'j')
                                                jump_opt = 0;
                                        else if (optarg[i] == 'q')
                                                square_opt = 0;
                                        else if (optarg[i] == 'c')
                                                copy_opt = 0;
                                        else if (optarg[i] == 'x')
                                                i64_opt = 0;
                                        else if (optarg[i] == 'd')
                                                disp_opt = 0;
                                        else {
                                                fatal(DI,
                                                      "unknown optimization \"%c\"",
                                                      optarg[i]);
                                        }
                                }
                        }
                }

                /* (book)

                   -P PNT1,PNT2,MD

                   Parameters for filtering symbol comparison.

                   PNT1 and PNT2 are the pixel number thresholds. These
                   thresholds are used to filter out bad candidates
                   when classifying symbols. The first threshold
                   is for strong similarity and the second for weak
                   similarity. The comparison algorithm performs two
                   passes. The first pass uses PNT1 to filter. The
                   second pass uses PNT2. So on the first pass only
                   patterns "quite similar" to the symbol to
                   classify are tried. On the second pass, we relax
                   and permit more patterns to be tried. This method
                   helps to achieve a good performance. As PNT1 becomes
                   larger, less patterns will be tried on the first
                   pass. As PNT2 becomes smaller, more patterns will be
                   tried on the second pass.

                   MD is the maximum clearance to try a skeleton. The
                   clearance must be an integer in the range 4..30
                   (default 6). The shape recognition algorithm will
                   refuse to try to fit an skeleton into a symbol if
                   the difference of the widths or heights of them is
                   larger than twice the clearance.

                   Examples:

                   -P 50,5,8
                   -P 40,3,6

                 */
                else if (c == 'P') {
                        read_pnt(optarg);
                }

                /* (book)

                   -R doubts

                   Maximum number of doubts per run. The argument must be
                   an integer (default 30).
                 */
                else if (c == 'R') {
                        max_doubts = atoi(optarg);
                        if (max_doubts > 1000)
                                max_doubts = 1000;
                        if (max_doubts < 1)
                                max_doubts = 1;
                }

                /* (book)

                   -V

                   Print version and compilation options and exit.
                 */
                else if (c == 'V') {
                        warn("this is Clara OCR version %s", version);
#ifdef HAVE_POPEN
                        warn("compiled with HAVE_POPEN");
#else
                        warn("compiled without HAVE_POPEN");
#endif
#ifdef MEMCHECK
                        warn("compiled with MEMCHECK");
#else
                        warn("compiled without MEMCHECK");
#endif
#ifdef FUN_CODES
                        warn("compiled with FUN_CODES");
#else
                        warn("compiled without FUN_CODES");
#endif
                        exit(0);
                }

                /* (book)

                   -W

                   Web mode. Will read from the doubts subdir the input
                   collected from web, and will dump on that same directory
                   the doubts to be reviewed.
                 */
                else if (c == 'W') {
                        web = 1;
                }

                /* (book)

                   -X 0|1

                   Switch off (0) or on (1) index checking. Index checking is
                   performed in some critical points in order to detect memory
                   leaks. Index checking is unavailable when Clara is compiled
                   with the symbol MEMCHECK undefined.
                 */
                else if (c == 'X') {
                        if (strcmp(optarg, "0") == 0) {
#ifdef MEMCHECK
                                dontcheck = 1;
#endif
                        }
                        if (strcmp(optarg, "1") == 0) {
#ifdef MEMCHECK
                                dontcheck = 0;
#else
                                warn("oops.. index checking is unavailable\n");
#endif
                        }
                }


                /* (book)

                   -Z ZPS

                   ZPS, that is, the size of the bitmap pixels measured in
                   display pixels, when in fat bit mode. Must be a small odd
                   integer (1, 3, 5, 7 or 9).
                 */
                else if (c == 'Z') {
                        ZPS = atoi(optarg);
                        if ((ZPS % 2) != 1)
                                --ZPS;
                        if (ZPS < 1)
                                ZPS = 1;
                        if (ZPS > 9)
                                ZPS = 9;
                }

                /*
                   Error detected. The error message will be generated
                   by getopt, so we just exit.
                 */
                else if (c == ':') {
                        fatal(DI, NULL);
                } else if (c == '?') {
                        fatal(DI, NULL);
                }

                /* parse finished */
                else if (c == -1)
                        e = 1;

                /* hope this will never happen */
                else {
                        fatal(IE, "unexpected getopt return value");
                }
        }

        /* workdir */
        if (!complexdir) {
                workdir = g_strdup(pagesdir);
        }



        /* free buffers */
        g_free(argb);
        g_free(largv);
        g_free(disp);
}

/*

Initialize main data structures

*/
void init_ds(void) {
        int i, j;

#ifdef HAVE_SIGNAL
        /* alarm queue */
        alrmq = g_new(int, (alrmqsz = 20));
#endif

        /* register GUI buttons */
#if 0
        register_button(bzoom, "zoom");
        register_button(bzone, "zone");
        register_button(bocr, "OCR");
        register_button(bstop, "stop");
        register_button(balpha, "");
        register_button(btype, "");
        register_button(bbad, "bad");
        if (pp_test)
                register_button(btest, "test");
#endif
        /* alphabets data */
        init_alphabet();

        /* build list of pages */
        build_plist(page_dir);

        /* alloc HTML text buffer */
        text = g_malloc(textsz = 15000);
        text[0] = 0;

        /* cfont initialization */
        for (i = 0; i < FS; ++i)
                for (j = 0; j < FS; ++j) {
                        cfont[i + j * FS] = 0;
                }

        /* pattern types initial size */
        enlarge_pt(10);

        /* minimum scores for matches */
        strong_match[CL_SKEL] = 64;
        weak_match[CL_SKEL] = 36;
        strong_match[CL_BM] = 100;
        weak_match[CL_BM] = 100;
        strong_match[CL_PD] = 100;
        weak_match[CL_PD] = 100;

        /*
           64-bit pixel mapping.
         */
        {
                int i, j;

                for (i = 0; i < 256; ++i) {
                        p64[i] = s64[i] = 0;
#ifdef BIG_ENDIAN
                        for (j = 1; j <= 128; j <<= 1) 
#else
                        for (j = 128; j >= 1; j >>= 1) 
#endif
                                {
                                        p64[i] = (p64[i] << 8);
                                        if (i & j) {
                                                p64[i] += BLACK;
                                                ++(s64[i]);
                                        } else
                                                p64[i] += WHITE;
                                }
                }
        }

        /*
           number of bits 1 per 8-bit value.
         */
        snbp(-1, NULL, NULL);

        /* pre-computed circumferences */
        comp_circ(30);
}

/*

Count number of classes used.

*/
int count_classes(void) {
        char *used;
        int i, k, c, *p;

        used = g_alloca(topp + 1);
        memset(used, 0, topp + 1);
        for (k = 0, p = ps; k <= topps; ++k, ++p) {
                i = id2idx(mc[*p].bm);
                if ((0 <= i) && (i <= topp)) {
                        used[i] = 1;
                }
        }
        for (c = 0, i = 0; i <= topp; ++i) {
                c += used[i];
        }
        return (c);
}

/*

Make a temporary name with at most MAXFNL+1 chars (including the
terminator) concatenating a and b. Optionally copies it into d.

*/
char *mkpath(char *d, char *a, char *b) {
        int la, lb;
        static char p[MAXFNL + 1];

        la = strlen(a);
        lb = strlen(b);
        if (la + lb > MAXFNL) {
                fatal(BO, "paths too long: %s, %s\n", a, b);
        }
        strcpy(p, a);
        strcat(p, b);
        if (d != NULL) {
                strcpy(d, p);
        }
        return (p);
}

/*

Write the report...

*/
void write_report(char *fn) {
        int cur_dw;
        int F;
        char *p;

        cur_dw = CDW;
        CDW = PAGE_LIST;
        ge2txt();
        p = mkpath(NULL, workdir, fn);
        F = open(p, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        write(F, text, strlen(text));
        close(F);
        show_hint(2, "Report saved on file %s", fn);
        CDW = cur_dw;
}

/*

Step 1
Save session.

*/
int save_session(int reset) {
        int r = 1;
        static int st, myreset;

        if (reset) {

                /*
                   A full OCR request, but the page to ocr is already
                   loaded, so don't save it.
                 */
                if ((onlystep < 0) && (to_ocr == cpage))
                        return (0);

                /* -T inhibits session saving */
                if (selthresh)
                        return (0);

                /* if quitting, ask for session saving */
                if (finish == 2) {
                        if (hist_changed || font_changed || sess_changed)
                                enter_wait("save session before quitting?",
                                           1, 3);
                        else {
                                finish = 1;
                                return (0);
                        }
                }

                st = 1;
                myreset = 1;
                return (1);
        }

        /* first save the acts */
        if (st == 1) {

                /* quit without saving */
                if (finish == 2) {
                        if (key_pressed == 0) {
                                finish = 1;
                                return (0);
                        } else
                                show_hint(3, NULL);
                }

                /* no need to save the acts */
                if (myreset && (!hist_changed)) {
                        myreset = 1;
                        st = 2;
                }

                /* still saving */
                else if (dump_acts(acts_file, myreset))
                        myreset = 0;

                /* just finished */
                else {
                        st = 2;
                        myreset = 1;
                }

                r = 1;
        }

        /* second save the patterns */
        else if (st == 2) {

                /* no need to save the font */
                if (myreset && (!font_changed)) {
                        st = 3;
                        myreset = 1;
                        r = 1;
                }

                /* still saving */
                else if (dump_patterns(patterns_file, myreset)) {
                        myreset = 0;
                        r = 1;
                }

                /* just finished */
                else {
                        if (cpage >= 0) {
                                st = 3;
                                myreset = 1;
                                r = 1;
                        } else
                                r = 0;
                }
        }

        /* third save the session */
        else if (st == 3) {

                /* free the pixmap */
                if ((myreset) && (pixmap != NULL)) {
                        spyhole(0, 0, 1, 0);
                        g_free(pixmap);
                        pixmap = NULL;
                }

                /*
                   No need to save (as the flag sess_changed was recently introduced,
                   we always save the session to avoid data losses, however we
                   expect to uncomment the sess_changed test below in the near future)
                 */
                if ((myreset) &&
                    ((cpage < 0) || (npages <= cpage) ||
                     (!cl_ready) /* || (sess_changed == 0) */ )) {

                        r = 0;
                }

                /* still saving */
                else if (dump_session(session_file, myreset)) {
                        myreset = 0;
                        r = 1;
                }

                /* just finished */
                else {
                        r = 0;
                }
        }

        /* now we can quit the program */
        if ((r == 0) && (finish == 2))
                finish = 1;

        return (r);
}

/*

Step 2
Load the page.

*/
int step_2(int reset) {
        int r = 1;
        static int st, myreset, k;

        if (reset) {
                free_page();
                st = 0;
                myreset = 1;
        }

        /* read acts */
        if (st == 0) {
                if (act == NULL)
                        recover_acts(acts_file);
                st = 1;
                myreset = 1;
                r = 1;
        }

        /* read patterns */
        else if (st == 1) {
                if (pattern == NULL) {
                        recover_patterns(patterns_file);
                        st = 2;
                        k = 0;
                } else
                        st = 3;

                myreset = 1;
                r = 1;
        }

        /* compute skeletons */
        else if (st == 2) {

                if (k < topp) {
                        if (k % DPROG == 0)
                                show_hint(0, "computing skeleton %d/%d", k, topp);
                        pskel(k++);
                        myreset = 0;
                } else {
                        st = 3;
                        myreset = 1;
                }
                r = 1;
        }

        /* read page */
        else if (st == 3) {

                /* continue reading the page */
                r = load_page(to_ocr, myreset, 0);

                /* finished reading */
                if (r == 0) {
                        new_page();
                } else
                        myreset = 0;
        }

        return (r);
}

/*

Step 3
Preprocessing.

*/
int step_3(int reset) {
        static int wd, st;
        int r;

        /* reset step */
        if (reset) {

                /* reset state */
                st = 1;
                r = 1;
        }

        /*
           Choose and reset the deskewer to use (if any).

           Remark: As we don't have by now a flag to inform that the
           deskewer was already applied, so if the user asks
           twice to apply it, it'll be applied twice.
         */
        else if (st == 1) {

                /*
                   Remark: Giulio Lunati's deskewer applies only for graymaps.
                 */
                if ((pp_deskew) && (pm_t == 8) && (pixmap != NULL) &&
                    (!cl_ready)) {
                        wd = 1;
                        deskew(1);
                } else
                        wd = 0;

                /* not finished */
                r = 1;
                ++st;
        }

        /* continue deskewing */
        else if (st == 2) {

                if ((wd == 1) && (deskew(0))) {
                } else {
                        ++st;
                        if (pp_bal)
                                show_hint(0, "applying balance..");
                }

                /* not finished */
                r = 1;
        }

        /* balance */
        else if (st == 3) {

                if (pp_bal) {
                        balance();
                }
                r = 1;
                ++st;
        }

        /* save the preprocessed page, if requested */
        else if (st == 4) {

                if (pp_save) {
                        char a[MAXFNL + 1];
                        mkpath(a, workdir, pagebase);
                        mkpath(a, a, "_pp.pgm");
                        wrzone(a, 0);
                }
                r = 1;
                ++st;
        }

        /* finished */
        else {
                r = 0;
        }

        /* ready to start next step */
        return (r);
}

/*

Step 4
Detect blocks

*/
int step_4(int reset) {
        static int st;

        if (dontblock == 0) {

                if (reset) {
                        if ((zones == 0) && (pixmap != NULL) &&
                            (pm_t == 8)) {
                                blockfind(1, 0);
                                st = 1;
                                return (1);
                        }
                }

                else if (st == 1) {
                        if (blockfind(0, 0))
                                return (1);
                }
        }

        /* finished */
        show_hint(0, "preparing to binarize..");
        return (0);
}

/*

Step 5
Binarization and Segmentation.

*/
int step_5(int reset) {
        static int myreset, pm;
        int r;

        if (reset) {

                /* switch off partial matching */
                pm = p_match;
                p_match = 0;

                /* nothing to do */
                if ((pixmap == NULL) || (cl_ready))
                        r = 0;
                else {

                        /* give back the spyhole buffer, if any */
                        spyhole(0, 0, 1, 0);

                        /* now compute the threshold, if requested */
                        if (h_thresh) {
                                thresh_val = pp_thresh();
                                /* printf("threshold is %d\n",thresh_val); */
                                show_hint(3, NULL);
                                show_hint(0, "now binarizing");
                        }

                        if (pp_avoid_links > eps)
                                avoid_links(pp_avoid_links);

                        /* now double resolution, if requested */
                        if (pp_double) {
                                int i, t, p;

                                hqbin();
                                for (t = 0; t < zones; ++t) {
                                        for (p = t * 8, i = 0; i < 8; ++i)
                                                limits[p++] *= 2;
                                }
                        }

                        r = 1;
                }
                myreset = 1;
        }

        /* binarize PGM */
        else {

                /* continue reading the page */
                r = load_page(to_ocr, myreset, 1);

                /* finished reading */
                if (r == 0) {
                        redraw_document_window();
                } else
                        myreset = 0;
        }

        /* restore partial matching status */
        if (r == 0)
                p_match = pm;

        return (r);
}

/*

Step 6
Consist the data structures.

*/
int step_6(int reset) {
        int r;

        if (reset) {


        }

        r = cons(reset);

        /* FATAL */
        if (r == 2) {

                fatal(DI, mb);
        }

        return (r);
}

/*

Step 7
Prepare patterns

*/
int step_7(int reset) {
        static int st = 0;

        /* wakeup */
        if (reset) {
                show_message("starting OCR");
                // show_hint(1,"starting OCR");
                //redraw_stline = 1;
                st = 1;

                if (symbols != 0)
                        ocr_r =
                            100 * (((float) symbols - doubts) / symbols);
                else
                        ocr_r = 0;

                prepare_patterns(1);
                return (1);
        }

        /*
           Continue preprocessing.
         */
        if (prepare_patterns(0) == 0) {

                /* ready to start next run */
                st = 0;
                return (0);
        }

        /* did not complete */
        return (1);
}

/*

Read and process revision data received from the web.

*/
void process_webdata(void) {
        int i, j;
        DIR *D;
        struct dirent *e;
        char *r, *a;

        /*
           The variable r must be long enough to store
           the complete path and filename for revision files.
         */

        /*
           Process revision data (web case).

           Scan the working directory looking for files

           doc.[0-9]+.[0-9]+.[A-Za-z0-9]+

           and read their contents to generate new font entries
           and confirm the transliteration of the page
           symbols.

           The revision file has one revision act codified as XML.
         */
        if (web) {

                if ((D = opendir(doubtsdir)) == NULL) {
                        fatal(IO, "could not open doubts dir \"%s\"",
                              doubtsdir);
                }

                while ((e = readdir(D)) != NULL) {

                        /* does the doubt filename spec match? */
                        a = e->d_name;
                        if (strncmp(a, pagebase, i = strlen(pagebase)) !=
                            0)
                                continue;
                        if (a[i] != '.')
                                continue;
                        for (++i; isdigit(a[i]); ++i);
                        if ((i <= 0) || (a[i] != '.'))
                                continue;
                        for (j = ++i; isdigit(a[i]); ++i);
                        if ((i <= j) || (a[i] != '.'))
                                continue;
                        for (j = ++i; isalnum(a[i]); ++i);
                        if ((i > j) && (a[i] == 0)) {

                                if (trace)
                                        tr("revision data file %s found", a);

                                /* process and remove revision file */
                                r = g_strconcat(doubtsdir, a, NULL);
                                prf(r);
                                unlink(r);
                                g_free(r);
                        }
                }
                closedir(D);
        }
}

/*

Step 8
Process revision data.

*/
int step_8(int reset) {
        static int st, myreset;
        int r;

        /* reset */
        if (reset) {
                st = 1;
                r = 1;
        }

        else if (st == 1) {

                /* process the material received from the web */
                if (web && batch_mode) {
                        process_webdata();
                        r = 0;
                }

                /* collect the revision data into one revision "act" */
                else {
                        r = from_gui();
                        if (r) {
                                push_text();
                                st = 2;
                                myreset = 1;
                        } else {
                                st = 0;
                        }
                }
        }

        else if (st == 2) {
                pop_text();
                r = review(myreset, NULL);
                myreset = 0;
                if (r == 0) {
                        to_rev = 0;
                        st = 0;
                }
        }

        /* just to avoid a compilation warning */
        else
                r = 0;

        return (r);
}

/*

Selector for bitmap comparison methods.

*/
int selbc(int c) {
        int r;

        if (classifier == CL_SKEL)
                r = classify(c, bmpcmp_skel, 1);
        else if (classifier == CL_BM)
                r = classify(c, bmpcmp_map, 1);
        else if (classifier == CL_PD)
                r = classify(c, bmpcmp_pd, 1);
        else if (classifier == CL_SHAPE)
                r = classify(c, bmpcmp_shape, 1);
        else
                r = 0;
        return (r);
}

/*

Step 9
Classify the symbols

*/
int step_9(int reset) {
        static int k;

        /* unavailable if nonsegmented */
        if (!cl_ready)
                return (0);

        /* prepare */
        if (reset) {

                /* choose first symbol to visit */
                show_hint(3, NULL);
                k = (justone) ? curr_mc : 0;
                selbc(-1);
                sess_changed = 1;
                return (1);
        }

        /* compute the zone */
        if (mc[k].c < 0) {
                int i, x, y;

                x = (mc[k].l + mc[k].r) / 2;
                y = (mc[k].t + mc[k].b) / 2;
                for (i = 0; (i < zones) && (mc[k].c < 0); ++i) {
                        if (inside(x, y, limits + 8 * i, 4) == 1)
                                mc[k].c = i;
                }
        }

        /*
           Compute the class of symbol k
         */
        if (selbc(k) == 0) {

                /*
                   Class computation finished. Add symbol to font
                   if no class was chosen.
                 */
                if ((!C_ISSET(mc[k].f, F_SS)) &&
                    (mc[k].ncl == 1) &&
                    (bf_auto) &&
                    ((mc[k].tr == NULL) || (!C_ISSET(mc[k].tr->f, F_BAD)))
                    && (sdim(k))) {
                        int i;

                        /* TODO: read alphabet and flags from surrounding word */
                        i = update_pattern(k, NULL, -1, UNDEF, -1, -1, 0);
                        C_SET(mc[k].f, F_SS);
                        mc[k].bm = pattern[i].id;
                        pattern[topp].cm = 1;
                }

                /* start next step */
                if ((justone == 1) || (++k > tops)) {
                        k = -1;
                        return (0);
                }

                /* ready to visit next symbol */
                else {
                        selbc(-1);
                }
        }

        /* did not complete */
        return (1);
}

/*

Step 10
Geometric merging

*/
int step_10(int reset) {
        static int k;
        int r;

        /* unavailable if nonsegmented */
        if (!cl_ready)
                return (0);

        if (reset) {
                k = 0;
        }

        geo_merge(k);

        if (++k > tops) {
                r = 0;
        } else {
                r = 1;
        }

        return (r);
}

/*

Step 11
Detect text lines and words.

*/
int step_11(int reset) {
        /* unavailable if nonsegmented */
        if (!cl_ready)
                return (0);

        /* Wakeup */
        if (reset) {

                build(1);
                return (1);
        }

        /* compute text lines and words */
        if (build(0) == 0) {

                /* ready to start next step */
                dw[PAGE_OUTPUT].rg = 1;
                dw[PAGE_SYMBOL].rg = 1;
                redraw_document_window();
                return (0);
        }

        /* did not complete */
        return (1);
}

/*

Step 12
Generate hints from dictionary.

*/
int step_12(int reset) {
        static int w;

        /* unavailable if nonsegmented */
        if (!cl_ready)
                return (0);

        if (reset) {
        }

        /* Wakeup */
        if (reset) {
                w = 0;
                return (1);
        }

        /* compute text lines and words */
        if (w <= topw) {
                int a, s;
                char t[81];

                /* transliteration of word w */
                for (s = 0, a = word[w].F; (s < 80) && (a >= 0);
                     a = mc[a].E) {

                        /* send to output the number of this symbol */
                        if (mc[a].tr == NULL)
                                t[s++] = '?';

                        /* concatenate the transliteration of this symbol */
                        else if (80 - s >= strlen(mc[a].tr->t)) {
                                strcpy(t + s, mc[a].tr->t);
                                s += strlen(mc[a].tr->t);
                        }
                }

                if (s > 0) {
                        t[s] = 0;
                        /* printf("examining word %d (%s)\n",w,t); */
                }

                /* prepare the next */
                ++w;
                return (1);
        }

        /* complete */
        return (0);
}

/*

Step 13
Generates output and stats.

*/
int step_13(int reset) {
        /* unavailable if nonsegmented */
        if (!cl_ready)
                return (0);

        if (cpage >= 0) {

                /* count  classes */
                classes = count_classes();

                /* generates output, count symbols and doubts */
                mk_page_output(outp_format);

                /* estimate links */
                {
                        int i, mw, mh;
                        sdesc *s;

                        mw = mh = 0;
                        for (i = 0; i < topp; ++i) {
                                if (pattern[i].tr != NULL) {
                                        if (pattern[i].bw > mw)
                                                mw = pattern[i].bw;
                                        if (pattern[i].bh > mh)
                                                mh = pattern[i].bh;
                                }
                        }
                        if (mw == 0)
                                mw = FS;
                        if (mh == 0)
                                mh = FS;

                        links = 0;
                        for (i = 0; i <= topps; ++i) {

                                s = mc + ps[i];
                                if (((s->r - s->l + 1) > FS) ||
                                    ((s->b - s->t + 1) > FS)) {

                                        ++links;
                                }
                        }
                }
        }

        if (cpage >= 0) {

                char txt[MAXFNL + 1];
                char st[80];
                int F, n;
                float r;
                time_t tp;

                /* save recognized text to file */
                mkpath(txt, workdir, pagebase);
                if (outp_format == 1)
                        mkpath(txt, txt, ".html");
                else if (outp_format == 2)
                        mkpath(txt, txt, ".txt");
                else if (outp_format == 3)
                        mkpath(txt, txt, ".dtxt");
                F = open(txt, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if ((!selthresh) && (F < 0)) {
                        if (verbose)
                                warn("oops.. unable to open \"%s\"", txt);
                } else if (!selthresh) {

                        /*
                           include stats on the output.

                           OOPS.. if you change this code, change clara.pl accordingly,
                           so it will be able to recover the stats from the .html file.
                         */
                        if (outp_format == 1) {
                                if (dl_ne[cpage] > 0)
                                        r = ((float) dl_ne[cpage] -
                                             dl_db[cpage]) / dl_ne[cpage];
                                else
                                        r = 0.0;
                                n = sprintf(st,
                                            "<P><B>Symbols: %d/%d (%0.2f)\n",
                                            dl_ne[cpage] - dl_db[cpage],
                                            dl_ne[cpage], r);
                                if (verbose && (write(F, st, n) != n)) {
                                        warn("oops.. failure writing \"%s\"", txt);
                                }
                                tp = time(NULL);
                                n = sprintf(st,
                                            "<BR>Last processed at %s</B>\n<P>",
                                            ctime(&tp));
                                if (write(F, st, n) != n) {
                                        warn("oops.. failure writing \"%s\"", txt);
                                }
                        }

                        /* add the OCR result */
                        if (write(F, text, topt + 1) != (topt + 1)) {
                                warn("oops.. failure writing \"%s\"", txt);
                        }
                        close(F);
                }

                return (0);
        }

        /* start next step */
        return (0);
}

/*

Step 14
Generate doubts for web revision.

*/
int step_14(int reset) {
        static int k = 0;
        char n[MAXFNL + 1];
        int i /*,j */ ;

        /* unavailable if nonsegmented */
        if (!cl_ready)
                return (0);

        /*
           The variable n must be long enough to store
           the complete path and filename of the bitmap files for
           revision.
         */
        if ((web) &&
            (strlen(doubtsdir) + 2 + strlen(pagebase) - 4 + 15 > MAXFNL)) {
                fatal(BO, "filenames for revision material too long!");
        }

        /*
           Generate files for revision. The file

           u.page.symbol.pbm

           Contains the bitmap of the word surrounding the doubt
           and the doubtful symbol. The revision process
           must generate the file

           time.pid.host

           it will be processed by the next OCR run.
         */
        if (web) {
                int *a, *p, b;
                int D, U, AA, K;
                int nd;

                /* alloc memory on stack */
                if (((a = g_newa(int, (topps + 1))) == NULL) ||
                    ((p = g_newa(int, (tops + 1))) == NULL)) {

                        fatal(ME, "memory exhausted");
                }

                /*
                   Computes the revision preference of each
                   preferred symbol. The per-symbol revision
                   preference is

                   KDUAA

                   where K is 2 if the symbol is unknown, 1 if it is
                   dubious, or 0 otherwise, D is 1 if the
                   transliteration status is FAIL, or 0 otherwise, U
                   is 1 if the surrounding word is defined and
                   contains other symbols, or 0 otherwise, and AA is
                   99 minus the number of anon revisions for this
                   symbol (or 0 if this number is negative).
                 */
                for (i = 0; i <= topps; ++i) {
                        a[i] = b = ps[i];

                        D = (C_ISSET(mc[b].f, F_SS));
                        U = (mc[b].sw >= 0) && (mc[b].sw != b);
                        K = (mc[b].tc ==
                             UNDEF) ? 2 : ((mc[b].tc == DUBIOUS) ? 1 : 0);
                        AA = 99 - mc[b].pb;
                        if (AA < 0)
                                AA = 0;

                        /* if (b == 2284)
                           p[b] = 30000;
                           else *//* if (mc[b].tc == SCHAR)
                           p[b] = K*10000 + D*1000 + U*100 + AA;
                           else
                           p[b] = 0; */

                        if (uncertain(mc[b].tc) && (mc[b].sw >= 0))
                                p[b] = 1;
                        else
                                p[b] = 0;
                }

                /* sort the symbols in revision preference order */
                qsi(a, 0, topps, (char *) p, sizeof(int), 0, 1);

                /* generate the bitmaps */
                for (k = nd = 0; (k <= topps) /* && (nd<max_doubts) */ ;
                     ++k) {
                        int b, j;

                        b = a[k];

                        if (p[b] >
                            0 /*(mc[b].sw >= 0) *//*&& (p[b] >= 1000) */ )
                        {

                                tr("selected doubt %d, preference %d", b,
                                   p[b]);

                                curr_mc = b;
                                j = b;

                                /* create entry on doubts dir */
                                if ((j == b) && (sdim(b) != 0)) {
                                        if (web) {
                                                sprintf(n, "%su.%s.%d.pbm",
                                                        doubtsdir,
                                                        pagebase, b);
                                                wrmc8(curr_mc, n, NULL);
                                        }
                                        /*
                                           doubt[nd] = b;
                                           mc[b].isd = 1;
                                         */

                                        ++nd;
                                        ++mc[b].pb;
                                }
                        }
                }
        }

        /* completed */
        return (0);
}

/*

Step 15
Caller-defined action.

This step executes special actions outside the OCR main steps. These
actions are executed using the OCR thread just to take advantage of
the existing architecture. By now the following actions exist:

1. Set pattern type
2. Search barcode
3. Instant thresholding
4. Image info
5. PP tests

*/
int step_15(int reset) {
        static int st;

        /*
           Set pattern type
           ----------------
         */
        if (ocr_other == 1) {
                char s[MMB + 1];
                static int t;

                if (reset) {
                        snprintf(s, MMB, "Change types from 0 to [1-%d]: ",
                                 toppt);
                        enter_wait(s, 1, 4);
                        st = 1;
                        return (1);
                }

                if (st == 1) {
                        t = atoi(inp_str);
                        if ((t < 0) || (100 < t)) {
                                show_hint(2, "invalid pattern type %d", t);
                                return (1);
                        } else {
                                char a[31];

                                if (t > toppt)
                                        enlarge_pt(t);
                                strncpy(a, pt[t].n, 30);
                                a[30] = 0;
                                snprintf(s, MMB,
                                         "Type %d (\"%s\") selected. Confirm?",
                                         t, a);
                                s[MMB] = 0;
                                enter_wait(s, 1, 3);
                                st = 2;
                                return (1);
                        }
                }

                else if (st == 2) {
                        if (key_pressed) {
                                int i, c;

                                for (i = 0, c = 0; i <= topp; ++i) {
                                        if (pattern[i].pt == 0) {
                                                pattern[i].pt = t;
                                                ++c;
                                        }
                                }
                                font_changed = 1;
                                show_hint(1,
                                          "changed %d types from 0 to %d",
                                          c, t);
                                dw[PATTERN_LIST].rg = 1;
                                dw[PATTERN_TYPES].rg = 1;
                                dw[PATTERN].rg = 1;
                        } else {
                                show_hint(1, "operation cancelled");
                        }
                }

                return (0);
        }

        /*
           Search barcode
           --------------
         */
        else if (ocr_other == 2) {

                if (reset) {
                        st = 1;
                        show_hint(1, "wait...");
                        return (1);
                }

                if (st == 1) {
                        search_barcode();
                        st = 2;
                        return (1);
                }

                return (0);
        }

        /*
           Instant thresholding
           --------------------
         */
        else if (ocr_other == 3) {

                if (reset) {
                        st = 1;
                        return (1);
                }

                /* display instant thresholding help */
                if (st == 1) {
                        char s[MMB + 1];

                        snprintf(s, MMB,
                                 "thresholding by %d (%1.2f), use <, >, + or - to change (ESC to finish)",
                                 thresh_val, ((float) thresh_val) / 256);
                        enter_wait(s, 0, 5);
                        st = 2;
                        return (1);
                }

                /* apply user's selection */
                if (st == 2) {
                        if (key_pressed == 0) {
                                return (0);
                        } else {
                                if (key_pressed == '<')
                                        thresh_val -= 10;
                                else if (key_pressed == '>')
                                        thresh_val += 10;
                                else if (key_pressed == '+')
                                        ++thresh_val;
                                else if (key_pressed == '-')
                                        --thresh_val;
                                if (thresh_val < 0)
                                        thresh_val = 0;
                                else if (thresh_val > 256)
                                        thresh_val = 256;
                                st = 1;
                                redraw_document_window();
                                return (1);
                        }
                }
        }

        /*
           Image info
           ----------
         */
        else if (ocr_other == 4) {

                /*
                   No page loaded.
                 */
                if (cpage < 0) {
                        printf("No page loaded, sorry\n\n");
                        return (0);
                }

                /* basic geometrical data */
                printf("Hello, the name of the loaded page is %s.\n",
                       pagename);
                printf("It has %d lines and each line has %d pixels\n",
                       YRES, XRES);

                /*
                   When a PBM or PGM file is loaded, the "pixmap" pointer points to
                   an array that stores the map of pixels as a sequence of lines. Each
                   pixel uses one bit (PBM) or one byte (PGM).
                 */
                if (pixmap != NULL) {

                        /* a bitmap (PBM file) is currently loaded */
                        if (pm_t == 1) {
                                printf
                                    ("It's an image where each pixel can assume just 'black' or 'white'.\n");
                        }

                        /* a graymap (PGM file) is currently loaded */
                        else if (pm_t == 8) {
                                printf
                                    ("It's an image where each pixel can assume one from 256 different\n");
                                printf
                                    ("'shades' (levels) of gray (0=Black, 255=White).\n");
                        }
                }

                /*
                   When Clara OCR reads a page already segmented, the pixmap array
                   points to NULL, so the original image is unavailable. Instead,
                   we have "closures" storing small pieces of that image.
                 */
                else {
                        printf
                            ("It's an already segmented image, read from a session file.\n");
                }

                printf("\n");
                return (0);
        }

        /*
           PP tests
           --------
         */
        else if (ocr_other == 5) {
                test();
        }

        /* unknown action */
        db("OCR_OTHER: unknown action %d\n", ocr_other);
        return (0);
}

gboolean continue_ocr_thunk(gpointer data) {
        continue_ocr();
        if (!ocring)
                return FALSE;
        else
                return TRUE;
}


/* (devel)

The function start_ocr
----------------------

Starts a complete OCR run or some individual OCR step on one
given page, or on all pages. For instance, start_ocr is called by
the GUI when the user presses the "OCR" button or when the user
requests loading of one specific page.

In fact, almost all user requested operation is performed as an
"ocr step"in order to take advantage from the execution model
implemented by the function continue_ocr. So start_ocr is the
starting point for attending almost all user requests.

If p is -1, process all pages, if p < -1, process only the current
page (cpage) otherwise process only the page p. If s>=0 then run only
step s, otherwise run all steps.

If the flag r is nonzero, will ignore the current classes (if
any) and recompute them again (this is meaningful only to the
symbol classification step).

*/
void start_ocr(int p, int s, int r) {
        /* ignore if OCRing */
        if (ocring)
                return;

        /* someone generated a bad call */
        if (s > LAST_STEP) {
                fatal(DI, "invalid page or step number when starting OCR");
        }

        /*
           Unless an OCR operation in course, set the
           flags that will be tested by the main loop and will
           provoke the operation startup.
         */
        if (!ocring) {
                starting = 1;
                ocring = 1;
                onlystep = s;

                g_idle_add(continue_ocr_thunk, NULL);
                if ((onlystep != OCR_SAVE) && (onlystep != OCR_LOAD)) {
                        // UNPATCHED: button[bocr] = 1;
                        //         redraw_button = bocr;
                }
                if (p == -1)
                        ocr_all = 1;
                else {
                        ocr_all = 0;
                        to_ocr = (p == -2) ? cpage : p;
                }
                recomp_cl = r;
                to_tr[0] = 0;
        }
}


/*

Continue to OCR.

*/
void continue_ocr(void) {
        static int st, reset, fp, last_dw, no_pattern, last_cdfc, lcr;
        int r = 1;

        /*
           This block is executed just once each time an OCR
           operation is requested.
         */
        if (starting) {

                /*
                   Remember the current page to come back after
                   finishing, but do not go back to the
                   WELCOME page.
                 */
                last_dw = (CDW == WELCOME) ? PAGE_LIST : CDW;

                starting = 0;
                stop_ocr = 0;
                cannot_stop = 0;
                st = 1;
                reset = 1;

                if (ocr_all) {
                        if ((cpage < 0) || (npages <= cpage)) {
                                to_ocr = 0;
                        } else
                                to_ocr = cpage;
                        fp = to_ocr;
                }

                if ((!ocr_all) &&
                    (onlystep != OCR_LOAD) &&
                    (onlystep != OCR_SAVE) &&
                    ((cpage < 0) || (npages <= cpage))) {

                        show_hint(1,
                                  "Please load a page before trying to OCR");
                        // UNPATCHED: button[bocr] = 0;
                        ocring = 0;
                        recomp_cl = 0;
                        return;
                }

                /*
                   if (limits_top <= 3) {
                   show_hint(1,"Please define an OCR zone before OCR");
                   ocring = button[bocr] = 0;
                   recomp_cl = 0;
                   return;
                   }
                 */

                /* avoiding exposing critial data structures to the display code */
                /* UNPATCHED:
                   if ((onlystep<0) && (CDW != PAGE_LIST))
                   setview(PAGE_LIST);
                 */
                /* remember statuses */
                lcr = cl_ready;
                no_pattern = ((cdfc < 0) || (topp < cdfc));
                last_cdfc = cdfc;
        }

        /*
           When the button "STOP" is pressed, a nonzero value is stored
           on the variable stop_ocr by the event handling routines. We
           test here the value of stop_ocr. If it's nonzero and the
           current OCR state is interruptible, then the current OCR
           operation is stopped.
         */
        if ((stop_ocr) && (!cannot_stop)) {
                ocring = 0;
                recomp_cl = 0;
                waiting_key = 0;
                ocr_all = -1;
                if (batch_mode == 0) {
                        // redraw_dw = 1;
                        show_message("OCR stopped");
                        gtk_widget_set_sensitive(btnOcr, TRUE);
                        gtk_widget_set_sensitive(btnStop, FALSE);
                }
                stop_ocr = 0;
        }

        /* prepare to account */
        ocr_start = time(NULL);

        /*

           The OCR steps.

         */
        switch (st) {

                /*
                   1. Save the session This step is executed when
                   the user requested a session saving or a
                   page load (in order to save the current page
                   before loading the new one).
                 */
        case 1:
                if ((onlystep <= 2) || ocr_all)
                        r = save_session(reset);
                else
                        r = 0;
                break;

                /*
                   2. Load the page.
                 */
        case 2:
                if ((onlystep == 2) || ocr_all)
                        r = step_2(reset);
                else
                        r = 0;
                break;

                /*
                   3. Preproc.
                 */
        case 3:
                if ((onlystep < 0) || (onlystep == 3))
                        r = step_3(reset);
                else
                        r = 0;
                break;

                /*
                   4. Detect blocks.
                 */
        case 4:
                if ((onlystep < 0) || (onlystep == 4))
                        r = step_4(reset);
                else
                        r = 0;
                break;

                /*
                   5. Binarization.
                 */
        case 5:
                if ((onlystep < 0) || (onlystep == 5) ||
                    ((onlystep == 15) && (ocr_other == 2)))
                        r = step_5(reset);
                else
                        r = 0;
                break;

                /*
                   6. Consist the data structures.
                 */
        case 6:
                if ((onlystep < 0) || (onlystep == 6))
                        r = step_6(reset);
                else
                        r = 0;
                break;

                /*
                   7. Optimize patterns
                 */
        case 7:
                if ((onlystep < 0) || (onlystep == 7))
                        r = step_7(reset);
                else
                        r = 0;
                break;

                /*
                   8. Process revision data.
                 */
        case 8:
                if ((onlystep < 0) || (onlystep == 8))
                        r = step_8(reset);
                else
                        r = 0;
                break;

                /*
                   9. Classify the symbols
                 */
        case 9:
                if ((onlystep < 0) || (onlystep == 9) ||
                    ((onlystep == 8) && get_flag(FL_AUTO_CLASSIFY))) {
                        r = step_9(reset);
                } else
                        r = 0;
                break;

                /*
                   10. Geometric merging
                 */
        case 10:
                if ((onlystep < 0) || (onlystep == 10))
                        r = step_10(reset);
                else
                        r = 0;
                break;

                /*
                   11. Detect text lines and words.
                 */
        case 11:
                if ((onlystep < 0) || (onlystep == 11))
                        r = step_11(reset);
                else
                        r = 0;
                break;

                /*
                   12. Generate hints from dictionary.
                 */
        case 12:
                if ((onlystep < 0) || (onlystep == 12))
                        r = step_12(reset);
                else
                        r = 0;
                break;

                /*
                   13. Generate output and stats. This step is
                   mandatory.
                 */
        case 13:
                r = step_13(reset);
                break;

                /*
                   14. Generate doubts for web revision.
                 */
        case 14:
                if ((onlystep < 0) || (onlystep == 14))
                        r = step_14(reset);
                else
                        r = 0;
                break;

                /*
                   15. Caller-defined action.
                 */
        case 15:
                if (onlystep == 15)
                        r = step_15(reset);
                else
                        r = 0;
                break;
        }

        /* account */
        ocr_time += time(NULL) - ocr_start;

        /* prepare the next call of continue_ocr */
        if (r == 0) {

                /* OCR finished */
                if (++st > LAST_STEP) {
                        st = 1;

                        if (verbose) {
                                printf("finished page %s\n", pagename);
                        }

                        /* stats */
                        dl_r[cpage] = ++runs;
                        dl_t[cpage] = ocr_time;
                        dl_lr[cpage] = ocr_r;
                        dl_c[cpage] = classes;

                        /* prepare next page to OCR */
                        if (ocr_all) {

                                /* next page to OCR */
                                if (++to_ocr >= npages)
                                        to_ocr = 0;

                                /* just processed the last page */
                                if (to_ocr == fp) {
                                        ocr_all = 0;
                                        // UNPATCHED: button[bocr] = 0;
                                        ocring = 0;
                                        recomp_cl = 0;

                                        /* save session if batch mode */
                                        if (batch_mode) {
                                                start_ocr(cpage, OCR_SAVE,
                                                          0);
                                        }


                                        /* enter three-state PAGE */
#if 0
                                        if ((!lcr) && (cl_ready) &&
                                            (*cm_o_pgo == 'X')) {
                                                *cm_o_pgo = ' ';
                                                opage_j1 = page_j1 =
                                                    PT + TH + (PH -
                                                               TH) / 3;
                                                opage_j2 = page_j2 =
                                                    PT + TH + 2 * (PH -
                                                                   TH) / 3;
                                                check_dlimits(0);
                                        }
#endif
                                        dw[PAGE_LIST].rg = 1;
                                        redraw_document_window();
                                }

                                else
                                        ocr_start = time(NULL);
                        }

                        /* reset statuses */
                        else {
                                ocring = 0;
                                // UNPATCHED: button[bocr] = 0;
                                recomp_cl = 0;
#if 0
                                /* enter three-state PAGE */
                                if ((!lcr) && (cl_ready) &&
                                    (*cm_o_pgo == 'X')) {
                                        *cm_o_pgo = ' ';
                                        opage_j1 = page_j1 =
                                            PT + TH + (PH - TH) / 3;
                                        opage_j2 = page_j2 =
                                            PT + TH + 2 * (PH - TH) / 3;
                                        check_dlimits(0);
                                }
#endif
                                /* prepare to quit if batch mode and already saved sesseion */
                                if (batch_mode && (onlystep == OCR_SAVE)) {
                                        finish = 1;
                                }

                                /* save session if batch mode */
                                else if (batch_mode) {
                                        start_ocr(cpage, OCR_SAVE, 0);
                                }

                                /* ack message for manual training */
                                else if ((dw[PAGE].v) &&
                                         (onlystep == OCR_REV)) {
                                        /* no ack by now */
                                }
                        }

                        /* must regenerate PAGE_LIST */
                        dw[PAGE_LIST].rg = 1;

                        /* must regenerate pattern form */
                        if ((no_pattern) || (cdfc != last_cdfc)) {
                                edit_pattern(0);
                        }
                        redraw_document_window();
                }

                reset = 1;
                cannot_stop = 0;

        } else {
                reset = 0;
                cannot_stop = 1;
        }
}
 
#ifdef HAVE_SIGNAL
/*

SIGPIPE handler.

Clara receives a SIGPIPE when the user presses the "destroy"
decoration button.

*/
void handle_pipe(int p) {
        if (debug)
                fatal(IO, "SIGPIPE received (oh! have you killed me?)");
        else
                exit(0);
}

/*

Set new alarm (delay d in microseconds).

*/
void new_alrm(unsigned d, int who) {
#ifdef __EMX__
        struct itimerval {
                struct timeval it_interval;     /* timer interval */
                struct timeval it_value;        /* current value */
        };
#endif
        struct itimerval it;

        *alrmq = who;
        topalrmq = 0;
        it.it_interval.tv_sec = 0;
        it.it_interval.tv_usec = 0;
        it.it_value.tv_sec = d / 1000000;
        it.it_value.tv_usec = d % 1000000;
#ifndef __EMX__
        setitimer(ITIMER_REAL, &it, NULL);
#endif
}


/*

SIGALRM handler.

*/
void handle_alrm(int p) {
        int who;

        who = *alrmq;
        topalrmq = -1;

        switch (*alrmq) {

#ifdef FUN_CODES

                /* slide banner */
        case 1:

                /* continue */
                if (dw[WELCOME].v) {
                        redraw_document_window();
                        new_alrm(50000, 1);
                }

                /* disable */
                else
                        fun_code = 0;

                break;

                /* move flea around bitmap */
        case 3:

                /* continue */
                if (dw[PAGE_FATBITS].v) {

                        /*
                           Continue only if nobody stopped the flea and the
                           last redraw request was attended.
                         */
                        if ((fun_code == 3) && (last_fp < 0)) {
                                last_fp = curr_fp;
                                if (++curr_fp > topfp)
                                        curr_fp = 0;
                                new_alrm(50000, 3);
                        }
                        redraw_flea();
                }

                break;

#endif

                /*
                   Presentation delay expired. Ideally we should
                   only set a flag to inform xevents or some other
                   routine outside the handler to call the function
                   "presentation".
                 */
        case 2:
                UNIMPLEMENTED();
                break;

        }
}
#endif
