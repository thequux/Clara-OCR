rem
rem  Compile and install Clara OCR on OS/2
rem  You need the usual environement ;-) sed, emx, XFree86,
rem  gnu-make (renamed to gmake.exe), perl, ...
rem  Quick and dirty and not perfect, but worked for me
rem  with emx-gcc 2.8.1 and Clara Version 0.9.9 :-)
rem  fbakan@gmx.net  9.5.2002
rem 

echo extproc perl -x >mkdoc.cmd
cat mkdoc.pl >>mkdoc.cmd
echo extproc perl -x >selthresh_fidian.cmd
cat selthresh_fidian.pl>>selthresh_fidian.cmd
echo extproc perl -x >clara.cmd
cat clara.pl >>clara.cmd
echo extproc perl -x >selthresh.cmd
cat selthresh >>selthresh.cmd
sed -f os2.sed Makefile 1>Makefile.os2
SET LANG=
gmake -f Makefile.os2 install
