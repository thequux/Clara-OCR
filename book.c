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

book.c: Documentation only

*/

/*

This module does not contain code, but only documentation blocks
that are'nt (currently) attached to specific pieces of code on
the other modules.

*/

/* (tutorial)

NAME
----

clara - a cooperative OCR

SYNOPSIS
--------

clara [options]


DESCRIPTION
-----------

Welcome. Clara OCR is a free OCR, written for systems supporting
the C library and the X Windows System. Clara OCR is intended for the
cooperative OCR of books. There are some screenshots available at
CLARA_HOME.

This documentation is extracted automatically from the comments
of the Clara OCR source code. It is known as "The Clara OCR
Tutorial". There is also an advanced manual known as "The Clara
OCR Advanced User's Manual" (man page clara-adv(1), also
available in HTML format). Developers must read "The Clara OCR
Developer's Guide" (man page clara-dev(1), also available in HTML
format).

CONTENTS
--------

Making OCR

    Starting Clara
    Some few command-line switches
    Training symbols
    Saving the session
    OCR steps
    Classification
    Note about how Clara OCR classification works
    Building the output
    Handling broken symbols
    Handling accents
    Browsing the book font
    Useful hints
    Fun codes

AVAILABILITY

CREDITS

*/

/* (book)


NAME
----

clara - a cooperative OCR

SYNOPSIS
--------

clara [options]


DESCRIPTION
-----------

Welcome. Clara OCR is a free OCR, written for systems supporting
the C library and the X Windows System. Clara OCR is intended for
the cooperative OCR of books. There are some screenshots
available at CLARA_HOME.

This documentation is extracted automatically from the comments
of the Clara OCR source code. It is known as "The Clara OCR
Advanced User's Manual". It's currently unfinished. First-time
users are invited to read "The Clara OCR Tutorial". Developers
must read "The Clara OCR Developer's Guide".


CONTENTS
--------

Welcome to Clara OCR

    Early historical notes
    Design notes
    Supported Alphabets
    Clara vs the others
    The requirements
    How to download and compile Clara
    Compilation and startup pitfalls

A first OCR project

    Scanning and thresholding
    Manual and histogram-based (global)
    Classification-based (local)
    Classification-based (global)
    Avoiding or correcting skew
    The work directory
    Building the book font
    Skeleton tuning
    Classification tentatives
    Alignment tuning

Complex procedures

    Using two directories
    Adding a page
    Multiple books
    Adding a book
    Removing a page
    Dealing with classification errors
    Rebuilding session files
    Importing revision data
    How to use the web interface
    Revision acts maintenance
    Analysing the statistics
    Upgrading Clara OCR

Reference of the Clara GUI

    The application window
    Tabs and windows
    The Application Buttons
    The Alphabet Map

Reference of the menus

    File menu
    Edit menu
    View menu
    Alphabets menu
    Options menu
    PAGE options menu
    PAGE_FATBITS options menu
    OCR steps menu

Reference of command-line switches

AVAILABILITY

CREDITS

*/

/* (devel)


NAME
----

clara - a cooperative OCR

SYNOPSIS
--------

clara [options]


DESCRIPTION
-----------

Welcome. Clara OCR is a free OCR, written for systems supporting
the C library and the X Windows System. Clara OCR is intended for the
cooperative OCR of books. There are some screenshots available at
CLARA_HOME.

This documentation is extracted automatically from the comments
of the Clara OCR source code. It is known as "The Clara OCR
Developer's Guide". It's currently unfinished. First-time users
are invited to read "The Clara OCR Tutorial". There is also an
advanced manual known as "The Clara OCR Advanced User's Manual".


CONTENTS
--------

Introducing the source code

    Language and environment
    Modularization
    The memory allocator
    Security notes
    Runtime index checking
    Background operation
    Global variables
    Path variables
    Bitmaps
    Execution model
    Return codes

Internal representation of pages

    Closures
    Symbols
    The sdesc structure and the mc array
    The preferred symbols
    Font size
    Symbol alignment
    Words and lines
    Acts and transliterations
    Symbol transliterations
    Transliteration preference
    Transliteration class computing
    The zones

Heuristics

    Skeleton pixels
    Symbol pairing
    The build step
    Resetting
    Synchronization
    The function list_cl

The GUI

    Main characteristics
    Geometry of the application window
    Geometry of windows
    Scrollbars
    Displaying bitmaps
    HTML windows overview
    Graphic elements
    XML support
    Auto-submission of forms

The Clara API

    Redraw flags
    OCR statuses
    The function setview
    The function redraw
    The function show_hint
    The function start_ocr

How to change the source code (examples)

    How to add a bitmap comparison method
    How to write a bitmap comparison function
    How to add an application button

Bugs and TODO list

AVAILABILITY

CREDITS


*/

/* (book)

Early historical notes
----------------------

For some years now we have tested and used OCR softwares, mainly
for old books. Popular OCR softwares (those bundled with
scanners) are useful tools. However, OCR is not a simple
task. The results obtained using those programs vary largely
depending on the the printed document, and, for most texts we're
interested on, the results are really poor or even unusable. In
fact, it's not a surprise that many digitalization projects
prefer not to use OCR, but typists only.

For a programmer, it is somewhat intuitive that OCR could achieve
good results even from low quality texts, when an add-hoc
approach is used, focusing one specific book (for
instance). Within this approach, OCR becomes a matter of finding
one software adequate for the texts you're trying to OCR, or
perhaps develop a new one. So a free and easy to customize OCR
(on the source code level) would be a valuable resource for text
digitalization projects.

Dealing with graphics is not among our main occupations, but
after analysing many scanned materials, we began to write some
simple and specialized recognition tools. More recently (in the
third quarter of 1999) a simple X interface linked to a naive
bitmap comparison heuristic was written. From that prototype,
Clara OCR evolved. Since then, many new ideas from various
persons helped to make it better.


Design notes
------------

It's not a bad idea to enumerate some principles that have driven
Clara OCR development. They'll make easier to understand the
features and limitations of the software (these principles may
change along time).

1. Clara is an OCR for printed texts, not for handwritten
texts.

2. Clara was not designed to be used to OCR one or two single
pages, but to OCR a large number of documents with the same
graphic characteristics (font, size, etc). So it can take
advantage of a fine (and perhaps expensive) training. This will
be tipically the case when OCRing an entire book.

3. We chose not support directly multiple graphic formats, but
only Jeff Poskanzer's raw PBM and PGM. Non-PBM/PGM files will be
read through filters.

4. Clara OCR wants to be a tool that makes viable the sum and
reuse of human revision effort. Because of this, on the OCR model
implemented by Clara, training and revision are one same
thing. The revision is a sum of punctual and independent acts and
alternates with reprocessing steps along a refinement process.

5. The Clara GUI was implemented and behaves like a minimalistic
HTML viewer. This is just an easy and standard way to implement a
forms interface.

6. We have tried to make the source code portable across
platforms that support the C library and the Xlib. Clara has no
special provision to be ported to environments that do not
support the Xlib. We avoided to use a higher level graphic
environment like Motif, GTK or Qt, but we do not discourage
initiatives to add code to Clara OCR adapt or adapt better to
these or other graphic environments.

7. We generally try to make the code efficient in terms of RAM
usage. CPU and disk usage (for session files) are less prioritary.


Clara vs the others
-------------------

Clara differs from other OCR softwares in various aspects:

1. Most known OCRs are non-free and Clara is free. Clara focus
the X Windows System. Clara offers batch processing, a web
interface and supports cooperative revision effort.

2. Most OCR softwares focus omnifont technology disregarding
training. Clara does not implement omnifont techniques and
concentrate on building specialized fonts (some day in the
future, however, maybe we'll try classification techniques that
do not require training).

3. Most OCR softwares make the revision of the recognized text a
process totally separated from the recognition. Clara
pragmatically joins the two processes, and makes training and
revision one same thing. In fact, the OCR model implemented by
Clara is an interactive effort where the usage of the heuristics
alternates with revision and visual fine-tuning of the OCR,
guided by the user experience and feeling.

4. Clara allows to enter the transliteration of each pattern
using an interface that displays a graphic cursor directly over
the image of the scanned page, and builds and maintains a mapping
between graphic symbols and their transliterations on the OCR
output. This is a potentially useful mechanism for documentation
systems, and a valuable tool for typists and reviewers. In fact,
Clara OCR may be seen as a productivity tool for typists, instead
of a typical OCR.

5. Most OCR softwares are integrated to scanning tools offerring
to the user an unified interface to execute all steps from
scanning to recognition. Clara does not offer one such integrated
interface, so you need a separate software (e.g. SANE) to
perform scanning.

6. Most OCR softwares expect the input to be a graphic file
encoded in tiff or other formats. Clara supports only raw
PBM/PGM.


*/

