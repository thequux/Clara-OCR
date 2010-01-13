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

pgmblock.c: grayscale loading and blockfinding.

*/

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include "common.h"
#include "gui.h"

#ifndef PI
#define PI M_PI
#endif

/*

Pixel buffers.

*/
int *ll=NULL,*rl=NULL,lsz=0;
int *rx,*ry,*rz,topr,rsz=0;

/*

Buffer to store the largest cluster.

*/
int *clusterize_r;

/*

*/
int fb(char *b)
{
    int i,r;

    for (i=r=0; i<8; ++i) {
        r = (r<<1) + ((b[i] == '1') ? 1 : 0);
    }

    return(r);
}

/*

This is experimental stuff to try to achieve better compression
rates for PGM, remapping the currently loaded PGM file before
compressing.

In some cases it's possible to enhance gzip compression ratio
using this service. However, we could not enhance bzip2
compression ratio.

To try:

    pgmmap(pixmap,XRES,YRES);
    pgmcut("test.pgm",pb,XRES,YRES,0,XRES-1,0,YRES-1,0);

And from the command line:

    gzip -9 test.pgm
    gzip -9 map

Now compare size(test.pgm) + size(map) with size(file.pgm.gz).

See also pgmunmap().

*/
void pgmmap(unsigned char *pb,int w,int h)
{
    int i,j,bpl;
    unsigned char map[256];
    unsigned char *m;
    int C[256];

    bpl = ((w/8) + ((w%8)!=0));
    m = c_realloc(NULL,h*bpl,NULL);
    memset(m,0,h*bpl);

    memset(C,0,sizeof(int)*256);

    for (j=0; j<h-1; ++j) {
        for (i=0; i<w; ++i) {

            ++(C[pb[j*w+i]]);
        }
    }

    {
        int i,j,F,t;
        int p[256],s[256];

        for (i=0; i<256; ++i)
            p[i] = i;
        true_qsi(p,0,255,(char *)C,sizeof(int),0,0);
        memset(s,0,sizeof(int)*256);

        for (i=0; i<128; ++i)
            s[p[i]] = 1;

        for (i=0, j=128, t=0; t<256; ++t) {
            if (s[t]) {
                map[t] = j;
                j += 1;
            }
            else {
                map[t] = i;
                i += 1;
            }
        }

        printf("finished mapping with i=%d j=%d\n",i,j);

        for (j=t=0; j<h; ++j) {
            for (i=0; i<w; ++i) {

                /* map of rare pixels */
                if (s[pb[j*w+i]]) {
                    m[j*bpl + i/8] |= (1<<(i%8));
                    ++t;
                }

                /* pixel value mapping */
                pb[j*w+i] = (map[pb[j*w+i]] & 127);

                /* line difference */
                if (j > 0) {
                    if (pb[(j-1)*w+i] >= pb[j*w+i]) {
                        pb[(j-1)*w+i] -= pb[j*w+i];
                    }
                    else {
                        pb[(j-1)*w+i] = (pb[j*w+i] - pb[(j-1)*w+i]);
                        pb[(j-1)*w+i] |= 128;
                    }
                }
            }
        }

        printf("%d rare pixels (%1.4f)\n",t,((float)t)/(w*h));

        F = open("map",O_WRONLY|O_CREAT);
        write(F,m,h*bpl);
        write(F,map,256);
        close(F);
    }

    c_free(m);
}

/*

Unmap PGM file.

This is experimental stuff to try to achieve better compression
rates for PGM. See also pgmmap().

To apply this service on the currently loaded file and save it
unmapped (assuming that it was previously mapped by pgmmap):

    pgmunmap(pixmap,XRES,YRES);
    pgmcut("test.pgm",pixmap,XRES,YRES,0,XRES-1,0,YRES-1,0);

*/
void pgmunmap(unsigned char *pb,int w,int h)
{
    int bpl;
    unsigned char map[256],invmap[256];
    unsigned char *m;

    bpl = ((w/8) + ((w%8)!=0));
    m = c_realloc(NULL,h*bpl,NULL);
    memset(m,0,h*bpl);

    {
        int i,j,F;

        F = open("map",O_RDONLY|O_CREAT);
        read(F,m,h*bpl);
        read(F,map,256);
        close(F);

        for (i=0; i<256; ++i)
            invmap[map[i]] = i;

        for (j=h-1; j>0; --j) {
            for (i=0; i<w; ++i) {

                /* line difference */
                if ((pb[(j-1)*w+i] & 128) == 0) {
                    pb[(j-1)*w+i] += pb[j*w+i];
                }
                else {
                    pb[(j-1)*w+i] = (pb[j*w+i] - pb[(j-1)*w+i]);
                }

                /* unmap */
                pb[j*w+i] = (map[pb[j*w+i]] & 127);

                /* map of rare pixels */
                if (m[j*bpl + i/8]) {
                    pb[(j-1)*w+i] |= 128;
                }
            }
        }
    }

    c_free(m);
}

