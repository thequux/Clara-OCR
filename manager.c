/*
  Copyright (C) 2001 Ricardo Ueda Karpischek

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

manager.c - scanner station manager

*/

/*


The Scanning Station
--------------------

This is the program that controls the scanning station, built by
the Clara OCR Project. The scanning station is a PC without
console running GNU/Linux. A scanner, a 12-key serial keyboard
and a CD-R drive are attached to it. Metallic extensions were
added to the PC cabinet, in such a manner that the entire thing
looks like one single and compact block. The main feature of this
station is the ease of use. The operator just presses one key to
scan and compress one page, or to burn a multi-session CD with
the already scanned pages (and compare the files before removing
them), or to shutdown and power off the machine. Customized
speaker sounds are used to report some relevant statuses.


Hardware description
--------------------

Due to cost restrictions, the scanning station was built using
old or cheap hardware. The keyboard is a very simple one, with 12
keys (digits 0-9, "CANCEL" and "ENTER"). The manager includes
serial communication code to accept input from it. The CD-R drive
is a 8x4 Mitsumi CR-4801TE CD-R. Yet the CD-R media is a bit
small for such application, it's quite cheap. The PC is a P200
with 24M RAM, Adaptec 2940 controller and two old HDs: SCSI 2.1G
and IDE 540M. The scanner is a Genius HR5 SCSI. The SCSI disk is
used to store the scanned pages. As the IDE disk is used to store
the CD image (in fact, half of the CD image, because the disk is
not large enough; the CD is burned in two sessions). The SCSI HD
is not used to store CD images because the HR5 scanner blocks the
SCSI bus two or three times along the scanning (see the file
sane-umax-config-doc.html from the SANE documentation), and in
our tests we observed underruns when using the SCSI HD to store
the CD image.


How it works
------------

From now to the end of this document, paths are subject to
changes depending on how the PATHS sections on the source code is
configured.

The manager is expected to be started and restarted by the init
program. So a line similar to

    sm:2345:respawn:/usr/local/sbin/manager

must be added to /etc/inittab.

On startup, the manager removes the subtree /var/manager/burned,
moves any files from /var/manager/toburn1 or /var/manager/toburn2
to /var/manager/toburn, removes /var/manager/toburn1 and
/var/manager/toburn2, and reads configuration information from
the file /var/manager/config.

Unless the disk space available to non-superuser is less than
100M, when the operator presses ENTER, the manager scans one page
and saves it as a new file in the directory /var/manager/new.

The manager compresses files in background and saves the
compressed files in the directory
/var/manager/toburn. Compression is performed by 'gzip -1'
because the CPU is slow. The manager stops compressing files when
the contents of the directory /var/manager/toburn reaches 620M
(640M is the data size limit for most CD-R medias).

When the user requests a CD burn, the manager waits the current
compression process (if any) to finish, forbids new compressions
and calls mkisofs to masterize the filesystem and cdrecord to
burn the media. On completion, it compares the media contents
with the contents of /var/manager/toburn and, unless some
difference is found, the directory /var/manager/toburn is renamed
to /var/manager/burned and deleted subsequently.

Messages are logged to /var/manager/log. This file is rotated by
the manager when it reaches 1M. Three files are maintaned (log,
log.1 and log.2).


Operations
----------

Operations are attached to keys as follows:

     key   |    operation
    -------+-------------------
      1    | burn CD (slow)
      2    | burn CD (fast)
      3    | configure scanner
      4    | report CD-R status
      5    | shutdown
      6    | exit
      7    | play all sounds
      8    | disk usage
    ENTER  | scan
    CANCEL | cancel current op

Notes:

1. The operations 1, 2, 5 and 6 must be confirmed by a subsequent
ENTER.

2. The key CANCEL may be used to cancel a nonconfirmed operation
1, 2, 3, 5 or 6, or a nofinished operation 3.

3. The operation 3 requires entering 4 numbers (each one finished
by ENTER): x size in millimeters, y size in millimeters,
resolution in dots per inch and mode (0=lineart, 1=grayscale,
2=color).

4. The operation 7 is used for training purposes.

5. When burning a CD in fast mode, scanning is forbidden. When
burning a CD in slow mode, scanning is permitted.

6. The operation 8 reports if there exists enough compressed data
to burn a full (640M) CD (sound 1 means "yes", sound 4 means
"no", see the section "Sounds").


Sounds
------

As the scanning station operates without monitor, the operator
knowns about the system status through 4 special sounds ("OK",
"BUSY", "FAIL", "WARN") produced by a very crude interface to the
PC speaker. To hear the sound 1, try the following command on the
console:

    $ echo -e "\33[10;400]\33[11;200]\a" ; usleep 200000 ; \
      echo -e "\33[10;650]\33[11;200]\a" ; usleep 200000 ; \
      echo -e "\33[10;900]\33[11;150]\a" ; usleep 150000

To hear the other sounds, just take a look on how they're defined
at make_beep() and build the corresponding shell command.

1. The sound 1 is used to inform the start or successfull end of a
CD burn operation, and the successfull configuration of the
scanner.

2. The sound 3 is used to inform the failure of a CD burning or
scanner configuration operation.

3. The sound 2 is used to report CD-R busy on operation 4. The
operation 4 reports the status of the last burn operation when
the CD-R is not busy.

4. The sound 4 is a simple beep, used to inform about an invalid
keypress, just like command-line interfaces do.

5. After scanning a page, the sound 1 will be played if the
compressed pages already achieved 640M, so the station is ready
to burn a CD.

6. If the station plays the sound 3 when the operator tries to
scan a page, the disk is full, and scanning is forbidden until a
CD burn operation is performed.

7. If the sound 3 is played for any operation, the manager
detected a fatal error condition. In this case, try to exit the
manager using operation 6. The init program will restart it and
the sound 1 will be played. Try now to use the station. If the
problem persists, try to shutdown the station using the operation
(5). Switch the computer on again. If the problem persists, the
manager is unable to handle the fatal error condition. Attach a
console and examine the file /var/manager/log.


Testing the manager
-------------------

Before trying to test the manager, please read the following
notes:

1. For tests, do not use a CD-R drive, but a CD-RW one. Define
the value of "testing" as "1". The manager will blank the media
before each burn test. Define the speeds FAST and SLOW
accordingly to the drive and to the media. Remember that the
system must be fast enough to support simultaneous scanning and
slow CD-burning. Please make sure that SLOW and FAST are
different speeds.

2. If a serial keyboard is not avaliable for tests, define the
value of "have_kbd" as "0" and use a standard PC keyboard. In
this case, use the numeric keypad keys 0-9, dot (as CANCEL) and
ENTER. ENTER must be pressed after each key, except ENTER.

3. The scanning station manager uses Eric Youngdale's mkisofs and
Joerg Schilling's cdrecord 1.9 to burn CD-Rs. The mkisofs version
bundled with cdrecord-1.9 is used too, because we have had
problems for producing multisession images using mkisofs 1.12b4
(we have not tried newer versions). SANE, gzip and GNU diff are
also required. All binaries are referred through absolute paths
because the manager requires root privileges (so maybe required
to customize these paths, see the section PATHS on the source
code). As we didn't found comments about the return code on the
manpages of scanimage, mkisofs and cdrecord, the checkings for
success or failure for these programs are somewhat indirect.

4. The following directories must be created by hand:

    /var/manager
    /var/manager/new

5. Change the device hardcoded definitions (sane_dev,
cdrecord_dev, kbd_dev) accordingly to your system. Use the
present definitions as examples, and see the SANE or cdrecord
documentation if required. Change the serial parameters and adapt
the keymap accordingly to your keyboard.

6. Define the value of "have_oper" as being 0 to generate
automatically the operator actions "scan a page" or "burn a
CD". Maintain one same document on the scanner and one CD-RW
media loaded. In this case, the station will scan and burn CDs
unattended until you kill the manager. It's a good test.

7. Define the value of "have_scanner" to 0 to simulate scanning
by sleeping 50 seconds and copying the file /var/manager/foopage
to /var/manager/newpage. Use this method if you do not have a
scanner, or if you want to preserve the scanner lamp, or if
you're debugging code unrelated to scanning.

8. If the disk is large enough to store the entire CD image,
define large_disk as 1, otherwise define it as 0. If set, this
flag makes the program use the first half image still on HD when
creating the second half image. Otherwise, the first half image
already stored on the media is used. Depending on your
hardware, this second alternative may not work (the simptom is
mkisofs stalls when trying to create the second half image).

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/vfs.h>

/*

PATHS

*/
#define MAXPATH 128
char *LOGP      = "/var/manager/log",
     *LOGP1     = "/var/manager/log.1",
     *LOGP2     = "/var/manager/log.2",
     *STDOUT    = "/var/manager/cmd.stdout",
     *STDERR    = "/var/manager/cmd.stderr",
     *CONFIG    = "/var/manager/config",
     *NEWCONFIG = "/var/manager/config.new",
     *NEWPAGE   = "/var/manager/newpage",
     /* *NEWPAGE   = "/mnt/data/newpage", */
     *FOOPAGE   = "/var/manager/foopage",
     *NEW       = "/var/manager/new",
     *TOBURN    = "/var/manager/toburn",
     *TOBURN1   = "/var/manager/toburn/1",
     *TOBURN2   = "/var/manager/toburn/2",
     *BURNED    = "/var/manager/burned",
     *IMAGE1    = "/var/manager/1.raw",
     *IMAGE2    = "/var/manager/2.raw",
     *MV        = "/bin/mv",
     *DIFF      = "/usr/bin/diff",
     *RM        = "/bin/rm",
     *CP        = "/bin/cp",
     *CDRECORD  = "/usr/bin/cdrecord",
     *MKISOFS   = "/usr/bin/mkisofs",
     *SCANIMAGE = "/usr/bin/scanimage",
     *DU        = "/usr/bin/du",
     *GZIP      = "/usr/bin/gzip",
     *MNTCD     = "/mnt/cdrom";