/* (book)

Scanning and thresholding
-------------------------

Clara OCR cannot scan paper documents by itself. Scanning must be
performed by another program. The Clara OCR development effort is
using SANE (http://www.mostang.com/sane) to produce 600 or 300
dpi images. The Clara OCR heuristics are tuned to 600 dpi.

Scanners offer three scanning modes: black-and-white (also known
as "bitmap" or "lineart", however the meaning of these words may
vary depending on the context), "grayscale" and "color". Clara
OCR requires black-and-white or grayscale input. Both
black-and-white and grayscale images may be saved in a variety of
formats by scanning programs. However, only PBM (for
black-and-white) and PGM (for grayscale) formats are
recognized. Generally grayscale 600 or 300 dpi will be the best
choice, but black-and-white 600 dpi may be good for new, high
quality printed materials. If your scanning program do not
support the PBM or PGM formats, try to save the images in TIFF
format and convert to PBM or PGM using the command tifftopnm. If
for some reason the TIFF format cannot be used, choose any other
format that preserves all data (don't use "compressing" formats
like JPEG), and for which a conversion tool is available, to
convert it to PBM or PGM.

Remark: Programs that scan or handle (e.g. rotate) images may
sometimes perform unexpected tasks, as applying dithering or
reducing algorithms by themselves. An image transformed to become
nice or small may be useless for OCR purposes.

Remark: The PBM and PGM formats do not carry the original resolution
(dots-per-inch) at which the image was scanned. As some
heuristics require that information, Clara OCR expects to be
informed about it through the command-line switch -y (so take
note of the resolution used).

Grayscale means that each pixel assumes one gray "level",
typically from 0 (black) to 255 (white). This is a good choice
for scanning old or low-quality printed materials, because it's
possible to use specialized programs to analyse the image and
choose a "threshold", in such a way that all pixels above that
threshold will be considered "white", and all others will be
considered black (when scanning in black-and-white mode, the
threshold is chosen by the scanning program or by the user). The
threshold may be global (fixed for the entire page) or local
(vary along the page).

In most cases grayscale will achieve better results. However, as
grayscale images are much larger than black-and-white images, 300
dpi (instead of 600 dpi) may be mandatory when using grayscale
due to disk consumption requirements.

Remark: Try to limit yourself to the optical resolution oferred by
the scanner. Most old scanners are 300 dpi, but the scanning
software obtains higher resolutions through interpolation. Newer
scanners may be optical 600 dpi or 1200 dpi or more.

Remark: the page 143 of Manuel Bernardes Branco Dictionary that
we're using along these tests was scanned using the SANE
scanimage command:

    scanimage -d microtek2:/dev/sga --mode gray -x 150 -y 210
              --resolution 300 > 143.pgm

Thresholding is not the only method for converting grayscale
images to black-and-white (such conversion is also called
"binarization"), but it's the current method used by Clara OCR.
In practice, a too low threshold will brake many symbols on their
thin parts, and a too high threshold will link symbols together
(in the figure, an "a-i" link and a broken "u").

               XX                  
               XX                  
                                
     XXXXX    XXX      XXX   XXX   
    X     XX   XX       XX    XX   
          XX   XX       XX    XX   
     XXXXXXX   XX       XX    XX   
    X     XX   XX       XX    XX   
    X     XX   XX       XX    XX   
     XXXXX XXXXXXX       XX  XXXX  

It's a hard task to detect broken and linked symbols. The Clara OCR
heuristics that handle these cases are incipient, so thresholding must
must be carefully performed, in order to not compromise the OCR
results. If the printing intensity, the noise level or the paper
quality vary from page to page, thresholding must be performed on a
per-page basis.

Remark: Now you can try avoid links in segmentation step. 
Just set "Try avoid links" parameter in Tune tab. (Normal values <=1)

The four thresholding methods currently avaliable are: manual
(global), histogram-based (global), classification-based (local),
classification-based (global).

Manual and histogram-based (global)
-----------------------------------

Histogram-based thresholding is the default method. It computes
automatically a thresholding value based on the distribution of
grayshades. To use it, just enter the TUNE tab and select (it's
selected by default) the "use histogram-based global
thresholder". To make a try, load a PGM image and press OCR or
ask the Segmentation OCR step.

Remark: You can correct the automatic-detected threshold with 
"Threshold factor" in Tune tab.

A global thresholding value can be manually specified. This
corresponds to the "use manual global thresholder" entry. The
choice of the thresholding value is performed through a visul
interface called "instant thresholding". To use it, load one PGM
image and select the "Instant thresholding" entry (Edit
menu). Then use '<', '>', '+' and '-' to change the thresholding
value. When ok, press ESC. Note that the selected value will be
applied only when the segmentation step runs.


Classification-based (local)
----------------------------

Global thresholding does not address those cases where the
printing intensity (or paper properties) vary along one same
page. Local thresholding methods are required on such
cases. Clara OCR implements a classification-based local
(per-symbol) thresholder. Saying that it's classification-based
means that the OCR engine is used to choose the threshold. In
other words, the threshold chosen is that for which the
classifier successfully recognized the symbol (in fact, this is a
brute-force approach).

The local binarizer can be manually applied at any symbol. To do
so, load one PGM page and click any symbol directly on the PAGE
tab. Two thresholding values will be chosen. The pixels found to
be "black" for each one are painted "black" (smaller value) and
"gray" (larger value). At this moment, it's possible to add the
thresholded symbol as a pattern (just press the key corresponding
to its transliteration). Remember that this thresholder relies on
the classifier, so if the OCR is not trained, you'll get no
benefit.

Two versions of the local binarizer were developed, a "weak" one
and a "strong" one. The "weak" one just tries to change the
threshold on those symbols not successfully classified using the
default threshold. The "strong" one (unfinished) also tries to
criticize locally the segmentation results. By default, the weak
version is used. To try the strong one, check the corresponding
checkbox at the TUNE tab.

Remark: As an alternative, use the "Balance" feature + global thresholding.


Classification-based (global)
-----------------------------

Clara OCR includes a simple threshold selection script to compute
global best thresholds based on classification results. Let's try
it on our 2-page book. Just create a directory, cd to it and run
the selthresh script informing the resolution and the names of
the images:

    $ cd /home/clara/books/BC
    $ mkdir pbm
    $ cd pbm
    $ selthresh -y 300 -l 0.45 0.55 ../pgm/*pgm
    selthresh: scaling 2 times
    Best thresholds:
    143-l.pgm 0.49
    143-r.pgm 0.51

In this case, selthresh will require around 4 minutes to
complete on a 500MHz CPU. For larger collections of pages,
selthresh may take much longer to complete (hours or days). If
needed, the execution can be safely interrupted using Control-C
(it's ok to shutdown the machine while selthresh is
running). The execution can be safely restarted from the point
where it was interrupted typing again the same command:

    $ cd /home/clara/books/MBB/pbm
    $ selthresh -y 300 -l 0.40 0.55 ../pgm/*pgm

The option -l is used to inform an interval of thresholds to
try. By now, selthresh is unable to choose by itself a "good"
interval. The user must manually check the results for some
thresholds in order to make a choice. For instance, to examine
the results for threshold 0.4 on page 143-l.pgm, try:

    $ pgmtopbm -threshold -value 0.4 ../pgm/143-l.pgm >143-l.pbm
    $ display 143-l.pbm

Change the threshold, repeat and, once found a threshold value
that produces a "nice" visual result, specify to -l the interval
centered at that threshold, and total width 0.1 or 0.2. The same
interval may be used for all pages because selthresh will warn
about a bad interval choice. Example:

    $ selthresh -y 300 -l 0.30 0.35 ../pgm/143-l.pgm
    selthresh: scaling 2 times
    Best thresholds:
    143-l.pgm 0.32 (bad interval, try -l 0.30 0.4)

If a "bad interval" warning appears on the final output for some
pages, it's ok to restart selthresh informing a new, wider
interval, as suggested by selthresh. Only the suspicious pages
will be re-examined. In fact, selecting a narrow initial interval
(and making it larger as required) may be a good strategy to
reduce the total running time.

Once the best thresholds are known, use pgmtopbm to produce the
black-and-white images. It's also a good idea to approach the
resolution to 600 dpi using pnmenlarge. Yet pnmenlarge does not
add information to the image, the classification heuristics will
behave better. In our case, the command should be

    $ cd /home/clara/books/BC/pbm
    $ pnmenlarge 2 ../pgm/143-l.pgm | \
          pgmtopbm -threshold -value 0.49 >143-l.pbm
    $ pnmenlarge 2 ../pgm/143-r.pgm | \
          pgmtopbm -threshold -value 0.51 >143-r.pbm

Remark: it's not a bad idea to visualize the PBM files, or at least
some of them. Yet selthresh produced good results for us, your
mileage may vary.

In order to capture the output of selthresh (to extract the
per-page best thresholds), it's ok to re-generate it as many
times as needed (just repeat the same selthresh command,
because once all computations become performed, the script will
just read the results from selthresh.out and output the results).

A final warning: selthresh may be fooled by too dark images. So
if the right limit is much larger than it should be, selthresh
may produce bad results. So be careful concerning the right limit
of the interval. As a practical advice, keep in mind that the
best threshold for most images is less then 0.6. In the near
future we'll use statistical measurements to choose the interval
to analyse, in order to prevent such problems and to make
unnecessary a manual choice.

remark: the tarball also includes an alternative selthresh, named
slethresh_fidian.pl. It contains instructions on how to use it.


Avoiding or correcting skew
---------------------------

Sometimes the printing is skewed relatively to the paper
margins. Skew is a problem to the OCR heuristics. As the Clara
OCR engine just detects components by pixel contiguity and builds
classes of symbols, in practice the effect of skew will be a
larger number of patterns, and therefore a larger revision cost.

In some cases, a careful manual scanning can solve the
problem. When acceptable, a set-square solves the problem: just
align one text line at one set-square rule and the edge of the
scanner glass at the other rule (we're supposing that the
bookbinding was disassembled).

The bundled preprocessor now includes a method to compute and
correct skew, but it's not on by default. To activate it, enter
the TUNE tab and select the "Use deskewer" checkbox. Now
deskewing will be applied when the OCR button is pressed (or when
the "Preprocessing" OCR step is requested). Note that
preprocessing is called only once per page, so if the page was
already preprocessed, it won't be deskewed.


Skeleton tuning
---------------

Currently, symbol classification can be performed by three
different classifiers: skeleton fitting, border mapping or pixel
distance. The choice is done on the TUNE tab. Border mapping is
currently experimental. Pixel distance has been used as an
auxiliar classifier. Skeleton fitting is a more mature code and
is highly customizable. It's the default classification method by
now.

When using skeleton fitting, two symbols are considered similar
when each one contains the skeleton of the other. So the
classification result depends strongly on how skeletons are
computed. As an example, the figure presents one symbol
("e"). The symbol black pixels are the dots ('.'). The skeleton
black pixels are stars ('*').

        .......
       ..******..
      .*.    ..*..
     ..*.    ...*.
     .*..    ...*..
    ..*.........*..
    ..***********..
    ..*.      ....
    ..*.
    ..*..
    ..*...     ...
     ..*..........
      ..********..
       .........


Clara OCR offers seven different methods for computing
skeletons. Each method has tunable parameters. The choice of the
method and the parameters can be done through a visual inteface
on the TUNE (SKEL) tab. To try it, first save the session (menu
"File"), then enter that tab. At least one pattern must
exist. Vary the parameters and observe the results. Press the
left and right arrows to navigate through the patterns, and use
the "zoom" button to choose a comfortable image size. The last
selection will be used for all skeleton computations. To discard
it, exit Clara OCR without saving the session.

Instead of trying the TUNE (SKEL) tab, it's possible to specify
skeleton computation parameters through the -k command-line
switch. Note however that if a selection was performed through
the TUNE (SKEL) tab, that selection will override the parameters
informed to -k, so be careful.

Clara OCR has an auto-tune feature to choose the "best" skeleton
computation parameters. To use it, check the "Auto-tune skeleton
parameters" entry on the TUNE tab. This feature is currently left
off by default because manual tuning can achieve better
results. Examples:

1. Quality printing without thin details

    use -k 2,1.4,1.57,10,3.8,10,4,4
     or -k 0,1.4,1.57,10,3.8,10,4,4

2. Quality printing with thin details

    use -k 2,1.4,1.57,10,3.8,10,1,1
     or -k 4,,,,,,3,

3. Poor printing without thin details

    use -k 2,1.4,1.57,10,3.8,10,1,1

4. Poor printing with thin details

    use -k 2,1.4,1.57,10,3.8,10,1,1

Yet the pattern computation parameters may change along the way,
it's wise to choose adequate skeleton computation parameters
before OCRing, and keep them fixed along the project. Every time
Clara OCR is started, inform the same parameters chosen. In our
case, we can use the default parameters. To do so, just enter
Clara OCR as before:

    $ cd /home/clara/books/BC/pbm
    $ clara &


Classification tentatives
-------------------------

To classify the book symbols (i.e. to discover the
transliteration of unknown symbols using the patterns), enter
Clara OCR, select "Work on all pages" ("Options" menu) and press
the OCR button using the mouse button 1, or press the mouse
button 3 and select "Classification". The classification may be
performed many times. Each time, different parameters may be
tried to refine the results already achieved.

When the classification finishes, observe the pages 5.pbm and
6.pbm. Much probably, some symbols will be greyed. In other
words, the classifier was unable to classify all symbols. The
statistics presented on the PAGE (LIST) tab may be useful now. To
reduce the number of unknown symbols there are three choices: add
more patterns, change the skeleton computation parameters, or try
another classifier.

To add more patterns, just train some greyed symbols and
reclassify all pages again. The reclassification will be faster
than the first classification because most symbols, already
classified, won't be touched.

To change the skeleton computation parameters, exit Clara OCR,
restart it informing the new parameters through -k, select
"Re-scan all patterns" ("Edit" menu), select "Work on all pages"
("Options" menu) and reclassify. May be easier to choose and set
the new parameters using the TUNE (SKEL) tab, as explained
earlier. However, remember that the parameters chosen through the
TUNE (SKEL) tab override the parameters informed through -k.

To try another classifier, first select the "Re-scan all
patterns" entry on the "Edit" menu. Then enter the TUNE tab and
select the classifier to use from the available choices
(skeleton-base, border mapping and pixel distance). The pixel
distance may be a good choice. Then reclassify all pages.

The "Re-scan all patterns" is required because for each symbol
Clara OCR remembers the patterns already tried to classify it,
and do not try those patterns again. However, when the skeleton
computation parameters change, or when the classifier changes,
those same patterns must be tried again. Maybe in the future
Clara OCR will decide by itself about re-scanning all patterns.


Symbol properties
-----------------

The bottom five buttons (alphabet, pattern type, "bold", "italic"
and "bad") carry the properties of the current symbol. If the
"PAGE" window is on the plate, the current symbol is the one
identified by the graphic cursor. If the window "PATTERN" is on
the plate, the current symbol is the pattern being exhibited. In
all other cases, the current symbol is undefined.

Let's comment in detail the symbol properties carried by those
five buttons:

a. The possible values for the alphabet are: latin, greek,
cyrillic, hebrew, arabic, kana, number, ideogram or "other". In
order to limit the available alphabets, the button circulates
only the values selected on the "Alphabet" menu.

b. The "pattern types" are the fonts and font sizes used by the
book. Example: 12pt roman and 12pt arial for the text, and 8pt
roman for the footnotes. In this case we have three "types"
identified as "1", "2" and "3".

c. Each one of the bold, italic and "bad" flags may be on or
off. The "bad" flag identifies a symbol not to be used as
pattern.

The user can inform Clara OCR about any of these properties for
the current symbol, just selecting the desired value on the
corresponding button (click it one or more times). The pattern
type, however, is read-only by default. To allow changing its
value, use the "pattern types are read-only" entry on the
"Options" menu.

In most cases, Clara OCR will compute automatically the
properties of each symbol, so it's not required to set them
manually. But just like the transliterations, Clara OCR will need
some initial information, so the user must identify some symbols
as being bold or italicized.


Merge tuning
------------

    merge internal fragments
    merge pieces on one same box
    merge close fragments
    recognition merging
    learned merging


Complex procedures
------------------

To OCR an entire book is a long process. Perhaps along it a
problem is detected. Bad choice of skeleton computation
parameters, or a bad page contaminating the bookfont, some files
loss due to a crash, etc. How to solve them?

Clara OCR does not offer currently a complete set of tools to
solve all these problems. In some cases, a simple solution is
available. In others, a solution is expected to become available
in future versions. This session will depict some practical
cases, and explain what can be done and what cannot be done for
each one.

Fixing transliterations
-----------------------

  Fixing pattern transliterations
  Fixing symbol transliterations

Removing patterns and synchronizing pages
-----------------------------------------

  Removing references to that pattern
      on the loaded page
      on other pages
      on the patter types

Removing a page
---------------

From the stats presented by the PAGE (LIST) tab it's possible to
detect problems on specific pages. A low factorization may be a
simptom of a bad choice of brightness for that page. In such a
case, it's probably a good idea to remove completely that page.

To remove a page is a delicate operation. Clara OCR currently
does not offer a "remove page" feature. Basically, it should
remove all patterns from that page, remove the revision data
acquired from that page, and remove the page image and its
session file.


Dealing with classification errors
----------------------------------

What to do when the OCR classifies incorrectly a large quantity of
symbols? (to be written)


Importing revision data
-----------------------

When OCRing a large book, a good approach is to divide its pages
into a number of smaller sections and OCR each one. So for a book
with, say, 1000 pages, we could OCR pages 1-200, then 201-400,
etc.

After finishing the first section, of course we desire reuse on
the second section the training and revision effort already
spent. This is not the same as adding the pages 201-400 to the
first section, because we do not want handle the pages 1-200
anymore.

Basically we need to import the patterns of the first section
when starting to process the second. Well, Clara OCR is currently
unable to make this operation.


How to use the web interface
----------------------------

The Clara OCR web interface allows remote training of symbols. To use
it, a web server able to run perl CGIs (e.g. Apache) is
required. Let's present the steps to activate the web interface for a
simple case, with only one book (named "book1"). Basically, one needs
to create a subtree anywhere on the server disk (say,
"/home/clara/www/"), owned by the user that will manage the project
(say, "clara"), with subdirectories, "bin", "book1" and
"book1/doubts":

    $ id
    uid=511(clara) gid=511(clara) groups=511(clara)
    $ cd /home/clara/
    $ mkdir www
    $ cd www
    $ mkdir bin book1
    $ mkdir book1/doubts

Then copy to the directory "bin" the files clara.pl and sclara.c from
the Clara OCR distribution (say, /usr/local/src/clara), edit clara.c
to change the hardcoded definition of the root directory to
"/home/clara/www", compile it and make it setuid:

    $ cd bin
    $ cp /usr/local/src/clara/clara.pl .
    $ cp /usr/local/src/clara/sclara.c .
    $ emacs sclara.c
    $ grep '^char *root' sclara.c
    char *root = "/home/clara/www";
    $ cc -o sclara -static sclara.c
    $ rm sclara.c
    $ chmod a+s sclara

Edit the script clara.pl. Example for the clara.pl configuration
section (the script clara.pl contains default definitions for some of
these variables, please comment out those definitions):

    $CROOT = "/home/clara/www";
    $U = "/cgi-bin/clara";
    $book[0] = 'Author, <I>Test 1</I>, City, year';
    $subdir[0] = "book1";
    $LANG = 'en';
    $opt = '-W -R 10 -b -k 2,1.4,1.57,10,3.8,10,4,1';

Now copy the PBM files to the directory "book1", create low-quality
jpeg previews, gzip the PBM files, and select some patterns:

    $ cd /home/clara/www/book1
    $ cp /usr/local/src/clara/imre.pbm .
    $ pbmreduce 8 imre.pbm | convert -quality 25 - imre.jpg
    $ gzip -9 imre.pbm
    $ clara -k 2,1.4,1.57,10,3.8,10,4,1

(load one PBM file, train some symbols, save the session and quit the
program).

Now we need to process the PBM files in order to create some
"doubts". The script clara.pl also requires a symlink to the clara
binary (change the path /usr/local/bin/clara as required):

    $ cd /home/clara/www/bin
    $ ln -s /usr/local/bin/clara clara
    $ ./clara.pl -s book1
    $ rm ../book1/*html
    $ ./clara.pl -p

Now your server must be instructed to exec /home/clara/www/bin/clara.pl
when a visitor requests "/cgi-bin/clara" (if you prefer another URL,
change the clara.pl customization too). An easy way to accomplish that
is creating a symlink on the default directory for CGIs. The default
directory of CGIs is platform-dependent (e.g. /home/httpd/cgi-bin,
/usr/local/httpd/cgi-bin, /var/lib/apache/cgi-bin, etc). Example:

    # cd /home/httpd/cgi-bin
    # ln -sf /home/clara/www/bin/clara.pl clara

Try to access the URL "/cgi-bin/clara" on your web server. The correct
behaviour is successfully loading a page entitled "Prototype of the
Cooperative Revision". If you have problems, be aware about some
common problems:

1. Apache expects to be explicitly allowed to follow symlinks. The
file access.conf should contain, in our case, a section similar to the
following:

    <Directory /home/httpd/cgi-bin>
    AllowOverride None
    Options ExecCGI FollowSymLinks
    </Directory>

2. The directory /home/clara must be world readable:

    # ls -ld /home/clara
    drwxr-xr-x  4 clara clara  1024 Sep 17 09:56 /home/clara

If you succeeded, congratulations! Note that from time to time it'll
be necessary to reprocess the pages, adding to the session files the
data collected from the web, just like done before:

    $ cd /home/clara/www/bin
    $ ./clara.pl -p
    $ ./clara.pl -s book1


Revision acts maintenance
-------------------------

Types of revision acts (to be written).

Discarding deduced data (to be written).



*/