/*

Convert PBM to PGM. Assumes that the buffer *pb is large enough
to store the resulting graymap.

*/
void pbm2pgm(unsigned char *pb,int w,int h)
{
    int bpl,m,i,j;
    unsigned char *p,*q;

    bpl = byteat(w);
    q = pb + w*h - 1;
    for (j=h; j>0; --j) {
        p = pb + j*bpl - 1;
        m = 1 << (w%8);
        for (i=0; i<w; ++i, --q) {
            *q = ((*p&m)==0) ? 255 : 0;
            if ((m <<= 1) > 128) {
                m = 1;
                --p;
            }
        }
    }
}

/*

Load PGM or PBM file into *pb.

*/
int loadpgm(int reset,char *f,unsigned char **pb,int *w,int *h)
{
    /* to read the file */
    static FILE *F;

    /* levels and line counter */
    static int v,n,pio,p,bpl;

    /* read header */
    if (reset) {
        int b,c,rc;

        F = zfopen(f,"r",&pio);

        /* just to avoid a compilation warning */
        c = 0;

        /* P5 magic number */
        if (((b=fgetc(F)) != 'P') || (((c=fgetc(F)) != '5') && (c!='4'))) {
            fprintf(stderr,"%s is not a pbm or pgm raw file\n",f);
            if (b=='P') {
                if (c=='6')
                    fprintf(stderr,"it seems to be a PPM (unsupported) file\n");
                else if (c=='6')
                    fprintf(stderr,"it seems to be a plain PPM (unsupported) file\n");
                else if (c=='2')
                    fprintf(stderr,"it seems to be a plain PGM (unsupported) file\n");
                else if (c=='1')
                    fprintf(stderr,"it seems to be a plain PBM (unsupported) file\n");
            }
            zfclose(F,pio);
            exit(1);
        }

        /* type of pixmap */
        pm_t = (c == '5') ? 8 : 1;

        /*
            Header reading loop. PGM defines "whitespace" as any from
            space, tab, CR or LF. Comments start with '#' and end on LF.
        */
        n = rc = 0;
        *w = *h = v = 0;
        while ((((pm_t==8) && (n<6)) || ((pm_t==1) && (n<4))) && ((c=fgetc(F)) != EOF)) {

            /* reading comment */
            if ((c == '#') || ((rc==1) && (c!='\n'))) {
                rc = 1;
            }
    
            /* non-"whitespace" */
            else if ((c!=' ') && (c!='\t') && (c!='\r') && (c!='\n')) {

                /* invalid char */
                if ((c < '0') || ('9' < c)) {
                    fprintf(stderr,"%s is not a pgm raw file (non-digit found)",f);
                    fclose(F);
                    exit(1);
                }

                /* reading width */
                if (n <= 1) {
                    n = 1;
                    *w = *w * 10 + c - '0';
                }

                /* reading heigth */
                else if (n <= 3) {
                    n = 3;
                    *h = *h * 10 + c - '0';
                }

                /* reading levels */
                else if (n <= 5) {
                    n = 5;
                    v = v * 10 + v - '0';
                }
            }

            /* "whitespace" character */
            else {

                /* stop reading width */
                if (n == 1)
                    n = 2;

                /* stop reading height */
                else if (n == 3)
                    n = 4;

                /* stop reading levels */
                else if (n == 5)
                    n = 6;

                /* stop reading comment */
                rc = 0;
            }
        }

        /* bytes per line */
        if (pm_t == 8)
           bpl = *w;
        else
           bpl = (*w/8) + ((*w%8) != 0);

        /*
            Alloc buffer large enough to store a graymap, even
            for PBM files.
        */
        *pb = c_realloc(*pb,*h * *w,NULL);

        if (*pb == NULL) {
            fprintf(stderr,"memory exhausted\n");
            exit(1);
        }

        /* prepare data reading loop */
        p = 0;

        /* reset distribution of grayshades */
        memset(graydist,0,256*sizeof(int));

        /* return unfinished code */
        return(1);
    }

    /*
        Continue reading page. We read only 512k in order to refresh the
        display. As some grayscale files may be quite large (and, if
        compressed, the loading time may be 10-20 seconds), we need to
        inform the user about the progress.
    */
    else {
        int r,d,s,t;

        /* pixmap size */
        t = *h * bpl;

        /* number of bytes to read and return status */
        d = (t - p);
        if (d > (512*1024)) {
            d = (512*1024);
            s = 1;
        }
        else
            s = 0;

        /* read d bytes */
        r = fread((*pb)+p,1,d,F);
        if (r != d)
            fatal(IO,"reading %s",f);
        p += d;

        /* update distribution of grayshades */
        {
            int i;
            unsigned char *q;

            for (q=(*pb)+p, i=0; i<d; ++i)
                ++(graydist[*(--q)]);
        }

        /* if finished, close */
        if (s == 0) {
            show_hint(1,"finished loading page");
            zfclose(F,pio);
            thresh_val = 128;

            /*
                Convert PBM to PGM. Currently this step is not
                mandatory, however the internal support to
                pre-segmentation black-and-white is expected to
                become broken and vanish soon.
            */
            if (pm_t == 1) {
                pbm2pgm(*pb,*w,*h);
                pm_t = 8;
            }
        }
        else {
            show_hint(1,"still loading, now %d%%",p/(t/100));
        }

        return(s);
    }
}