/*

Devices.

*/
char *kbd_dev      = "/dev/ttyS3",
     *cdrecord_dev = "dev=1,0,0",
     *sane_dev     = "umax:/dev/sg0" /* "microtek2:/dev/sgb" */;

/*

External processes.

*/
int scan_pid,burn_pid,compress_pid;

/*

Scanner configuration.

*/
int mode;
int x0,y0,xsz,ysz,res;

/*

Current file ID.

*/
int fid;

/*

Various flags and statuses.

*/
int toburn_full=0,
    burn_st=0,
    burn_rc,
    burn_speed,
    burn_fail,
    fatal_failure=0,
    large_disk=1;

/*

Test flags.

*/
int testing      = 0,
    have_kbd     = 0,
    have_oper    = 1,
    have_scanner = 1,
    silently     = 0;

/*

CD-R speed.

*/
int FAST =  4,
    SLOW =  2;

/*

Sounds, current sound and sound state.

*/
#define ALL_SND  0
#define OK_SND   1
#define BUSY_SND 2
#define FAIL_SND 3
#define WARN_SND 4
int sound = 0,
    sound_st,
    sound_rep,
    sound_all,
    term6 = -1;

/*

LOG descriptor.

*/
FILE *LOG=NULL;

/*

Log a message and optionally enter fatal failure state.

*/
void warn(int f,char *m, ...)
{
    va_list args;
    char *ct,*a;
    time_t t;
    int s;
    struct stat fb;

    /* open log file */
    if (LOG == NULL) {
        LOG = fopen(LOGP,"a");
    }

    /* rotate logfile */
    if ((LOG != NULL) && (stat(LOGP,&fb) == 0) && (fb.st_size > 1024*1024)) {
        fclose(LOG);
        rename(LOGP1,LOGP2);
        rename(LOGP,LOGP1);
        LOG = fopen(LOGP,"w");
    }

    /* timestamp */
    t = time(NULL);
    ct = ctime(&t);
    s = strlen(ct);
    a = alloca(strlen(ct));
    strncpy(a,ct,s-1);
    a[s-1] = 0;

    /* log message */
    if (LOG == NULL) {
        va_start(args,m);
        fprintf(stderr,"%s ",a);
        vfprintf(stderr,m,args);
        fprintf(stderr,"\n");
    }
    else {
        va_start(args,m);
        fprintf(LOG,"%s ",a);
        vfprintf(LOG,m,args);
        fprintf(LOG,"\n");
        fflush(LOG);
    }

    /* enter fatal failure state */
    if (f)
        fatal_failure = 1;
}

/*

Some disk procedures used more than once.

*/
void hd(int op)
{
    char path[MAXPATH+1];
    int e1,e2;
    struct stat fb;

    /*
        Move any files in TOBURN1 or TOBURN2 to TOBURN and
        unlinks TOBURN1 and TOBURN2.
    */
    if (op == 1) {

        /* stat both dirs */
        e1 = (stat(TOBURN1,&fb) == 0);
        e2 = (stat(TOBURN2,&fb) == 0);

        /* move */
        if (e1) {
            snprintf(path,MAXPATH,"%s %s/* %s",MV,TOBURN1,TOBURN);
            path[MAXPATH] = 0;
            system(path);
        }
        if (e2) {
            snprintf(path,MAXPATH,"%s %s/* %s",MV,TOBURN2,TOBURN);
            path[MAXPATH] = 0;
            system(path);
        }

        /* unlink dirs */
        if (e1 && (rmdir(TOBURN1) < 0))
            warn(0,"could not remove %s",TOBURN1);
        if (e2 && (rmdir(TOBURN2) < 0))
            warn(0,"could not remove %s",TOBURN2);
    }

    /* remove BURNED */
    else if (op == 2) {
        snprintf(path,MAXPATH,"%s -rf %s",RM,BURNED);
        path[MAXPATH] = 0;
        system(path);
    }

    /* remove IMAGE1 */
    else if (op == 3) {
        e1 = (stat(IMAGE1,&fb) == 0);
        if (e1 && (unlink(IMAGE1) < 0)) {
            warn(1,"could not remove %s",IMAGE1);
        }
    }

    /* remove IMAGE2 */
    else if (op == 4) {
        e2 = (stat(IMAGE2,&fb) == 0);
        if (e2 && (unlink(IMAGE2) < 0)) {
            warn(1,"could not remove %s",IMAGE2);
        }
    }

    /* create TOBURN */
    else if (op == 5) {
        if ((stat(TOBURN,&fb) < 0) && (errno == ENOENT)) {
            if (mkdir(TOBURN,0775) < 0)
                warn(1,"could not create %s",TOBURN);
        }
    }
}