/* (devel)

Bugs and TODO list
------------------

(Some) Major tasks

1. Vertical segmentation (partially done).

2. Heuristics to merge fragments.

3. Spelling-generated transliterations

4. Geometric detection of lines and words

5. Finish the documentation

6. Simplify the revision acts subsystem


Minor tasks

1. Change sprintf to snprintf.

2. Fix assymetric behaviour of the function "joined".

3. Optimize bitmap copies to copy words, not bits, where possible
(partially done).

4. Support Multiple OCR zones (partially done).

5. Make sure that the access to the data structures is blocked
during OCR (all functions that change the data structures must
check the value of the flag "ocring").

6. Use 64-bit integers for bitmap comparisons and support
big-endian CPUs (partially done).

7. Clear memory buffers before freeing.

8. Allow the transliterations to refer multiple acts (partially
done).

9. Rewrite composition of patterns for classification of linked
symbols.

10. The flea stops but do not disappear when the window lost and
regain focus.

11. Substitute various magic numbers by per-density and
per-minimum-fontsize values.

12. Synchronization destroys the result of partial matching
because partial matching assigns to the symbol only one
pattern as its best match.

*/

/* (book)

Welcome to Clara OCR
--------------------

Clara is an optical character recognition (OCR) software, a
program that tries to identify the graphic images of the
characters from a scanned document, converting their digital
images to ASC, ISO or other codes.

The name Clara stands for "Cooperative Lightweight chAracter
Recognizer".

Clara offers two revision interfaces: a standalone GUI and and a
web interface, able to be used by various different reviewers
simultaneously. Because of this feature Clara is a "cooperative"
OCR (it's also "cooperative" in the sense of its free/open status
and development model).



*/

/* (book)


The requirements
----------------

Clara OCR will run on a PC (386, 486 or Pentium) with GNU/Linux
and Xwindows. Clara OCR will hopefully compile and run on a PC
with any unix-like operating system and Xwindows. Currently Clara
OCR won't run on big-endian CPUs (e.g. Sparc) nor on systems
lacking X windows support (e.g. MS-Windows). Higher-level
libraries like Motif, GTK or Qt are not required.

A relatively fast CPU is recommended (300MHz or more). Memory
usage depends on the documents, and may range from some few
megabytes to various tenths os megabytes The normal operation
will create session files on your hard disk, so some megabytes of
free disk space are required (a large project may require plents
of gigabytes). Clara OCR can read and write gzipped files (see
the -z command-line switch).

If you need to build the executable and/or the documentation,
then an ANSI C compiler (with some GNU extensions) and a (version
5) perl interpreter are required.


How to download and compile Clara
---------------------------------

For those who need to download and compile the source code
(hopefully this will be unnecessary for most users as soon as
Clara binary distributions become available), it may be
downloaded from CLARA_HOME. It's a
compressed tar archive with a name like clara-x.y.tar.gz (x.y is
the version number).

The compilation will generally require no more than issue the
following commands on the shell prompt:

    $ gunzip clara-x.y.tar.gz
    $ tar xvf clara-x.y.tar
    $ cd clara-x.y
    $ make
    $ make doc

Now you can copy the executable (the file "clara") to some
directory of binaries (like /usr/local/bin), and the man page
(file "clara.1") to some directory of man pages (like
/usr/local/man/man1). By now there is no "make install" to
perform these copies automatically.

If some of these steps fail, please try to obtain assistance from
your local experts. They will solve most simple problems
concerning wrong paths or compiler options. You can also read the
subsection "Compilation and startup pitfalls".


Compilation and startup pitfalls
--------------------------------

This subsection is intended to help people that are experiencing
fatal errors when building the executable or when starting
it. After each error message we'll point out some hints.

Bear in mind that most hints given below are very elementary
concerning Unix-like systems. If you have problems, try to read
all hints because details explained once are not repeated. If you
cannot understand them, please try to ask your local experts, or
try to read an introductory book on Unix things. Please don't
email questions like these to the Clara developers, except when
the hint suggests it.

1. Path-related pitfalls

    $ make
    bash: make: command not found

The shell could not find the "make" utility. Maybe there is no
such utility installed on your system, or maybe the path to it is
unknown to the shell. You can try to find the "make" utility with
a command like

    $ find /usr -name make -print

The following command will display the current path:

    $ echo $PATH

Remember that on Unix-like systems the environment is
per-process. So if you change the PATH variable on the shell
prompt within an xterm, this won't affect the other running
shells (on the other xterms). Remember that the Unix shells
expect to be explicitly informed about which variables must be
exported to subprocesses (use "export" in Bourne-like shells and
"setenv" on C-like shells).

    $ make
    gcc -I/usr/X11R6/include -g   -c gui.c -o gui.o
    make: gcc: Command not found
    make: *** [gui.o] Error 127

The make utility could not find the gcc compiler. Check if gcc is
installed. If not, check if some other C compiler is installed
(for instance, "cc"), and edit the makefile to chage the value of
the CC variable.

If you don't know what I'm speaking about, take a look on the
directory where the Clara source codes are, and you'll see there
a file named "makefile". This file contains the names of the
tools to be used and rules to build the Clara executable. It
contains also important paths, like those where the system
headers (files .h) and libraries can be found. If the names or
the paths don't reflect those on your system, you need to edit
the makefile accordingly.

    $ make
    gcc -I/usr/X11R6/include -g   -c gui.c -o gui.o
    In file included from gui.c:16:
    gui.h:12: X11/Xlib.h: No such file or directory
    make: *** [gui.o] Error 1

The compiler could not find the header Xlib.h. Maybe your system
does not include such header, or maybe it is on another directory
not explicited on the makefile through the INCLUDE variable.

    $ make
    gcc -o clara clara.o skel.o gui.o mc.o ...
    /usr/bin/ld: cannot open -lX11: No such file or directory
    make: *** [clara] Error 1

The linker could not find the X11 library. Maybe your system does
not include such library, or maybe it is on another directory not
explicited on the makefile through the LIBPATH variable.

2. Compilation pitfalls

    $ make
    gcc -I/usr/X11R6/include -g   -c clara.c -o clara.o
    clara.c:70: parse error before `int'
    make: *** [clara.o] Error 1

A syntax error on the line 70 of the file clara.c. Double check
if the sources were not changed. Try to obtain the sources
again. If you're a programmer, try to fix the problem. In any
case, report it to claraocr@claraocr.org.

    $ make
    clara.c: In function `process_cl':
    clara.c:2293: `ZPS' undeclared (first use in this function)
    clara.c:2293: (Each undeclared identifier is reported only once
    clara.c:2293: for each function it appears in.)
    make: *** [clara.o] Error 1

A reference to an undeclared variable. Double check if the
sources were not changed. Try to obtain the sources again. If
you're a programmer, try to fix the problem. In any case, report
it to claraocr@claraocr.org.


3. Runtime pitfalls

    $ clara &
    [1] 1924
    bash: clara: command not found

The Clara executable does not exist or is not on the path. Most
Unix systems don't include the current directory ("./") on the
path, so if you're trying to start Clara from the directory where
it was compiled, specify the current directory ("./clara").

    $ ./clara &
    [1] 1922
    _X11TransSocketUNIXConnect: Can't connect: errno = 111
    cannot connect to X server

Clara could not connect the X server. The X Windows System is a
client-server system. The applications (xterm, xclock, etc)
connect to a display server (the X server). If the server is not
running, clients cannot connect to it. In some cases, it's
required to inform explicitly the client about the server it must
connect, using the environment variable DISPLAY.

    $ ./clara
    Segmentation fault (core dumped)

If you can reproduce the problem, report it
to claraocr@claraocr.org. If you're a programmer and Clara was
compiled with the -g option, try a debugger to locate the point
of the source code where the segmentation fault happened. Using
gdb, it's quite easy:

    $ gdb clara
    (gdb) run

Now try to reproduce the steps that led to the segmentation
fault.


*/