/*

Clusterize and compute largest component.

This is a simple find-largest-cluster method. Given N, T and a
distance function dist(i,j), we build the graph with vertices
0..(n-1) and edge i-j if and only if dist(i,j)<=T. Next we compute
the largest connected component of this graph.

*/
int clusterize(int N,int T,int dist(int,int))
{
    /* edges */
    static int *e=NULL,esz=0,tope;
    int *fe,de;

    /* marks and masks */
    unsigned int *m,*m2,*m3,mask[32];
    int msz=0;

    /* stack */
    static int *st,sp,stsz=0;

    /* other */
    int i,j,k,l,f,lcs,lcf,n,p;

    /* "first edge" pointers */
    fe = alloca((N+1)*sizeof(int));

    /* "delta" to increase edges array */
    if ((de = N/5) < 128)
        de = 128;

    /*
        Build the graph with vertices 0..(N-1) where
        the i-j edge exists if and only if dist(i,j) <= T.
    */
    for (tope=-1, i=0; i<N; ++i) {

        /* i-relative "first edge" */
        fe[i] = (tope + 1);

        /* search new edges */
        for (j=i+1; j<N; ++j) {

            /* add edge i-j */
            if (dist(i,j) <= T) {

                if (++tope >= esz)
                    e = c_realloc(e,(esz=tope+de)*sizeof(int),NULL);
                e[tope] = j;
            }
        }
    }

    /* sentinel */
    fe[N] = (tope + 1);

    /* array of marks */
    msz = (N/32) + ((N%32) != 0);
    m = alloca(msz*sizeof(int));
    m2 = alloca(msz*sizeof(int));
    m3 = alloca(msz*sizeof(int));
    if ((m==NULL) || (m2==NULL) || (m3==NULL))
        fatal(ME,"at clusterize");
    memset(m,0,msz*sizeof(int));

    /* masks */
    for (i=0; i<32; ++i)
        mask[i] = ((unsigned int) 1) << i;

    /* stack */
    sp = -1;
    if (stsz < N)
        st = c_realloc(st,(stsz=N)*sizeof(int),NULL);

    /*
        Search the largest component of the graph.
    */
    for (lcs=0, i=0; i<N; ++i) {

        /* already visited */
        if ((m[i/32] & mask[i%32]) != 0)
            continue;

        /* first element */
        f = i;

        /* auxiliar marks */
        memset(m2,0,msz*sizeof(int));

        /* push i, mark and account */
        st[++sp] = i;
        m[i/32] |= mask[i%32];
        m2[i/32] |= mask[i%32];
        n = 1;

	/* compute component */
        while (sp >= 0) {

            /* pop */
            k = st[sp--];

            /* search neighbours smaller than k */
            for (l=0, p=0; l<fe[k]; ++l) {
	
                if (e[l] == k) {
	
                    /* search the first end */
                    while (((p+1) < k) && (fe[p+1] <= l)) ++p;
	
                    /* if p is unmarked */
                    if ((m[p/32] & mask[p%32]) == 0) {

                        /* push, mark and account */
                        st[++sp] = p;
                        m[p/32] |= mask[p%32];
                        m2[p/32] |= mask[p%32];
                        ++n;
                    }
                }
            }
	
            /* visit neighbours larger than k */
            for (l=fe[k]; l<fe[k+1]; ++l) {
	
                p = e[l];
	
                /* if p is unmarked */
                if ((m[p/32] & mask[p%32]) == 0) {
		    
                    /* push, mark and account */
                    st[++sp] = p;
                    m[p/32] |= mask[p%32];
                    m2[p/32] |= mask[p%32];
                    ++n;
                }
            }
        }

        if (n > lcs) {
            lcs = n;
            lcf = f;
            memcpy(m3,m2,msz*sizeof(int));
        }
    }

    /* list the largest component */
    for (i=0, n=0; i<N; ++i) {
        if ((m3[i/32] & mask[i%32]) != 0) {
            st[n++] = i;
        }
    }

    if (n!=lcs)
        printf("oops.. internal failure\n");

    clusterize_r = (int *) st;

    /* return the largest component size */
    return(lcs);
}