/*

Beep on frequency f by t milliseconds. Returns t*1000.

*/
int mybeep(int f,int t)
{
    unsigned char msg[49];

    if (!silently) {
        snprintf(msg,48,"%c[10;%d]%c[11;%d]\a",27,f,27,t);
        msg[48] = 0;
        write(term6,msg,strlen(msg));
    }
    return(t*1000);
}

/*

Produce some nice beeps.

*/
int make_beep(void)
{
    int nt=0;

    if (sound == 0)
        return(0);

    /* sound "OK" */
    if (sound == 1) {

        if (sound_st == 1) {
            nt = mybeep(400,200);
            ++sound_st;
        }

        else if (sound_st == 2) {
            nt = mybeep(650,200);
            ++sound_st;
        }

        else if (sound_st == 3) {
            nt = mybeep(900,150);
            sound_st = 0;
        }
    }

    /* sound "BUSY" */
    else if (sound == 2) {

        if (sound_st == 1) {
            nt = 2 * mybeep(450,300);
            ++sound_st;
        }

        else {
            sound_st = 0;
            nt = 0;
        }
    }

    /* sound "FAILURE" */
    else if (sound == 3) {

        if (sound_st <= 2) {

            if (sound_st & 1) {
                nt = mybeep(900,500);
            }

            else {
                nt = mybeep(600,800) + 300000;
            }

            ++sound_st;
        }

        else {
            sound_st = 0;
            nt = 0;
        }
    }

    /* normal warn "BEEP" */
    else if (sound == 4) {
        if (sound_st == 1) {
            nt = mybeep(1000,200);
            sound_st = 0;
        }
    }

    /* sound "EMERGENCY" (bad results) */
    else if (sound == 5) {

        if (sound_st < 80) {
            nt = mybeep((2-1/(((float)sound_st))/2+0.5)*400,15);
            nt -= 5000;
            ++sound_st;
        }

        else {
            sound_st = 0;
        }
    }

    return(nt);
}

/*

Initialize sound_rep.

*/
void init_sound_rep(void)
{
    if (sound == 1)
        sound_rep = 1;
    else if (sound == 2)
        sound_rep = 4;
    else if (sound == 3)
        sound_rep = 3;
    else if (sound == 4)
        sound_rep = 1;
    else
        sound_rep = 1;
}

/*

SIGALRM handler.

*/
void handle_alrm(int p)
{
    struct itimerval it;
    int nt;

    /* beep accordingly to current sound and step */
    nt = make_beep();

    /* prepare next repetition (if any) */
    if ((sound_st <= 0) && (--sound_rep > 0)) {
        sound_st = 1;
        nt += 200000;
    }

    /* if sound_all is set, prepare next sound to play */
    if ((sound_st <= 0) && (sound_all) && (sound < 4)) {
        ++sound;
        sound_st = 1;
        init_sound_rep();
        nt += 500000;
    }

    /* set next alarm accordingly to make_beep answer */
    if (sound_st > 0) {
        it.it_interval.tv_sec = 0;
        it.it_interval.tv_usec = 0;
        it.it_value.tv_sec = nt / 1000000;
        it.it_value.tv_usec = nt % 1000000;
        setitimer(ITIMER_REAL,&it,NULL);
    }
}

/*

Produce the specified sound.

*/
void make_sound(int id)
{
    /* play all sounds */
    if (id == ALL_SND) {
        sound = 1;
        sound_all = 1;
    }

    /* play only the specified sound */
    else {
        sound = id;
        sound_all = 0;
    }

    /* prepare and start the handler */
    sound_st = 1;
    init_sound_rep();
    handle_alrm(0);
}

/*

Free manager resources before exec.

*/
void prepare_child(void)
{
    /* close descriptors */
    if (LOG != NULL)
        fclose(LOG);
    if (term6 >= 0)
        close(term6);
}

/*

Fork and exec. This service is to be used by burn_cd only because
it always use the same fixed paths for capturing stdout and stderr.

*/
int fork_exec(const char *path, char *const argv[])
{
    int pid,F;

    /* fork failure */
    if ((pid = fork()) < 0) {
        warn(1,"fork failure");
    }

    /* this is the child */
    else if (pid == 0) {

        /* free resources */
        prepare_child();

        /* capture stdout to STDOUT */
        close(1);
        F = open(STDOUT,O_WRONLY|O_CREAT|O_TRUNC,0660);
        if ((F < 0) || ((F != 1) && (dup2(F,1) < 0))) {

            /* return nonzero status to the manager */
            exit(255);
        }
        if ((F != 1) && (close(F) < 0))
            warn(0,"error closing auxiliar descriptor");

        /* capture stderr to STDERR */
        close(2);
        F = open(STDERR,O_WRONLY|O_CREAT|O_TRUNC,0660);
        if ((F < 0) || ((F != 2) && (dup2(F,2) < 0))) {

            /* return nonzero status to the manager */
            exit(255);
        }
        if ((F != 2) && (close(F) < 0))
            warn(0,"error closing auxiliar descriptor");

        /* exec */
        execv(path,argv);

        /* return nonzero status to the manager */
        exit(255);
    }

    /* this is the manager */
    return(pid);
}

/*

Search error message on command output.

*/
int search_error(char *msg)
{
    FILE *F;
    char l[4097];
    int i,s,t;

    /* message lenght */
    s = strlen(msg);

    /* search on stdout */
    if ((F = fopen(STDOUT,"r")) != NULL) {
        while (fgets(l,4096,F) != NULL) {
            t = strlen(l) - s;
            for (i=0; i<t; ++i) {
                if (strncmp(msg,l+i,t) == 0) {
                    fclose(F);
                    return(1);
                }
            }
        }
    }
    fclose(F);

    /* search on stderr */
    if ((F = fopen(STDERR,"r")) != NULL) {
        while (fgets(l,4096,F) != NULL) {
            t = strlen(l) - s;
            for (i=0; i<t; ++i) {
                if (strncmp(msg,l+i,t) == 0) {
                    fclose(F);
                    return(1);
                }
            }
        }
    }
    fclose(F);

    /* didn't find */
    return(0);
}