/* (tutorial)

Making OCR
----------

This section is a tutorial on the basic OCR features offerred by
Clara OCR. Clara OCR is not simple to use. A basic knowledge
about how it works is required for using it. Most complex
features are not covered by this tutorial. If you need to compile
Clara from the source code, read the INSTALL file and check (if
necessary) the compilation hints on the Clara OCR Advanced User's
Manual.


Starting Clara
--------------

So let's try it. Of course we need a scanned page to do so. Clara
OCR requires graphic format PBM or PGM (TIFF and others
must be converted, the netpbm package contains various conversion
tools). The Clara distribution package contains one small PBM
file that you can use for a first test. The name of this file is
imre.pbm. If you cannot locate it, download it or other files
from CLARA_HOME. Alternatively, you can produce your own 600-dpi
PBM or PGM files scanning any printed document (hints for
scanning pages and converting them to PBM are given on the
section "Scanning books" of the Clara OCR Advanced User's
Manual).

Once you have a PBM or PGM file to try, cd to the directory where
the file resides and fire up Clara. Example:

    $ cd /tmp/clara
    $ clara &

In order to make OCR tests, Clara will need to write files on
that directory, so write permission is required, just like some
free space.

Remark: As to version CLARA_VERSION, Clara OCR heuristics are tuned
to handle 600 dpi bitmaps. When using a different resolution,
inform it using the -y switch:

    $ clara -y 300 &

Then a window with menus and buttons will appear on your X
display:


    +-----------------------------------------------+
    | File Edit OCR ...                             |
    +-----------------------------------------------+
    | +--------+     +----+ +--------+ +-------+    |
    | |  zoom  |     |page| |patterns| | tune  |    |
    | +--------+   +-+    +-+        +-+       +-+  |
    | +--------+   | +-------------------------+ |  |
    | |  zone  |   | |                         | |  |
    | +--------+   | |                         | |  |
    | +--------+   | |                         | |  |
    | |  OCR   |   | |        WELCOME TO       | |  |
    | +--------+   | |                         | |  |
    | +--------+   | |    C L A R A    O C R   | |  |
    | |  stop  |   | |                         | |  |
    | +--------+   | |                         | |  |
    |      .       | |                         | |  |
    |      .       | |                         | |  |
    |              | |                         | |  |
    |              | |                         | |  |
    |              | +-------------------------+ |  |
    |              +-----------------------------+  |
    |                                               |
    | (status line)                                 |
    +-----------------------------------------------+


Welcome aboard! The rectangle with the welcome message is called
"the plate". As you already guessed, the small rectangles with
the labels "zoom", "OCR", "stop", etc, are "the buttons". The
"tabs" are those flaps labelled "page", "patterns"
and "tune". On the menu bar you'll find the File menu, the Edit
menu, and so on. Popup the "Options" menu, and change the current
font size for better visualization, if required.

Press "L" to read the GPL, or select the "page" tab, and
subsequently, select on the plate the imre.pbm page (or any other
PBM or PGM file, if any). The OCR will load that file showing the
progress of this operation on the status line on the bottom of
the window.

note: the "page" tab is the flap labelled "page". This is
unrelated to the "tab" key.

When the load operation completes, Clara will display the
page. Press the OCR button and wait a bit. The letters will
become grayed and the plate will split into three windows. Move
the pointer along the plate and you'll see the tab label follow
the current window: "page", "page (output)" or "page
(symbol)". Move the pointer along the entire application window,
and, for most components, you'll see a short context help message
on the status line when the pointer reaches it (the buttons, for
instance). Dialogs (user confirmations) also use the status line
(like Emacs), instead of dialog boxes.

You can resize both the Clara application window or each of the
three windows currently on the plate ("page", "page (output)" and
"page (symbol)"). To resize the windows, select any point between
two of them and drag the mouse. The scrollbars can become hidden
(use the "hide scrollbars" on the View menu).

When the tab label is "page", press the "zoom" button using the
mouse button 1 and the scanned image will zoom out. If you use
the mouse button 3, the image will zomm in (the behaviour of the
"zoom" button depends on the current window).

Now try selecting the "page" tab many times, and you will
circulate the various display modes shared by this tab. These
modes are and will be referred as "PAGE", "PAGE (fatbits)" and
"PAGE (list)". Each display mode may have one or more windows
We've chosen this uncommon approach because an excess of tabs
transforms them in a useless decoration. The other tabs also
offer various modes, some will be presented later by this
tutorial.


Some few command-line switches
------------------------------

Besides the -y option used in the last subsection, Clara accepts
many others, documented on the Clara OCR Advanced User's
Manual. By now, from the various different ways to start Clara,
we'll limit ourselves to some few examples:

  clara
  clara -h

In the first case, Clara is just started. On the second, it will
display a short help and exit.

  clara -f path
  clara -f path -w workdir

The option -f informs the relative or absolute path of a scanned
page or a directory with scanned pages (PBM or PGM files). The
option -w informs the relative or absolute path of a work
directory (where Clara will create the output and data files).

  clara -i -f path -w workdir
  clara -b -f path -w workdir

The option -i activates dead keys emulation for composition of
accents and characters. The -b switch is for batch
processing. Clara will automatically perform one OCR run on the
file informed through -f (or on all files found, if it is the
path of a directory) and exit without displaying its window.

  clara -Z 1 -F 7x13

Clara will start with the smallest possible window size.

A full reference of command-line switches is given on the section
"Reference of command-line switches" of the Clara OCR Advanced
User's Manual.


Training symbols
----------------

Yes, Clara OCR must be trained. Training is a tedious procedure,
but it's a must for those who need a customizable OCR, apt to
adapt to a perhaps uncommon printing style.

Before training, a process called segmentation must be
performed. Press the right button of the mouse over the OCR
button, select "Segmentation" on the menu that will pop out and
wait the operation complete.

Now, on the "page" tab, observe the image of the document
presented on the top window. You'll see the symbols greyed,
because the OCR currently does not know their
transliterations. Try to select one symbol using the mouse (click
the mouse button 1 over it). A black elliptic cursor will appear
around that symbol. This cursor is called the "graphic
cursor". You can move the graphic cursor around the document
using the arrow keys.

Now observe the bottom window on the "page" tab. That window
presents some detailed information on the current symbol (that
one identified by the graphic cursor). When the "show web clip"
option on the "View" menu is selected, a clip of the document
around the current symbol, is displayed too. In some cases, this
clip is useful for better visualization. The name "web clip" is
because this same image is exported to the Clara OCR web
interface when cooperative training and revision through the
Internet is being performed.

To inform the OCR about the transliteration of one symbol, just
type the corresponding key. For instance, if the current symbol
is a letter "a", just type the "a" key. Observe that the trained
symbol becomes black. Each symbol trained will be learned by the
OCR, its bitmap will be called a "pattern", and it will be used
as such when trying to deduce the transliteration of unknown
symbols.

Remark: in our test, the user chose the symbol to be trained. However,
Clara OCR can choose by itself the symbols to be trained. This feature
is called "build the bookfont automatically" (found on the "tune"
tab). To use it, select the corresponding checkbos and classify the
symbols as explained later.

Finally, when the transliteration cannot be informed through one
single keystroke or composition (for instance when you wish to
inform a TeX macro as being the transliteration of the current
symbol), write down the transliteration using the text input
field on the bottom window (select it using the mouse before).


Symbol properties
-----------------

Obs: most features described in this paragraph are still
experimental.

The bottommost three buttons (in this order: alphabet, pattern
type, and "bad") show properties of the current symbol.

If a symbol is defective, it's generally useful not use it as a
pattern. In such a case, when informing the symbol
transliteration, press the ESC key once before training that
symbol (or press the BAD button). The OCR will mark that symbol
as "bad".

The behaviour of the "alphabet" button is as follows: by default
it is in the state "other". If the current symbol is trained as a
latin letter ('a', 'b', 'c', etc), this property is automatically
set to "latin". If the current symbol is trained as a decimal
digit ('0', '1', etc), this property is automatically set to
"number". If the button state is manually set to "greek" and a
letter is input from a latin keyboard, it will be automatically
mapped to the corresponding greek letter ("a" to "alpha", "b" to
"beta", etc). Note that the alphabet button circulates only those
alphabets selected on the "Alphabets" menu. By now, Clara OCR
does not include mappings for other alphabets.

The "pattern types" button presents the classification of the
symbol regarding the font types (Clarendom, Times, etc) and sizes
(9pt, 10pt, etc) found on the book. It's not mandatory to
classify the patterns, and there is some preliminar code to
perform this classification automatically. However, it's
currently expected to be performed manually, if desired. For
instance: first train some symbols, all of same type and
size. All just created patterns are put on type 0. Then use the
"set pattern type" on Edit menu to change their types from 0 to
some other at your choice.


Saving the session
------------------

Before going further, it's important to know how to save your
work. The file menu contains one item labelled "save
session". When selected, it will create or overwrite three files
on the working directory: "patterns", "acts" and "page.session",
where "page" is the name of the file currently loaded, without
the "pbm" or "pgm" tag (in out example, "imre"). So, to remove
all data produced by OCR sessions, remove manually the files
"*.session", "patterns" and "acts".

Note that the files "patterns" and "acts" are shared by all PBM
or PGM pages, so a symbol trained from one page is reused on the
other pages. The ".session" files however are per-page. Pages
with the same graphic characteristics, and only them, must be put
on one same directory, in order to share the same patterns.

When the "quit" option of the "File" menu is selected, the OCR
prompts the user for saving the session (answer pressing the key
"y" or "n"), unless there are no unsaved changes.



OCR steps
---------

The OCR process is divided into various steps, for instance
"classification", "build", etc. These steps are acessible clicking
the mouse button 3 over the OCR button. Each one can be started
independently and/or repeated at any moment. In fact, the more
you know about these steps, the better you'll use them.

Clicking the "OCR" button with the mouse button 1, all steps will
be started in sequence. The "OCR" button remains on the
"selected" state while some step is running.

Yet we won't cover this stuff in the tutorial, a basic knowledge
on what each step perform is required for fine-tuning Clara OCR.
The tuning is an interactive effort where the usage of the
heuristics alternates with training and revision, guided by the
user experience and feeling.


Classification
--------------

After training some symbols, we're ready to apply the just
acquired knowledge to deduce the transliteration of non-trained
symbols. For that, Clara OCR will compare the non-trained symbols
with those trained ("patterns"). Clara OCR offers nice visual
modes to present the comparison of each symbol with each
pattern. To activate the visual modes, enter the View menu and
select (for instance) the "show comparisons" option.

Now start the "classification" step (click the mouse button 3
over the OCR button and select the "classification" item) and
observe what happens. Depending on your hardware and on the size
of the document, this operation may take long to complete
(e.g. 5 minutes). Hopefully it'll be much faster (say, 30
seconds).

When the classification finishes, observe that some nontrained
symbols became black. Each such symbol was found similar to some
pattern. Select one black symbol, and Clara will draw a gray
ellipse around each class member (except the selected symbol,
identified by the black graphic cursor). You can switch off this
feature unselecting the "Show current class" item on the "View"
menu.

In some cases, Clara will classify incorrectly some symbols. For
instance, a defective "e" may be classified as "c". If that
happens, you can inform Clara about the correct transliteration
of that symbol training it as explained before (in this example,
select the symbol and press "e"). This action will remove that
symbol from its current class, and will define a new class,
currently unitary and containing just that symbol.


Note about how Clara OCR classification works
---------------------------------------------

The usual meaning of "classification" for OCRs is to deduce for
each symbol if it is a letter "a" or the letter "b", or a digit
"1", etc. As the total number of different symbols is small (some
tenths), there will be a small quantity of classes.

However, instead of classifying each symbol as being the letter
"a", or the digit "1", or whatever, Clara OCR builds classes of
symbols with similar shapes, not necessarily assigning a
transliteration for each symbol. So as sometimes the bitmap
comparison heuristics consider two true letters "a" dissimilar
(due to printing differences or defects), the Clara OCR
classifier will brake the set of all letters "a" in various
untransliterated subclasses.

Therefore, the classification result may be a much larger number
of classes (thousands or more), not only because of those small
differences or defects, but also because the classification
heuristics are currently unable to scale symbols or to "boldfy"
or "italicize" a symbol.

Note that each untransliterated subclass of letters "a" depends
on a punctual human revision effort to become transliterated
(trained). This is not an absurd strategy, because the revision
of each subset corresponds to part of the unavoidable human
revision effort required by any real-life digitalization
project. This is one of the principles that make possible to see
Clara OCR not as a traditional OCR, but as a productivity tool
able to reduce costs. Anyway, we expect to the future more
improvements on the Clara OCR classifier, in order to lessen the
number of subclasses created.


Building the output
-------------------

Now we're ready to build the OCR output. Just start the
"build" step. The action performed will be basically
to detect text words and lines, and output the transliterations,
trained or deduced, of all symbols. The output will be presented
on the "PAGE (output)" window.

Each character on the "PAGE (output)" window behaves like a
HTML hyperlink. Click it to select the current symbol both
on the "PAGE" window and on the "PAGE (symbol)" window. Note
that the transliteration of unknow symbols is substituted by
their internal IDs (for instance "[133]").

The result of the word detection heuristic can be visualized
checking the "show words" item on the "View" menu.


Handling broken symbols
-----------------------

Remark: As to version CLARA_VERSION the merging heristics are only
partially implemented, and in most cases they won't produce any effect.

The build heuristics also try to merge the pieces of broken
symbols, just like the "u", the "h" and the "E" on the figure
(observe the absent pixels). Some letters have thin parts, and
depending on the paper and printing quality, these parts will
brake more or less frequently.


                 XXX            XXXXXXXXXXX
                  XX             XXX      X
                  XX             XXX
                  XX             XXX
    XXX   XXX     XX   XXX       XXX     X
     XX    XX     XXX     X      XXX  XXXX
     XX    XX     XX      XX     XXX     X
     XX    XX     XX      XX     XXX
     XX    XX     XX      XX     XXX
     XX    XX     XX      XX     XXX      X
      XX  XXXX   XXXX     XXX   XXXXXXXXXXX


Clara OCR offers three symbol merging heuristics:
geometric-based, recognition-based and learned. Each one may be
activated or deactivated using the "tune" tab.

Geometric merging applies to fragments on the interior of the
symbol bounding box, like the "E" on the figure, and to some other
cases too.

The recognition merging searches unrecognized
symbols and, for each one, tries to merge it with some
neighbour(s), and checks if the result becomes similar to some
pattern.

Finally, learned merging will try to reproduce the
cases trained by the user. To train merging, just select the
symbol using the mouse button 1
(say, the left part of the "u" on the figure), click the mouse
button 3 on the fragment (the right part of the "u"), and select
the "merge with current symbol" entry. On the other hand, the
"disassemble" entry may be used to break a symbol into its
components.

Remark: do not merge the "i" dot with the "i" stem. See the
subsection "handling accents" for details.

Handling accents
----------------

Now let's talk about accents.

As a general rule, Clara OCR does not consider accents as parts
of letters, so merging does not apply to accents. Accents are
considered individual symbols, and must be trained
separately. The "i" dot is handled as an accent. Clara OCR will
compose accents with the corresponding letters when generating
the output. The exception is when the accent is graphically
joined to the letter:

           XXX
           XX          XXX
          XX           XX
                      XX
       XXXX         XXXX
     XX    XX     XX    XX
    XX      XX   XX      XX
    XXXXXXXXXX   XXXXXXXXXX
    XX           XX
    XX           XX
     XX    XX     XX    XX
       XXXX         XXXX


In the figure we have two samples of "e" letter with acute
accent. In the first one, the accent is graphically separated
from the letter. So the accent transliteration will be trained or
deduced as being "'", the letter transliteration
will be trained or deduced as beig "e". When generating the output,
Clara OCR will compose them as the macro "\'e" (or as the ISO
character 233, as soon as we provide this alternative behaviour).

On the second case the accent isn't graphically separable from
the letter, so we'll need to train the accented character as the
corresponding ISO character (code 233) or as the macro "\'e". As
the generation of accented characters depend on the local X
settings, the "Emulate deadkeys" item on the "Options" menu may
be useful in this case. It will enable the composition of accents
and letters performed directly by Clara OCR (like Emacs
iso-accents-mode feature).


Browsing the book font
----------------------

As explained earlier, trained symbols become patterns (unless you
mark it "bad"). The collection of all patterns is called "book
font" (the term "book" is to distinguish it from the GUI
font). Clara OCR stores all pattern in the "patterns" file on the
work directory, when the "save session" entry on the "File" menu
is selected.

Clara OCR itself can choose the patterns and populate the book
font. To do so, just select the "Build the font automatically"
item on the "tune" tab, and classify the symbols.

To browse the patterns, click the "pattern" tab one or more times
to enter the "Pattern (list)" window. The "PATTERN (list)" mode
displays the bitmap and the properties
of each pattern in a (perhaps very long) form.
Click the "zoom" button to
adjust the size of the pattern bitmaps. Use the scroolbar or
the Next (Page Down) or Previous (Page Up) keys to navigate. Use
the sort options on the "Edit" menu to change the presentation order.

Now press the "pattern" tab again to reach the "Pattern" window. It
presents the "current" pattern with detailed properties. try
activating the "show web clip" option on the "View" menu to
visualize the pattern context. The left and
right arrows will move to the previous and to the next patterns. To
train the current pattern (being exhibited on the "Pattern" window),
just press the key corresponding to its transliteration (Clara will
automatically move to the next pattern) or fill the
input field. There is no need to press ENTER to submit the input
field contents.


Useful hints
------------

If the GUI becomes trashed or blank, press C-l to redraw it.

By now, the GUI do not support cut-and-paste. To save to a file
the contents of the "PAGE (list)" window, use the "Write report"
item on the "File" menu.

The "OCR" button will enter "pressed" stated in some unexpected
situations, like during dialogs. This behaviour will be fixed
soon.

The "STOP" button do not stop immediately the OCR operation in
course (e.g. classification). Clara OCR only stops the operation
in course in "secure" points, where all data structures are
consistent.

The OCR output is automatically saved to the file page.html (or
page.txt if the option -o was used), where "page" is the name of
the currently loaded page, without the "pbm" or "pgm" tag. This
file is created by the "generate output" step on the menu that
appears when the mouse button 3 is pressed over the OCR button.

Some OCR steps are currently unfinished and perform no
action at all.


Fun codes
---------

Clara OCR "fun codes" are similar to videogame "codes" (for those
who have never heard about that, videogame "codes" are special
sequences of mouse or key clicks that make your player
invulnerable, or obtain maximum energy, or perform an unexpected
action, etc).

The difference is that Clara OCR "fun codes" are not secret
(videogame "codes" are normally secret and very hard to discover
by chance). Clara OCR contains no secret feature. Fun codes are
intended to be used along public presentations. By now there is
only one fun code: just click one or more times the banner on the
welcome window to make it scroll.


*/