#ifdef TEST

/*

Distance function for the tests 1 and 2 of service clusterize().

The elements are the following points (coordinates given by the
arrays x and y):

      ^
      |
    7 +  X  X                                      X  X  X
      |
    6 +     X  X  X                       X  X  X  X  X  X  X
      |
    5 +        X                          X  X                       X  X  X
      |
    4 +           X  X  X  X                                            X  X
      |
    3 +           X  X                             X  X  X
      |
    2 +        X  X  X                             X  X
      |
    1 +           X  X  X  X  X  X  X  X  X  X  X  X  X
      |
    --+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--->
      0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23

The distance is defined as the square of the euclidean
distance. Using threshold 2, the largest cluster size must be
33. Using threshold 1, the largest cluster size must be 27.

*/
int test_dist_1(int i,int j)
{
    int x[50] = { 1, 2, 2, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 7, 7, 8,
                  9,10,11,12,12,12,13,13,13,14,14,15,15,15,15,15,16,16,16,16,
                 16,17,17,17,18,21,22,22,23,23};

    int y[50] = { 7, 7, 6, 6, 5, 2, 6, 4, 3, 2, 1, 4, 3, 2, 1, 4, 1, 4, 1, 1,
                  1, 1, 1, 6, 5, 1, 6, 5, 1, 6, 1, 7, 6, 3, 2, 1, 7, 6, 3, 2,
                  1, 7, 6, 3, 6, 5, 5, 4, 5, 4};

    int u,v;

    if ((i < 0) || (50 <= i) || (j < 0) || (50 <= j))
        fatal(DI,"invalid call to test_dist");

    u = x[i] - x[j];
    v = y[i] - y[j];

    return(u*u+v*v);
}

/*

Distance function for the tests 3 and 4 of service clusterize().

*/
int test_dist_2(int i,int j)
{
    if ((i < 0) || (500 <= i) || (j < 0) || (500 <= j))
        fatal(DI,"invalid call to test_dist");

    return(abs(i-j));
}

