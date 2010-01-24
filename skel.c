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

skel.c: Skeleton computation

*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include "common.h"
#include "gui.h"

#ifndef PI
#define PI M_PI
#endif

/*

Epsilon.

*/
float eps = 0.0001;

/*

Candidates for skeleton parameters.

These are selected skeleton parameters. Instead of trying all possible
skeleton parameters when auto-tuning, we try only these.

*/
short CSP[] = {

   /* SA   RR   MA   MP   ML   MB   RX   BT */

       2,  14, 157,  10,  38,  10,   1,   1,
       2,  14, 157,  10,  38,  10,   2,   2,
       2,  14, 157,  10,  38,  10,   3,   3,

       1,  14, 157,  10,  38,  10,   2,   2,
       1,  14, 157,  10,  38,  10,   3,   3,

       0,  14, 157,  10,  38,  10,   2,   2,
       0,  14, 157,  10,  38,  10,   3,   3,

       3,   0,   0,   0,   0,   0,   0,   1,
       3,   0,   0,   0,   0,   0,   0,   2,
       3,   0,   0,   0,   0,   0,   0,   3,

       4,   0,   0,   0,   0,   0,   3,   0,
       4,   0,   0,   0,   0,   0,   4,   0,

/*
       5,   0,   0,   0,   0,   0,   3,   2,
       5,   0,   0,   0,   0,   0,   3,   3,
       5,   0,   0,   0,   0,   0,   4,   2,
       5,   0,   0,   0,   0,   0,   4,   3,
       5,   0,   0,   0,   0,   0,   4,   4,
*/

      -1
};


/* (devel)

Skeleton pixels
---------------

The first method implemented by Clara OCR for symbol classification
was skeleton fitting. Two symbols are considered similar when each
one contains the skeleton of the other.

Clara OCR implements five heuristics to compute skeletons. The
heuristic to be used is informed through the command-line option
-k as the SA parameter. The value of SA may be 0, 1, 2, 3 or 4.

Heuristics 0, 1 and 2 consider a pixel as being a skeleton pixel
if it is the center of a circle inscribed within the closure, and
tangent to the pattern boundary in more than one point.

The discrete implementation of this idea is as follows: for each
pixel p of the closure, compute the minimum distance d from p to
some boundary pixel. Now try to find two pixels on the closure
boundary such that the distance from each of them to p does not
differ too much from d (must be less than or equal to RR). These
pixels are called "BPs".

To make the algorithm faster, the maximum distance from p to the
boundary pixels considered is RX. In fact, if there exists a
square of size 2*BT+1 centered at p, then p is considered a
skeleton pixel.

As this criteria alone produces fat skeletons and isolated
skeleton pixels along the closure boundary, two other conditions
are imposed: the angular distance between the radiuses from p to
each of those two pixels must be "sufficiently large" (larger
than MA), and a small path joining these two boundary pixels
(built only with boundary pixels) must not exist (the "joined"
function computes heuristically the smallest boundary path
between the two pixels, and that distance is then compared to
MP).

The heuristics 1 and 2 are variants of heuristic 0:

1. (SA = 1) The minimum linear distance between the two BPs
is specified as a factor (ML) of the square of the radius. This
will avoid the conversion from rectangular to polar coordinates
and may save some CPU time, but the results will be slightly
different.

2. (SA = 2) No minimum distance checks are performed, but a
minimum of MB BPs is required to exist in order to consider the
pixel p a skeleton pixel.

The heuristic 3 is very simple. It computes the skeleton removing
BT times the boundary.

The heuristic 4 uses "growing lines". For angles varying in steps
of approximately 22 degrees, a line of lenght RX pixels is drawn
from each pixel. The heuristic check if the line can or cannot be
entirely drawn using black pixels. Depending on the results, it
decides if the pixel is an skeleton pixel or not. For instance:
if all lines could be drawn, then the pixel is center of an
inscribed circle, so it's considered an skeleton pixels. All
considered cases can be found on the source code.

The heuristic 5 computes the distance from each pixel to the
border, for some definition of distance. When the distance is
at least RX, it is considered a skeleton pixel. Otherwise,
it will be considered a skeleton pixel if its distance to the
border is close to the maximum distance around it (see the code
for details).

All parameters for skeleton computation are informed to Clara
through the -k command-line option, as a list in the following
order: SA,RR,MA,MP,ML,MB,RX,BT. For instance:

    clara -k 2,1.4,1.57,10,3.8,10,4,4

The default values and the valid ranges for each parameter must
be checked on the source code (see the declaration of the
variables SA, RR, MA, MP, ML, MB, RX, and BT, and the function
skel_parms). Note that BT must be at most RX.

*/

/*

Parameters for skeleton computation of closures.

*/
int   DEF_SA = 2;
float DEF_RR = 1.4;
float DEF_MA = (PI/2);
int   DEF_MP = 10;
float DEF_ML = 3.8;
int   DEF_MB = 10;
int   DEF_RX = 1; /* was 4 */
int   DEF_BT = 1; /* was 4 */

int   SA;
float RR;
float MA;
int   MP;
float ML;
int   MB;
int   RX;
int   BT;

/* arrays required for skeleton computation */
int *xbp=NULL,*ybp=NULL;
float *bpd=NULL;
float *rmdist=NULL,*rmdistp=NULL;

/* derived from RX */
int RX2,DRX=0;

/* geometric limits of the skeleton */
int fc_bp,lc_bp,fl_bp,ll_bp;

/* cb and cb2 are buffers for symbols, used to compute skeletons */
char cb[LFS*FS],cb2[LFS*FS];

/*

barcode detection parameters
----------------------------

bc_minbl .. Minimum bar length (in millimetres)
bc_maxbw .. Maximum bar width (in millimetres)
bc_maxbd .. Maximum bar distance (in millimetres)
bc_maxbl .. Maximum bar length (in millimeters)
bc_minnb .. Minimum number or bars

original values

      bc_maxbw = 1.4,
      bc_maxbd = 1.4;
      bc_minnb = 10;

values suggested by John Wehle

      bc_maxbw = 3.4,
      bc_maxbd = 3.4;
      bc_minnb = 9;

*/
float bc_minbl = 4.0,
      bc_maxbl = 50.0,
      bc_maxbw = 3.4,
      bc_maxbd = 3.4;

int   bc_minnb = 9;

/*

Pre-computed small circumferences.

The pixels of the circumference of size r centered at (0,0) are
(circpx[t],circpy[t]), (circpx[t+1],circpy[t+1]), ...,
(circpx[t+n-1],circpy[t+n-1]), where t is circ[r] and n is
circ_np[r].

The index of the last used entry of circpx and circpy is
topcircp. The size (in entries) of circpx and circpy is
circp_sz. The size in entries of circ and circ_np is circ_sz.

*/
short *circpx = NULL, *circpy = NULL;
int *circ = NULL, *circ_np, topcircp=-1, circp_sz=0, circ_sz;

/*

Parameters for extremity pixels.

*/
int extr_mal = 3, /* minimum arc length */
    extr_isr = 3, /* inferior search radius */
    extr_ssr = 5, /* superior search radius */
    extr_mel = 8, /* minimum extension length */
    extr_mvr = 4; /* maximum variation for intersection lenght when extending radius */

float extr_msl = 1.00; /* maximum slope */


/*

Bar list.

*/
int *barlist;
float *barsk,*barlen;

/*

Values computed by wbt().

*/
int WBT_R,WBT_S,WBT_N;

/*

Copy the skeleton parameters to/from the specified
locations. Both locations may be the macro SP_GLOBAL or the macro
SP_DEF or a valid pattern index.

global skeleton parameters to the default buffers.

*/
void spcpy(int to,int from)
{
    pdesc *d;

    if ((to == SP_DEF) && (from == SP_GLOBAL)) {
        DEF_SA = SA;
        DEF_RR = RR;
        DEF_MA = MA;
        DEF_MP = MP;
        DEF_ML = ML;
        DEF_MB = MB;
        DEF_RX = RX;
        DEF_BT = BT;
    }

    else if ((to == SP_GLOBAL) && (from == SP_DEF)) {
        SA = DEF_SA;
        RR = DEF_RR;
        MA = DEF_MA;
        MP = DEF_MP;
        ML = DEF_ML;
        MB = DEF_MB;
        RX = DEF_RX;
        BT = DEF_BT;
        consist_skel();
    }

    else if ((to == SP_GLOBAL) && (0 <= from) && (from <= topp)) {

        d = pattern + from;
        if (d->p[0] >= 0) {
            SA = d->p[0];
            RR = ((float)d->p[1]) / 10;
            MA = ((float)d->p[2]) / 100;
            MP = d->p[3];
            ML = ((float)d->p[4]) / 10;
            MB = d->p[5];
            RX = d->p[6];
            BT = d->p[7];
            consist_skel();
        }
    }

    else if ((0 <= to) && (to <= topp) && (from == SP_GLOBAL)) {

        pattern[to].p[0] = SA;
        pattern[to].p[1] = RR * 10;
        pattern[to].p[2] = MA * 100;
        pattern[to].p[3] = MP;
        pattern[to].p[4] = ML * 10;
        pattern[to].p[5] = MB;
        pattern[to].p[6] = RX;
        pattern[to].p[7] = BT;
    }
}

/*

Converts rectangular to polar coordinates.

*/
float ro(int x,int y)
{
    float r=0.0;

    if (x == 0) {
        if (y > 0)
            r = (PI/2);
        else if (y < 0)
            r = (3*PI/2);
        else {
            db("unexpected R->P conversion");
        }
    }
    else if (x > 0) {
        if (y > 0)
            r = (atan(((double)y)/x));
        else
            r = (2*PI+atan(((double)y)/x));
    }
    else
        r = (PI+atan(((double)y)/x));
    return(r);
}

/*

Consist skeleton parameters.

BUG: This service must be called after *any* changes to skeleton
parameters for (possible) buffer enlargements.

*/
void consist_skel(void)
{
    int u,v;

    if ((SA < 0) || (SA > 7))
        SA = DEF_SA;
    if ((RR < 0.0) || (RR > 4.0))
        RR = DEF_RR;
    if ((MA < 0.0) || (MA >= PI))
        MA = DEF_MA;
    if ((MP < 0) || (MP > 20))
        MP = DEF_MP;
    if ((ML < 0.0) || (ML >= 4))
        ML = DEF_ML;
    if ((MB < 0) || (MB >= 15))
        MB = DEF_MB;
    if ((RX < 0) || (RX >= 10))
        RX = DEF_RX;
    if ((BT < 0) || (BT > RX))
        BT = (DEF_BT > RX) ? RX : DEF_BT;

    RX2 = 2 * RX + 1;
    DRX = (RX2 * RX2);

    /* arrays required for skeleton and boxes computation */
    xbp = c_realloc(xbp,sizeof(int)*DRX,NULL);
    ybp = c_realloc(ybp,sizeof(int)*DRX,NULL);
    rmdistp = c_realloc(rmdistp,sizeof(float)*RX2*RX2,NULL);
    bpd = c_realloc(bpd,sizeof(float)*DRX,NULL);

    /* build rmdist */
    rmdist = rmdistp + RX*RX2 + RX;
    for (u=-RX; u<=RX; ++u)
        for (v=-RX; v<=RX; ++v)
            rmdist[u+v*RX2] = sqrt(u*u+v*v);

}

/*

Process the list of parameters for skeleton computation.

This service is called while the program is processing the
command-line parameters. The argument to -k is passed to
skel_parms to be parsed.

*/
void skel_parms(char *s)
{
    int i,j,n;
    char b[21];

    for (n=i=j=0; j<=strlen(s); ++j) {
        if ((s[j] == ',') || (s[j] == 0)) {

            /* copy next argument to b */
            if (j-i > 20) {
                fatal(DI,"argument to -k too long");
            }
            if (j > i)
                strncpy(b,s+i,j-i);
            b[j-i] = 0;

            /* algorithm to use */
            if (n == 0) {
                if (strlen(b) > 0)
                    SA = atoi(b);
            }

            /* acceptable error for radius */
            else if (n == 1) {
                if (strlen(b) > 0)
                    RR = atof(b);
            }

            /* minimum acceptable angular distance */
            else if (n == 2) {
                if (strlen(b) > 0)
                    MA = atof(b);
            }

            /* maximum path threshold */
            else if (n == 3) {
                if (strlen(b) > 0)
                    MP = atoi(b);
            }

            /* minimum acceptable linear distance */
            else if (n == 4) {
                if (strlen(b) > 0)
                    ML = atof(b);
            }

            /* minimum acceptable number of BPs */
            else if (n == 5) {
                if (strlen(b) > 0)
                    MB = atoi(b);
            }

            /* maximum radius for squares */
            else if (n == 6) {
                if (strlen(b) > 0)
                    RX = atoi(b);
            }

            /* black square threshold */
            else if (n == 7) {
                if (strlen(b) > 0)
                    BT = atoi(b);
            }

            else {
                fatal(DI,"too many arguments to -k");
            }

            /* prepare next iteration */
            i = j + 1;
            ++n;
        }
    }

    if (n < 8) {
        fatal(DI,"too few arguments to -k (%d)",n);
    }

    consist_skel();
}

/*

Computes recursively the border from the seed (i,j).

*/
int border(int i,int j)
{
    int g=1;

    if ((i<0) || (j<0) || (i>=LFS) || (j>=FS))
        return(0);
    if (cb2[i+j*LFS] == BLACK) {
        cb2[i+j*LFS] = WHITE;
        g &= border(i-1,j-1);
        g &= border(i-1,j);
        g &= border(i-1,j+1);
        g &= border(i,j-1);
        g &= border(i,j+1);
        g &= border(i+1,j-1);
        g &= border(i+1,j);
        g &= border(i+1,j+1);
        if (g == 0)
            cb[i+j*LFS] = GRAY;
    }
    return((cb[i+j*LFS] == WHITE) ? 0 : 1);
}

/*

Computes the border of the bitmap stored on cb

W and H may be used to make this service faster. By default use
W=0 and H=0. To achieve time savings, pass on W and H the
width and heigth of the bitmap.

*/
void cb_border(int W,int H)
{
    int i,j;

    /* skel computation requires a buffer */
    memcpy(cb2,cb,LFS*FS);

    /* by default analyse the entire bitmap */
    if ((!shape_opt) || (W == 0))
        W = LFS;
    if ((!shape_opt) || (H == 0))
        H = FS;

    /*
        Computes the boundary for each component. This
        block will call border(i,j) only when (i,j) is a black pixel
        with a white neighbour. So if the border of the component
        that contains (i,j) was already computed, then (i,j) will not
        have a white neighbour because the border function makes the
        boundary gray.
    */
    for (i=0; i<W; ++i)
        for (j=0; j<H; ++j)
            if ((cb[i+j*LFS] == BLACK) &&
               (((i>0) && (cb[(i-1)+j*LFS] == WHITE)) ||
                ((j>0) && (cb[i+(j-1)*LFS] == WHITE)) ||
                ((i+1<LFS) && (cb[(i+1)+j*LFS] == WHITE)) ||
                ((j+1<FS) && (cb[i+(j+1)*LFS] == WHITE))))
                border(i,j);
}