/* (book)

Supported Alphabets
-------------------

Clara OCR focuses the Latin Alphabet ("a", "b", "c", ...),
used by most European languages, and the decimal digits
("0", "1", "2", ...), but we're trying to support as many
alphabets as possible.

To say that Clara OCR supports a given alphabet means that
Clara OCR

(a) is able to be trained from the keyboard for the symbols of
that alphabet, eventually applying some transliteration from that
alphabet to latin. For instance, when OCRing a greek text, if the
user presses the latin "a" key (assuming that the keyboard has
latin labels), Clara is expected to train the current symbol as
"alpha".

(b) knows the vertical alignment of each letter of that alphabet,
for instance, knows that the bottom of an "e" is aligned at the
baseline;

(c) knows which letters accept or require which signs (accents
and others, like the dot found on "i" and "j");

(d) contains code to help avoiding common mistakes, like
recognizing "e" as "c", "l" as "1", etc.

To say that Clara OCR supports a given alphabet does not
necessarily mean that Clara OCR

(a) knows some particular encoding (ISO-8859-X, Unicode, etc)
for that alphabet;

(b) contains or is able to use fonts for that alphabet to
display the OCR output on the PAGE (OUTPUT) window.

Even ignoring the standard encondings for one given
alphabet (e.g. ISO-LATIN-7 for Greek), Clara eventually
will be able to produce output using TeX macros, like
{\Alpha}.

*/

/* (devel)

Introducing the source code
---------------------------

This Guide is a collection of entry points to the Clara OCR
source code. Some notes explain punctual details about how this
or that feature was implemented. Others are higher-level
descriptions about how one entire subsystem works.

Language and environment
------------------------

Clara OCR is written in ANSI C (with some GNU extensions) and
requires the services of the C library and the Xlib. The
development is using 32-bit Intel GNU/Linux (various different
distributions), GCC, Gnu Make, Bash, XFree86 and Perl 5 (required
for producing the documentation).

Modularization
--------------

Clara source code started, of course, as being one only file
named clara.c. At some point we divided it into smaller
pieces. Currently there are 18 files:

  book.c     .. Documentation only
  build.c    .. The function build
  clara.c    .. Startup and OCR run control
  cml.c      .. ClaraML dumper and recover
  common.h   .. Common declarations
  consist.c  .. Consistency tests
  event.c    .. GUI initialization and event handler
  gui.h      .. Declarations that depend on X11
  html.c     .. HTML generation and parse
  pattern.c  .. Book font stuff
  pbm2cl.c   .. Import PBM
  pgmblock.c .. grayscale loading and blockfinding
  preproc.c  .. internal preprocessor
  redraw.c   .. The function redraw
  revision.c .. Revision procedures
  skel.c     .. Skeleton computation
  symbol.c   .. Symbol stuff
  welcome.c  .. Welcome stuff

Along this document we'll not refer these files, but the
identifiers (names of functions and variables).

Note that there are only two headers: common.h and gui.h. It's
complex to maintain one header for each module. Most functions
are not prototyped, but we intend to prototype all them in the
near future.


Security notes
--------------

Concerning security, the following criteria is being used:

1. string operations are generally performed using services that
accept a size parameter, like snprint or strncpy, except when the code
itself is simple and guarantees that a overflow won't occur.

2. The CGI clara.pl invokes write privileges through sclara, a program
specially written to perform only a small set of simple operations
required for the operation of the Clara OCR web interface.

The following should be done:

1. Memory blocks should be cleared before calling free().


Runtime index checking
----------------------

A naive support for runtime index checking is provided through the
macro checkidx. This checking is performed only if the code is
compiled with the macro MEMCHECK defined and the command-line switch
'-X 1' is used.

In fact, only those points on the source code where the macro checkidx
is explicitly used will perform index checking. We've added calls to
checkidx on some critical functions due to its complexity, or because
segfaults were already were detected there.

Background operation
--------------------

Clara OCR decides at runtime if the GUI will be used or not. So
even when using Clara OCR in batch mode (-b command-line switch),
linking with the X libraries is required.

When the -b command-line switch is used, Clara OCR just won't
make calls to X services. The source code tests the flag
"batch_mode" before calling X services. So it won't create the
application window on the X display, and automatically starts a
full OCR operation on all pages found (as if the "OCR" button was
pressed with the "work on all pages" option selected).  Upon
completion, Clara OCR will exit.


Synchronization
---------------



Execution model
---------------

In order to allow the GUI to refresh the application window while
one OCR run is in course, Clara does not use multiple
threads. The main function alternates calls to xevents() to
receive input and to continue_ocr() to perform OCR. As the OCR
operations may take long to complete, a very simple model was
implemented to allow the OCR services to execute only partially.

Such services (for instance load_page()) accept a "reset" parameter
to allow resetting all static data, and they're expected to
return 0 when finished, or nonzero otherwise. Therefore, a call to
such services must loop until completion. The continue_ocr() calls
the OCR steps using this model, and some OCR steps call other
services (like load_page()) that implement this model too.




Resetting
---------

XML support
-----------

We decided to use XML because of the facilities of using
non-binary encodings to store, analyse, change and transmit
information, and also because XML is a standard. Currently we do
not have DTDs, and until now we didn't try to load, using the
Clara parser, XML code not produced by Clara itself.


The GUI
-------


Main characteristics
--------------------

1. Clara OCR GUI uses only 5 colors: white, gray, darkgray,
verydarkgray and black. The RGB value for each one is
customizable at startup (-c command-line option). On truecolor
displays, graymaps are displayed using more graylevels than the 5
listed above.

2. The X I/O is not buffered. Buffered X I/O is implemented but
it's not being used.

3. Only one X font is used for all needs (button lables, menu
entries, HTML renderization, and messages).

4. Asynchronous refresh. The OCR operations just set the redraw
flags (redraw_button, redraw_wnd, redraw_int, etc) and let the
redraw() function make its work.

5. No toolkit is used. The graphic code is very specific to
Clara, and it was not written to be reusable. So it's very
small. The disadvantage of this approach is that Clara look and
behaviour will be slightly different from the typical ones found
on popular environments like GNOME or KDE.



The Clara API
-------------


*/