/*

Test the service clusterize.

*/
int test_clusterize(void)
{
    /* data for test 2 */
    int i;
    int r[27] = { 5, 7, 8, 9,10,11,12,13,14,15,16,17,18,19,
                 20,21,22,25,28,30,33,34,35,38,39,40,43};
    /*
        test 1
    */
    if (clusterize(50,2,test_dist_1) == 33)
        printf("clusterize test 1 ok\n");
    else
        printf("clusterize test 1 failed\n");

    /*
        test 2
    */
    if (clusterize(50,1,test_dist_1) == 27) {

        for (i=0; (i<27) && (clusterize_r[i]==r[i]); ++i);

        if (i >= 27)
            printf("clusterize test 2 ok\n");
        else
            printf("clusterize test 2 failed (cluster)\n");
    }
    else {
        printf("clusterize test 2 failed (size)\n");
    }

    /*
        test 3.
    */
    if (clusterize(500,1,test_dist_2) == 500)
        printf("clusterize test 3 ok\n");
    else
        printf("clusterize test 3 failed\n");

    /*
        test 3.
    */
    if (clusterize(500,0,test_dist_2) == 1)
        printf("clusterize test 4 ok\n");
    else
        printf("clusterize test 4 failed\n");

    return(1);
}

#endif

/*

Distance function for clusterization of rules.

*/
int dist_rules(int i,int j)
{
    int it,ib,jt,jb,u,v;

    if ((i<0) || (topr<i) || (j<0) || (topr<j))
        fatal(DI,"invalid call to dist_rules");

    /* vertical limits */
    it = ry[i];
    ib = it + rz[i] - 1;
    jt = ry[j];
    jb = it + rz[j] - 1;

    /* horizontal distance */
    u = rx[i] - rx[j];

    /* minimum vertical distance */
    if (jt > ib)
        v = jt - ib;
    else if (it > jb)
        v = it - jb;
    else
        v = 0;

    /* euclidean distance (square) */
    return(u*u+v*v);
}

/*

Locate vertical lines.

Returns the bounding box (l,r,t,b) of the vertical lines and the
slope signal: if from top to bottom the lines go from left to
right, returns +1. If the lines go from right to left, returns
-1. If they're perfectly vertical, returns 0. If the service
could not detect consistently the slope signal, returns -2.

    +-----------+    +-----------+    +-----------+
    |           |    |           |    |           |
    |    \      |    |      /    |    |     |     |
    |     \     |    |     /     |    |     |     |
    |      \    |    |    /      |    |     |     |
    |           |    |           |    |           |
    +-----------+    +-----------+    +-----------+
         +1               -1                0

*/
int vlines2(unsigned char *pb,int w,int h,int *l,int *r,int *t,int *b)
{
    int a,i,j,k,cr,cs,x,y,sl;
    long long sx,sy,sx2,sy2,sxy,dx,dy;
    long long n;
    float s;

    /* line buffers */
    if (lsz < YRES) {
        lsz = YRES;
        ll = c_realloc(ll,lsz*sizeof(int),NULL);
        rl = c_realloc(rl,lsz*sizeof(int),NULL);
    }

    /* rules */
    topr = -1;

    /* search vertical rules */
    for (j=0; j<h; ++j) {
        for (i=w/3; (i<2*w/3); ++i) {

            for (k=0; (j+k<YRES) && (pb[(j+k)*w+i] < 102); ++k);

            if (k >= 100) {

                if (++topr >= rsz) {
                    rsz = (topr + 128);
                    rx = c_realloc(rx,rsz*sizeof(int),NULL);
                    ry = c_realloc(ry,rsz*sizeof(int),NULL);
                    rz = c_realloc(rz,rsz*sizeof(int),NULL);
                }
                rx[topr] = i;
                ry[topr] = j;
                rz[topr] = k;

                /* mark pixels */
                while (--k>=0)
                    pb[(j+k)*w+i] += 102;

            }
        }
    }

    /* unmark pixels of all rules */
    for (cr=0; cr<=topr; ++cr) {

        x = rx[cr];
        y = ry[cr];

        /* unmark pixels */
        for (k=rz[cr], n+=k; (--k>=0); ++y) {

            /* unmark */
            pb[y*w+x] -= 102;
        }
    }

    /* clusterize rules */
    cs = clusterize(topr+1,200,dist_rules);

    /* sum of lengths of selected rules */
    for (i=sl=0; i<cs; ++i) {
        a = clusterize_r[i];
        sl += rz[a];
    }

    /* too small */
    if (sl < (YRES/5))
        return(-2);

    /*
        Prepare linear regression. It's used both to consist
        the box dimension and to compute the slope signal.
    */
    n = sx = sy = sx2 = sy2 = sxy = 0;

    /* compute limits and LR for of selected rules */
    {
        *l = w;
        *r = 0;
        *t = h;
        *b = 0;

        for (i=0; i<cs; ++i) {

            /* adjust left and right */
            a = clusterize_r[i];
            x = rx[a];
            if (x < *l)
                *l = x;
            if (x > *r)
                *r = x;

            /* adjust top and bottom */
            y = ry[a];
            if (y < *t)
                *t = y;
            y += (rz[a]-1);
            if (y > *b)
                *b = y;

            /* accumulate */
            sx  += x;
            sx2 += (x*x);
            sy  += y;
            sy2 += (y*y);
            sxy += (x*y);
        }
    }

    /* slope */
    dx = sx2 - ((float) sx*sx) / n;
    dy = sy2 - ((float) sy*sy) / n;
    s = sxy - ((float) sx*sy) / n;
    if (abs(dx) > abs(dy)) {

        /* something strange happened.. */
        return(-2);
    }

    else {
        int a;

        s /= dy;

        /* check the box dimension */
        a = s * (*b-*t+1);

        if (a > 10*(*r-*l+1))
            return(-2);
    }

    /* slope signal */
    if (fabs(s) < eps)
        return(0);
    else if (s > 0.0)
        return(1);
    else
        return(-1);
}