/*

Compute the smallest size of a gray path joining (x,y) and (u,v)
on cb. Returns 1 if this size is not larger than MP.

This function assumes some intuitive topological properties of
the closure boundary that we're not sure if are always
true. Basically, as the boundary is a closed line, there will be
exactly two paths joining each two boundary points (well, if one
point is on the internal boundary and other on the external, then
there will be no paths at all). We assume also that it is
possible to walk from (x,y) to (u,v) simply moving to the left,
right, top or botton neighbour, and avoiding to go back to the
last pixel visited (as this strategy could potentially produce an
infinite loop, the walk stopp after MP iterations). These
hypothesis allow us to implement this functions as a very simple
loop, and not as a typical recursive minimum path computation.

BUGS: this function is assymetric, that is, joined(x,y,u,v) may
differ from joined(u,v,x,y). It's not clear how to fix this.

*/
int joined(int x,int y,int u,int v)
{
    int a,b,c,d,r;
    int k[8],t,l;

    /* compute candidates to the second pixel */
    t = 0;
    if ((x+1<LFS) && (cb[(x+1)+y*LFS]==GRAY)) {
        k[t++] = x+1;
        k[t++] = y;
    }
    if ((y-1>=0) && (cb[x+(y-1)*LFS]==GRAY)) {
        k[t++] = x;
        k[t++] = y-1;
    }
    if ((x-1>=0) && (cb[(x-1)+y*LFS]==GRAY)) {
        k[t++] = x-1;
        k[t++] = y;
    }
    if ((y+1<FS) && (cb[x+(y+1)*LFS]==GRAY)) {
        k[t++] = x;
        k[t++] = y+1;
    }

    /* there will be at most two paths (?) */
    for (r=0; r<t/2; ++r) {

        /* select candidate */
        a = x;
        b = y;
        if (t >= 2*r+2) {
            c = k[2*r];
            d = k[2*r+1];
        }
        else
            return(0);

        /* build path to (u,v) */
        for (l=1; (l<MP) && (((c!=u) || (d!=v)) && ((c!=x) || (d!=y))); ++l) {

            /* compute the next pixel */
            if ((c+1<LFS) && ((c+1!=a) || (d!=b)) && (cb[(c+1)+d*LFS]==GRAY)) {
                a = c;
                b = d;
                c = c+1;
            }
            else if ((d-1>=0) && ((c!=a) || (d-1!=b)) && (cb[c+(d-1)*LFS]==GRAY)) {
                a = c;
                b = d;
                d = d-1;
            }
            else if ((c-1>=0) && ((c-1!=a) || (d!=b)) && (cb[(c-1)+d*LFS]==GRAY)) {
                a = c;
                b = d;
                c = c-1;
            }
            else if ((d+1<FS) && ((c!=a) || (d+1!=b)) && (cb[c+(d+1)*LFS]==GRAY)) {
                a = c;
                b = d;
                d = d+1;
            }
        }

        /* (x,y) reached: there is no path */
        if ((c==x) && (d==y))
            return(0);

        /* path is short: return true */
        if ((c==u) && (d==v) && (l < MP))
            return(1);
    }

    /* there is no short path: return false */
    return(0);
}

/*

White to black transitions.

*/
int wbt(int i,int j,int W,int H) {
    int l,k,t,b,r,s;

    l = ((0<=i) && (0<j))        ? cb[i+(j-1)*LFS]   : WHITE;
    k = ((0<i) && (0<j))         ? cb[i-1+(j-1)*LFS] : WHITE;
    t = ((l == WHITE) && (k != WHITE));
    b = (k != WHITE);

    l = k;
    k = ((0<i) && (0<=j))        ? cb[i-1+j*LFS]     : WHITE;
    t += ((l == WHITE) && (k != WHITE));
    b += (k != WHITE);
    r = s = (k != WHITE);
    
    l = k;
    k = ((0<i) && ((j+1)<H))     ? cb[i-1+(j+1)*LFS] : WHITE;
    t += ((l == WHITE) && (k != WHITE));
    b += (k != WHITE);
    
    l = k;
    k = ((0<=i) && ((j+1)<H))    ? cb[i+(j+1)*LFS]   : WHITE;
    t += ((l == WHITE) && (k != WHITE));
    b += (k != WHITE);
    s &= (k != WHITE);
    
    l = k;
    k = (((i+1)<W) && ((j+1)<H)) ? cb[i+1+(j+1)*LFS] : WHITE;
    t += ((l == WHITE) && (k != WHITE));
    b += (k != WHITE);
    
    l = k;
    k = (((i+1)<W) && (0<=j))    ? cb[i+1+j*LFS]     : WHITE;
    t += ((l == WHITE) && (k != WHITE));
    b += (k != WHITE);
    r &= (k != WHITE);

    l = k;
    k = (((i+1)<W) && (0<j))     ? cb[i+1+(j-1)*LFS] : WHITE;
    t += ((l == WHITE) && (k != WHITE));
    b += (k != WHITE);
    
    l = k;
    k = ((0<=i) && (0<j))        ? cb[i+(j-1)*LFS]   : WHITE;
    t += ((l == WHITE) && (k != WHITE));
    b += (k != WHITE);
    r &= (k != WHITE);
    s &= (k != WHITE);

    WBT_N = b;
    WBT_R = r;
    WBT_S = s;

    return(t);
}