/* (book)

Building the book font
----------------------

Patterns are selected symbols from the book. They're obtained
from manual training, or from automatic selection. The patterns
are used to deduce the transliteration of the unknown symbols by
the bitmap comparison heuristics. In other words, the OCR
discovers that one symbol is the letter "a" or the digit "1"
comparing it with the patterns.

The book font is the collection of all patterns. The term "book
font" was chosen to make sure that we're not talking about the X
font used by the GUI. The book font is stored on a separate file
("patterns", on the work directory). Clara OCR classifies the
patterns into "types", one type for each printing font. By now,
most of this work must be done manually. Someday in the future,
the auto-tuning features and the pre-build customizations will
hopefully make this process less painful.

So, before OCRing one book, it's convenient to observe the
different fonts used. In our case, we have three fonts (the
quotations refer the page 5.pbm):

    Unknown Latin 9pt         ("Todos sao iguais...")
    Unknown Latin 9pt bold    ("Art. 5")
    Unknown Latin 8pt italic  (footings)

It's not mandatory to exactly identify each font by its "correct"
name or style or size (Roman, Arial, Courier, etc). In our case,
we've chosen the labels above ("Unknown Latin 9pt" and the
others). These labels can be manually entered using the PATTERN
(TYPES) tab, one "type" for each "font". So we'll have 3 "types",
and, for each one, various parameters can be manually
informed. At least the alphabet must be informed. In fact, the
PATTERN (TYPES) tab allows structuring very carefully all fonts
used along the book. Even some intrincated details, like the
classification techniques that can be used for each symbol, can
be set.

Now we can select some patterns from the pages 143-l.pbm and
143-r.pbm. Try:

    $ cd /home/clara/books/MBB/pbm
    $ clara &

Load the page 143-l.pbm. Observe the symbols, select a nice one
using the mouse button 1 or the arrows (say, a letter "a", small)
and train it pressing the corresponding key (the "a" key). Repeat
this process for various symbols, all from one same type (so do
not mix bold with non-bold, etc). The entered patterns belong by
default to "type 0". The "Set pattern type" entry of the Edit
menu can be used to move all "type 0" patterns to some other type
(1, 2 or 3 in our case). To display the letters and digits for
which few or no samples are trained, click the mouse right button
over the PAGE tab and select "Show pattern type". This way, one
can complete all fonts used along the book.

At this point, the "Auto-classify" feature (Edit menu) may be
quite useful. When on, Clara OCR will apply the just trained
pattern to solve all unknown symbols, so after training an "a",
only those "a" letters dissimilar to that trained will remain
unknown (grayed).

Now save the session (menu "File"), exit Clara OCR (menu "File"),
and enter Clara OCR again using the same commands above. Try to
load one file and/or to observe the patterns on the tabs PATTERN,
PATTERN (list), TUNE (SKEL), etc. This is a good way to
experience that Clara OCR is started and exited many times along
the duration of one OCR project.

The last remark in this subsection: instead of the just described
manual pattern selection, Clara OCR is able to select by itself
the patterns to use from the pages. In order to use this feature,
after selecting the checkbox "Build the bookfont automatically"
(TUNE tab), classify the symbols (just press the OCR button using
the mouse button 1, or press the mouse button 3 over it and
select the "classify" item). However, the current recommendation
is to prefer the manual selection of patterns, at least as a
first step.

*/

/* (book)

Reference of the Clara GUI
--------------------------

In this section, the Clara application window will be described
in detail, both to document all its features and to define the
terminology.



The application window
----------------------

The application window is divided into three major areas: the
buttons ("zoom", "OCR", "stop", etc) the "plate" (right),
including the tabs ("page", "symbol" and "font"), and one or more
"document windows" inside the plate.

We say "document window" because each window is exhibiting one
"document". This "document" may be the scanned page (PAGE
window), the current OCR output for this page (PAGE OUTPUT
window), the symbol form (PAGE SYMBOL window), the GPL (GPL
window) and so on. However, we'll refer the document windows
merely as "windows".

Around each window there are two scrollbars. On the botton of the
application window there is a status line. On the top there is
a menu bar (fully documented on the section "Reference of the
menus").


    +-----------------------------------------------+
    | File Edit OCR ...                             |
    +-----------------------------------------------+
    | +--------+     +----+ +--------+ +-------+    |
    | |  zoom  |     |page| |patterns| | tune  |    |
    | +--------+   +-+    +-+        +-+       +-+  |
    | +--------+   | +-------------------------+ |  |
    | |  zone  |   | |                         | |  |
    | +--------+   | |                         | |  |
    | +--------+   | |                         | |  |
    | |  OCR   |   | |        WELCOME TO       | |  |
    | +--------+   | |                         | |  |
    | +--------+   | |    C L A R A    O C R   | |  |
    | |  stop  |   | |                         | |  |
    | +--------+   | |                         | |  |
    |      .       | |                         | |  |
    |      .       | |                         | |  |
    |              | |                         | |  |
    |              | |                         | |  |
    |              | +-------------------------+ |  |
    |              +-----------------------------+  |
    |                                               |
    | (status line)                                 |
    +-----------------------------------------------+


Tabs and windows
----------------

Three tabs are oferred, and each one may operate in one or more
"modes". For instance, pressing the PATTERN tab many times will
circulate two modes: one presenting the windows "pattern" and
"pattern (props)" and another with the window "pattern
(list)".

On each tab, Clara OCR displays on the plate one or more
windows. Each such window is called a "document window" to
distinguish them from the application window. Each such window
is supposed to be displaying a portion of a larger document, for
instance

    The scanned page (graphic)
    The OCR output (text)
    The list of pages (text)
    The list of patterns (text)
    The symbol description (text)

Unless the user hides them, two scrollbars are displayed for each
document window, one horizontal and one vertical. On each one, a
cursor is drawn to show the relative portion of the full document
currently visible ont the display.

All available tabs and the modes for each one are listed
below. The numbers (1, 2, etc) are only to make easier to
distinguish one mode from the others. There is no effective
association between the modes and the numbers.

     tab      mode      windows
    -------------------------------

               1       WELCOME

               2       GPL

               3       PATTERN_ACTION

    page       4       PAGE_LIST

               5       PAGE
                       PAGE_OUTPUT
                       PAGE_SYMBOL

               6       PAGE_FATBITS
                       PAGE_MATCHES

    pattern    7       PATTERN

               8       PATTERN_LIST

               9       PATTERN_TYPES

    tune      10       TUNE

              11       TUNE_PATTERN
                       TUNE_SKEL

              11       TUNE_ACTS


Note that the windows WELCOME and GPL have no corresponding
tab. When these windows are displayed, there is no active
tab. Except in these cases, the name of the current window is
always presented as the label of the active tab.

The Alphabet Map
----------------

When the "Show alphabet map" option of the "View" menu is selected,
the GUI will include an alphabet map between the buttons and the
plate. This map presents all symbols from the current alphabet. The
current alphabet is selected using the alphabet button. The alphabet
button circulates all alphabets selected on the "Alphabets" menu.

Clara OCR offers an initial support for multiple alphabets. To become
useful, it needs more work. The alphabet map currently does not offer
any functionality. For some alphabets (Cyrillic and Arabic) the
alphabet map is disabled on the source code due to the large alphabet
size. Currently Clara OCR does not contain bitmaps for displaying
Katakana.


Reference of the menus
----------------------

Most menus are acessible from their labels menu bar (on the top of the
application window). The labels are "File", "Edit", etc. Other menus
are presented when the user clicks the mouse button 3 on some special
places (for instance the button "OCR"). Let's describe all menus and
their entries.

*/

/* (devel)

Geometry of windows
-------------------

The current window is informed through the CDW global variable
(set by the setview function). The variable CDW is an index for
the dw array of dwdesc structs. Some macros are used to refer the
fields of the structure dw[CDW]. The list of all them can be
found on the headers under the title "Parameters of the current
window".

The portion of the document being displayed is defined by the
macros X0, Y0, HR and VR, where (X0,Y0) is the top left and HR
and VR are the width and heigth, measured in pixels (graphic
documents) or characters (text documents):


         X0  X0+HR-1
         |     |
    +----+-----+--+
    |             |
    |             |
    |    +-----+  +- Y0
    |    |     |  |
    |    |     |  |
    |    |     |  |
    |    +-----+  +- Y0+VR-1
    |             |
    |             |
    |             |
    |             |
    |             |
    |             |
    +-------------+
     The document


Regarding the application window, the document window is a
portion of the plate, defined by DM, DT, DW and DH, where (DM,DT)
is the top left and DW and DH are the width and heigth measured
in display pixels.


          DM              DM+DW-1
          |                 |
    +-----+-----------------+----+
    |                            |
    |                            |
    |                            |
    |     +-----------------+    +- DT
    |     |                 | |  |
    |     |                 | X  |
    |     |                 | X  |
    |     |    Document     | X  |
    |     |     window      | |  |
    |     |                 | |  |
    |     |                 | |  |
    |     |                 | |  |
    |     |                 | |  |
    |     +-----------------+    +- DT+DH-1
    |      -----XXXXXXXXXXX-     |
    |                            |
    |                            |
    +----------------------------+
         Application window


The rectangle (X0,Y0,HR,VR) from the document is exhibited into
the display rectangle (DM,DT,DW,DH). When displaying the scanned
page, the reduction factor RF applies. Each square RFxRF of
pixels from the document will be mapped to one display pixel.
When displaying the scanned page in fat bit mode, each document
pixel will be mapped to a square ZPSxZPS of display pixels, and a
grid will be displayed too.


Scrollbars
----------

The scrollbars inform the relative portion of the document being
exhibited. The viewable region of the document (in the sense just
defined) is defined by X0, Y0, HR and VR:

              Y0    Y0+HR-1

         +----+-------+-------+ - 0
         |                    |
      X0 +    +-------+       |
         |    |       |       |
         |    |       |       |
         |    |       |       |
         |    |       |       |
 X0+VR-1 +    +-------+       |
         |                    |
         |                    |
         |                    |
         |                    |
         +--------------------+ - GRY-1

         |                    |
         0                   GRX-1

The variables GRX and GRY contain the total width and height of
the full document, measured in pixels. The interpretation of the
contents of the variables X0, Y0, HR and VR is not simple. In some
cases, they will contain values measured in pixels. In other cases,
in characters. The variables HR and VR define the size of the
window. However, in some cases this size is the size
from the viewpoint of the document and, in others, of the display
(the difference is a reduction factor).

            +------------+  -
            |            |  |
            |            |  |
            |            |  X
            |            |  X
            |            |  X
            |            |  |
            |            |  |
            +------------+  -

            |---XXXX-----|


Note that the parameters X0, Y0, HR, VR, GRX and GRY are macros
that refer the corresponding fields of the structure dw[CDW],
that stores the parameters of the current DW.


Displaying bitmaps
------------------

The Bitmaps on HTML windows and on the PAGE window are exhibited
in "reduced" fashion (a square RFxRF of pixels from the bitmap is
mapped to one display pixel). If RF=1, then each bitmap pixel
will map to one display pixel.

The windows PATTERN, PAGE_FATBITS, and PAGE_MATCHES exhibit
bitmaps in "zoomed" mode (one bitmap pixel maps to a ZPSxZPS
square of display pixels). In this case a grid is displayed to
make easier to distinguish each pixel. The variables GW and GS
contain the grid width and the "grid separation" (GS=ZPS+GW).

                   ZPS     GS              GW
                |<---->|<----->|   --->||<---

               ++------++------++------++----
               ++------++------++------++----
               ||      ||      ||      ||
               ||      ||      ||      ||
               ||      ||      ||      ||
               ++------++------++------++----
               ++------++------++------++----
               ||      ||      ||      ||
               ||      ||      ||      ||
               ||      ||      ||      ||


Note that the parameters RF, GS and GW are macros that refer the
corresponding fields of the structure dw[CDW], that stores the
parameters of the current DW.


Auto-submission of forms
------------------------

The Clara OCR GUI tries to apply immediately all actions taken by
the user. So the HTML forms (e.g. the PATTERN window) do not
contain SUBMIT buttons, because they're not required (some forms
contain a SUBMIT button disguised as a CONSIST facility, but this
is just for the user's convenience).

The editable input fields make auto-submission mechanisms a bit
harder, because we cannot apply consistency tests and process the
form before the user finishes filling the field, so
auto-submission must be triggered on selected events. The
triggers must be a bit smart, because some events must be
attended before submission (for instance toggle a CHECKBOX),
while others must be attended after submission (for instance
changing the current tab). So auto-submission must be carefully
studied. The current strategy follows:

a. When the window PAGE (symbol) or the window PATTERN is
visible, auto-submit just after attending the buttons that change
the current symbol/pattern data (buttons BOLD, ITALIC, ALPHABET
or PTYPE).

b. When the window PAGE (symbol) or the window PATTERN is
visible, auto-submit just before attending the left or right
arrows.

c. When the user presses ENTER and an active input field exists,
auto-submit.

d. Auto-submit as the first action taken by the setview service,
in order to flush the current form before changing the current
tab or tab mode.

e. Auto-submit just after opening any menu, in order to flush
data before some critic action like quitting the program or
starting some OCR step.

f. Auto-submit just after attending CHECKBOX or RADIO buttons.

Auto-submission happens when the service auto_submit_form is
called, so it's easy to locate all triggering points (just search
the string auto_submit_form). This service takes no action when
the current form is unchanged.

The Clara API
-------------

This section describes the variables and functions exported by
Clara OCR for extensionability purpuses. Note that Clara OCR
currently does not have an interface for extensions. The first
such interface planned to be added will use the Guile
interpreter, available from the GNU Project.

*/