/*

Consist scanning parameters.

*/
int consist_sp(int x0,int y0,int xsz,int ysz,int res,int fid,int mode)
{
    if ((x0 < 0) || (230 < x0) ||
        (y0 < 0) || (350 < y0) ||
        (xsz < 0) || (230 < (x0+xsz)) ||
        (ysz < 0) || (350 < (y0+ysz)) ||
        (res < 0) || (600 < res) ||
        (fid < 0) || (1000000 < res) ||
        (mode < 0) || (2 < mode)) {

        return(0);
    }
    return(1);
}

/*

Save current configuration.

*/
void write_conf(void)
{
    FILE *F;

    /* open temporary file for writing */
    F = fopen(NEWCONFIG,"w");
    if (F == NULL)
        warn(1,"could not open %s",NEWCONFIG);

    /* dump parameters and rename the temporary file */
    else {

        fprintf(F,"x0=%d\n",x0);
        fprintf(F,"y0=%d\n",y0);
        fprintf(F,"xsz=%d\n",xsz);
        fprintf(F,"ysz=%d\n",ysz);
        fprintf(F,"res=%d\n",res);
        fprintf(F,"fid=%d\n",fid);
        fprintf(F,"mode=%d\n",mode);
        fclose(F);

        if (rename(NEWCONFIG,CONFIG) < 0)
            warn(1,"could not rename %s",NEWCONFIG);
    }
}

/*

Read current configuration.

*/
void read_conf(void)
{
    FILE *F;
    char s[256];
    struct stat fb;

    /* does the config file exists? */
    if ((stat(CONFIG,&fb)<0) && (errno==ENOENT)) {

        /* default values */
        x0 = 0;
        y0 = 0;
        xsz = 165;
        ysz = 215;
        res = 600;
        fid = 0;
        mode = 1;

        /* create the file */
        write_conf();
    }

    /* open config file for reading and check for failure */
    F = fopen(CONFIG,"r");
    if (F == NULL)
        warn(1,"could not open config file");

    /* force invalid values */
    xsz = ysz = res = fid = mode = -1;

    /* read the entire file */
    while (fgets(s,256,F) != NULL) {

        /* strange line... */
        if (strlen(s) >= 255)
            warn(1,"line too long in config file");

        /* read next parameter */
        if (strncmp(s,"x0=",3) == 0)
            x0 = atoi(s+3);
        else if (strncmp(s,"y0=",3) == 0)
            y0 = atoi(s+3);
        else if (strncmp(s,"xsz=",4) == 0)
            xsz = atoi(s+4);
        else if (strncmp(s,"ysz=",4) == 0)
            ysz = atoi(s+4);
        else if (strncmp(s,"res=",4) == 0)
            res = atoi(s+4);
        else if (strncmp(s,"fid=",4) == 0)
            fid = atoi(s+4);
        else if (strncmp(s,"mode=",5) == 0)
            mode = atoi(s+5);
        else if (s[0] != '\n')
            warn(1,"invalid field in config file");
    }

    /* finished reading */
    fclose(F);

    /* test against some reasonable limits */
    if (consist_sp(x0,y0,xsz,ysz,res,fid,mode) == 0) {
        warn(1,"invalid field in config file");
    }
}

/*

Configure scanner.

*/
void scan_conf(int c_xsz,int c_ysz,int c_res,int c_mode)
{
    /* configure */
    if (consist_sp(x0,y0,c_xsz,c_ysz,c_res,fid,c_mode) == 1) {
        xsz = c_xsz;
        ysz = c_ysz;
        res = c_res;
        mode = c_mode;
        write_conf();
        make_sound(OK_SND);
    }

    /* beep */
    else {
        make_sound(FAIL_SND);
    }
}

/*

Obtain exit status for some process.

*/
void my_wait(void)
{
    int s,p;
    char to[MAXPATH+1];

    /* nothing to wait */
    if ((scan_pid < 0) && (burn_pid < 0) && (compress_pid < 0))
        return;

    /* someone exited? */
    p = waitpid(-1,&s,WNOHANG);

    /* nobody exited */
    if (p == 0)
        return;

    /* wait error */
    else if (p < 0) {
        warn(0,"wait error caught");
    }

    /* yes, someone exited */
    else {

        /* finished scanning */
        if (p == scan_pid) {
            struct stat fb;
            int size;

            warn(0,"scanning finished");
            if (toburn_full)
                make_sound(OK_SND);

            /*
                Minimum expected file size. As we do not know how
                scanimage truncates/rounds the resolution in pixels,
                we apply a 0.9 factor.
            */
            size = 0.9 * xsz * (res/25.4) * ysz * (res/25.4);
            if (mode == 0)
                size /= 8;
            else if (mode == 2)
                size *= 3;

            /* cannot stat, assume that scanimage failed and continue */
            if (stat(NEWPAGE,&fb) < 0) {
                warn(0,"scanimage failed (cannot stat file)");
            }

            /* explicit scanning failure or file too small */
            if ((WIFEXITED(s) == 0) || (fb.st_size < size)) {

                if (fb.st_size < size)
                    warn(0,"scanimage failed (file too small)");
                else
                    warn(0,"scanimage failed (as reported)");

                /* unlink */
                if (unlink(NEWPAGE) < 0)
                    warn(1,"could not unlink %s",NEWPAGE);
            }

            /* move the just acquired page to NEW */
            else {

                /* save new file id */
                ++fid;
                write_conf();

                /* move */
                snprintf(to,MAXPATH-4,"%s/%d",NEW,fid);
                to[MAXPATH-4] = 0;
                if (mode == 0)
                    strcat(to,".pbm");
                else if (mode == 1)
                    strcat(to,".pgm");
                else
                    strcat(to,".pnm");
                if (rename(NEWPAGE,to) < 0)
                    warn(1,"failed to move %s",NEWPAGE);
            }

            /* no scanning process anymore */
            scan_pid = -1;
        }

        /* finished some burning step */
        else if (p == burn_pid) {
            burn_pid = -1;
            if (WIFEXITED(s)) {
                 burn_rc = WEXITSTATUS(s);
            }
        }

        /* finished compressing */
        else if (p == compress_pid) {
            warn(0,"compress finished");
            compress_pid = -1;
        }
    }
}

/*

Scan a page.

*/
int scan_page(void)
{
    int F;
    char xsz_str[21],ysz_str[21],res_str[21],*mode_str;
    int pid;

    /* fork failure */
    if ((pid = fork()) < 0) {
        warn(1,"fork failure");
    }

    /* this is the child */
    else if (pid == 0) {

        /* free resources */
        prepare_child();

        /* open file to dump stdout */
        close(1);
        F = open(NEWPAGE,O_WRONLY|O_CREAT|O_TRUNC,0600);
        if (F < 0)
            warn(1,"could not open %s",NEWPAGE);

        /* redirect stdout */
        if (F != 1) {
            if (dup2(F,1) < 0) {

                /* return nonzero status to the manager */
                exit(255);
            }
        }

        /* prepare parameters */
        snprintf(xsz_str,20,"%d",xsz);
        snprintf(ysz_str,20,"%d",ysz);
        snprintf(res_str,20,"%d",res);
        xsz_str[20] = 0;
        ysz_str[20] = 0;
        res_str[20] = 0;
        if (mode == 0)
            mode_str = "lineart";
        else if (mode == 1)
            mode_str = "gray";
        else
            mode_str = "color";

        /* scan */
        if (have_scanner) {
            execl(SCANIMAGE,"scanimage",
                  "-d",sane_dev,
                  "--mode",mode_str,
                  "-x",xsz_str,
                  "-y",ysz_str,
                  "--resolution",res_str,
                  NULL);
        }
        else {
            sleep(50);
            execl(CP,"cp",FOOPAGE,NEWPAGE,NULL);
        }

        /* this should not happen */
        exit(255);
    }

    /* this is the manager */
    else
        scan_pid = pid;

    /* successfully started the scan process */
    return(1);
}