/*

Computes the skeleton of the bitmap stored on the
buffer cb. The skeleton pixels are painted BLACK. The others
(non-white) are painted GRAY.

Also computes the leftmost, rightmost, topmost and bottomost
coordinates of skeleton pixels (the horizontal coordinate of the
leftmost|righmost|topmost|bottomost skeleton pixel is stored on
fc_bp|lc_bp|fl_bp|ll_bp).

For algorithms 0, 1 and 2, if (i0,j0) is a valid pixel coordinate
of the buffer cb, then use it as a seed and computes only the
skeleton of the component that contains (i0,j0).

For algorithms 0, 1 and 2, if (i0==-2) then compute only the
skeleton of the components that belong to the symbol j0. In this
case, assumes that cb is aligned at the top and left limits of
symbol j0.

In all other cases compute the closure of all components found on
the buffer cb.

W and H may be used to make this service faster. By default use
W=0 and H=0. To achieve time savings, pass on W and H the
width and heigth of the bitmap.

*/
void skel(int i0,int j0,int W,int H)
{
    char cb3[FS*LFS];
    int i,j,x,y;

    /*
    printf("SA=%d\n",SA);
    printf("RR=%1.2f\n",RR);
    printf("MA=%1.2f\n",MA);
    printf("MP=%d\n",MP);
    printf("ML=%1.2f\n",ML);
    printf("MB=%d\n",MB);
    printf("RX=%d\n",RX);
    printf("BT=%d\n\n",BT);
    fflush(stdout);
    */

    /*
        if someone changed the skeleton parms but didn't call
        consist_skel(), we can get into trouble. Hopefully we
        can prevent using this test. Pay attention: don't
        replace (2*RX+1) by RX2!
    */
#ifdef MEMCHECK
    if (DRX < ((2*RX+1)*(2*RX+1))) {
        fatal(IE,"someone changed skel parms but didn't call consist_skel");
    }
#endif

    /* by default analyse the entire bitmap */
    if ((!shape_opt) || (W == 0))
        W = LFS;
    if ((!shape_opt) || (H == 0))
        H = FS;

    /* limits for black pixels */
    fc_bp = FS;
    lc_bp = -1;
    fl_bp = FS;
    ll_bp = -1;

    /*
        Algorithm 7
        -----------

        This is the thinning method described in T. M. Ha and H. Bunke
        Handbook of Character Recognition and Document Image Analysis
        p. 33. It's intended to be used by the classifier 4.
    */
    if (SA == 7) {
        int t,r,s,n,m;
        char *p;

        /* must remember original bitmap */
        memcpy(cb3,cb,LFS*FS);

        do {

            /* make the boundary gray */
            cb_border(W,H);

            /* compute the boundary pixels to be removed */
            for (i=0; i<W; ++i) {
                for (j=0; j<H; ++j) {
                    if (cb[i+j*LFS] == GRAY) {

                        t = wbt(i,j,W,H);
                        r = WBT_R;
                        s = WBT_S;
                        n = WBT_N;

                        if ((2 <= n) && (n<=6) &&
                            (t == 1) &&
                            ((r == 0) || (wbt(i,j-1,W,H) != 1)) &&
                            ((s == 0) || (wbt(i-1,j,W,H) != 1)))

                            cb[i+j*LFS] = D_MASK;
                    }
                }
            }

            /* remove them */
            for (i=m=0; i<W; ++i) {
                for (j=0; j<H; ++j) {
                    if (cb[i+j*LFS] == D_MASK) {
                        cb[i+j*LFS] = WHITE;
                        ++m;
                    }
                }
            }

            /* make the boundary black */
            for (i=0; i<W; ++i)
                for (j=0; j<H; ++j)
                    if (cb[i+j*LFS] == GRAY)
                        cb[i+j*LFS] = BLACK;

        } while (m > 0);

        /* make the non-skeleton pixels gray */
        for (i=0; i<W; ++i)
            for (j=0; j<H; ++j)
                if ((cb3[i+j*LFS] == BLACK) && (cb[i+j*LFS] == WHITE))
                    cb[i+j*LFS] = GRAY;

        /* compute limits */
        for (y=0; y<H; ++y) {
            p = cb + y*LFS;
            for (x=0; x<W; ++x,++p) {
                if (*p == BLACK) {
                    if (fc_bp > x)
                        fc_bp = x;
                    if (lc_bp < x)
                        lc_bp = x;
                    if (fl_bp > y)
                        fl_bp = y;
                    ll_bp = y;
                }
            }
        }

        return;
    }

    /*
        Algorithm 6
        -----------
    */
    if (SA == 6) {
        int d;

        for (i=0; i<LFS; ++i) {
            for (j=0; j<FS; ++j) {
                if (cb[i+j*LFS] == BLACK)
                    cb3[i+j*LFS] = GRAY;
                else
                    cb3[i+j*LFS] = WHITE;
            }
        }

        cb_border(W,H);

        for (d=1; d; ) {

            for (i=0; i<FS; ++i) {
                for (j=0; j<FS; ++j) {
                    if (cb[i+j*LFS] == GRAY)
                        cb[i+j*LFS] = WHITE;
                }
            }

            cb_border(W,H);

            for (i=d=0; i<FS; ++i) {
                for (j=0; j<FS; ++j) {
                    if (cb[i+j*LFS] == GRAY) {
                        int ia,ib,ja,jb;

                        ia = i-1;
                        ib = i+1;
                        ja = j-1;
                        jb = j+1;

                        if (((i==0) || (cb[ia+j*LFS] != BLACK)) &&
                            ((ib==FS) || (cb[ib+j*LFS] != BLACK)) &&
                            ((j==0) || (cb[i+ja*LFS] != BLACK)) &&
                            ((jb==FS) || (cb[i+jb*LFS] != BLACK)) &&
                            ((i==0) || (j==0) || (cb[ia+ja*LFS] != BLACK)) &&
                            ((i==0) || (jb==FS) || (cb[ia+jb*LFS] != BLACK)) &&
                            ((ib==FS) || (ja==0) || (cb[ib+ja*LFS] != BLACK)) &&
                            ((ib==FS) || (jb==FS) || (cb[ib+jb*LFS] != BLACK))) {

                            cb3[i+j*LFS] = BLACK;

                        }
                        cb[i+j*LFS] = WHITE;
                    }
                    else if (cb[i+j*LFS] == BLACK) {
                        d = 1;
                    }
                }
            }


        }

        memcpy(cb,cb3,FS*LFS);

        /* compute limits */
        for (y=0; y<FS; ++y) {
            char *p;

            p = cb + y*LFS;
            for (x=0; x<FS; ++x,++p) {
                if (*p == BLACK) {
                    if (fc_bp > x)
                        fc_bp = x;
                    if (lc_bp < x)
                        lc_bp = x;
                    if (fl_bp > y)
                        fl_bp = y;
                    ll_bp = y;
                }
            }
        }

        return;
    }

    /*
        Algorithm 5
        -----------

        Based on the distance from each pixel to the border.
    */
    if (SA == 5) {
        int f,t;

        /* compute the border */
        cb_border(W,H);

        /*
            Skeleton pixels are scored 0, other bitpmap pixels are
            scored -1 and white pixels are scored -2.
        */
        for (i=0; i<FS; ++i) {
            for (j=0; j<FS; ++j) {
                if (cb[i+j*LFS] == GRAY)
                    cb[i+j*LFS] = 0;
                else if (cb[i+j*LFS] == BLACK)
                    cb[i+j*LFS] = -1;
                else
                    cb[i+j*LFS] = -2;
            }
        }

        /*
            Computes the "distance" from each bitmap pixel to the
            border, for some definition of "distance".
        */
        for (t=0, f=1; f; ++t) {
            f = 0;
            for (i=0; i<FS; ++i) {
                for (j=0; j<FS; ++j) {
                    if (cb[i+j*LFS] == -1) {
                        if (((i>0) && (cb[(i-1)+j*LFS]==t)) ||
                            ((i+1<LFS) && (cb[(i+1)+j*LFS]==t)) ||
                            ((j>0) && (cb[i+(j-1)*LFS]==t)) ||
                            ((j+1<LFS) && (cb[i+(j+1)*LFS]==t))) {

                            cb[i+j*LFS] = t+1;
                            f = 1;
                        }
                    }
                }
            }
        }

        /* store the distances */
        memcpy(cb2,cb,LFS*FS);

        for (i=0; i<FS; ++i) {
            for (j=0; j<FS; ++j) {
                int a,pa,ua,b,pb,ub,m,n,t=3,v;

                if ((m = v = cb2[i+j*LFS]) >= 0) {

                    /*
                        Computee the limits of a rectangle around (i,j).
                        BUG: the magic number t==3 is hardcoded.
                    */
                    if ((pa = i-t) < 0)
                        pa = 0;
                    if ((ua = i+t) >= FS)
                        ua = FS-1;
                    if ((pb = j-t) < 0)
                        pb = 0;
                    if ((ub = j+t) >= FS)
                        ub = FS-1;

                    /* compute the maximum distance within the rectangle */
                    for (a = pa; a <= ua; ++a) {
                        for (b = pb; b <= ub; ++b) {

                            if ((n=cb2[a+b*LFS]) > m)
                                m = n;
                        }
                    }

                    /*
                        Mark (i,j) as a skeleton pixel if its distance
                        is large enough or if it is close to the local
                        maximum.
                    */
                    if (((v <= BT) && (m == v)) ||
                        ((v > BT) && (v+1 >= m)) ||
                        (v >= RX))

                        cb[i+j*LFS] = -1;
                }
            }
        }

        /* make skeleton pixels black */
        for (j=0; j<FS; ++j) {
            for (i=0; i<FS; ++i) {
                if (cb[i+j*LFS] == -2)
                    cb[i+j*LFS] = WHITE;
                else if (cb[i+j*LFS] == -1)
                    cb[i+j*LFS] = BLACK;
                else
                    cb[i+j*LFS] = GRAY;
            }
        }

        /* compute limits */
        for (y=0; y<FS; ++y) {
            char *p;

            p = cb + y*LFS;
            for (x=0; x<FS; ++x,++p) {
                if (*p == BLACK) {
                    if (fc_bp > x)
                        fc_bp = x;
                    if (lc_bp < x)
                        lc_bp = x;
                    if (fl_bp > y)
                        fl_bp = y;
                    ll_bp = y;
                }
            }
        }

        return;
    }

    /*
        Algorithm 4
        -----------

        Growing lines heuristic.

        The considered angles are numbered from 0 to 15. All paths can
        be performed alternating the steps 1 and 2.

            number angle step 1 step 2
            ------+-----+------+------
               0      0   1, 0   1, 0
               1     27   1, 0   1,-1
               2     45   1,-1   1,-1
               3     63   0,-1   1,-1
               4     90   0,-1   0,-1
               5    117   0,-1  -1,-1
               6    135  -1,-1  -1,-1
               7    153  -1, 0  -1,-1
               8    180  -1, 0  -1, 0
               9    207  -1, 0  -1, 1
              10    225  -1, 1  -1, 1
              11    243   0, 1  -1, 1
              12    270   0, 1   0, 1
              13    287   0, 1   1, 1
              14    315   1, 1   1, 1
              15    333   1, 0   1, 1

        For instance: starting from (23,14) the first 5 pixels on the
        path number 9 are:

              (23,14)
              (22,14) = (23,14) + (-1,0)
              (21,13) = (22,14) + (-1,-1)
              (20,13) = (21,13) + (-1,0)
              (19,12) = (20,13) + (-1,-1)

    */
    if (SA == 4) {

        /* results, centers and distances */
        int R[16],C[16],D[16];

        /* the steps */
        int dx1[16] = { 1, 1, 1, 0, 0, 0,-1,-1,-1,-1,-1, 0, 0, 0, 1, 1};
        int dy1[16] = { 0, 0,-1,-1,-1,-1,-1, 0, 0, 0, 1, 1, 1, 1, 1, 0};
        int dx2[16] = { 1, 1, 1, 1, 0,-1,-1,-1,-1,-1,-1,-1, 0, 1, 1, 1};
        int dy2[16] = { 0,-1,-1,-1,-1,-1,-1,-1, 0, 1, 1, 1, 1, 1, 1, 1};

        /* other */
        int c,c1,c2,f,g,n,r,s,u,v;

        /* must remember original bitmap */
        memcpy(cb3,cb,LFS*FS);

        /* test each pixel */
        for (i=0; i<LFS; ++i) {
            for (j=0; j<FS; ++j) {

                /* ignore non-BLACK pixels */
                if (cb3[i+j*LFS] != BLACK)
                    continue;

                /* draw each line */
                for (f=r=0; r<16; ++r) {

                    /* try RX steps */
                    for (g=1, s=0, u=i, v=j; g && (s<RX); ++s) {

                        /* next step */
                        if (s & 1) {
                            u += dx2[r];
                            v += dy2[r];
                        }
                        else {
                            u += dx1[r];
                            v += dy1[r];
                        }

                        /* failure to extend */
                        if ((u < 0) || (LFS <= u) ||
                            (v < 0) || (FS <= v) ||
                            cb3[u+v*LFS] != BLACK) {

                            g = 0;
                            ++f;
                        }
                    }

                    /* result for angle r */
                    R[r] = g;
                    D[r] = s-1;
                }

                /* centers of inscribed circles are skeleton pixels */
                if (f == 0)
                    continue;

                /* discard pixels with no successfull path */
                if (f == 16) {
                    cb[i+j*LFS] = GRAY;
                    continue;
                }

                /* initialize the centers */
                for (r=0; r<16; ++r)
                    C[r] = 0;

                /*
                    Search the angles that are centers of maximal
                    arrays of consecutive failures.
                */
                for (c1=c2=-1, n=0, r=15, g=-1; r>g; --r) {

                    if (R[r])
                        continue;

                    /*
                        Now that we know that for angle r we've failed to
                        draw the line, let's try to  extend the failure
                        clockwise and counterclockwise, finding the limits
                        u and v:

                                u r  v    0
                           1111100000011111

                               ------>
                              clockwise
                    */

                    /* extend the failure conter-clockwise */
                    for (u=r; R[s=(u==15)?0:u+1] == 0; u=s);

                    /*
                        Extend the failure clockwise.

                        As we're starting with r=15, the v limit will never
                        cross the boundary from angle 0 to angle 15, because
                        if R[0] == R[15] == 0 happens, then on the first
                        failure (that is, r==15), the u limit already crossed
                        the boundary from 15 to 0.
                    */
                    for (v=r; (v>0) && (R[v-1] == 0); --v);

                    /* choose center */
                    if (r <= u) {
                        c = (u+v) / 2;
                        C[c] = u-v+1;
                    }
                    else {

                        /* (no need to visit again the entries from 0 to u) */
                        g = u;

                        /* translate, compute center and translate center */
                        u += 16;
                        c = (u+v) / 2;
                        if (c >= 16)
                            c -= 16;
                        C[c] = u-v+1;
                    }

                    /* account centers */
                    if (++n == 1)
                        c1 = c;
                    else if (n == 2)
                        c2 = c;

                    /* prepare next search */
                    r = v-2;
                }

                /*
                    1. If we have only one failure, than the
                    pixel is on the extreme portion of
                    one not-thin component. Such pixels are not
                    considered skeleton pixels, unless the failure
                    covers at most three angles. This exception
                    tries to cover those cases where the component
                    has width 2*RX. In this extreme case, all
                    pixels will be rejected unless we admit that
                    exception.
                */
                if (n == 1) {
                    if (C[c1] > 3)
                        cb[i+j*LFS] = GRAY;
                }

                /*
                    2. If we have just two large opposed failures,
                    then the pixel is on a thin portion
                    of the component. Such pixels are considered
                    skeleton pixels. However, we try to discard some
                    pixels when the component width is exactly 2.
                */
                else if (n == 2) {

                    /* width 2 */
                    if ((D[c1] == 1) && (D[c2] == 0))
                        cb[i+j*LFS] = GRAY;
                }

                /*
                    All other cases are discarded by now.
                */
                else {
                    cb[i+j*LFS] = GRAY;
                }
            }
        }

        /* compute limits */
        for (y=0; y<FS; ++y) {
            char *p;

            p = cb + y*LFS;
            for (x=0; x<FS; ++x,++p) {
                if (*p == BLACK) {
                    if (fc_bp > x)
                        fc_bp = x;
                    if (lc_bp < x)
                        lc_bp = x;
                    if (fl_bp > y)
                        fl_bp = y;
                    ll_bp = y;
                }
            }
        }

        return;
    }

    /*
        Algorithm 3
        -----------

        Compute skeleton removing the boundary BT times
    */
    if (SA == 3) {
        int n;
        char *p;

        /* must remember original bitmap */
        memcpy(cb3,cb,LFS*FS);

        for (n=0; n<BT; ++n) {

            /* make the boundary gray */
            cb_border(W,H);

            /* make the boundary white */
            for (i=0; i<LFS; ++i)
                for (j=0; j<FS; ++j)
                    if (cb[i+j*LFS] == GRAY)
                        cb[i+j*LFS] = WHITE;
        }

        /* make the non-skeleton pixels gray */
        for (i=0; i<LFS; ++i)
            for (j=0; j<FS; ++j)
                if ((cb3[i+j*LFS] == BLACK) && (cb[i+j*LFS] == WHITE))
                    cb[i+j*LFS] = GRAY;

        /* compute limits */
        for (y=0; y<FS; ++y) {
            p = cb + y*LFS;
            for (x=0; x<FS; ++x,++p) {
                if (*p == BLACK) {
                    if (fc_bp > x)
                        fc_bp = x;
                    if (lc_bp < x)
                        lc_bp = x;
                    if (fl_bp > y)
                        fl_bp = y;
                    ll_bp = y;
                }
            }
        }

        return;
    }

    /*

        Here begins algorithms 0, 1 and 2. Both depend on the
        computation of the border. Border pixels are used by
        the heuristics that build the skeleton. So we need to
        compute the border of all components on the buffer cb.

        Alternatively, we can compute the border only of those
        components to be considered. So skel() offers three
        options:

    */

    /*
        1. compute the closure boundary only of the component
           specified by the seed (i0,j0).
    */
    if ((0<=i0) && (i0<LFS) && (0<=j0) && (j0<FS)) {
        memcpy(cb2,cb,LFS*FS);
        border(i0,j0);
    }

    /*
        2. compute the closure boundary of all components of
           the symbol j0.
    */
    else if (i0 == -2) {
        sdesc *m;
        cldesc *c;
        int n;

        memcpy(cb2,cb,LFS*FS);
        m = mc + j0;
        for (n=0; n < m->ncl; ++n) {
            c = cl + m->cl[n];
            border(c->seed[0]-c->l,c->seed[1]-c->t);
        }
    }

    /*
        3. compute the border of all components.
    */
    else {
        cb_border(W,H);
    }

    /* clear the buffers */
    memset(cb2,WHITE,LFS*FS);
    memset(cb3,0,LFS*FS);

    /* for each black pixel on the board */
    for (y=0, x=0; y<H; ((void)((++x<W) || (x=0,++y)))) {
        int r,rmin,rc;
        float d,dmin,ma;
        int topbp;
        int umin,vmin;
        int a,u,v;

        /* ignore non-black pixels */
        if (cb[x+y*LFS] != BLACK)
            continue;

        /* maximum size for a r-square */
        if (y < (r = x))
            r = y;
        if ((x + r) >= LFS)
            r = LFS-x-1;
        if ((y + r) >= FS)
            r = FS-y-1;
        if (r > RX)
            r = RX;

        /*
            Search the gray pixels around (x,y) using squares
            centered at (x,y).
        */
        dmin = r + 1;
        umin = vmin = r+1;
        topbp = 0;
        rmin = 1;

        /*
            Remember the size of the largest black square centered
            at (x,y) as cb3[x+y*LFS]. This information is used
            to process pixels (x+1,y) or (x,y+1), if the square
            optimization is on.
        */
        if (square_opt != 0) {
            if ((x > 0) && ((a=cb3[(x-1)+y*LFS]-2) > rmin))
                rmin = a;
            if ((y > 0) && ((a=cb3[x+(y-1)*LFS]-2) > rmin))
                rmin = a;
        }

        for (rc=rmin; (rc<=r) && (rc-dmin<=RR); ++rc) {
            char *pp,*pl;
            int mu,mv;

            /* top and bottom */
            mu = rc;
            pp = cb + x - mu + (y-rc)*LFS;
            pl = cb + x - mu + (y+rc)*LFS;
            for (u=-mu; (u<=mu); ++u,++pp,++pl) {
                if ((d = rmdist[u+rc*RX2]) < dmin+RR) {
                    if (*pp == GRAY) {
                        if (d < dmin) {
                            dmin = d;
                            umin = abs(u);
                            vmin = rc;
                        }
                        xbp[topbp] = u;
                        ybp[topbp] = -rc;
                        bpd[topbp++] = d;

                    }
                    if (*pl == GRAY) {
                        if (d < dmin) {
                            dmin = d;
                            umin = abs(u);
                            vmin = rc;
                        }
                        xbp[topbp] = u;
                        ybp[topbp] = rc;
                        bpd[topbp++] = d;
                    }
                }
            }

            /* left and right */
            mv = rc - 1;
            pp = cb + x - rc + (y-mv)*LFS;
            pl = cb + x + rc + (y-mv)*LFS;
            for (v=-mv; v<=mv; ++v,pp+=LFS,pl+=LFS) {
                if ((d = rmdist[rc+v*RX2]) < dmin+RR) {
                    if (*pp == GRAY) {
                        if (d < dmin) {
                            dmin = d;
                            umin = rc;
                            vmin = abs(v);
                        }
                        xbp[topbp] = -rc;
                        ybp[topbp] = v;
                        bpd[topbp++] = d;
                    }
                    if (*pl == GRAY) {
                        if (d < dmin) {
                            dmin = d;
                            umin = rc;
                            vmin = abs(v);
                        }
                        xbp[topbp] = rc;
                        ybp[topbp] = v;
                        bpd[topbp++] = d;
                    }
                }
            }
        }

        /* remember */
        cb3[x+y*LFS] = (umin > vmin) ? umin-1 : vmin-1;

        /*
            If there exists a black square of size 2*BT+1 centered at
            (x,y), then the pixel is automatically considered a
            skeleton pixel.
        */
        if (dmin > BT)
            ma = MA;

        /* Otherwise the BPs (boundary pixels) will be analysed */
        else {

            /*
                remove pixels at distance from (x,y) outside
                the interval [dmin-RR,dmin+RR].
            */
            ma = 0;
            for (i=j=0; i<topbp; ++i) {
                if (fabs(dmin-bpd[i]) <= RR) {
                    if (i != j) {
                        xbp[j] = xbp[i];
                        ybp[j] = ybp[i];
                        bpd[j] = bpd[i];
                    }
                    ++j;
                }
            }
            topbp = j;

            /*
                Algorithm 2
                -----------

                algorithm 2 means "accept those pixels with at
                least MB BPs". Faster but less accurate.
            */
            if (SA == 2) {
                if (topbp >= MB)
                    ma = MA;
            }

            /*
                locate the pairs of BPs (i,j) where the distance
                from i to j is "large", check if their angular
                distance is greater then MA and if there is not
                a small path joining them.
            */
            else if (topbp > 2) {
                float d2=dmin*dmin;

                /* for each pair of BPs */
                for (i=0, j=1;
                    (ma<MA) && (i+1<topbp);
                    ((void)((++j<topbp) || (j=++i+1)))) {

                    /*
                        Algorithm 1
                        -----------

                        Ignore if the linear distance is too small.
                    */
                    if (SA == 1) {
                        u = xbp[i] - xbp[j];
                        v = ybp[i] - ybp[j];
                        if ((u*u + v*v) < ML*d2)
                            continue;
                        ma = MA;
                    }

                    /*
                        Algorithm 0
                        -----------

                        Ignore if the angular distance is too small.

                        BUG: some calls to the R->P service ro() are
                        bad, they inform (0,0) as parameters.
                    */
                    else {
                        if ((ma = fabs(ro(xbp[i],ybp[i])-ro(xbp[j],ybp[j]))) > PI)
                            ma = 2*PI - ma;
                        if (ma < MA)
                            continue;
                    }

                    if ((debug) &&
                        (joined(x+xbp[i],y+ybp[i],x+xbp[j],y+ybp[j]) !=
                         joined(x+xbp[j],y+ybp[j],x+xbp[i],y+ybp[i]))) {

                        /* commented out to not pollute the output */
                        /*
                        db("assymetric behaviour of joined");
                        */
                    }

                    /* ignore if there is a small path joining them */
                    if ((joined(x+xbp[i],y+ybp[i],x+xbp[j],y+ybp[j]) != 0) ||
                        (joined(x+xbp[j],y+ybp[j],x+xbp[i],y+ybp[i]) != 0))
                        ma = 0;
                }
            }
        }

        /* accept pixel */
        if (ma >= MA)
            cb2[x+y*LFS] = BLACK;
    }

    /*
        Finally, make all skeleton pixels BLACK. Skeleton pixels
        are currently painted BLACK on the cb2 auxiliar buffer.

        Pixels from cb that originally were BLACK but do not
        belong to the skeleton are painted GRAY.
    */
    for (y=0; y<H; ++y) {
        for (x=0; x<W; ++x) {
            if (cb2[x+y*LFS] == BLACK) {

                /* skeleton pixel */
                cb[x+y*LFS] = BLACK;

                /* adjust limits for black pixels */
                if (x < FS) {
                    if (x < fc_bp)
                        fc_bp = x;
                    if (x > lc_bp)
                        lc_bp = x;
                    if (y < fl_bp)
                        fl_bp = y;
                    if (y > ll_bp)
                        ll_bp = y;
                }
            }

            /* non-skeleton non-white pixel */
            else if (cb[x+y*LFS] == BLACK)
                cb[x+y*LFS] = GRAY;
        }
    }
}