/*

Locate horizontal margin (left if dx=1, right if dx=-1).

*/
int hmargin(unsigned char *pb,int w,int h,int dx)
{
    int i,j,k,f,d,n,cx;
    int p,*L,lm,r;

    lm = (h/100) + 1;
    L = alloca(sizeof(int) * lm);

    /* compute margin */
    for (j=p=0; (j<h) && (p<lm); j+=d, ++p) {

        if (h-j < 130)
            d = h-j;
        else
            d = 100;

        L[p] = (dx==1) ? w : 0;

        i = (dx == -1) ? (w-30) : 30;

        for (cx=f=0; (cx<w) && (f<2); i+=dx, ++cx) {

            /* search blank area */
            if (f == 0) {

                /* account black pixels */
                for (k=n=0; k<d; ++k)
                    if (pb[(j+k)*w+i] < 153)
                        ++n;

                if (n*25 < d)
                    f = 1;
            }

            /* search characters */
            else if (f == 1) {

                /* account black pixels */
                for (k=n=0; k<d; ++k)
                    if (pb[(j+k)*w+i] < 102)
                        ++n;

                if (n*8 > d) {
                    f = 2;
                    L[p] = i;
                }
            }
        }
    }

    if (dx == 1) {
        r = w;
        for (p=0; p<lm; ++p)
            if (L[p] < r)
                r = L[p];
    }

    else {
        r = 0;
        for (p=0; p<lm; ++p)
            if (L[p] > r)
                r = L[p];
    }

    return(r);
}

/*

Locate vertical margin (top if dy==1, bottom if dy==-1).

*/
int vmargin(unsigned char *pb,int w,int h,int dy)
{
    int i,j,n,r,f,cy;

    j = (dy == 1) ? 0 : h-1;

    /* locate top margin */
    for (cy=0, r=-1, f=0; (j<h) && (cy<h); j+=dy, ++cy) {

        if (f == 0) {

            /* account black pixels */
            for (i=n=0; i<w; ++i)
                if (pb[j*w+i] < 153)
                    ++n;

            if (25*n < w) {
                f = 1;
            }
        }

        else if (f == 1) {

            /* account black pixels */
            for (i=n=0; i<w; ++i)
                if (pb[j*w+i] < 102)
                    ++n;

            if (6*n > w) {
                f = 2;
                if (r < 0)
                    r = j;
            }

            else if ((20*n > w) && (r < 0))
                r = j;

            else if (20*n < w)
                r = -1;
        }
    }

    return(r);
}

/*

Display on stdout the distribution of shades of gray on the
currently loaded PGM file (assumes that one such file is
loaded).

*/
void shades_acc(void)
{
    int w,h;

    /* translate old names */
    w = XRES;
    h = YRES;

    {
        int i,j,c[256];

        for (i=0; i<256; ++i) {
            c[i] = 0;
        }

        for (j=0; j<h; ++j) {
            for (i=0; i<w; ++i)
                ++c[pixmap[j*w+i]];
        }

        for (j=0; j<16; ++j) {
            for (i=0; i<16; ++i) {
                printf("%8d",c[j*16+i]);
            }
            printf("\n");
        }
    }
}