/*

Try to move one page to the directory TOBURN.

*/
void move_toburn(void)
{
    DIR *D;
    FILE *F;
    int t,du;
    struct dirent *e;
    char *a,f[MAXPATH+1],h[MAXPATH+1],r[256],path[MAXPATH+1];
    struct stat fb;

    /*
        While compressing, we forbid moving a .gz file because maybe
        gzip is creating it just now.
    */
    if (compress_pid >= 0)
        return;

    /*
        Get first filename (if any). This strategy is OK
        if all files have similar size.
    */
    if ((D = opendir(NEW)) == NULL) {
        warn(1,"could not open directory %s",NEW);
    }
    for (f[0]=0; (f[0]==0) && ((e=readdir(D)) != NULL); ) {

        /* if the spec match, select it */
        a = e->d_name;
        t = strlen(a);
        if ((t >= 8) &&
            ((strcmp(a+t-7,".pbm.gz")==0) ||
             (strcmp(a+t-7,".pgm.gz")==0) ||
             (strcmp(a+t-7,".pnm.gz")==0))) {

            strcpy(f,NEW);
            strcat(f,"/");
            strcpy(h,TOBURN);
            strcat(h,"/");
            if (((strlen(f)+t) <= MAXPATH) && ((strlen(h)+t) <= MAXPATH)) {
                strcat(f,a);
                strcat(h,a);
            }
            else
                f[0] = 0;
        }
    }
    closedir(D);

    /* no file to move */
    if (f[0] == 0)
        return;

    /* create TOBURN */
    hd(5);

    /* get disk usage at TOBURN */
    snprintf(path,MAXPATH,"%s -s -b %s",DU,TOBURN);
    path[MAXPATH] = 0;
    F = popen(path,"r");
    fgets(r,255,F);
    pclose(F);
    du = atoi(r);

    /* add file size */
    if (stat(f,&fb) < 0)
        warn(1,"cannot stat file");
    du += fb.st_size;

    /* too much */
    if (du > (625*1024*1024)) {
        toburn_full = 1;
        warn(0,"toburn is full");
        make_sound(OK_SND);
        return;
    }

    warn(0,"moving %s to %s",f,h);

    /* move */
    if (rename(f,h) < 0) {
        warn(1,"could not move to %s",TOBURN);
    }
}

/*

Try to compress a page.

*/
void start_compress(void)
{
    DIR *D;
    int t,pid;
    struct dirent *e;
    char *a,f[MAXPATH+1];

    /* already compressing */
    if (compress_pid >= 0)
        return;

    /* get first filename (if any) */
    if ((D = opendir(NEW)) == NULL) {
        warn(1,"could not open directory %s",NEW);
    }
    for (f[0]=0; (f[0]==0) && ((e=readdir(D)) != NULL); ) {

        /* if the spec match, select it */
        a = e->d_name;
        t = strlen(a);
        if ((t >= 5) &&
            ((strcmp(a+t-4,".pbm")==0) ||
             (strcmp(a+t-4,".pgm")==0) ||
             (strcmp(a+t-4,".pnm")==0))) {

            snprintf(f,MAXPATH,"%s/",NEW);
            f[MAXPATH] = 0;
            if ((strlen(f)+t) <= MAXPATH)
                strcat(f,a);
            else
                f[0] = 0;
        }
    }
    closedir(D);

    /* no file to compress */
    if (f[0] == 0)
        return;

    /* fork failure */
    if ((pid = fork()) < 0) {
        warn(1,"fork failure");
    }

    /* this is the child */
    else if (pid == 0) {

        /* free resources */
        prepare_child();

        warn(0,"going to compress %s",f);

        /* compress */
        execl(GZIP,"gzip","-1",f,NULL);

        /* this should not happen */
        exit(255);
    }

    /* this is the manager */
    else
        compress_pid = pid;
}