/*

Rate the skeleton quality for pattern p.

This is the service used by the skeleton parameters auto-tune
facility. The meaning of "skeleton quality" is defined by the
code itself.

Roughly speaking, the code applies various basic tests. If for
any test we get a strong failure (example: the skeleton height is
much smaller than the symbol height) then the quality immediately
drops to 0. Otherwise each test computes a quality and the final
result is the sum of all those qualities.

By now, two basic tests are implemented: clearance and
coverage. The coverage test is not as good as it should be.

*/
int skel_quality(int p)
{
    pdesc *d;
    int cx,cy,i,j,t,x,y;
    int S,C;
    int f,bp,sp,bad,verybad;

    /* prepare */
    pskel(p);
    d = pattern + p;
    x = d->sx;
    y = d->sy;

    /*
        Clearance
        ---------

        BUG: hardcoded distances in pixels.
    */
    cx = d->bw - d->sw;
    cy = d->bh - d->sh;
    if ((cx <= 0) || (cy <= 0) || (10 <= cx) || (10 <= cy))
        C = 0;
    if ((cx <= 2) || (cy <= 2) || (8 <= cx) || (8 <= cy))
        C = 8;
    else
        C = 10;


    /*
        Skeleton coverage
        -----------------

        BUG: hardcoded distances in pixels.
    */

    /* copy the bitmap to cb and compute its border */
    bp = bm2cb(d->b,0,0);
    cb_border(0,0);

    /*
        Border pixels are scored 0, other bitmap pixels are
        scored -1 and white pixels are scored -2.
    */
    for (i=0; i<d->bw; ++i) {
        for (j=0; j<d->bh; ++j) {
            if (cb[i+j*LFS] == GRAY)
                cb[i+j*LFS] = 0;
            else if (cb[i+j*LFS] == BLACK)
                cb[i+j*LFS] = -1;
            else
                cb[i+j*LFS] = -2;
        }
    }

    /*
        Computes the "distance" of each bitmap pixel to the
        border, for some definition of "distance". On finish
        t is the maximum distance.

        WARNING: as cb is an array of bytes, the maximum
        distance cannot exceed 127. Nowadays, the pattern maximum
        width (and height) is 96, so the maximum euclidean
        distance of one pixel to the border is expected to
        be sqrt(2)*96/2 ~ 68.
    */
    for (f=1, t=0; f && (t<127); ++t) {
        f = 0;
        for (i=0; i<d->bw; ++i) {
            for (j=0; j<d->bh; ++j) {
                if (cb[i+j*LFS] == -1) {
                    if (((i>0) && (cb[(i-1)+j*LFS]==t)) ||
                        ((i+1<LFS) && (cb[(i+1)+j*LFS]==t)) ||
                        ((j>0) && (cb[i+(j-1)*LFS]==t)) ||
                        ((j+1<LFS) && (cb[i+(j+1)*LFS]==t))) {

                        cb[i+j*LFS] = t+1;
                        f = 1;
                    }
                }
            }
        }
    }

    /* dump map of distances (for debug) */
    /*
    {
        for (j=0; j<d->bh; ++j) {
            for (i=0; i<d->bw; ++i) {
                if (cb[i+j*LFS] < 0)
                    putchar(' ');
                else if ((0 <= cb[i+j*LFS]) && (cb[i+j*LFS] <= 9))
                    putchar('0'+cb[i+j*LFS]);
                else
                    putchar('*');
            }
            putchar('\n');
        }
    */

    /* copy distances to cb2 and copy skeleton to cb */
    memcpy(cb2,cb,LFS*FS);
    sp = bm2cb(d->s,x,y);

    /* count bad pixels */
    bad = verybad = 0;
    for (i=0; i<d->bw; ++i) {
        for (j=0; j<d->bh; ++j) {
            int v;

            v = cb2[i+j*LFS];

            /*
                (i,j) belongs to the skeleton and it's "close" to
                the border. If some interior neighbour exists,
                count (i,j) as a verybad pixel.
            */
            if ((cb[i+j*LFS] == BLACK) && (v < 2)) {

                if (((i>0) && (cb2[(i-1)+j*LFS] > v)) ||
                    ((i+1<LFS) && (cb2[(i+1)+j*LFS] > v)) ||
                    ((j>0) && (cb2[i+(j-1)*LFS] > v)) ||
                    ((j+1<LFS) && (cb2[i+(j+1)*LFS] > v))) {

                    if (v < 2)
                        ++verybad;
                    else
                        ++bad;
                }
            }

            /*
                Found non-skeleton pixel at "large" distance from the
                border. If no neighbour belongs to the skeleton,
                count it as a bad pixel.
            */
            else if ((cb[i+j*LFS] != BLACK) && (cb2[i+j*LFS] >= 3)) {

                if (((i>0) && (cb[(i-1)+j*LFS] != BLACK)) &&
                    ((i+1<LFS) && (cb[(i+1)+j*LFS] != BLACK)) &&
                    ((j>0) && (cb[i+(j-1)*LFS] != BLACK)) &&
                    ((j+1<LFS) && (cb[i+(j+1)*LFS] != BLACK))) {

                    ++bad;
                }
            }
        }
    }

    /* estimates the coverage */
    if (10*sp < bp) {
        S = 0;
    }
    else if (verybad > 0) {
        S = 0;
    }
    else if (bad*3 >= bp) {
        S = 0;
    }
    else if (bad*7 < bp) {
        S = 10;
    }
    else {

        /*
            note that 3 < (bp/bad) <= 40
        */
        S = ((((float)bp)/bad)-3)/4;
    }

    /*
        Skeleton coverage (2) - TO BE DONE
        ----------------------------------

        Compute the skeleton of the difference b-s (to do)
    */

    /*
        False positives - TO BE DONE
        ----------------------------

        Search matching patterns with different transliterations
        (like 'e' and 'c'). This code was already implemented,
        but it's not being called yet (see prepare_patterns steps
        4 and 5).
    */

    /* final result */
    return(S+C);
}

/*

Search the "best" skeleton parameters on a global fashion.

(under implementation)

*/
int tune_skel_global(int reset,int p)
{
    int i,c,b,q,r,bl,blr;
    static int *Q=NULL;
    static int n;

    /* "unfinished" return status */
    r = 1;

    if (reset) {

        /* count candidates */
        for (n=0; CSP[n] >= 0; n+= 8);
        n /= 8;

        /* reset statuses and counters */
        Q = c_realloc(Q,n*sizeof(int),NULL);
        for (i=0; i<n; ++i)
            Q[i] = 0;

        /* reset best quality */
        st_bq = -1.0;
    }

    else {

        /* accumulate qualities for each candidate */
        for (bl=-1, blr=0, i=c=0; i<n; ++i) {

            /* copy candidate to skeleton parameters buffer */
            SA = CSP[c++];
            RR = ((float)CSP[c++]) / 10;
            MA = ((float)CSP[c++]) / 100;
            MP = CSP[c++];
            ML = ((float)CSP[c++]) / 10;
            MB = CSP[c++];
            RX = CSP[c++];
            BT = CSP[c++];
            consist_skel();

            /* compute and accumulate quality */
            q = skel_quality(p);
            Q[i] += q;

            /* local best until now? */
            if ((bl < 0) || (q > blr)) {
                blr = q;
                bl = i;
            }
        }

        /* finished: which is the best one? */
        if (p >= topp) {

            b = -1;
            for (i=0; i<n; ++i) {
                if ((b<0) || (Q[b]<Q[i]))
                    b = i;
            }

            /* remember best quality */
            if (topp >= 0)
                st_bq = ((float) Q[b]) / (topp+1);

            /* adopt the best one */
            c = b*8;
            SA = CSP[c++];
            RR = ((float)CSP[c++]) / 10;
            MA = ((float)CSP[c++]) / 100;
            MP = CSP[c++];
            ML = ((float)CSP[c++]) / 10;
            MB = CSP[c++];
            RX = CSP[c++];
            BT = CSP[c++];
            consist_skel();

            /* "finished" status */
            r = 0;
        }

        /* set the best local (for visualization) */
        else if ((bl+1 < n) && get_flag(FL_SHOW_SKELETON_TUNING)) {
            c = bl*8;
            SA = CSP[c++];
            RR = ((float)CSP[c++]) / 10;
            MA = ((float)CSP[c++]) / 100;
            MP = CSP[c++];
            ML = ((float)CSP[c++]) / 10;
            MB = CSP[c++];
            RX = CSP[c++];
            BT = CSP[c++];
            consist_skel();
        }
    }

    return(r);
}

/*

Search the "best" skeleton parameters for pattern p. Per-pattern
skeleton parameters is an almost abandoned feature. It's not a bad
idea, though. Now we're using a global approach. See
tune_skel_global().

*/
int tune_skel(int p)
{
    static int st=0,c=0,bc,bv;
    int v;

    /* reset */
    if (st == 0) {
        st = 1;
        bc = c = 0;
        bv = 0;
    }

    /* try next candidate */
    else if (st == 1) {

        if (CSP[c] >= 0) {

            SA = CSP[c++];
            RR = ((float)CSP[c++]) / 10;
            MA = ((float)CSP[c++]) / 100;
            MP = CSP[c++];
            ML = ((float)CSP[c++]) / 10;
            MB = CSP[c++];
            RX = CSP[c++];
            BT = CSP[c++];
            consist_skel();

            printf("trying %d %f %f %d %f %d %d %d\n",SA,RR,MA,MP,ML,MB,RX,BT);

            pskel(p);
            v = skel_quality(p);

            spcpy(SP_GLOBAL,SP_DEF);

            if (v > bv) {
                bc = c-8;
                bv = v;
            }

            /* obs: rendering of TUNE_PATTERN will call skel_quality again! */
            if (get_flag(FL_SHOW_SKELETON_TUNING)) {
                dw[TUNE_PATTERN].rg = 1;
                dw[TUNE_SKEL].rg = 1;
                redraw_document_window();
            }
        }

        /* finish */
        else {

            /* store best parameters */
            pattern[p].p[0] = CSP[bc++];
            pattern[p].p[1] = CSP[bc++] * 10;
            pattern[p].p[2] = CSP[bc++] * 100;
            pattern[p].p[3] = CSP[bc++];
            pattern[p].p[4] = CSP[bc++] * 10;
            pattern[p].p[5] = CSP[bc++];
            pattern[p].p[6] = CSP[bc++];
            pattern[p].p[7] = CSP[bc++];

            st = 0;
            return(0);
        }
    }

    return(1);
}

/*

This is a macro used by straight_borders. If (i,j) is a black
pixels, it is added to the pixel list and cleared on b2.

*/
#define addcond(i,j) { \
    if (pix(b2,bpl,i,j)) { \
        ++plt; \
        pl[2*plt] = i; \
        pl[2*plt+1] = j; \
        unsetpix(b2,bpl,i,j); \
    } \
}

