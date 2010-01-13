#
#  OS/2 specific sed-script for generating a Makefile.OS2
#  Tested with clara Version 0.9.9 and EMX-GCC 2.8.1
#  fbakan@gmx.net   9.5.2002
#
s|BINDIR=/usr/bin|BINDIR=/XFree86/bin|1
s|MANDIR=/usr/man/man1|MANDIR=/XFree86/man/man1|1
s|DOCDIR=/usr/share/doc/clara|DOCDIR=/XFree86/doc/clara|1
s/INCLUDE = -I\/usr\/X11R6\/include/INCLUDE = -I\/XFree86\/include/1
s/LIBPATH = -L\/usr\/X11R6\/lib/LIBPATH = -L\/XFree86\/lib/1
s/\.\/mkdoc.pl/\.\\mkdoc.cmd/g
s/install selthresh\.pl/install selthresh\.cmd/1
s/install clara/install clara.exe/1
s/LDFLAGS = -g/LDFLAGS= -Zexe -Zmtd -D__ST_MT_ERRNO__ -O2 -Zsysv-signals -s/1