/*

Locate PGM left and right text blocks, assuming that exists a
vertical separation line.

The parameter pc controls the method to choose the block limits
(see the code for details). Possible values: zero and nonzero.

*/
int blockfind(int reset,int pc) {
    static int w,h,l,r,t,b,l0,r0,t0,b0,ss,st;
    static unsigned char *pb;

    if (reset) {

        /* translate old names */
        w = XRES;
        h = YRES;
        pb = pixmap;

        /* prepare nest step */
        show_hint(3,NULL);
        show_hint(0,"locating vertical separator");
        st = 1;
    }

    else if (st == 1) {

        /* locate vertical lines */
        ss = vlines2(pb,w,h,&l,&r,&t,&b);

        /* could not detect the vertical lines */
        if (ss == -2) {

            /* use the entire page as one zone */
            zones = 1;
            limits[0] = 0;
            limits[1] = 0;
            limits[2] = 0;
            limits[3] = YRES-1;
            limits[4] = XRES-1;
            limits[5] = YRES-1;
            limits[6] = XRES-1;
            limits[7] = 0;
            return(0);
        }
 
        /* prepare nest step */
        show_hint(0,"locating left margin");
        st = 2;
    }

    else if (st == 2) {

        /* locate left margin */
        l0 = hmargin(pb,w,h,1);

        /* prepare nest step */
        show_hint(0,"locating right margin");
        st = 3;
    }

    else if (st == 3) {

        /* locate right margin */
        r0 = hmargin(pb,w,h,-1);

        /* prepare nest step */
        show_hint(0,"locating top margin");
        st = 4;
    }

    else if (st == 4) {

        /* locate top margin */
        t0 = vmargin(pb,w,h,1);

        /* prepare nest step */
        show_hint(0,"locating bottom margin");
        st = 5;
    }

    else if (st == 5) {
        int d,m;

        /* locate bottom margin */
        b0 = vmargin(pb,w,h,-1);

        if (l0 > 30)
            l0 -= 30;
        else
            l0 = 0;

        if (r0+30 < w)
            r0 += 30;
        else
            r0 = w-1;

        /*
            prefer the top limit based on the vertical lines detection.
        */
        if ((!pc) || (t0 > t))
            t0 = t;

        if (t0 > 30)
            t0 -= 30;
        else
            t0 = 0;

        /*
            prefer the bottom limit based on the vertical lines detection.
        */
        if ((!pc) || (b0 < b))
            b0 = b;

        if (b0+30 < h)
            b0 += 30;
        else
            b0 = h-1;

        d = abs(((r-l0)-(r0-l)));

        /*
        if (d > 80) {
            printf("block division under suspection (%d)\n",d);
        }
        else {
            printf("block widths difference %d\n",d);
        }
        */

        /*
        printf("left block: %d,%d,%d,%d\n",l0,r,t0,b0);
        printf("right block: %d,%d,%d,%d\n",l,r0,t0,b0);
        */

        m = (r + l) / 2;

        /* create the left zone */
        limits[0] = l0;
        limits[1] = t0;
        limits[2] = l0;
        limits[3] = b0;
        limits[4] = (ss==0) ? m : ((ss>0) ? r : l);
        limits[5] = b0;
        limits[6] = (ss==0) ? m : ((ss>0) ? l : r);
        limits[7] = t0;

        /* create the right zone */
        limits[8] = (ss==0) ? (m+1) : ((ss>0) ? (l+1) : (r+1));
        limits[9] = t0;
        limits[10] = (ss==0) ? (m+1) : ((ss>0) ? (r+1) : (l+1));
        limits[11] = b0;
        limits[12] = r0;
        limits[13] = b0;
        limits[14] = r0;
        limits[15] = t0;

        zones = 2;

        redraw_dw = 1;
        redraw_zone = 1;

        /* finished */
        show_hint(0,"finished");
        return(0);
    }

    /* not finished */
    return(1);
}