/*

Compute a circular path joining all border pixels found on bitmap
b. Alternatively, span one connected component of the bitmap b,
starting at a given seed.

The parameters w and h are the width and height of the
bitmap. The border path is stored on fp: (fp[0],fp[1]) is the
first pixel, (fp[2],fp[3]) is the second, etc. No terminator is
added. The parameter m informs the maximum number of pixels to
store on fp. The computed path may be the union of disconnected
paths. For instance:

      *******
      *XXXXX*
      *X***X*
      *X* *X*
      *X***X*
      *XXXXX*
      *******

In the figure, the border pixels are marked with '*', the others
are marked with 'X'. The border is composed by two disconnected
paths. The caller can detect the end of one path testing pixel
contiguity along the fp entries. The build method assures that
all border pixels have exactly two (top-, bottom-, left- or
right-) neighbours.

If (u0,v0) is a valid coordinate of some pixel bitmap, the
service will use (u0,v0) as a seed, and will ignore all pixels
outside the component spanned from (u0,v0).

If bp is NULL, instead of computing the border path, this service
spans from (u0,v0) one component and removes from b all pixels
outside that component. In this case, if (u0,v0) is not a valid
pixel coordinate, the service performs no operation and returns
-2.

If bp is not NULL, returns the number of pixels stored in
bp. Unless the limit m was reached, this is the size of the
path. If bp is NULL, returns 0 if (u0,v0) spanned to a component,
or -1 if (u0,v0) is not an interior pixel.

This service considers one black pixel as being "interior" if all
its 8 neighbours are black. This definition assures the
properties needed by the code that computes the border
path. However, if the caller does not want to compute the border
path but only span a component, it's suggested to use the relaxed
mode (just inform a nonzero value as the parameter relax). This
redefines the definition of interior pixel (all black pixels are
considered interior). The effect of relaxation if to include in
the spanned component all pixels, eve some very extreme, despised
if using the default definition. Relaxed mode cannot be used if
the caller wants to compute the border path.

*/
int border_path(unsigned char *b,int w,int h,short *bp,int m,int u0,int v0,int relax)
{
    unsigned char *b2,*b3;
    int bpl,bs,ps;
    int u,v,i,j,f;
    int plt,plp;
    short *pl;

    bpl = (w / 8) + ((w%8) != 0);

    /*
        Alloc buffer to store the pixel list (to
        span the current component simulating recursion).
    */
    pl = alloca(2*sizeof(short)*w*h);
    plp = 0;
    plt = -1;

    /*
        Alloc bitmap buffers. b2 is used to mark the already
        visited pixels. b3 has a similar usage, but restricted to
        the construction of the border path.
    */
    b2 = alloca(bs=h*bpl);
    b3 = alloca(bs);
    memcpy(b2,b,bs);

    /* path size */
    ps = 0;

    /* main loop finish flag */
    f = 0;

    /* by default prepare to visit all pixels */
    u = v = 0;

    /* the caller didn't provide a seed */
    if ((u0 < 0) || (w <= u0) || (v0 < 0) || (h <= v0)) {

        /* give up because it's mandatory to provide a seed when bp==NULL */
        if (bp == NULL) {
            ps = -2;
            f = 1;
        }

        /* use u0 as a flag */
        else {
            u0 = -1;
        }
    }

    /* use the provided seed */
    else {

        /*
            The caller provided a seed but it isn't a black pixel: don't
            enter the main loop.
        */
        if (pix(b,bpl,u0,v0) == 0) {
            ps = -1;
            f = 1;
        }

        /*
            The seed is a black pixel, so go on. However, if it isn't
            an interior pixel, the main loop will exit after the first
            iteration.
        */
        else {
            u = u0;
            v = v0;
        }
    }

    /*
        Relaxed mode cannot be used when the caller wants a
        border path.
    */
    if ((bp != NULL) && (relax)) {
        relax = 0;
        db("relaxed mode cannot be used when building a border path");
    }

    /*
        Compute the border path for the requested components.
    */
    while (f == 0) {

        unsigned char r;

        /* get from the pixel list the next pixel to visit */
        if (plp <= plt) {
            i = pl[2*plp];
            j = pl[2*plp+1];
        }

        /*
            The pixel list is exhausted. So we've finished visiting
            all pixels of the current component or we're at the very
            first iteration of the loop.
        */
        else {

            /*
                The caller does not want a path. Clear in b all components
                other than the one just spanned. As the current component
                was cleared in b2, this is just a matter of computint an
                exclusive or between b and b2.
            */
            if ((bp == NULL) && (plt >= 0)) {
                int t;
                unsigned int *b4,*b8;

                /* compute the XOR, 32 bits at a time */
                b4 = (unsigned int *) b;
                b8 = (unsigned int *) b2;
                t = (h*bpl) / 4;
                for (i=0; i<t; ++i)
                    b4[i] ^= b8[i];

                /* now the remaining tag bytes (if any) */
                for (t*=4, i=(h*bpl-1)%4; i>=0; --i)
                    b[t+i] ^= b2[t+i];

                /* stop if the caller provided a seed */
                if (u0 >= 0) {
                    f = 1;
                }
            }

            /*
                Build a path using the border pixels.
            */
            else if ((bp != NULL) && (plt >= 0)) {
                int k,l;
                int n[4],t,s;

                /*
                    remark: to be faster, should clear only the
                    area covered by the component.
                */
                memset(b3,0,bs);

                /*
                    set border pixels on b2 and b3.
                */
                for (plp=0; plp<=plt; ++plp) {

                    if ((i=pl[2*plp]) >= 0) {
                        j = pl[2*plp+1];
                        setpix(b2,bpl,i,j);
                        setpix(b3,bpl,i,j);
                    }
                }

                /* extract next border */
                for (plp=0; plp<=plt; ++plp) {
                    int first;

                    /* not an unvisited border pixel */
                    i = pl[2*plp];
                    j = pl[2*plp+1];
                    if ((i < 0) || (pix(b3,bpl,i,j) == 0))
                        continue;

                    /* prepare to extract border */
                    k = i = pl[2*plp];
                    l = j = pl[2*plp+1];
                    s = w * h;
                    first = 1;

                    /*
                        Compute a circular path joining all border pixels.
                    */
                    do {

                        /* remove (i,j) from b3 */
                        unsetpix(b3,bpl,i,j);

                        /* add (i,j) to the path */
                        if (ps < m) {
                            bp[2*ps] = i;
                            bp[2*ps+1] = j;
                            ++ps;
                        }

                        /*
                            Search the neighbours of (i,j) checking the
                            right, top, left and bottom pixels. By
                            construction, all border pixels must have exactly
                            two neighbours.
                        */
                        t = 0;
                        if (((i+1) < w) && pix(b3,bpl,i+1,j))
                            n[t++] = 0;
                        if ((j > 0) && (pix(b3,bpl,i,j-1)))
                            n[t++] = 1;
                        if ((i > 0) && pix(b3,bpl,i-1,j))
                            n[t++] = 2;
                        if (((j+1) < h) && (pix(b3,bpl,i,j+1)))
                            n[t++] = 3;

                        /* unexpected topology */
                        if ((first && (t!=2)) || ((!first) && (t>1))) {
                            db("found pixel with %d neighbours\n",t);
                            return(0);
                        }

                        /* move to the unvisited neighbour */
                        else if (t > 0) {
                            switch (n[0]) {
                                case 0: ++i; break;
                                case 1: --j; break;
                                case 2: --i; break;
                                case 3: ++j; break;
                            }
                        }

                        /* not the first anymore */
                        first = 0;

                    } while ((t > 0) && (--s > 0));

                    /* is it a closed path? */
                    if ((abs(i-k)+abs(j-l)) != 1) {
                        db("open border\n");
                        return(0);
                    }
                }

                /* clear pixel list */
                plp = 0;
                plt = -1;

                /* stop if the caller provided a seed */
                if (u0 >= 0)
                    f = 1;
            }

            /* finished visiting all pixels */
            if (v >= h) {
                f = 1;
            }

            /* get out from the main loop */
            if (f)
                continue;

            /* get (u,v) */
            i = u;
            j = v;
            if (++u >= w) {
                u = 0;
                ++v;
            }

            /* skip (i,j) because it's non-black */
            if (pix(b2,bpl,i,j) == 0)
                continue;
        }

        /*
           Is (i,j) an interior pixel?
        */
        {
            unsigned char *s;

            /*
                The first interior pixel must be searched in
                b2 to disregard the already unmarked interior
                pixels. All others must be searched in b
                because some pixels are unmarked in b2.
            */
            s = (plt >= 0) ? b : b2;

            /* if relaxed, accept any black pixel as interior */
            if (relax)
                r = 1;

            /* trivial (bitmap limit) case */
            else if ((i<=0) || ((i+1)>=w) || (j<=0) || ((j+1)>=h))
                r = 0;

            /* non-optimized case (25%) */
            else if (((i-1)%8) >= 6) {
                r = pix(s,bpl,i+1,j) &&
                    pix(s,bpl,i-1,j) &&
                    pix(s,bpl,i-1,j-1) &&
                    pix(s,bpl,i,j-1) &&
                    pix(s,bpl,i+1,j-1) &&
                    pix(s,bpl,i-1,j+1) &&
                    pix(s,bpl,i,j+1) &&
                    pix(s,bpl,i+1,j+1);
            }

            /* 3-bit optimization (75%) */
            else {
                r = pix(s,bpl,i+1,j) &&
                    pix(s,bpl,i-1,j) &&
                    pix3(s,bpl,i-1,j-1) &&
                    pix3(s,bpl,i-1,j+1);

            }
        }

        /* yes, it is */
        if (r) {

            /* if list empty, add to list */
            if (plt < 0) {
                pl[0] = -i;
                pl[1] = j;
                plt = 0;
            }

            /* mark as interior */
            else {
                pl[2*plp] = -pl[2*plp];
            }

            /*
               Add unmarked neighbours to the list and
               mark them. Relaxed mode requires careful
               limits checking.
            */
            unsetpix(b2,bpl,i,j);
            if (relax) {
                if (i > 0)
                    addcond(i-1,j);
                if ((i+1) < w)
                    addcond(i+1,j);
                if (j > 0) {
                    if (i > 0)
                        addcond(i-1,j-1);
                    addcond(i,j-1);
                    if ((i+1) < w)
                        addcond(i+1,j-1);
                }
                if ((j+1) < h) {
                    if (i > 0)
                        addcond(i-1,j+1);
                    addcond(i,j+1);
                    if ((i+1) < w)
                        addcond(i+1,j+1);
                }
            }
            else {
                addcond(i-1,j);
                addcond(i+1,j);
                addcond(i-1,j-1);
                addcond(i,j-1);
                addcond(i+1,j-1);
                addcond(i-1,j+1);
                addcond(i,j+1);
                addcond(i+1,j+1);
            }
        }

        /*
            No, it isn't: give up if it's the informed seed (the
            informed seed is not an interior pixel).
        */
        else if ((u0 >= 0) && (plt < 0)) {
            ps = -1;
            f = 1;
        }

        /*
            Increment pointer to visit the next component pixel.
        */
        if (plt >= 0)
            ++plp;
    }

    return(ps);
}

/*

Build a path for closure k, paint it GRAY and activate the flea
over the path.

*/
int closure_border_path(int k)
{
    cldesc *c;
    int w,h;
    int lps,ps;

    if ((k < 0) || (topcl < k)) {
        db("invalid call to closure_border_path");
        return(0);
    }

    /* prepare */
    topfp = -1;
    lps = ps = 0;

    /* closure and geometry */
    c = cl + k;
    w = c->r - c->l + 1;
    h = c->b - c->t + 1;

    /* path border */
    if (FS*FS > ps) {
        lps = ps;
        ps += border_path(c->bm,w,h,fp,FS*FS-ps,-1,-1,0);
    }

    /* prepare display */
    copychar();

    /* add the closure border to the flea path */
    {
        int i,j,dx,dy,u,v;

        /* top left relative to the visible grid */
        dx = c->l - dw[PAGE_FATBITS].x0;
        dy = c->t - dw[PAGE_FATBITS].y0;

        /*
            Account only visible pixels. Maximum FS*FS due to
            fp buffer size.
        */
        for (i=j=lps; (i<ps) && (j<FS*FS); ++i) {

            u = (fp[2*i] += dx);
            v = (fp[2*i+1] += dy);

            if ((0<=u) && (u<dw[PAGE_FATBITS].hr) && (0<=v) && (v<dw[PAGE_FATBITS].vr)) {
                cfont[v*dw[PAGE_FATBITS].hr+u] = GRAY;
                if (j < i) {
                    fp[2*j] = u;
                    fp[2*j+1] = v;
                }
                ++j;
            }
        }

        /* top */
        topfp = j-1;
    }

#ifdef FUN_CODES

    /* activate the flea */
    if (topfp > 0) {
        last_fp = -1;
        fun_code = 3;
        new_alrm(50000,3);
    }

#endif

    return(0);
}

/*

Compute angle from slope, where the slope was computed from linear
regression over a segment of the border path. Basically, this
conversion is quite simple:

    angle = atan(slope)

But it becomes a bit more complex due to the way we represent slopes
(using three floats). Also, we want not only angle (like a 2d line,
angle in [0,PI[), but also orientation (like a 2d vector, angle in
[0,2*PI[). So a quadrant conversion may be required depending on the
geometric relation between the segment start and the segment end.

Due to numerical representation problems, we store slopes as values
slxy=N*sxy-sx*sy, sly=N*sy2-sy*sy, and slx=N*sx2-sx*sx. The standard
slope is computed as sl = slxy/slx. In order to handle the cases
(slx==0) and (fabs(sly) >> fabs(slx)), we define

    sl = slxy/slx if ((slx!=0) && (fabs(slx) >= fabs(sly)))
         slxy/sly otherwise.

As defined, we have -1 <= sl <= +1. The equation angle = atan(sl) is
not true anymore. Now slopes distribute through quadrants 1 and 4 as
follows:

                        ^
                        | sl>0    .
                        | (*)   .
                        |     .
                        |   .    sl>0
                        | .
            ------------+------------->
                        | .
                        |   .    sl<0
                        |     .
                        |       .
                        | sl<0    .
                        | (*)

The case (sl==0) corresponds to the horizontal axis (non-inverted) or
to the vertical axis (inverted). The dotted lines are the axis (y=x)
and (y=-x). The star marks the inverted case.

This service also receives the array of border pixels and the indexes
i and j where the straight line starts and ends. Note that this
service must be called only for segments that really approach a
straight line, because the vector from the first pixel to the last
pixel is used to deduce the orientation of the segment.

If we imagine the interpolated line as a vector (rho,theta) starting
at the "i" end (this vector is quite close to P(j)-P(i), where P(i) =
pixel at index i), the value returned by this service approaches the
angle theta, measured in radians in the range [0,2*pi[.

If (i<0), this service do not perform quadrant conversion. In this
case, the result will be in the range [0,PI[.

*/
float s2a(float slxy,float sly,float slx, short *bp,int i,int j)
{
    float a,sl;
    int u,v;

    /* the vector from the first to the last pixel */
    if (i >= 0) {
        u = bp[2*j] - bp[2*i];
        v = bp[2*j+1] - bp[2*i+1];
    }

    /* just to avoid a compilation warning */
    else
        u = v = 0;

    /* inverted case */
    if ((fabs(slx) < 0.001) || (fabs(sly/slx) > 1)) {

        sl = slx/sly;

        /* it is a vertical line */
        if (fabs(sl) < 0.0001) {
            if ((i<0) || (v < 0))
                a = PI/2;
            else
                a = -PI/2;
        }

        /* it is not a vertical line */
        else {
            a = atan(sl);
            if (sl > 0.0)
                a = PI/2 - a;
            else
                a = -PI/2 - a;
        }
    }

    /* non-inverted case */
    else {
        a = atan(sly/slx);
    }

    /* change quadrant */
    if ((i>=0) && (u < 0)) {
        a += PI;
    }
    else {
        if (a < 0)
            a += 2*PI;
    }

    return(a);
}