/*

Burn a CD.

The images are created on the IDE disk mounted at /mnt/data
because the SCSI bus is busy with the UMAX scanner. As the IDE
disk has only 540M, we burn a multisession CD.

*/
void burn_cd(void)
{
    char path[MAXPATH+1];
    char *argv[20],speed[17];
    static int sz1,sz2,mnt;
    struct stat fb;

    /* must wait */
    if (burn_pid >= 0)
        return;

    /* sentinel */
    path[MAXPATH] = 0;

    /* burning speed */
    snprintf(speed,16,"speed=%d",burn_speed);
    speed[16] = 0;

    /*
        STEP 1
        ------

        blank media (for tests using CD-RW)
    */
    if (burn_st == 1) {

        make_sound(OK_SND);
        warn(0,"blanking the media");

        if (testing) {
            strncpy(path,CDRECORD,MAXPATH);
            argv[0] = "cdrecord";
            argv[1] = "-blank=fast";
            argv[2] = speed;
            argv[3] = cdrecord_dev;
            argv[4] = NULL;
            burn_pid = fork_exec(path,argv);
        }

        ++burn_st;
    }

    /*
        STEP 2
        ------

        divide toburn in two parts
    */
    else if (burn_st == 2) {
        DIR *D;
        int t,*sz;
        struct dirent *e;
        char *a,f[MAXPATH+1],h[MAXPATH+1];

        /* reset statuses */
        mnt = 0;
        burn_fail = 0;

        /* create subdirectories "1" and "2" */
        strncpy(f,TOBURN1,MAXPATH);
        f[MAXPATH] = 0;
        if ((mkdir(f,0755) < 0) && (errno != EEXIST))
            warn(1,"could not create %s",f);
        strncpy(f,TOBURN2,MAXPATH);
        f[MAXPATH] = 0;
        if ((mkdir(f,0755) < 0) && (errno != EEXIST))
            warn(1,"could not create %s",f);

        /* divide loop */
        if ((D = opendir(TOBURN)) == NULL) {
            warn(1,"could not open directory %s",TOBURN);
        }
        for (sz1=sz2=0; (e=readdir(D)) != NULL; ) {

            /* if the filename spec matches */
            a = e->d_name;

            t = strlen(a);
            if ((t >= 8) &&
                ((strcmp(a+t-7,".pbm.gz")==0) ||
                 (strcmp(a+t-7,".pgm.gz")==0) ||
                 (strcmp(a+t-7,".pnm.gz")==0))) {

                /* choose directory 1 or 2 */
                if (sz1 <= sz2) {
                    snprintf(h,MAXPATH,"%s/",TOBURN1);
                    h[MAXPATH] = 0;
                    sz = &sz1;
                }
                else {
                    snprintf(h,MAXPATH,"%s/",TOBURN2);
                    h[MAXPATH] = 0;
                    sz = &sz2;
                }

                /* build full paths */
                snprintf(f,MAXPATH,"%s/",TOBURN);
                f[MAXPATH] = 0;
                if (((strlen(f)+t) <= MAXPATH) && ((strlen(h)+t) <= MAXPATH)) {
                    strcat(f,a);
                    strcat(h,a);
                }
                else {
                    warn(1,"strange filename");
                }

                /* add file size */
                if (stat(f,&fb) < 0)
                    warn(1,"cannot stat file");
                *sz += fb.st_size;

                /* move */
                if (rename(f,h) < 0) {
                    warn(1,"could not move file while dividing");
                }
            }
        }
        closedir(D);

        /* nothing to burn */
        if ((sz1 <= 0) && (sz2 <= 0)) {
            warn(0,"nothing to burn");
            burn_fail = 1;
        }

        ++burn_st;
    }

    /*
        STEP 3
        ------

        masterize the first part
    */
    else if (burn_st == 3) {

        if (sz1 > 0) {

            warn(0,"ready to create the first image");

            strncpy(path,MKISOFS,MAXPATH);
            argv[0] = "mkisofs";
            argv[1] = "-r";
            argv[2] = "-J";
            argv[3] = "-o";
            argv[4] = IMAGE1;
            argv[5] = TOBURN1;
            argv[6] = NULL;
            burn_pid = fork_exec(path,argv);
        }

        ++burn_st;
    }

    /*
        STEP 4
        ------

        burn the first part
    */
    else if (burn_st == 4) {
        int ie;

        /* image exists and has reasonable size? */
        if ((sz1 > 0) &&
            (((ie=stat(IMAGE1,&fb)) < 0) || (fb.st_size < sz1))) {

            if (ie == 0)
                warn(0,"image 1.raw not ok, waiting >%d found %d",sz1,fb.st_size);
            else
                warn(0,"didn't find image 1.raw");
            burn_fail = 1;
        }

        /* burn it */
        if ((!burn_fail) && (sz1 > 0)) {

            warn(0,"going to burn the first image");

            strncpy(path,CDRECORD,MAXPATH);
            argv[0] = "cdrecord";
            argv[1] = "-multi";
            argv[2] = speed;
            argv[3] = cdrecord_dev;
            argv[4] = IMAGE1;
            argv[5] = NULL;
            burn_pid = fork_exec(path,argv);
        }
        ++burn_st;
    }

    /*
        STEP 5
        ------

         masterize the second part
    */
    else if (burn_st == 5) {
        FILE *F;
        char r[256];
        int t,retries;
        int c1,c2,c3;

        /* remove the first image unless the disk is large */
        if (!large_disk)
            hd(3);

        /* search error message */
        if (search_error("I/O error") ||
            search_error("Input/output error") ||
            search_error("Medium Error")) {

            burn_fail = 1;
        }

        if ((!burn_fail) && (sz2 > 0)) {

            /*
                Get the session start numbers. As the drive sometimes is not
                ready at this point, we try three times.
            */
            for (retries=3, r[0]=0; (r[0]==0) && (retries>0); --retries) {

                warn(0,"will run cdrecord -msinfo");

                /* run cdrecord -msinfo */
                snprintf(path,MAXPATH,"%s -msinfo %s",CDRECORD,cdrecord_dev);
                path[MAXPATH] = 0;
                F = popen(path,"r");
                fgets(r,255,F);
                pclose(F);
                r[255] = 0;

                /* remove LF */
                t = strlen(r) - 1;
                while ((t>=0) && ((r[t]== '\r') || (r[t]== '\n')))
                    r[t--] = 0;

                warn(0,"cdrecord answer is %s",r);

                /* count digits and commas */
                for (c3=0; ((t>=0) && (('0'<=r[t]) && (r[t]<='9'))); ++c3, --t);
                for (c2=0; ((t>=0) && (r[t]==',')); ++c2, --t);
                for (c1=0; ((t>=0) && (('0'<=r[t]) && (r[t]<='9'))); ++c1, --t);

                /* failure */
                if ((t >= 0) || (c1 <= 0) || (c2 <= 0) || (c3 <= 0)) {

                    warn(0,"failure: t=%d c1=%d c2=%d c3=%d",t,c1,c2,c3);
                    sleep(3);
                    r[0] = 0;
                }
            }

            /* failed, ignore the second part */
            if (r[0] == 0)
                burn_fail = 1;

            /* create the second image */
            if ((!burn_fail) && (sz2 > 0)) {
                char *cddev;

                /* skip the dev= or -dev= portion of cdrecord_dev */
                for (cddev=cdrecord_dev; (*cddev!='=') && (*cddev!=0); ++cddev);
                if (*cddev == '=')
                    ++cddev;
                else
                    warn(1,"invalid cdrecord device %s",cdrecord_dev);

                warn(0,"ready to create the second image");

                strncpy(path,MKISOFS,MAXPATH);
                argv[0] = "mkisofs";
                argv[1] = "-r";
                argv[2] = "-J";
                argv[3] = "-C";
                argv[4] = r;
                argv[5] = "-M";
                argv[6] = large_disk ? IMAGE1 : cddev;
                argv[7] = "-o";
                argv[8] = IMAGE2;
                argv[9] = TOBURN2;
                argv[10] = NULL;
                burn_pid = fork_exec(path,argv);
            }
        }

        ++burn_st;
    }

    /*
        STEP 6
        ------

        burn the second part
    */
    else if (burn_st == 6) {
        int ie;

        /* image exists and has reasonable size? */
        if ((sz2 > 0) &&
            (((ie=stat(IMAGE2,&fb)) < 0) || (fb.st_size < sz2))) {

            if (ie == 0)
                warn(0,"image 2.raw not ok, waiting >%d found %d",sz2,fb.st_size);
            else
                warn(0,"didn't find image 2.raw");
            burn_fail = 1;
        }

        /* burn it */
        if ((!burn_fail) && (sz2 > 0)) {

            warn(0,"going to burn the second image");

            strncpy(path,CDRECORD,MAXPATH);
            argv[0] = "cdrecord";
            argv[1] = speed;
            argv[2] = cdrecord_dev;
            argv[3] = "-data";
            argv[4] = "-pad";
            argv[5] = IMAGE2;
            argv[6] = NULL;
            burn_pid = fork_exec(path,argv);
        }
        ++burn_st;
    }

    /*
        STEP 7
        ------

        compare
    */
    else if (burn_st == 7) {

        /* remove the first image, if it's still there */
        if (large_disk)
            hd(3);

        /* remove the second image */
        hd(4);

        /* search error message */
        if (search_error("I/O error") ||
            search_error("Input/output error") ||
            search_error("Medium Error")) {

            burn_fail = 1;
        }

        /* move back and unlink dirs */
        hd(1);

        /* mount the media run comparing procedure */
        if ((!burn_fail) && ((sz1 > 0) || (sz2 > 0))) {

            warn(0,"now will mount the media and compare");

            /* mount CD */
            system("/bin/mount -t iso9660 /dev/scd0 /mnt/cdrom");
            mnt = 1;

            /* compare */
            burn_rc = -1;
            strncpy(path,DIFF,MAXPATH);
            path[MAXPATH] = 1;
            /*
            argv[0] = "diff";
            argv[1] = TOBURN;
            argv[2] = MNTCD;
            argv[3] = NULL;
            */
            argv[0] = "sh";
            argv[1] = "-c";
            argv[2] = "/bin/gunzip -t /mnt/cdrom/*";
            argv[3] = NULL;
            burn_pid = fork_exec(path,argv);
        }

        ++burn_st;
    }

    /*
        STEP 8
        ------

        clear TOBURN and reset
    */
    else if (burn_st == 8) {

        /* umount CD */
        if (mnt) {
            warn(0,"umounting the media");
            system("/bin/umount /mnt/cdrom");
            mnt = 0;
        }

        if ((sz1 > 0) || (sz2 > 0)) {

            /* uh! */
            if ((burn_fail) || (burn_rc != 0)) {
                if (burn_fail) {
                    warn(0,"CD is not OK (early failure)");
                }
                else {
                    warn(0,"CD is not OK (code %d)",burn_rc);
                    burn_fail = 1;
                }
            }

            /* remove pages */
            else {
                warn(0,"CD is OK (code %d)",burn_rc);
                warn(0,"removing scanned pages");
                if (rename(TOBURN,BURNED) < 0) {
                    warn(1,"could not rename %s to %s",TOBURN,BURNED);
                }
                hd(2);
                toburn_full = 0;
            }
        }

        /* reset and beep */
        burn_st = 0;
        if (burn_fail == 0) {
            warn(0,"finished burning procedure (success)");
            make_sound(OK_SND);
        }
        else {
            warn(0,"finished burning procedure (failure, status is %d)",burn_fail);
            make_sound(FAIL_SND);
        }
    }
}