/* (all)

AVAILABILITY
------------

Clara OCR is free software. Its source code is distributed under
the terms of the GNU GPL (General Public License), and is
available at CLARA_HOME. If you don't know what is the GPL,
please read it and check the GPL FAQ at
http://www.gnu.org/copyleft/gpl-faq.html. You should have
received a copy of the GNU General Public License along with this
software; if not, write to the Free Software Foundation, Inc., 59
Temple Place - Suite 330, Boston, MA 02111-1307, USA. The Free
Software Foundation can be found at http://www.fsf.org.


CREDITS
-------

Clara OCR was written by Ricardo Ueda Karpischek. Giulio Lunati
wrote the internal preprocessor. Clara OCR includes bugfixes
produced by other developers. The Changelog
(http://www.claraocr.org/CHANGELOG) acknowledges all them (see
below). Imre Simon contributed high-volume tests, discussions
with experts, selection of bibliographic resources, propaganda
and many ideas on how to make the software more useful.

Ricardo authored various free materials, some included (at least)
in Conectiva, Debian, FreeBSD and SuSE (the verb conjugator
"conjugue", the ispell dictionary br.ispell and the proxy
axw3). He recently ported the EiC interpreter to the Psion 5
handheld and patched the Xt-based vncviewer to scale framebuffers
and compute image diffs. Ricardo works as an independent
developer and instructor. He received no financial aid to develop
Clara OCR. He's not an employee of any company or organization.

Imre Simon promotes the usage and development of free
technologies and information from his research, teaching and
administrative labour at the University.

Roberto Hirata Junior and Marcelo Marcilio Silva contributed
ideas on character isolation and recognition. Richard Stallman
suggested improvements on how to generate HTML output. Marius
Vollmer is helping to add Guile support. Jacques Le Marois helped
on the announce process. We acknowledge Mike O'Donnell and Junior
Barrera for their good criticism. We acknowledge Peter Lyman for
his remarks about the Berkeley Digital Library, and Wanderley
Antonio Cavassin, Janos Simon and Roberto Marcondes Cesar Junior
for some web and bibliographic pointers. Bruno Barbieri Gnecco
provided hints and explanations about GOCR (main author: Jorg
Schulenburg). Luis Jose Cearra Zabala (author of OCRE) is gently
supporting our tentatives of using portions of his code. Adriano
Nagelschmidt Rodrigues and Carlos Juiti Watanabe carefully tried
the tutorial before the first announce. Eduardo Marcel Macan
packaged Clara OCR for Debian and suggested some
improvements. Mandrakesoft is hosting claraocr.org. We
acknowledge Conectiva and SuSE for providing copies of their
outstanding distributions. Finally, we acknowledge the late Jose
Hugo de Oliveira Bussab for his interest in our work.

Adriano Nagelschmidt Rodrigues donated a 15" monitor.

The fonts used by the "view alphabet map" feature came from
Roman Czyborra's "The ISO 8859 Alphabet Soup" page at
http://czyborra.com/charsets/iso8859.html.

The names cited by the CHANGELOG (and not cited before) follow
(small patches, bug reports, specfiles, suggestions,
explanations, etc).

Brian G. (win32),
Bruce Momjian,
Charles Davant (server admin),
Daniel Merigoux,
De Clarke,
Emile Snider (preprocessor, to be released),
Erich Mueller,
Franz Bakan (OS/2),
groggy,
Harold van Oostrom,
Ho Chak Hung,
Jeroen Ruigrok,
Laurent-jan,
Nathalie Vielmas,
Romeu Mantovani Jr (packager),
Ron Young,
R P Herrold,
Sergei Andrievskii,
Stuart Yeates,
Terran Melconian,
Thomas Klausner (NetBSD),
Tim McNerney,
Tyler Akins.

*/

/* (faq)

WELCOME
-------

These are the Clara OCR Frequently Asked Questions. They're
useful for a first contact with Clara OCR. If you're looking for
information on how to use Clara OCR, please try the Clara OCR
Tutorial instead. Clara OCR can be found at CLARA_HOME.

CONTENTS
--------

What is Clara OCR?
Why is Clara a "cooperative OCR"?
Is Clara OCR Free? Open Source?
Is Clara OCR a GNU program?
On which platforms does Clara OCR run?
Does Clara OCR have a command-line interface?
Does Clara OCR run on KDE? GNOME?
Which languages are supported by Clara OCR?
Does Clara OCR support Unicode?
Is Clara OCR omnifont?
How does Clara differ from other OCRs?
What is PBM/PGM/PPM/PNM?
How can I scan paper documents using Clara OCR?
I've tried Clara OCR, but the results disappointed me
How can I get support on Clara OCR?
Does Clara OCR induce to Copyright Law infringements?
How can I help the Clara OCR development effort?



What is Clara OCR?
------------------

Clara is an OCR program. OCR stands for "Optical Character
Recognition". An OCR program tries to recognize the characters
from the digital image of a paper document. The name Clara stands
for "Cooperative Lightweight chAracter Recognizer".


Why is Clara a "cooperative OCR"?
---------------------------------

Clara is a cooperative OCR because it offers an web interface for
training and revision, so these tasks can benefit from the
revision effort of many people across the Internet. However,
Clara OCR also offers a powerful X-based GUI for standalone
usage.


Is Clara OCR Free? Open Source?
-------------------------------

Clara OCR is distributed within the terms of the Gnu Public
License (GPL) version 2. Yes, Clara OCR is Free. Yes, Clara OCR
is Open Source. Clara OCR is not "Shareware", nor "Public
Domain".


Is Clara OCR a GNU program?
---------------------------

Clara OCR is unrelated to the GNU Project but its development is
strongly based on GNU programs (GCC, Emacs and others), as well
as on other free softwares, like the Linux kernel and XFree86.

Clara OCR is free software because we agree on the free software
ideal as stated by the GPL. To make this agreement explicit we
also adopted some suggestions from the Free Software
Foundation. These suggestions apply to the Clara OCR
documentation:

(a) GPL programs are referred as "free software", not "open
source".

(b) The term "GNU/Linux (operating system)" is used rather
than "Linux (operating system)".

(c) We do not recommend non-free softwares and do not refer
the user to non-free documentation for free softwares.

Furthermore, Clara OCR will support Guile as an extension
language in the near future.

Remark: We write "free software" instead of "open source"
just for coherence. We dislike antagonisms between the various
initiatives created along the years to freely produce, use,
change and distribute software.


On which platforms does Clara OCR run?
--------------------------------------

Clara OCR is being developed on 32-bit Intel running GNU/Linux.
Currently Clara OCR won't run on big-endian CPUs (e.g. Sparc) nor
on systems lacking X windows support (e.g. MS-Windows). A
relatively fast CPU (300MHz or more) is recommended. There is a
port initiative to MS-Windows being worked. See also the next
question.


Does Clara OCR have a command-line interface?
---------------------------------------------

Yes, but the X Windows headers and libraries are required anyway
to compile the source code, and the X Windows libraries are
required to run even the Clara OCR command-line interface. Unless
someone reworks the code, it's not possible to detach the GUI in
order to compile Clara OCR on systems that do not support X
Windows.



Does Clara OCR run on KDE? GNOME?
---------------------------------

Clara OCR will hopefully run on any graphic environment based on
Xwindows, including KDE, GNOME, CDE, WindowMaker and
others. Clara OCR depends only on the X library, and does not
require GTK, Qt or Motif to run. Clara OCR does not use the X
Toolkit (aka "Xt"). Clara OCR has been successfully tested on
X11R5 and X11R6 environments with twm, fvwm, mwm and others.


Which languages are supported by Clara OCR?
-------------------------------------------

As a generic recogniser, Clara OCR may be tried with any language
and any alphabet. However, there are some restrictions. Currently
Clara OCR expects the words to be written horizontally, and there
are some heuristics that suppose some geometric relationships
typical for the Latin Alphabet and the accents used by most
european languages. Support for language-specific spell checking
is expected to be added soon.


Does Clara OCR support Unicode?
-------------------------------

No, Clara OCR does not support Unicode, and the support to the
ISO-8859 charsets is partial.


Is Clara OCR omnifont?
----------------------

No, Clara OCR is not omnifont. Clara OCR implements an OCR model
based on training. This model makes training and revision one
same thing, making possible to reuse training and revision
information (see also the next question).


How does Clara differ from other OCRs?
--------------------------------------

This is a quote from the Clara Advanced User's Manual:

Clara differs from other OCR softwares in various aspects:

1. Most known OCRs are non-free and Clara is free. Clara focus
the X windows system. Clara offers batch processing, a web
interface and supports cooperative revision effort.

2. Most OCR softwares focus omnifont technology disregarding
training. Clara does not implement omnifont techniques and
concentrate on building specialized fonts (some day in the
future, however, maybe we'll try classification techniques that
do not require training).

3. Most OCR softwares make the revision of the recognized text a
process totally separated from the recognition. Clara
pragmatically joins the two processes, and makes training and
revision parts of one same thing. In fact, the OCR model
implemented by Clara is an interactive effort where the usage of
the heuristics alternates with revision and fine-tuning of the
OCR, guided by the user experience and feeling.

4. Clara allows to enter the transliteration of each pattern
using an interface that displays a graphic cursor directly over
the image of the scanned page, and builds and maintains a mapping
between graphic symbols and their transliterations on the OCR
output. This is a potentially useful mechanism for documentation
systems, and a valuable tool for typists and reviewers. In fact,
Clara OCR may be seen as a productivity tool for typists.

5. Most OCR softwares are integrated to scanning tools offerring
to the user an unified interface to execute all steps from
scanning to recognition. Clara does not offer one such integrated
interface, so you need a separate software (e.g. SANE) to
perform scanning.

6. Most OCR softwares expect the input to be a graphic file
encoded in tiff or other formats. Clara supports only raw PBM
and PGM.


What is PBM/PGM/PPM/PNM?
------------------------

PBM, PGM and PPM are graphic file formats defined by Jef
Poskanzer. PNM is not a graphic file format, but a generic
reference to those three formats. In other words, to say that a
program supports PNM means that it handles PBM, PGM and PPM.

    PBM = Portable BitMap
    PGM = Protable GrayMap
    PPM = Portable PixMap
    PNM = Portable aNyMap

PBM files are black-and-white images, 1 bit per pixel. PGM files
are grayscale images, 8 bits per pixel. PPM files are color
images, 24 bits per pixel. Currently Clara OCR likes raw PBM and
raw PGM files only. A scanned page stored in some format other
than PBM or PGM can be converted to PBM or PGM using the netpbm
tools, ImageMagick or others.

PNM files may be "raw" or "plain". The plain versions are rarely
used. Clara OCR does not support plain PBM nor plain PGM. To make
sure about the file format, try the "file" utility, for instance

    file test.pbm

Remember that image conversion sometimes implies data loss. For
instance, to convert a color image to black-and-white, each pixel
must be mapped to either black or white, so the original color
(say, red, lightblue, seagreen, tomato, mistyrose, etc) is
dropped. Also, the conversion process should decide for each
pixel if it will be mapped to black or to white. Generally, the
program that performs the conversion offers a variety of
different mapping criteria. The OCR results depend strongly on
the criterion chosen.


How can I scan paper documents using Clara OCR?
-----------------------------------------------

You cannot. Clara OCR includes no support for scanners. To scan
paper documents, use another software, like the one bundled with
your scanner, or SANE (http://www.mostang.com/sane/). The
development tests are using SANE.


I've tried Clara OCR, but the results disappointed me
-----------------------------------------------------

All OCR programs will disappoint you depending on the texts
you're trying to recognize. If you're a developer, join the Clara
OCR development effort and try to make it behave better on your
texts. If your are not a developer, wait a new version and try
again.


How can I get support on Clara OCR?
-----------------------------------

If the documentation did not solve your problems, try the
discussion list.


Does Clara OCR induce to Copyright Law infringements?
-----------------------------------------------------

No. Clara OCR is just a tool for character recognition like many
others that can be purchased or are bundled with scanners. The
Clara OCR Project claims all users to be aware about the
Copyrigth Law and not infringe it. The Clara OCR Project
abominates any try to infringe the legitimate laws of any
country.

Nonetheless, the Clara OCR Project supports the free and public
availability of materials produced to be free, or of materials
out of copyright due to its age. The Clara OCR Project recognizes
the right of anyone to produce free or non-free materials.


How can I help the Clara OCR development effort?
------------------------------------------------

The best way is to use Clara OCR to recognize the texts you're
interested on, and try to make it adapt better to them. The
Developer's Guide should help in this case (C programming skills
are required). The Clara OCR Project acknowledges all efforts to
make Clara OCR more widely known and used.


*/