/*

Search straight lines on the border of the closure cl[e].

The search is based on linear regression applied to segments of
size t. There are two criteria to decide if one segment is a
straight line or not: correlation (also referred as "quadratic")
and maximum pixel distance to the interpolated line (also
referred as "linear").

To use correlation, inform crit=1 and pass the square of the
minimum correlation as parameter "val". To use pixel distance,
inform crit=2 and pass the square of the maximum distance as
parameter "val".

Suggested values:

      method    crit  t    val
    --------------------------
    linear       2   any  0.36
    quadratic    1   any  0.90

The result is returned in "res" as pairs of coordinates of the
flea path (res[0]..res[1], res[2]..res[3], etc). The parameter mr
inform the maximum number of pairs to return (that is, m is half
the size of the buffer res). The number of straight lines is
returned as the value of the function. Returns -1 if the border
path size is too large or -2 if the closure id is invalid, or
-3 if (bar==1) the closure has more than one border component.

If bar==1, fail if the closure has more than one border component
(to avoid recognizing "O" as a bar).

Problem: The interpolated line is always computed by linear
regression. In some cases, linear regression results are not as
good as we desire for lines close to the horizontal or vertical
axis (vertical or horizontal lines are correctly handled by the
code). Example:

      +-------------+
    0 |      *      |
    1 |     **      |
    2 |     *       |
    3 |     *       |
    4 |     *       |
    5 |     **      |
      +-------------+

    slope = -1 (!)
    corr  = -0.269

Remark: this service computes squares of correlations and distances
in order to avoid calls to sqrt(). Anyway, it does a lot of
floating point arithmetic.

*/
int closure_border_slines(int e,int t,int crit,float val,short *res,int mr,int bar)
{
    /* path buffer */
    short *bp;
    int ps;

    /* closure and dimension */
    cldesc *c;
    int w,h;

    /* liner regression */
    int sx,sy,sx2,sy2,sxy;
    float sl,it;
    float N,m,max_sd;
    float r;

    /* per-pixel parameters and flags */
    float *pp,*psl;
    char *pf;

    /* segment limits */
    int i,j;

    /* limits of the current closed path */
    int a,b;

    /* other */
    int k,x,y,kx,ky,x0,y0,lines,nc;
    float xa;

    /* never heard about this closure */
    if ((e < 0) || (topcl < e)) {
        db("invalid call to closure_border_slines");
        return(-2);
    }

    /* refresh display */
    if (dw[PAGE_FATBITS].v)
        copychar();

    /* closure and geometry */
    c = cl + e;
    w = c->r - c->l + 1;
    h = c->b - c->t + 1;

    /* prepare to use the flea path buffer */
    bp = fp;
    topfp = -1;

    /* compute the path border */
    ps = border_path(c->bm,w,h,bp,FS*FS,-1,-1,0);
    pp = fpp;
    psl = fsl;

    /* path too large? */
    if (ps > FS*FS)
        return(-1);
    topfp = ps-1;

    /*
        inverted coordinates flag (bit 2), per-pixel (bit 1) and
        per-segment (bit 0) validation flags.
    */
    pf = alloca(ps);
    memset(pf,3,ps);

    /* this is to avoid casting the segment size */
    N = t;

    /* maximum angular difference is 5 degrees */
    max_sd = 0.087;

    /* number of lines to report */
    lines = 0;

    /*
        The closure border may be the union of various disconnected
        closed paths (e.g. "o" has two closed paths). Each iteration
        of this loop will analyse just one closed path.
    */
    for (a=b=0; a<ps; a=b) {

        /*
            Search the right limit of the closed path. This limit can
            be detect using a trivial contiguity test.
        */
        if (a == b) {
            for (++b; (b<ps) &&
                      ((abs(bp[2*(b-1)]-bp[2*b])+abs(bp[2*(b-1)+1]-bp[2*b+1]))==1); ++b);
        }

        /* the closed path [a,b[ is too small: ignore it */
        if ((b-a) < t) {
            continue;
        }

        /* bars must have only one border component */
        if ((bar) && (b<ps)) {
            return(-3);
            printf("more than one border\n");
        }

        /* prepare linear regression */
        sx = sy = sx2 = sy2 = sxy = 0;

        /* account initial segment */
        for (k=0; k<t; ++k) {

            /* pixel coordinates */
            x = bp[2*(a+k)];
            y = bp[2*(a+k)+1];

            /* accumulate */
            sx  += x;
            sx2 += (x*x);
            sy  += y;
            sy2 += (y*y);
            sxy += (x*y);
        }

        /* compute correlation or maximum distance, and prepare next segment */
        for (i=a, j=i+t-1; i<b; ++i) {
            int nc;

            /* slope */
            psl[3*i] = t*sxy - (sx*sy);
            psl[3*i+1] = t*sy2 - sy*sy;
            psl[3*i+2] = t*sx2 - sx*sx;

            /*
                Use correlation as criteria.
            */
            if (crit == 1) {

                /*
                    Check for horizontal or vertical line. Such lines cannot
                    be detected by correlation because their correlation is 0.
                */
                if ((nc=t*sxy-sx*sy) == 0) {
                    int k;

                    x0 = bp[2*i];
                    y0 = bp[2*i+1];
                    kx = ky = 1;
                    k = i;
                    do {
                        if (k==j)
                            k = -1;
                        else {
                            if (++k >= b)
                                k = a;
                            if (bp[2*k] != x0)
                                kx = 0;
                            if (bp[2*k+1] != y0)
                                ky = 0;
                        }
                    } while ((kx||ky) && (k>=0));
                    r = (kx||ky) ? 1.0 : 0.0;
                }

                /* correlation (square) */
                else {
                     r = ((float)(nc*nc)) / ((t*sx2-sx*sx)*(t*sy2-sy*sy));
                }

                /* remember correlation */
                pp[i] = fabs(r);
            }

            /*
                Compute maximum distance instead of the correlation
                to decide if the current segment seems to be a straight
                line or not.
            */
            else {
                int inv;

                /* inverted case (see the s2a documentation) */
                if ((fabs(psl[3*i+2]) < 0.001) || (fabs(xa=psl[3*i]/psl[3*i+2]) > 1)) {
                    inv = 1;
                    sl = psl[3*i]/psl[3*i+1];
                    it = (sx-sl*sy)/N;
                }
                else {
                    inv = 0;
                    sl = xa;
                    it = (sy-sl*sx)/N;
                }

                /* compute per-pixel maximum distance to the interpolated line */
                for (m=0.0, k=i; k>=0; ) {
                    float d;

                    /* pixel coordinates */
                    if (inv) {
                        y = bp[2*k];
                        x = bp[2*k+1];
                    }
                    else {
                        x = bp[2*k];
                        y = bp[2*k+1];
                    }

                    /*
                        Reminder: the distance from (x,y) to Ax + By + C = 0 is
                        (Ax+By+C)/sqrt(A*A+B*B). The interpolated line in
                        standard form is -sl*x + y - it = 0.
                    */
                    d = (-sl*x + y - it);
                    d = d*d / (sl*sl+1);
                    if (d > m)
                        m = d;

                    /* prepare next pixel */
                    if (k==j)
                        k = -1;
                    else if (++k >= b)
                        k = a;
                }

                /* remember maximum distance */
                pp[i] = m;
            }

            /* remove pixel i */
            x = bp[2*i];
            y = bp[2*i+1];
            sx  -= x;
            sx2 -= (x*x);
            sy  -= y;
            sy2 -= (y*y);
            sxy -= (x*y);

            /* new limit j */
            if (++j >= b)
                j = a;

            /* add pixel j */
            x = bp[2*j];
            y = bp[2*j+1];
            sx  += x;
            sx2 += (x*x);
            sy  += y;
            sy2 += (y*y);
            sxy += (x*y);
        }

        /*
            Extend straight lines and paint them GRAY.
        */
        do {
            int best,n,l;
            float msl,sd,sa;

            /* search best segment */
            for (best=-1, i=a; i<b; ++i) {

                /* found straight line */
                if ((pf[i]&1) &&
                    (((crit==1) && (pp[i] > val)) ||
                     ((crit==2) && (pp[i] < val)))) {

                    /* is it the best until now? */
                    if (best < 0)
                        best = i;
                    else if (crit == 2) {
                        if (pp[i] < pp[best])
                            best = i;
                    }
                    else if (pp[i] > pp[best])
                        best = i;
                }
            }

            /* (i,j) are the best straight segment limits */
            if (best > 0) {
                i = best;
                if ((j=i+t-1) >= b)
                    j = a + (i+t-b+1);
            }

            /* no other straight segment */
            else
                continue;

            /* prepare medium angle */
            msl = s2a(psl[3*i],psl[3*i+1],psl[3*i+2],bp,i,j);

            /* left extension by segment */
            for (k=i, n=1; k>=0; ) {

                sd = 0.0;

                /* start and end of the segment at left */
                if ((k-=t) < a)
                    k = b-(a-k);
                if ((l=i-1) < a)
                    i = b-1;

                /*
                    Now we need to check if the segment starting at k
                    intersect the current straight line i..j. There
                    are two cases to consider:

                             a     i     j     b
                             +-----XXXXXXX-----(

                    When i<j, there will be intersection if
                    and only if ((i<=k) && (j<=k)).

                             a     j     i     b
                             XXXXXXX-----XXXXXX(

                    When j<i, there will be intersection if
                    and only if ((k<=j) || (i<=k)).
                */
                if ((pf[k]&1) &&
                    (((crit==1) && (pp[k] > val)) ||
                     ((crit==2) && (pp[k] < val)))) {

                    if (i<j) {
                        if ((i<=k) && (k<=j))
                            k = -1;
                    }
                    else {
                        if ((k<=j) || (i<=k))
                            k = -2;
                    }
                }
                else
                    k = -3;

                /*
                    Now we need to compare the angle of the
                    segment startig at k with the medium
                    angle. We do not admit angle differences
                    larger than max_sd.
                */
                if (k >= 0) {

                    sa = s2a(psl[3*k],psl[3*k+1],psl[3*k+2],bp,k,l);

                    if ((sd=fabs(sa-msl)) < max_sd) {

                        /* ok, extend */
                        i = k;

                        /* new medium slope */
                        msl = msl*n + sa;
                        msl /= ++n;
                    }
                    else
                        k = -4;
                }
            }

            /* right extension by segment */
            for (k=j; k>=0; ) {

                sd = 0.0;

                /* start and end of the segment at right */
                if ((l=j+1) >= b)
                    l = a;
                if ((k+=t) >= b)
                    k = a + (k-b);

                /*
                    See above (left extension) the documentation for
                    this block.
                */
                if ((pf[l]&1) &&
                    (((crit==1) && (pp[l] > val)) ||
                     ((crit==2) && (pp[l] < val)))) {

                    if (i<j) {
                        if ((i<=k) && (k<=j))
                            k = -1;
                    }
                    else {
                        if ((k<=j) || (i<=k))
                            k = -2;
                    }
                }
                else
                    k = -3;

                /*
                    Now we need to compare the angle of the
                    segment startig at k with the medium
                    angle. We do not admit angle differences
                    larger than max_sd.
                */
                if (k >= 0) {

                    sa = s2a(psl[3*l],psl[3*l+1],psl[3*l+2],bp,l,k);

                    if (fabs(sa-msl) < max_sd) {

                        /* ok, extend */
                        j = k;

                        /* new medium angle */
                        msl = msl*n + sa;
                        msl /= ++n;
                    }
                    else
                        k = -4;
                }
            }

            /*
                Prepare extensions by pixel.
            */
            {
                /* prepare */
                sx = sy = sx2 = sy2 = sxy = 0;
                if (i<j)
                    n = j-i+1;
                else
                    n = (j-a+1) + (b-i);
                kx = ky = 1;
                x0 = bp[2*i];
                y0 = bp[2*i+1];

                /* account segment */
                for (k=i-1; k!=j; ) {

                    if (++k >= b)
                        k = a;

                    /* pixel coordinates */
                    x = bp[2*k];
                    y = bp[2*k+1];

                    /* accumulate */
                    sx  += x;
                    sx2 += (x*x);
                    sy  += y;
                    sy2 += (y*y);
                    sxy += (x*y);

                    /* problematic cases */
                    if (kx)
                        kx = (x==x0);
                    if (ky)
                        ky = (y==y0);
                }
            }

            /*
                Extensions by pixel.

                Currently disable because at pixel level it's hard to
                avoid adding a non-straight tag.
            */
            if (0) {
                int f;

                /*
                    Left extension by pixel.
                */
                for (k=i, f=1; f; ) {

                    if (--k < a)
                        k = b-1;

                    if (pf[k]&1) {
                        if (k == j)
                            f = 0;
                    }
                    else
                        f = 0;

                    if (f) {

                        /* add pixel k */
                        ++n;
                        x = bp[2*k];
                        y = bp[2*k+1];
                        sx  += x;
                        sx2 += (x*x);
                        sy  += y;
                        sy2 += (y*y);
                        sxy += (x*y);

                        /* correlation */
                        if ((kx && (x==x0)) || (ky && (y==y0)))
                            r = 1.0;
                        else {
                            nc = n*sxy-sx*sy;
                            r = ((float)(nc*nc)) / ((n*sx2-sx*sx)*(n*sy2-sy*sy));
                        }

                        if (r > 0.9) {
                            i = k;

                            /* problematic cases */
                            if (kx)
                                kx = (x==x0);
                            if (ky)
                                ky = (y==y0);
                        }
                        else {
                            f = 0;

                            /* remove pixel k */
                            sx  -= x;
                            sx2 -= (x*x);
                            sy  -= y;
                            sy2 -= (y*y);
                            sxy -= (x*y);
                            --n;
                        }
                    }
                }

                /*
                    right extension by pixel
                */
                for (k=j, f=1; f; ) {

                    if (++k >= b)
                        k = a;

                    if (pf[k]&1) {
                        if (k==i) {
                            f = 0;
                        }
                    }
                    else
                        f = 0;

                    if (f) {

                        /* add pixel k */
                        ++n;
                        x = bp[2*k];
                        y = bp[2*k+1];
                        sx  += x;
                        sx2 += (x*x);
                        sy  += y;
                        sy2 += (y*y);
                        sxy += (x*y);

                        /* problematic cases */
                        if (kx)
                            kx = (x==x0);
                        if (ky)
                            ky = (y==y0);

                        /* correlation */
                        if ((kx && (x==x0)) || (ky && (y==y0)))
                            r = 1.0;
                        else {
                            nc = n*sxy-sx*sy;
                            r = ((float)(nc*nc)) / ((n*sx2-sx*sx)*(n*sy2-sy*sy));
                        }

                        if (r > 0.9) {
                            j = k;

                            /* problematic cases */
                            if (kx)
                                kx = (x==x0);
                            if (ky)
                                ky = (y==y0);
                        }
                        else {
                            f = 0;

                            /* remove pixel k */
                            sx  -= x;
                            sx2 -= (x*x);
                            sy  -= y;
                            sy2 -= (y*y);
                            sxy -= (x*y);
                            --n;
                        }
                    }
                }
            }

            /* paint it GRAY */
            {
                int dx,dy,u,v,m;

                /* top left relative to the visible grid */
                dx = c->l - dw[PAGE_FATBITS].x0;
                dy = c->t - dw[PAGE_FATBITS].y0;
                
                /* paint the pixels of the segment */
                for (m=D_MASK, k=i; k!=j; ) {
                
                    u = bp[2*k] + dx;
                    v = bp[2*k+1] + dy;

                    if ((0<=u) && (u<dw[PAGE_FATBITS].hr) &&
                        (0<=v) && (v<dw[PAGE_FATBITS].vr)) {

                        cfont[v*dw[PAGE_FATBITS].hr+u] = (GRAY | m);
                        m = 0;
                    }

                    if (++k >= b)
                        k = a;
                }
            }

            /*
                Invalidate all segments that intersect the painted pixels
                and all painted pixels.
            */
            {
                int l;

                /* correlation */
                if ((kx) || (ky))
                    r = 1.0;
                else {
                    nc = n*sxy-sx*sy;
                    r = ((float)(nc*nc)) / ((n*sx2-sx*sx)*(n*sy2-sy*sy));
                }

                /* slope */
                psl[3*i] = n*sxy - (sx*sy);
                psl[3*i+1] = n*sy2 - sy*sy;
                psl[3*i+2] = n*sx2 - sx*sx;

                /* invalidate segments starting at pixels i..j and pixels i..j */
                for (k=i-1; k!=j; ) {
                    if (++k >= b)
                        k = a;
                    pf[k] &= 4;
                }

                /* invalidate segments starting at pixels preceding i */
                for (k=i,l=1; l<t; ++l) {

                    if (--k < a)
                        k = b-1;
                    pf[k] &= 6;
                }
            }

            /* remember straight line */
            if ((lines+1) <= mr) {
                res[2*lines] = i;
                res[2*lines+1] = n;
                ++lines;
            }

        } while (i<b);
    }

    /*
        Remove pixels from bp except start of segment. This is because
        at this point only the start pixels have a consistent slope.
    */
    for (i=0; i<ps; ++i) {
        bp[2*i] = -bp[2*i] - 1;
    }
    for (i=0; i<lines; ++i) {
        j = res[2*i];
        bp[2*j] = -bp[2*j] - 1;
    }

    /* success */
    return(lines);
}

/*

Obtain A, B and C such that Ax + By + C = 0 is the line that
contains (u,v) and forms an angle a with the horizontal axis. The
angle a is assumed to be in the range [0,PI[.

*/
void line_eq(float *A,float *B,float *C,int u,int v,float a)
{

    if (fabs(a) < eps) {
        *A = 0.0;
        *B = 1.0;
        *C = -v;
    }
    else if (fabs(a-PI/2) < eps) {
        *A = 1.0;
        *B = 0.0;
        *C = -u;
    }
    else if (((PI/4) < a) && (a < (3*PI/4))) {
        *A = 1.0;
        *B = -tan((PI/2)-a);
        *C = -*A * u - *B * v;
    }
    else {
        *A = -((a>(PI/2)) ? tan(a-PI) : tan(a));
        *B = 1.0;
        *C = -*A * u - *B * v;
    }
}

