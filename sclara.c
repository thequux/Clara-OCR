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

sclara.c: This is a sudo-like tool for Clara OCR

*/

/* configure the root directory to use */
char *root = "/home/clara/www";

/*

sclara: perform operations (requested by the web interface)
that require privilege.

BUGS: should read the path of the clara root directory from
root-protected file and perform operations only on that
subtree. Must restrict -u option only to "doubts"
subdirectories. Must restrict append only to true input files.

*/

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

/*

usage:

    sclara file parameters

(create revision file)

or

    sclara -u path

(unlink the file specified)

*/

/*

Add string a to s, writing each character in hexadecimal.
Returns the number of characters added to s.

*/
int add_string(char *s, char *a) {
        int i, t, ts;

        for (i = t = 0, ts = strlen(a); i < ts; ++i) {
                if (i > 0)
                        t += sprintf(s + t, ",%x", a[i]);
                else
                        t += sprintf(s + t, "%x", a[i]);
        }
        t += sprintf(s + t, "\n");
        return (t);
}

/*

The program.

*/
int main(int argc, char *argv[]) {
        int F;
        char s[20 * 1024];
        char fn[257];
        char host[65];

        /*
           BUG: we should use chroot here, but that requires root privileges.
           Using chdir makes the sclara security depend on the administrator.
         */
        chdir(root);

        if (strcmp(argv[1], "-u") == 0) {

                /* refuse absolute paths */

                /* refuse ".." */

                unlink(argv[2]);
        } else {
                int i, t, ts;
                char *a;

                /* revision record */
                {
                        t = 0;

                        /* preamble */
                        t += sprintf(s + t, "<Clara>\n");
                        t += sprintf(s + t, "<adesc idx=-1\n");

                        /* REV_TR = 1 */
                        t += sprintf(s + t, "t=1\n");

                        /* document name */
                        t += sprintf(s + t, "d=");
                        t += add_string(s + t, argv[10]);

                        /* F_PROPAG = 16 */
                        t += sprintf(s + t, "f=16\n");

                        /* symbols */
                        t += sprintf(s + t, "mc=%s\n", argv[8]);
                        t += sprintf(s + t, "a1=-1\n");
                        t += sprintf(s + t, "a2=-1\n");

                        /* transliteration (1 is Latin) */
                        t += sprintf(s + t, "a=1\n");
                        t += sprintf(s + t, "tr=");
                        t += add_string(s + t, argv[11]);

                        /* reviewer data (revtype 1 is ANON) */
                        t += sprintf(s + t, "r=");
                        t += add_string(s + t, argv[5]);
                        t += sprintf(s + t, "rt=1\n");
                        t += sprintf(s + t, "sa=");
                        t += add_string(s + t, argv[3]);
                        t += sprintf(s + t, "dt=%s\n", argv[4]);

                        /* original revision data */
                        /*
                           t += sprintf(s+t,"or");
                           t += sprintf(s+t,"ob");
                         */
                        t += sprintf(s + t, "om=-1\n");
                        t += sprintf(s + t, "od=0\n");
                        t += sprintf(s + t, "></adesc>\n");

                        /* end */
                        t += sprintf(s + t, "</Clara>\n");
                }
                s[20 * 1024 - 1] = 0;

                /* hostname */
                gethostname(host, 64);
                host[64] = 0;
                for (i = 0; (host[i] != 0) && (host[i] != '.'); ++i);
                host[i] = 0;

                /* filename */
                strncpy(fn, argv[1], 255);
                t = strlen(fn);
                if ((t > 0) && (fn[t - 1] != '/')) {
                        strcat(fn, "/");
                        ++t;
                }
                snprintf(fn + t, 256, "%s.%d.%d.%s", argv[9], time(NULL),
                         getpid(), host);
                fn[256] = 0;

                /* create file and write revision record */
                fprintf(stderr, "will write record to %s\n", fn);
                F = open(fn, O_WRONLY | O_APPEND | O_CREAT, 0644);
                if (F < 0) {
                        fprintf(stderr, "could not open file %s\n", fn);
                        exit(1);
                }
                if (write(F, s, strlen(s)) != strlen(s)) {
                        fprintf(stderr, "could not write record to %s\n",
                                fn);
                        exit(1);
                }
                close(F);
        }

        exit(0);
}