/*

Generate key

*/
int genkey(char *kbuf)
{
    int r=0;
    char lf;
    static int scan_at=-1,burn_at=-1,next=-1;
    time_t t;

    /* the operator decides using the keyboard */
    if (have_oper) {
        r = read(0,kbuf,1);
        if ((r > 0) && (*kbuf != '\n'))
            read(0,&lf,1);
        if (r > 0) {
            switch (*kbuf) {
                case '1': *kbuf = 'b' ; break;
                case '2': *kbuf = 'R' ; break;
                case '3': *kbuf = 'J' ; break;
                case '4': *kbuf = 'a' ; break;
                case '5': *kbuf = 'Q' ; break;
                case '6': *kbuf = 'I' ; break;
                case '7': *kbuf = 'd' ; break;
                case '8': *kbuf = 'T' ; break;
                case '9': *kbuf = 'L' ; break;
                case '.': *kbuf = '`' ; break;
                case '0': *kbuf = 'P' ; break;
                case '\n': *kbuf = 'H'  ; break;
                default: *kbuf = 0;
            }
        }
    }

    /* generate automatic actions (for tests) */
    else {

        /* bufferized key */
        if (next >= 0) {
            *kbuf = next;
            r = 1;
            next = -1;
        }

        else {

            t = time(NULL);

            /* schedule or generate scan command */
            if (scan_pid < 0) {
                if (scan_at < 0) {
                    scan_at = t + 5 + (rand() % 10);
                }
                else if (t >= scan_at) {
                    r = 1;
                    *kbuf = 'H';
                    scan_at = -1;
                }
            }

            /* schedule or generate burn CD command */
            if ((r==0) && (toburn_full) && (burn_st <= 0)) {
                if (burn_at < 0) {
                    burn_at = t + 10 + (rand() % 20);
                }
                else if (t >= burn_at) {
                    r = 1;
                    *kbuf = 'b';
                    next = 'H';
                    burn_at = -1;
                }
            }
        }
    }

    return(r);
}