/*

Test if closure k seems to be a bar.

Returns 1 if the closure seems to be a bar, <0 otherwise. If
successfull, returns the skew in *sk, and the bar length in *bl.

Diagnostics:

    -1 too small
    -2 too large
    -3 too wide
    -4 too complex
    -5 nonparallel borders
    -6 too thin/non straight

*/
int isbar(int k,float *sk,float *bl)
{
    short r[256];
    int rs,w,h,i,a,b,n,t,u,v;
    cldesc *m;
    float aa,ba,ma,A,B,C;

    /* barcode parameters in pixels */
    int minbl,maxbw,maxbl;
    float topix = DENSITY/25.4;

    /* barcode parameters in pixels */
    {
        minbl = bc_minbl * topix;
        maxbl = bc_maxbl * topix;
        maxbw = bc_maxbw * topix;
    }

    /* geometry */
    m = cl + k;
    w = m->r - m->l + 1;
    h = m->b - m->t + 1;

    /* detect straight lines on border */
    rs = closure_border_slines(k,8,2,0.36,r,128,1);

    /* fail */
    if (rs < 0)
        return(-4);
    else if (rs == 0)
        return(-6);

    /* ignore if too many straight lines */
    if (rs > 128)
        return(-4);

    /*
        First test:
        -----------

        Check minimum and maximum length
    */
    {
        float l;

        l = sqrt(w*w+h*h);
        if (l < minbl) {
            return(-1);
        }
        if (l > maxbl) {
            return(-2);
        }
        if (bl != NULL)
            *bl = l/topix;
    }

    /*
        Second test:
        ------------

        Test if each two large straight lines are parallel and close
        enough one to the other.
    */
    for (n=0,a=0,ma=0.0; a<rs; ++a) {
        int m2 = maxbw*maxbw;

        /* segment extremities */
        i = r[2*a];
        t = r[2*a+1];

        if (t > 15) {

            /* get angle */
            aa = s2a(fsl[3*i],fsl[3*i+1],fsl[3*i+2],NULL,-1,0);

            /* equation Ax+By+C=0 */
            line_eq(&A,&B,&C,fp[2*i],fp[2*i+1],aa);

            /* accumulate angles */
            ma += aa;
            ++n;

            for (b=a+1; b<rs; ++b) {

                /* segment extremities */
                i = r[2*b];
                t = r[2*b+1];

                if (t > 15) {
                    float d;

                    /* get angle */
                    ba = s2a(fsl[3*i],fsl[3*i+1],fsl[3*i+2],NULL,-1,0);

                    u = fp[2*i];
                    v = fp[2*i+1];

                    /* maximum acceptable difference is 5 degrees */
                    if (fabs(ba-aa) > (5*(PI/180))) {
                        return(-5);
                    }

                    /* maximum acceptable distance is maxbw */
                    d = (A*u + B*v + C);
                    d = d*d / (A*A+B*B);
                    if (d > m2) {
                        return(-3);
                    }
                }
            }
        }
    }

    /* no large segment: give up */
    if (n < 2)
        return(-4);

    /* medium angle */
    ma /= n;

    /*
        Third test:
        -----------

        Test if each small straight lines is parallel or
        perpendicular to the large ones.

        OOPS.. this strategy is bad!
    */
#ifdef INCLUDE_BAD_IDEAS
    for (a=rs; a<rs; ++a) {
        float d;

        /* segment extremities */
        i = r[2*a];
        t = r[2*a+1];

        if (t <= 15) {

            /* get angle */
            aa = s2a(fsl[3*i],fsl[3*i+1],fsl[3*i+2],NULL,-1,0);

            /* maximum acceptable difference is 10 degrees */
            d = fabs(aa-ma);
            if (d > 10.0)
                d = fabs(d-90);
            if (d > (10*(PI/180))) {
                return(-4);
            }
        }
    }
#endif

    /*
        Fourth test:
        ------------

        A bar must approach a diagonal of its bounding box.

        (to be implemented)
    */
    {
    }

    /* approved! */
    if (sk != NULL)
        *sk = ma;
    return(1);
}

/*

Compute the "distance" between two bars.

*/
int dist_bar(int i,int j)
{
    cldesc *p,*q;
    int toofar = (1<<30);

    p = cl + barlist[i];
    q = cl + barlist[j];

    /* same skew? */
    if (fabs(barsk[i]-barsk[j]) > 10) {
        return(toofar);
    }

    /* same length? */
    {
        float m;

        if ((m=barlen[i]) < barlen[j])
            m = barlen[j];
        if (fabs(barlen[i]-barlen[j]) > (m/5)) {
            return(toofar);
        }
    }

    /* close enough? */
    {
        int u,v;

        u = (p->l+p->r)/2 - (q->l+q->r)/2;
        v = (p->b+p->t)/2 - (q->b+q->t)/2;
        return(u*u+v*v);
    }
}

/*

Barcode search.

Current problems:
-----------------

1. Depends on hardcoded parameters (see the variables bc_*).

2. Thin bars are not detected by the border analysis. They're
detected only by the laserbeam. As the clustering is based on
border analysis, a barcode composed by thin bars only won't be
detected.

3. The laserbeam should be traced in various different positions
to not be fooled by noise.

4. Cannot be requested from the command line. (solved)

*/
int search_barcode(void)
{
    /* message to be sent to the user */
    char lb[MMB+1];

    /* closure, skew and length buffers */
    static int *b=NULL,bsz=0;
    int topb,*bc,bcsz,topbc;
    static float *sk=NULL,*bl=NULL;

    /* medium bar skew and length */
    float ms,ml;

    /* barcode parameters in pixels */
    int maxbw,maxbd;
    float topix = DENSITY/25.4;

    /* other */
    cldesc *p;
    int d,k,n;

    /* consist barcode parameters (to be completed) */
    if (bc_minnb < 1) {
        fatal(IE,"invalid bc_minnb");
    }

    /* barcode parameters in pixels */
    {
        maxbw = bc_maxbw * topix;
        maxbd = bc_maxbd * topix;
    }

    /* prepare buffers */
    d = (topcl+1)/10;
    if (d <= 0)
        d = 128;
    topb = -1;
    bcsz = 0;
    bc = NULL;
    topbc = -1;

    /* search bars */
    for (k=0; k<topcl; ++k) {

        if ((topb+1) >= bsz) {
            bsz += d;
            b = c_realloc(b,bsz*sizeof(int),NULL);
            sk = c_realloc(sk,bsz*sizeof(float),NULL);
            bl = c_realloc(bl,bsz*sizeof(float),NULL);
        }

        if (isbar(k,sk+(topb+1),bl+(topb+1))>0) {
            b[++topb] = k;
        }
    }

    /* remember buffer address */
    barlist = b;
    barsk = sk;
    barlen = bl;

    /* clusterize bars */
    n = clusterize(topb+1,25*maxbw*maxbw,dist_bar);

    /* prepare */
    ms = 0.0;
    ml = 0.0;

    /* copy cluster to bc */
    if (n >= bc_minnb) {

        if (n > bcsz)
            bc = c_realloc(bc,(bcsz=(n+48))*sizeof(int),NULL);

        for (k=0; k<n; ++k) {
            bc[k] = b[clusterize_r[k]];
            ms += sk[k];
            ml += bl[k];
        }

        topbc = n-1;
    }

    /* compute medium skew and length */
    if (topbc >= 0) {
        ms /= (topbc + 1);
        ml /= (topbc + 1);
    }

    /* draw laserbeam */
    if (topbc >= 0) {

        /* line stuff */
        float A,B,C,dl;
        int u,v,u0,v0,w;

        /* laserbeam buffer */
        char *lb=NULL;
        int lbsz,lbp;

        /* other */
        float a,m;
        int c,r;

        /* search the bar 'most close' to the medium skew */
        for (r=0, k=1, m=fabs(sk[0]-ms); k<=topbc; ++k) {

            if ((a=fabs(sk[k]-ms)) < m) {
                r = k;
                m = a;
            }
        }

        /* choose coordinate to vary */
        if (ms < (PI/4)) {
            a = ms + (PI/2);
            w = 1;
        }
        else if (ms < (PI/2)) {
            a = ms + (PI/2);
            w = 0;
        }
        else if (ms < (3*PI/4)) {
            a = ms - (PI/2);
            w = 0;
        }
        else {
            a = ms - (PI/2);
            w = 1;
        }

        /* laserbeam starting point and line equation */
        p = cl + bc[r];
        u0 = (p->l + p->r) / 2;
        v0 = (p->t + p->b) / 2;
        line_eq(&A,&B,&C,u0,v0,a);

        /* initial laserbeam buffer */
        lb = c_realloc(NULL,lbsz=512,NULL);
        lbp = 0;

        /* width variation */
        if (w) {
            dl = sqrt(1+B*B/A*A);
        }
        else {
            dl = sqrt(1+A*A/B*B);
        }

        /* search start */
        for (n=0, u=u0, v=v0; (n*dl)<maxbd; ) {

            if (w) {
                --v;
                u = (-C -B*v) / A;
            }
            else {
                --u;
                v = (-C -A*u) / B;
            }

            c = -1;
            if ((u<0) || (XRES<=u) || (v<0) || (YRES<=v)) {
                n = maxbd;
            }
            else {
                if ((c=closure_at(u,v,1)) >= 0)
                    n = 0;
                else
                    ++n;
            }

            /* add pixel to laserbeam buffer */
            if (lbp >= lbsz)
                lb = c_realloc(lb,(lbsz+=256),NULL);
            lb[lbp++] = (n == 0);

            /* add closure to cluster if it's not there */
            if (c>=0) {
                int k;

                for (k=0; (k<=topbc) && (bc[k]!=c); ++k);
                if (k > topbc) {
                    if (++topbc >= bcsz)
                        bc = c_realloc(bc,(bcsz+=32)*sizeof(int),NULL);
                    bc[topbc] = c;
                }
            }
        }

        /* invert buffer */
        {
            int a,k,l;

            for (k=0, l=lbp-1; k<l; ++k,--l) {
                a = lb[k];
                lb[k] = lb[l];
                lb[l] = a;
            }
        }

        /* search end */
        for (n=0, u=u0, v=v0; n<maxbd; ) {

            if (w) {
                ++v;
                u = (-C -B*v) / A;
            }
            else {
                ++u;
                v = (-C -A*u) / B;
            }

            c = -1;
            if ((u<0) || (XRES<=u) || (v<0) || (YRES<=v))
                n = maxbd;
            else {
                if ((c=closure_at(u,v,1)) >= 0)
                    n = 0;
                else
                    ++n;
            }

            /* add pixel to laserbeam buffer */
            if (lbp >= lbsz)
                lb = c_realloc(lb,(lbsz+=256),NULL);
            lb[lbp++] = (n == 0);

            /* add closure to cluster if it's not there */
            if (c >= 0) {
                int k;

                for (k=0; (k<=topbc) && (bc[k]!=c); ++k);
                if (k > topbc) {
                    if (++topbc >= bcsz)
                        bc = c_realloc(bc,(bcsz+=32)*sizeof(int),NULL);
                    bc[topbc] = c;
                }
            }
        }

        /* send laserbeam to decoder */
        if (searchb == 1) {
            FILE *F;
            char *argv[] = {"obd","-i","scanline","/tmp/clara.barcode",NULL};

            F = fopen("/tmp/clara.barcode","w");
            if (F == NULL)
                fatal(IO,"could not open /tmp/clara.barcode");
            for (k=0; k<lbp; ++k) {
                if (lb[k])
                    fprintf(F,"X");
                else
                    fprintf(F," ");
            }
            fprintf(F,"\n");
            fclose(F);
            obd_main(4,argv);
            if (unlink("/tmp/clara.barcode") != 0)
                fatal(IO,"could not remove /tmp/clara.barcode");
        }

        /* send laserbeam to stdout */
        else {
            for (k=0; k<lbp; ++k) {
                if (lb[k])
                    printf("X");
                else
                    printf(" ");
            }
            printf("\n");
        }

        /* free laserbeam buffer */
        c_free(lb);
    }

    /* create zone and centralize */
    if (topbc >= 0) {
        int zl,zr,zt,zb;

        zl = XRES;
        zr = 0;
        zt = YRES;
        zb = 0;

        for (k=0; k<=topbc; ++k) {

            p = cl + bc[k];
            if (p->l < zl)
                zl = p->l;
            if (p->r > zr)
                zr = p->r;
            if (p->t < zt)
                zt = p->t;
            if (p->b > zb)
                zb = p->b;
        }

        zones = 1;
        limits[0] = zl;
        limits[1] = zt;
        limits[2] = zl;
        limits[3] = zb;
        limits[4] = zr;
        limits[5] = zb;
        limits[6] = zr;
        limits[7] = zt;

        // TODO: redraw_zone = 1;
        snprintf(lb,MMB,"barcode found, sk=%3.2f rad, barlen=%3.2f mm",ms,ml);

        /* try to centralize */
        if (dw[PAGE].v) {
            CDW = PAGE;
            X0 = ((zl+zr) - HR) / 2;
            Y0 = ((zb+zt) - VR) / 2;
            check_dlimits(0);
            redraw_document_window();
        }
    }
    else
        snprintf(lb,MMB,"no barcode found");

    lb[MMB] = 0;
    show_hint(2,lb);
    /*enter_wait(lb,1,1);*/

    /* free buffers */
    c_free(bc);

    return(0);
}

/*

Macro used by draw_circ.

*/
#define incr_topcircp { \
    if (++topcircp >= circp_sz) { \
        circp_sz = topcircp + 256; \
        circpx = c_realloc(circpx,circp_sz*sizeof(short),NULL); \
        circpy = c_realloc(circpy,circp_sz*sizeof(short),NULL); \
    } \
}

/*

Draw a circumference with center (0,0) and radius r. The pixels
are stored as (circpx[t],circpy[t]), (circpx[t+1],circpy[t+1]),
..., where t is topcircp at the moment where draw_circ was
called. The arrays circpx and cricpy are enlarged when
necessary. Returns the total number of points or -1 if the radius
is invalid (negative).

*/
int draw_circ(int r)
{
    int x,x2,r2,y,y2,px;
    int i,t,mt;

    /* invalid radius */
    if (r < 0)
        return(-1);

    /* last top */
    t = topcircp;

    /* particular case */
    if (r == 0) {
        incr_topcircp;
        circpx[topcircp] = circpy[topcircp] = 0;
        return(1);
    }

    /* prepare */
    r2 = r * r;
    x = r;
    x2 = r2;
    y = y2 = 0;

    /* draw the arc from 0 to PI/4 */
    while (x >= y) {

        /*
            square of the intersection of the arc and the
            horizontal line at height y.
        */
        px = r2 - y2;

        /*
            Compute the best integer approximation of the
            square root of px. As the best approximation is
            x or (x-1), we must decide if x must be
            decreased or not. The condition (x2-x) >= px
            suffices because it implies both x > sqrt(x)
            and

                x2-x >= px   =>   4*x2+4x >= 4*px
                             =>   (2*x+1)*(2*x+1) > 4*px
                             =>   2*x+1 > 2*sqrt(px)
                             =>   x-sqrt(px) > sqrt(x)-(x+1)

        */
        if ((x2-x) >= px) {
            x2 -= 2*x-1;
            --x;
        }

        /* add point (x,y) to the arc */
        if (x >= y) {
            incr_topcircp;
            circpx[topcircp] = x;
            circpy[topcircp] = y;
            y2 += 2*y+1;
            ++y;
        }
    }

    /*
        draw the arc from PI/4 to PI/2.
    */
    for (i=topcircp; i > t; --i) {
        if (circpx[i] != circpy[i]) {
            incr_topcircp;
            circpx[topcircp] = circpy[i];
            circpy[topcircp] = circpx[i];
        }
    }

    /*
        draw the arc from PI/2 to PI.
    */
    for (mt=topcircp, i=t+2; i <= mt; ++i) {
        incr_topcircp;
        circpy[topcircp] = circpx[i];
        circpx[topcircp] = -circpy[i];
    }

    /*
        draw the arc from PI to 2*PI.
    */
    for (mt=topcircp, i=t+2; i < mt; ++i) {
        incr_topcircp;
        circpx[topcircp] = -circpx[i];
        circpy[topcircp] = -circpy[i];
    }

    /* number of points added */
    return(topcircp-t);
}