/* (glossary)

WELCOME
-------

This is the Clara OCR glossary. It's somewhat specific to Clara
OCR. The entries that do not refer an author were written by
Ricardo Ueda Karpischek. Send new entries or suggestions to
claraocr@claraocr.org. This glossary is part of the Clara OCR
documentation. Clara OCR is distributed under the terms of the
GNU GPL.


CONTENTS
--------

algorithm
binarization
bitmap
bitmap comparison
border
border mapping
clara
classification
density
depth
digital image
dpi
function
graphic format
graymap
heuristic
image size
mapping
OCR
page
pattern
pixel
pixel distance
pixmap
PBM
PGM
PNM
PPM
resolution
skeleton
skeleton fitting
symbol
thresholding
Xlib


*/

/* (glossary)


image size
----------

As a digital image uses to be a rectangular matrix of pixels, its
size in pixels can be conveniently described giving the rectangle
width and height, usually in the form WxH. For instance, a 200x100
image is a rectangle of pixels having width 200 and height 100.

depth
-----

the number of bits available to store the color of each pixel.
Black-and-white images have depth 1. Graymaps use to have depth
8 (256 graylevels). The larger the depth, the larger will be the
amount of disk or ram space required to store a digital image.
For instance, an image of size 100x100 and depth 8 requires
100*100*8 = 80000 bits = 8000 bytes to be stored.

graphic format
--------------

A standardised way to store the color of each pixel from a digital
image in a disk file. The graphic format may include other
information, like density and image annotations. Some graphic
formats include a provision to compress the data. In some cases,
this compression, if used, may change the color of some pixels
or regions to colors close to the original ones, but different.
So the usage of some graphic formats may imply in data loss.
Examples of graphic formats are TIFF, JPEG, GIF, BMP, PNM, etc.

clara
-----

Cooperative Lightweight Recognizer. "Clara" is also a personal
name: Clara (Latin, Portuguese, Spanish), "Chiara" (Italian),
Claire (English).


OCR
---

Optical Character Recognition. Some people feel hard to
understand conveniently what OCR is due to the lack of knowledge
on how computers store and process text and image data. Most
users think OCR as being a required step before editing and
spell-checking documents got from the scanner (it's not wrong,
though).

algorithm
---------

a well defined procedure. The term "algorithm" is usually
reserved for procedures whose properties can be assured,
generally through a rigorous mathematical proof. For instance,
the procedure learned by children to multiply two numbers from
their multi-digit decimal representations is an algorithm (see
heuristic).

binarization
------------

the conversion from color or grayscale (PGM) to
black-and-white. The Clara OCR classification heuristics
currently available require black-and-white input, so when the
input is grayscale (PGM), Clara OCR needs to convert it to
black-and-white before OCR. Note that to binarize an image, some
choice must be done on how to map colors or graylevels to either
black or white. Also and mainly, and the OCR results depends
strongly on that choice.

bitmap
------

The Clara OCR documentation tries to use the term "bitmap" to
mean only rectangular, black-and-white digital images. Grayscale
rectangular digital images are called "graymaps" (see also
pixel).

bitmap comparison
-----------------

any method intended to decide if two given bitmaps are
similar. Clara OCR implements three such methods: skeleton
fitting, border mapping and pixel distance.

border
------

the line formed by the bitmap black pixels that have white
neighbours. Note that the definition of "neighbour" may
vary. Clara OCR generally consider that the neighbours of one
pixel are all 8 pixels contiguous to it (top left, top, top
right, left, right, bottom left, bottom, bottom right).

border mapping
--------------

a bitmap comparison technique that builds a mapping from the
border pixels of one bitmap to the border pixels of another
bitmap. If this mapping is found to satisfy certain mathematical
properties, the bitmaps are considered similar.

classification
--------------

the process that recognizes a given bitmap as being the letter
"a" or the digit "5", etc. Instead of saying that the bitmap was
"recognized" as a letter "a", it's common to say that it was
"classified" as a letter "a". All Clara OCR classification
methods are currently based on bitmap comparison techniques.

density
-------

see dpi.

digital image
-------------

see pixel.

dpi
---

dots-per-inch. A measure of linear image density. Example:
scanning an A4 (210x297mm) page at 300 dpi results an image of
size 2481x3508 (remember that 1 inch equals 25.4 millimeters). In
most cases, all relevant visual details from printed characters
can be conveniently captured at 600dpi (in some cases, 300dpi
suffices). Some file formats, like TIFF or JPEG, include density
information. Others, like PBM, PGM or PPM, don't. So when
converting from TIFF to PGM, remember that the density
information is dropped. So if, for instance, you ask SANE to scan
a page creating a TIFF file, and subsequently convert it to PPM,
and from PPM to TIFF again, the last file will not be equal to
the first one. Density information uses to be irrelevant when
displaying images on the computer monitor, because in this case a
1-1 mapping between image pixels and display pixels is
assumed. However, density information is quite important when
printing an image on paper, or when performing OCR. Clara OCR
expects to be informed explicitly about the image density
(default 600 dpi).

function
--------

a rule that assigns, for each given element, another element, in
a unique fashion. For instance, the equation y = x+1 defines a
function that assigns to each number x the number x+1. A 2d
digital image may be seen as a function that assigns to each dot,
given by its horizontal and vertical coordinates, a color
("black", "white", "green", etc). Functions are also called
"mappings".

graymap
-------

see bitmap.

heuristic
---------

a procedure whose properties are not assured. Heuristics are
generally the expression of some more or less vague feeling, or a
naive, initial approch for a complex problem. If an heuristic can
be proven to satisfy some interesting property, then it can be
referred as an algorithm (in regard of that property). Some
experts say that OCR is an engeneering field, not a mathematical
field. Perhaps we can express this same idea saying that by its
own nature, OCR is a field where nothing else than heuristics can
be stated.

mapping
-------

see function.

page
----

a scanned document. The Clara OCR documentation tries to avoid
using terms like "document", "image" or "file" to signify a
scanned document. "Page" is used instead.

pattern
-------

in the Clara OCR context, it's a letter, digit or accent
instance, used to classify the page symbols through bitmap
comparison. Clara OCR builds a set of patterns based on manual
training or automatic selection, and uses it to classify all page
symbols.

pixel
-----

each one of the individual dots that compose a digital image
(quite frequently, the term "pixel" is used to refer only the
non-white dots of an image). A digital image uses to be a
rectangular matrix of dots. To each one it's possible to assign
one from many available colors, in order to form an image. If the
available colors are only "black" and "white", the image thus
formed is a "black-and-white image". As the representation of one
from two possible values may be done using a bit, and the
assignment of geometrically well positioned dots to colors may be
seen as a function or mapping, a black-and-white image is also
called a "bitmap". Similarly, if the colors available are only
gray levels, usually from 0 (black) to 255 (white), then the
image is a "grayscale image" or a graymap, and a generic
assignment of pixels to colors is called a "pixmap".

pixel distance
--------------

a bitmap comparison technique that builds a mapping from all
pixels of one bitmap to the pixels of another bitmap. If this
mapping is found to satisfy certain mathematical properties, the
bitmaps are considered similar.

pixmap
------

see pixel.

PBM
---

see PNM.

PGM
---

see PNM.

PNM
---

Portable aNyMap. PNM is a generic reference to the graphic file
formats PBM, PGM and PPM defined by Jef Poskanzer. In other
words, to say that a program supports PNM means that it handles
PBM, PGM and PPM. PBM (Portable BitMap) files are black-and-white
images, 1 bit per pixel. PGM (Protable GrayMap) files are
grayscale images, 8 bits per pixel. PPM (Portable PixMap) files
are color images, 24 bits per pixel. Currently Clara OCR likes
PBM and PGM files only. A scanned page stored in some format
other than PBM or PGM can be converted to PBM or PGM using the
netpbm tools, ImageMagick or others. PNM files may be "raw" or
"plain". The plain versions are rarely used. Clara OCR does not
support plain PBM nor plain PGM.

PPM
---

see PNM.

resolution
----------

this term is used along the Clara OCR documentation to refer
either the image size (for instance: 640x480 pixels) or the image
density (for instance: 300 pixels per inch).

skeleton
--------

ideally, it's a minimal structural bitmap. From an algorithmic
standpoint, the skeleton of a symbol is the bitmap obtained
clearing a number of its peripheric pixels, whose remotion does
not destroy the symbol shape.

skeleton fitting
----------------

a bitmap comparison technique that decides that two given bitmaps
are similar if and only if the skeleton of each one fits into the
other.

symbol
------

an instance of a letter or digit in a page. So if the word
"classical" occurs in a page, all its letters ("c", "l", "a",
"s", "s", "i", "c", "a", "l") are individual symbols. At the
source code level, things that are not letters not digits are
sometimes called symbols (for instance, pieces of broken symbols,
dots, accents, noise, etc).

thresholding
------------

a simple binarization method. It decides to map each pixel from a
graymap to either black or white just testing if its gray level
is smaller or larger than a given threshold. So, if the threshold
is, say, 171, then all gray levels from 0 to 170 are mapped to 0
(black) and all graylevels from 171 to 255 are mapped to 255
(white). The thresholding is said to be global if one fixed
(per-page) binarization threshold is used to decide the mapping
of all page pixels. The thresholding is said to be local if the
threshold is allowed to vary along the page, due to irregular
printing intensity.

Xlib
----

the low-level, standard, Xwindows library. It offers
basic graphic primitives, similar to others found on most graphic
environments, like "draw line", "draw pixel", "get next event",
etc, as well as services more specific to the Xwindows way of
doing things, like "connect to an X display", properties
(resources) handling, etc. The Xlib does not include facilities
to create menus, buttons, etc. Application programs usually take
these facilities from "toolkits" like Xt, GTK, Qt and
others. Clara OCR creates the few facilities it needs using
the Xlib primitives.

*/


/*

Alignment drafts

    s_pair(a,b)
        complete_align(a,b)
            get_ap(a)
                use hardcoded data
            get_ap(b)
                use hardcoded data
            get_dd(a,x,b,d)
                estimate from alignment data
        geo_align(a)
        geo_align(b)


1. geometrical line detection.

2. compute per-symbol geometrical alignment.

3. add per-symbol alignment data to the pattern types.

4. add alignment filtering rule to the classification service.

*/