/*

The program begins here.

*/
int main(int argc,char *argv[])
{
    /* key and number input buffers */
    char kbuf,nbuf[49];
    int m,n,ni,np;

    /* serial keyboard map */
    char map[256];

    /* serial I/O */
    struct termios oldtio,tio;
    int fd,r;

    /* user interface: operation and state in course */
    int op,st;

    /* candidate parameters for configuring the scanner */
    int c_xsz,c_ysz,c_res,c_mode;

    /* counters and flags */
    int i,f,l;

    /* random seed */
    srand(0);

    /* open keyboard */
    if (have_kbd) {
        fd = open(kbd_dev,O_RDONLY|O_NOCTTY|O_NONBLOCK);
        if (fd <0) {
            warn(1,"coud not find keyboard");
        }
    }
    else {
        fd = 0;
        fcntl(0,F_SETFL,O_NONBLOCK);
    }

    /* open terminal 6 (for producing beeps) */
    term6 = open("/dev/tty1",O_WRONLY);

    /* install handler */
    signal(SIGALRM,handle_alrm);

    /* various defaults */
    scan_pid = -1;
    burn_pid = -1;
    compress_pid = -1;

    /* read configuration */
    read_conf();

    /* save current port settings */
    if (have_kbd)
        tcgetattr(fd,&oldtio);

    /*

        Create the keyboard mapping accordingly to the following
        table:

            +----------------------+
            |     |      CODE      |
            | KEY |----------------|
            |     |ASC DEC    BIN  |
            +----------------------+
            |  1  | b   98  1100010|
            |  2  | R   82  1010010|
            |  3  | J   74  1001010|
            |  4  | a   97  1100001|
            |  5  | Q   81  1010001|
            |  6  | I   73  1001001|
            |  7  | d  100  1100100|
            |  8  | T   84  1010100|
            |  9  | L   76  1001100|
            |ANULA| `   96  1100000|
            |  0  | P   80  1010000|
            |ENTRA| H   72  1001000|
            +----------------------+

    */
    for (i=0; i<256; ++i)
        map[i] = 0;
    map['b'] = '1';
    map['R'] = '2';
    map['J'] = '3';
    map['a'] = '4';
    map['Q'] = '5';
    map['I'] = '6';
    map['d'] = '7';
    map['T'] = '8';
    map['L'] = '9';
    map['`'] = 'A';
    map['P'] = '0';
    map['H'] = 'E';

    if (have_kbd) {

        /* configure */
        bzero(&tio,sizeof(tio));
        tio.c_cflag = B1200 | CS7 | CLOCAL | CREAD;
        tio.c_iflag = IGNPAR;
        tio.c_oflag = 0;
        tio.c_lflag = 0;
        tio.c_cc[VTIME] = 0;

        /* number of characters to read */
        tio.c_cc[VMIN] = 1;

        /* prepare */
        tcflush(fd,TCIFLUSH);
        tcsetattr(fd,TCSANOW,&tio);
    }

    /* just to avoid a compilation warning */
    np = 0;
    st = 0;
    c_xsz = 0;
    c_ysz = 0;
    c_res = 0;

    /* tell the operator that we're alive */
    make_sound(OK_SND);

    /* prepare directories */
    hd(1);
    hd(2);
    hd(3);
    hd(4);

    /*
        Main loop for reading the keyboard and activating the
        various handlers.
    */
    for (f=ni=op=l=0; f==0; ) {

        /* try to read one keypress */
        if (have_kbd)
            r = read(fd,&kbuf,1);
        else
            r = genkey(&kbuf);

        /* fatal failure: just beep */
        if ((r > 0) && (fatal_failure)) {

            m = map[(int)kbuf];
            if (((ni == 0) && (m == 5)) ||
                ((ni == 0) && (m == 5)) ||
                ((ni == 2) && (op == 2)) ||
                ((ni == 2) && (op == 5))) {

                /* down or exit in course: process key */
            }

            /* ignore anything else */
            else {
                make_sound(FAIL_SND);
                r = 0;
            }
        }

        /* got one! */
        if (r > 0) {

            /* print the translated character (for tests) */
            warn(0,"read %c from keyboard",map[*((unsigned char *)&kbuf)]);

            /* map */
            m = map[(int)kbuf];

            /* cancel ongoing input (if any) */
            if (m == 'A') {
                ni = 0;
                make_sound(OK_SND);
            }

            /*
                This block processes ongoing operations waiting a
                simple (ENTER) confirmation
            */
            else if (ni == 2) {

                /* not confirmed: reset */
                if (m != 'E') {
                    make_sound(WARN_SND);
                }

                /* shutdown confirmed */
                else if (op == 2) {
                    warn(0,"going down");
                    make_sound(OK_SND);
                    execl("/sbin/shutdown","-h","now",NULL);
                }

                /* burn CD (slow) confirmed */
                else if (op == 3) {
                    burn_speed = SLOW;
                    burn_st = 1;
                }

                /* burn CD (fast) confirmed */
                else if (op == 4) {
                    burn_speed = FAST;
                    burn_st = 1;
                }

                /* exit confirmed */
                else if (op == 5) {
                    f = 1;
                }

                ni = 0;
            }

            /*
                This block processes operations waiting
                numeric input
            */
            else if (ni == 1) {

                /*
                    Numeric input finished. Convert and discover what to
                    do with the number.
                */
                if (m == 'E') {
                    nbuf[np] = 0;
                    n = atoi(nbuf);

                    /* use it as a parameter for scanner configuration */
                    if (op == 1) {

                        /* use if as xsz and continue reading */
                        if (st == 1) {
                            c_xsz = n;
                            st = 2;
                        }

                        /* use if as ysz and continue reading */
                        else if (st == 2) {
                            c_ysz = n;
                            st = 3;
                        }

                        /* use if as resolution and continue reading */
                        else if (st == 3) {
                            c_res = n;
                            st = 4;
                        }

                        /* use if as mode and finish */
                        else if (st == 4) {
                            c_mode = n;
                            scan_conf(c_xsz,c_ysz,c_res,c_mode);
                            ni = 0;
                        }
                    }
                }

                /* concatenate digit and continue reading */
                else if (np <= 48)
                    nbuf[np++] = m;
            }

            /* configure scanner */
            else if (m == '3') {

                /* busy: ignore */
                if ((scan_pid >= 0) || (burn_st >= 0) || (compress_pid >= 0)) {
                    make_sound(BUSY_SND);
                }

                /* enter configuration state */
                else {
                    ni = 1;
                    np = 0;
                    op = 1;
                    st = 1;
                }
            }

            /* start scanning */
            else if (m == 'E') {
                struct statfs fbfs;

                /* forbid if already scanning or burning CD in fast mode */
                if ((scan_pid < 0) && ((burn_speed==SLOW) || (burn_st==0))) {
                    int diskfull = 0;

                    /*
                        Forbid if <100M free. Actually, forbid if
                        <450M or <800M free because we must reserve
                        space to the CD image. Your mileage may vary,
                        depending on how the disk is partitioned.
                    */
                    if (statfs(NEW,&fbfs) < 0)
                        warn(1,"cannot statfs root");
                    if (large_disk) {
                        if (fbfs.f_bavail > (100+700)*1024*1024/fbfs.f_bsize)
                            scan_page();
                        else
                            diskfull = 1;
                    }
                    else {
                        if (fbfs.f_bavail > (100+350)*1024*1024/fbfs.f_bsize)
                            scan_page();
                        else
                            diskfull = 1;
                    }
                    if (diskfull) {
                        warn(0,"disk full");
                        if (have_oper)
                            make_sound(FAIL_SND);
                    }
                }
                else
                    make_sound(WARN_SND);
            }

            /* start burning CD (slow) */
            else if (m == '1') {
                if (burn_st == 0) {
                    ni = 2;
                    op = 3;
                }
                else
                    make_sound(WARN_SND);
            }

            /* start burning CD (fast) */
            else if (m == '2') {
                if ((burn_st == 0) && (scan_pid < 0)) {
                    ni = 2;
                    op = 4;
                }
                else
                    make_sound(WARN_SND);
            }

            /* test CD-R status */
            else if (m == '4') {
                if (burn_st > 0)
                    make_sound(BUSY_SND);
                else if (burn_fail)
                    make_sound(FAIL_SND);
                else
                    make_sound(OK_SND);
            }

            /* shutdown */
            else if (m == '5') {
                warn(0,"down requested");
                ni = 2;
                op = 2;
            }

            /* exit (to be restarted by initd) */
            else if (m == '6') {
                warn(0,"exiting");
                ni = 2;
                op = 5;
            }

            /* play all sounds */
            else if (m == '7') {
                make_sound(ALL_SND);
            }

            /* disk usage */
            else if (m == '8') {
                if (toburn_full)
                    make_sound(OK_SND);
                else
                    make_sound(WARN_SND);
            }
        }

        /* sleep 0.1 seconds */
        else {
            struct timeval tv;

            tv.tv_sec = 0;
            tv.tv_usec = 100000;
            select(0,NULL,NULL,NULL,&tv);
        }

        /* each 0.5 seconds try to perform some op */
        if (++l >= 5) {

            /* try to compress a file */
            start_compress();

            /* try to perform one burn step */
            if ((burn_st > 0) && (burn_pid < 0))
                burn_cd();

            /* wait children */
            my_wait();

            /* try to move a file */
            if ((!toburn_full) && (compress_pid < 0))
                move_toburn();

            /* reset counter */
            l = 0;
        }
    }

    /* restore and exit */
    if (have_kbd)
        tcsetattr(fd,TCSANOW,&oldtio);
    if (LOG != NULL)
        fclose(LOG);
    if (term6 >= 0)
        close(term6);
    exit(0);
}