/*

Dump the points (vx[0],vy[0]),(vx[1],vy[1]),...,(vx[t-1],vy[t-1])
to the terminal.

*/
void dump_points(short *vx,short *vy,int t,int r)
{
    int i,j;
    char a[61];

    if ((r<0) || (2*r > 60))
        return;

    for (j=-r; j<=r; ++j) {

        memset(a,' ',2*r+1);
        a[2*r+1] = 0;

        for (i=0; i<t; ++i) {
            if (vy[i] == j) {
                if ((-r <= vx[i]) && (vx[i] <= r))
                    a[vx[i]+r] = 'X';
            }
        }
        printf("%s\n",a);
    }
}

/*

Compute circumferences.

*/
void comp_circ(int max)
{
    int r;

    /* pointers to the start of each circumference at circpx and circpy */
    circ_sz = (max + 1);
    circ = c_realloc(circ,circ_sz * sizeof(int),NULL);
    circ_np = c_realloc(circ_np,circ_sz * sizeof(int),NULL);

    /* compute each circumference */
    for (r=0; r<=max; ++r) {
        circ[r] = topcircp + 1;
        circ_np[r] = draw_circ(r);

        /* for tests only */
        /*
        printf("%d points for radius %d\n",circ_np[r],r);
        dump_points(circpx+circ[r],circpy+circ[r],circ_np[r],r);
        */
    }
}

/*

Decides if (i,j) is an an extremity pixel. Returns 1 if yes, 0
otherwise. All computations are relative to the buffer cb, that is,
the image analysed must be stored on cb and (i,j) are cb-relative
coordinates.

Current problems:
-----------------

1. 600 dpi hardcoded parameters (see the section "parameters for
extremity pixels" and the comments along the source code).

2. Actually good for sans-serif.

*/
int is_extr(int i,int j,int dx,int dy)
{
    /* flags */
    int f,tag_ok;

    /* radiuses */
    int r,r0;

    /* counters */
    int b;

    /* per-radius results */
    int *rho,*theta;

    /* display flags */
    int d1=0,d2=0;

    /* per-radius results */
    rho = alloca(circ_sz*sizeof(int));
    theta = alloca(circ_sz*sizeof(int));

    /*
        Search the smallest radius r>=4 for which the intersection
        of the circumference of radius r centered at (i,j) with
        the closure is a unique arc with at least extr_mal pixels.

        In the figure, (i,j) is '#', the black pixels of the closure
        are the X's (an acute accent), the circumference of radius r
        is drawn using +'s and *'s.

                                         XXXXXXX
                                        XXXXXXXXX
                                     XXXXXXXXXXXXX
                                    XXXXXXXXXXXXXX
                                 XXXXXXXXXXXXXXXXX
                               XXXXXXXXXXXXXXXXXXX
                             XXXXXXXXXXXXXXXXXXXXXX
                          XXXXXXXXXXXXXXXXXXXXXXXXX
              +++++   XXXXXXXXXXXXXXXXXXXXXXXXXXXX
            ++     +*XXXXXXXXXXXXXXXXXXXXXXXXXXXXX
           +     XXXX*XXXXXXXXXXXXXXXXXXXXXXXXXXXX
          +   XXXXXXXX*XXXXXXXXXXXXXXXXXXXXXXXXXX
          +  XXXXXXXXX*XXXXXXXXXXXXXXXXXXXXXXXXX
         +   XXXXXXXXXX*XXXXXXXXXXXXXXXXXXXXXXX
         +  XXXXXXXXXXX*XXXXXXXXXXXXXXXXXXXXX
         +   XXX#XXXXXX*XXXXX
         +   XXXXXXXXXX*X
         +    XXXXXXX  +
          +           +
          +           +
           +         +
            ++     ++
              +++++

        The intersection between the circumference and the closure is
        the arc drawn with *'s. This arc can be parametrized as
        (theta,rho), where theta is the index of the first pixel
        (counterclockwise) and rho is the number of pixels. To
        understand what is the index of the first pixel, remember
        that the circumferences are precomputed by comp_circ as a
        per-radius list of pixels, the first one being (r,0). So
        on the example above the intersection can be parametrized as
        (39,8).
    */
    for (r0=-1, f=1, r=extr_isr; f && (r<=extr_ssr); ++r) {
        short *x,*y;
        int t,u,v,st;

        /*
            The statuses carried by st along the traverse are:

            0 - all blank
            1 - at first segment, started at 0
            2 - at first segment, did'nt start at zero
            3 - at first interval, started at 0
            4 - at first interval, didn't start at 0
            5 - at second segment, started at zero
            6 - too complex
        */
        st = 0;

        /* first pixel to visit, that is (r,0) */
        x = circpx + circ[r];
        y = circpy + circ[r];

        /* visit all circumference pixels */
        for (b=0, t=circ_np[r]; (st<6) && (t>0); --t,++x,++y) {

            /* cb-relative coordinates */
            u = i + *x;
            v = j + *y;

            /* found black pixel */
            if ((0<=u) && (u<FS) && (0<=v) && (v<FS) && (cb[u+v*LFS]==BLACK)) {
                ++b;

                /* first intersection began */
                if (st == 0) {
                    theta[r] = circ_np[r] - t;
                    if (t == circ_np[r])
                        st = 1;
                    else
                        st = 2;
                }

                /* second intersection began */
                else if (st == 3) {
                    theta[r] = circ_np[r] - t;
                    st = 5;
                }
                else if (st == 4)
                    st = 6;
            }

            /* first intersection finished */
            else if (st == 1)
                st = 3;
            else if (st == 2)
                st = 4;

            /* second intersection finished */
            else if (st == 5)
                st = 6;
        }

        /*
            final result for radius r is -1 or the
            intersection size otherwise.
        */
        if ((1 <= st) && (st <= 5) && (b >= extr_mal) && (2*b < circ_np[r])) {
            rho[r] = b;
            r0 = r;
            f = 0;
        }
        else {
            rho[r] = -1;
        }
    }

    /* no intersection found */
    if (r0 < 0) {
        tag_ok = 0;

        show_hint(0,"not a extremity, no intersection");
    }

    else {

        /*
            Traverse the circumference of radius r0 again in order
            to paint the intersection GRAY.
        */
        if (d1) {
            short *x,*y;
            int t,u,v;

            /* first pixel to visit, that is (r,0) */
            x = circpx + circ[r];
            y = circpy + circ[r];

            /* visit all circumference pixels */
            for (b=0, t=circ_np[r]; t>0; --t,++x,++y) {

                /* cb-relative coordinates */
                u = i + *x;
                v = j + *y;

                /* found black pixel */
                if ((0<=u) && (u<FS) && (0<=v) && (v<FS) && (cb[u+v*LFS]==BLACK)) {
                    int du,dv;

                    /* draw it GRAY */
                    du = u + dx;
                    dv = v + dy;

                    if ((0<=du) && (du<dw[PAGE_FATBITS].hr) &&
                        (0<=dv) && (dv<dw[PAGE_FATBITS].vr)) {

                        cfont[dv*dw[PAGE_FATBITS].hr+du] = GRAY;
                    }
                }
            }
        }

        /*
            Extend the intersection enlarging the radius.
        */
        for (r=r0+1, f=1; f && (r<circ_sz) && ((r-r0)<=extr_mel); ++r) {
            short *x,*y;
            int t,t1,u,v;

            /* compute medium pixel for the last radius */
            t = theta[r-1] + rho[r-1]/2;
            t1 = t %= circ_np[r-1];

            /* obtain the corresponding one for the current radius */
            t = (((float)t)/circ_np[r-1]) * circ_np[r];
            x = circpx + circ[r] + t;
            y = circpy + circ[r] + t;
            u = i + *x;
            v = j + *y;

            /* it's nonblack, give up */
            if ((u<0) || (FS<=u) || (v<0) || (FS<=v) || (cb[u+v*LFS]!=BLACK))
                f = 0;

            /*
                Extend the pixel t to the intersection of the
                circumference of radius r and center (i,j) with the
                closure that contains t.
            */
            else {
                int a,la=0,b,lb=0,c;

                /* draw it GRAY */
                if (d2) {
                    int du,dv;

                    du = u + dx;
                    dv = v + dy;
                    if ((0<=du) && (du<dw[PAGE_FATBITS].hr) &&
                        (0<=dv) && (dv<dw[PAGE_FATBITS].vr)) {

                        cfont[dv*dw[PAGE_FATBITS].hr+du] = GRAY;
                    }

                }

                /* search bottommost black pixel */
                for (a=t, c=1; c && f; ) {

                    /* move back pointers */
                    la = a;
                    if (--a < 0) {
                        a = circ_np[r]-1;
                        x = circpx + circ[r] + a;
                        y = circpy + circ[r] + a;
                    }
                    else {
                        --x;
                        --y;
                    }

                    /* back to the origin! */
                    if (a == t)
                        f = 0;

                    /* non-black pixel: stop */
                    u = i + *x;
                    v = j + *y;
                    if ((u<0) || (FS<=u) || (v<0) || (FS<=v) || (cb[u+v*LFS]!=BLACK))
                        c = 0;
 
                    /* draw it GRAY */
                    else if (d2) {
                        int du,dv;
			
                        du = u + dx;
                        dv = v + dy;
                        if ((0<=du) && (du<dw[PAGE_FATBITS].hr) &&
                            (0<=dv) && (dv<dw[PAGE_FATBITS].vr)) {

                            cfont[dv*dw[PAGE_FATBITS].hr+du] = GRAY;
                        }
                    }
                }

                /* last valid pointer */
                a = la;

                /* search upmost black pixel */
                x = circpx + circ[r] + t;
                y = circpy + circ[r] + t;
                for (b=t, c=1; c && f; ) {

                    /* advance pointers */
                    lb = b;
                    if (++b>=circ_np[r]) {
                        b = 0;
                        x = circpx + circ[r];
                        y = circpy + circ[r];
                    }
                    else {
                        ++x;
                        ++y;
                    }

                    /* back to the origin! */
                    if (b == t)
                        f = 0;
                    u = i + *x;
                    v = j + *y;

                    /* non-black pixel: stop */
                    if ((u<0) || (FS<=u) || (v<0) || (FS<=v) || (cb[u+v*LFS]!=BLACK))
                        c = 0;

                    /* draw it GRAY */
                    else if (d2) {
                        int du,dv;
			
                        /* draw it GRAY */
                        du = u + dx;
                        dv = v + dy;

                        if ((0<=du) && (du<dw[PAGE_FATBITS].hr) &&
                            (0<=dv) && (dv<dw[PAGE_FATBITS].vr)) {

                            cfont[dv*dw[PAGE_FATBITS].hr+du] = GRAY;
                        }
                    }
                }

                /* ajust limit */
                b = lb;

                /* obtain (theta,rho) parameters for radius r */
                if (b > a) {
                    theta[r] = a;
                    rho[r] = b-a+1;
                }
                else {
                    theta[r] = a;
                    rho[r] = (b+1) + (circ_np[r]-a);
                }

                /* too large variation */
                if (abs(rho[r]-rho[r-1]) > extr_mvr)
                    f = 0;
            }
        }

        /* too few radiuses */
        if ((f==0) || (r-r0 < extr_mel)) {
            tag_ok = 0;

            show_hint(0,"not a extremity, only %d radiuses",r-r0);
        }

        /* analyse how the size of the intersections vary along the radiuses */
        else {
            int x,y,sx,sy,sx2,sy2,sxy;
            int t;
            float sl,N;

            /* liner regression */
            sx = sy = sx2 = sy2 = sxy = 0;
            for (t=r0; t<r; ++t) {
                x = t-r0;
                y = rho[t];
                sx  += x;
                sx2 += (x*x);
                sy  += y;
                sy2 += (y*y);
                sxy += (x*y);
            }

            /* slope */
            N = r - r0;
            sl = (sxy - sx*sy/N) / (sx2 - sx*sx/N);

            /* soft enough */
            tag_ok = (fabs(sl) < extr_msl);

            if (tag_ok)
                show_hint(0,"found extremity (%d,%d) r0=%d r=%d sl=%1.3f: ",i,j,r0,r,sl);
            else
                show_hint(0,"not a extremity sl=%1.3f",sl);
        }
    }

    return(tag_ok);
}

/*

Detect extremities of closure k and returns the number of
extremities found.

*/
int dx(int k,int i0,int j0)
{
    /* the closure */
    cldesc *m;
    int w,h;

    /* indexes and counters */
    int i,j,n;

    /* closure displacement relative to FATBITS window */
    int dx,dy;

    /* never heard about this closure */
    if ((k < 0) || (topcl < k)) {
        db("invalid call to dx");
        return(-2);
    }

    /* geometry */
    m = cl + k;
    w = m->r - m->l + 1;
    h = m->b - m->t + 1;

    /* top left relative to the visible grid */
    dx = m->l - dw[PAGE_FATBITS].x0;
    dy = m->t - dw[PAGE_FATBITS].y0;

    /* copy bitmap to cb */
    memset(cb,WHITE,LFS*FS);
    add_closure(cl+k,0,0);

    /* number of extremities found */
    n = 0;

    /* pixel-specific call */
    if (i0>=0) {

        /* prepare display */
        copychar();

        if (is_extr(i0,j0,dx,dy)) {
            /* printf("(%d,%d) is extremity\n",i0,j0); */
        }
        else {
            /* printf("(%d,%d) is not extremity\n",i0,j0); */
        }
    }

    /* for each black pixel */
    else {
        for (i=0; i<FS; ++i) {
            for (j=0; j<FS; ++j) {

                /* ignore non-BLACK pixels */
                if (cb[i+j*LFS] != BLACK)
                    continue;

                if (is_extr(i,j,dx,dy)) {
                    ++n;
                }

                /* didn't find */	
                else {
                    int u,v;
	
                    u = (i + dx);
                    v = (j + dy);

                    if ((0<=u) && (u<dw[PAGE_FATBITS].hr) &&
                        (0<=v) && (v<dw[PAGE_FATBITS].vr)) {

                        cfont[v*dw[PAGE_FATBITS].hr+u] = GRAY;
                        m = 0;
                    }

                }
            }
        }
    }

    redraw_document_window();
    return(n);
}
