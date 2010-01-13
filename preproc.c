/*
  Copyright (C) 2002 Giulio Lunati <dongiulio@iol.it>

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

preproc.c: Deskewing, balancing, thresholding  routines.

*/

#define STAT 0

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "common.h"
#include "gui.h"

#define MSG(args...) fprintf(stderr, ## args)
#define CKP(M) fprintf(stderr,"[CKP %d OK!]\n",M) 

#define DIV(a,b) (((a)+abs(b)/((a)>0?2:-2))/(b))
#define MIN(A,B) ((A)<(B)?(A):(B))
#define MAX(A,B) ((A)>(B)?(A):(B))
#define TRIM(X,A,B) MIN(MAX((X),(A)),(B))
/* interpolation */
#define ITP(A,B,R)  (1-R)*(A)+(R)*(float)(B)

#define MAXVAL 255
#define MAXHIST MAX(LFS+1,MAXVAL)

/* compatability stuff */
float modff(float A,float *B)
{
    double x;
    float r;

    r = modf((A),&x);
    *B = x;
    return(r);
}

/****************************** PIXMAP STUFF ******************************/

#define PIX(x,y) (pixmap+(x)+(y)*XRES)
#define NPIX(p) ++(p)
/* 
  
  return interpolated pixel
  x,y: coordinates
  map: pointer to graymap
  X: x-width of map
  Y: y-width of map
   
*/

inline int PIXFI(float x, int y, unsigned char *map, int X, int Y) 
{
  int ix;
  unsigned char *p;
  
  x=TRIM(x,0,X-1);
  y=TRIM(y,0,Y-1);
  x-=(ix=floor(x));
  p=map+ix+y*X;
  ix=rintf(ITP(*p,*(NPIX(p)),x));
  return(ix);
}

/****************************** HISTOGRAM STUFF ******************************/

float h0[MAXHIST],h1[MAXHIST];
float H_dark,H_light,H_thr;
float h_cnt,h_mean,h_var,h_dens;

char *graf(float i, float min, float max, int wid) 
{
  static char c[200];
  char *p;
  float f;

  f=(i-min)*wid/(max-min);
  for (p=c; f>0; f--, p++) *p='*';
  *p = 0;
  return(c);
}

void h_print(float *h, int width)
{
  int l,M;
  
  for (l=1,M=h[0]; l<=MAXVAL; l++) if (h[l]>M) M=h[l];
  for (l=0; l<=MAXVAL; l++) 
    if (h0[(int)l]!=0) 
    printf("%4d %6.0f %s\n",(int)l,h0[(int)l],graf(h0[(int)l],0,M,70));
}


/* smooth h0[] with radius d and contrast correction f; 
   use h1[] aux buffer ; N = dim of h0[] and h1[] */
void h_smooth(int d, float f, int N, float *h0, float *h1) 
{
  int i;
  float s;
  
  for (s=0,i=0; i<=d; i++) s+=h0[i];
  for (i=0; i<N; i++) {
    h1[i]=s/(2*d+1);
    s+=(i+d+1<N? h0[i+d+1] : 0);
    s-=(i-d>=0? h0[i-d] :0);
  }
  for (i=0; i<N; i++) h0[i]=h1[i];
  
  if (f==0) return;
  /* contrast correction */
  f=1/3+1/2/d-1/d/d;
  for (i=0; i-d<0; i++)
    h0[i]=h1[i]-f*(2*h1[i]-h1[0]-h1[i+d]);
  for (; i+d<N; i++)
    h0[i]=h1[i]-f*(2*h1[i]-h1[i-d]-h1[i+d]);
  for (; i<N; i++)
    h0[i]=h1[i]-f*(2*h1[i]-h1[i-d]-h1[N-1]);
}

int h_thr_stat() 
{
  int i,I; 
  float m0,m1,m2,M0,M1,M2,s,S;
  
  for(i=0; i<=MAXVAL; i++) {
    h1[i]=log(1+h0[i]);
  }
  for(i=M0=M1=M2=0; i<=MAXVAL; i++) {
    M0+=h1[i];
    M1+=h1[i]*i;
    M2+=h1[i]*i*i;
  }
  S =(M2-M1*M1/M0);
  for(i=I=m0=m1=m2=0; i<=MAXVAL; i++) {
    m0+=h1[i];
    m1+=h1[i]*i;
    m2+=h1[i]*i*i;
    s =(m2-m1*m1/m0);
    s+=((M2-m2)-(M1-m1)*(M1-m1)/(M0-m0));
    if (s<S) S=s, I=i;
  }
  return(I);
}

int h_split()
{
  int I,i/*,n*/;
  float M,m0,m1,m2;
  /*double p,d,l;*/
  
  for (m0=m1=m2=i=0; i<=MAXVAL; i++) {
    m0+=h0[i];
    m1+=h0[i]*i;
    m2+=h0[i]*i*i;
  }
  m2=sqrt((m2-m1*m1/m0)/m0);
  m1/=m0;

  h_smooth(m2/4,0,MAXVAL,h0,h1);
  h_smooth(m2/4,0,MAXVAL,h0,h1);

  for (i=MAXVAL; i>0; i--) {
    if (h0[i]>1 && h0[i]>h0[i-1]) break;
  }
  H_light=i;

  for (i=0; i+1<=MAXVAL; i++) {
    if (h0[i]>1 && h0[i]>h0[i+1]) break;
  }
  H_dark=i; 
  
  for (I=i=H_dark, M=h0[i]; i<=H_light; i++) {
    if (h0[i]<M) I=i,M=h0[i];
  }
  H_thr=I;
  return H_thr;
}

int h_dark()
{
  int i,n;
  double F;
  
  for (i=F=n=0; i<=MAXVAL; i++) {
    n+=h0[i];
    F+=h0[i]*exp(-i);
  }
  F=-log(F/n);
  return F;
}

/****************************** DESKEW STUFF ******************************/

int Cols;

float skew_score(float F, float Delta)
{ 
#define NP 10
  float y,dy,t,s,q,l,m,delta,n;
  int x,c,dx,h,N,i,w;
  float *p;

  w=XRES/Cols/3;
  dx=w/NP+1; /* max NP test-point per row */
  h=abs(F*w)+2; 

  p=alloca(NP*sizeof(*p));
  for (i=c=0; c<w; c+=dx, i++) 
    p[i]=F*c;

  N=n=s=q=delta=0; 
  for ( dy=YRES/20; 
        (delta>=n*Delta) && dy >=1; 
	dy*=0.618
      ) 
  {
    l= (N>0? s/N : -1);
    for (x=3*w/2; x+3*w/2<XRES; x+=3*w) 
    for (y=h+dy/2; y+h<YRES; y+=dy) 
    {

      /* m = min */
      for (m=MAXVAL,i=c=0,t=0; c<w; c+=dx,i++) 
	if(m>(t=*PIX( (int)(x+c),(int)(y+p[i]) ))) m=t;
      for (i=c=0; c<w; c+=dx,i++) 
	if(m>(t=*PIX( (int)(x-c),(int)(y-p[i]) ))) m=t;
      s += m;
      N++;
    }
#define R 0.5
      delta*=R; delta+=fabs(s/N-l);
      n*=R; n++;
    if (Delta <0 && l>=0) break;
  }
  return s/N;
  exit(0);
#undef NP
#undef R
}

float skew_calc(float Eps)
{
#define MaxSkew 0.2
#define R 0.618034
  float x0,x1,y1,y0,D;

  D=0;
  x1=MaxSkew, x0=-x1;
  for (
    x1=(x1-x0)*R+x0, y1=skew_score(x1,D),
      x0=(x1-x0)*R+x0, y0=skew_score(x0,D);
    fabs(x1-x0)>Eps;
    D=(pp_deskew_accurate? 0 : fabs(y1-y0)/30)
  ) {
    if        (y0<y1) x0=x1/R-x0*R, y0=skew_score(x0,D); 
      else if (y0>y1) x1=x0/R-x1*R, y1=skew_score(x1,D);
      else    break;
  }  
  return (x0+x1)/2;
  exit(0);
#undef MaxSkew
#undef R
}


void pp_shear_y(float f)
{
 int D,x,y,s,x0,x1;
 float *m1,m0;
 unsigned char *p;
 int *d;

 s=abs((int)(f*(XRES-1)));
 if (s==0) return;
 s=s++/2;
 p= alloca(YRES);

 d= alloca(XRES*sizeof(d));
 m1= alloca(XRES*sizeof(*m1));
 
  
 D=XRES/2;
 for (x=0; x<XRES; x++)
 {
   /* calc shear parameters for x column dy=d[x]+m1[x] */
   if ( ( m1[x]=modff(f*(x-D),&m0) ) < 0 ) m0--,m1[x]++;
   d[x]=m0;
 }

 /* shear up half map (x0<=x<x1) */
 if (f>0) x0=XRES/2+1, x1=XRES;
     else x0=0, x1=XRES/2;

 for (y=0; y+s<YRES; y++)
 for (x=x0; x<x1; x++) {
   D=y+d[x]; 
   *PIX(x,y)=(float)0.5+ITP(*PIX(x,D),*PIX(x,++D),m1[x]);
 }

 for (; y<YRES; y++)
 for (x=x0; x<x1; x++) {
   D=y+d[x]; D=MIN(D,YRES-2);
   *PIX(x,y)=(float)0.5+ITP(*PIX(x,D),*PIX(x,++D),m1[x]); 
 }

 /* shear down half map (x0<=x<x1) */
 if (f<0) x0=XRES/2+1, x1=XRES;
     else x0=0, x1=XRES/2;

 for (y=YRES-1; y>=s; y--) {
   for (x=x0; x<x1; x++) {
     D=y+d[x];
     *PIX(x,y)=(float)0.5+ITP(*PIX(x,D),*PIX(x,++D),m1[x]);
   }
 }

 for (; y>=0; y--)
 for (x=x0; x<x1; x++) {
   D=y+d[x]; D=MAX(D,1);
   *PIX(x,y)=(float)0.5+ITP(*PIX(x,D),*PIX(x,++D),m1[x]);
 }
}


void pp_shear_x(float f)
{
 int D,d,x,y,s;
 float m0,m1;
 unsigned char *p;
 
 s=(int)(f*(YRES-1));
 if (s==0) return;
 p= alloca(XRES);
 D=-s/2;
 for (y=0; y<YRES; y++)
 {
   if ( (m1=modff(f*y+D,&m0)) < 0 ) m0--,m1++;
   d=m0; 
   if (d<0) {
     for (x=0; x+d<0 ; x++) p[x]=*PIX(0,y);
     for (; x<XRES; x++) p[x]=*PIX(x+d,y);
   } else {
     for (x=0; x+d<XRES; x++) p[x]=*PIX(x+d,y);
     for (; x<XRES; x++) p[x]=*PIX(XRES-1,y);
   }
   for (x=0; x<XRES-1; x++) 
     *PIX(x,y)= ITP(p[x],p[x+1],m1);
   *PIX(x,y)=p[x];
 }
}

/*

deskew 
  pixmap: graymap pointer
  XRES,YRES: width,heigth of graymap
  
*/
int deskew(int reset)
{
    static float f,a,b,c;
    static int st;
    int r;

    /* default to incomplete */
    r = 1;
 
    if (reset) {
        st = 1;
    }

    else if (st == 1) {
	Cols = TC;
        show_hint(2,"DESKEW: detecting...");
        st = 2;
    }

    else if (st == 2) {
        f=skew_calc(0.5/XRES);
        show_hint(2,"DESKEW: found <%f>, now rotating",f);
        st = 3;
    }

    else if (st == 3) {
        if (fabs(f)<0.1 && !pp_deskew_accurate) { /* fast: quasi-rotation */
	  a=0; b=f; c=-f;
	}
	else { /* exact rotation */
          c=a = -(hypot(1,f)-1)/f;
          b = -2*a/(a*a+1);
        }
        pp_shear_x(a);
        pp_shear_y(b);
        pp_shear_x(c);
        show_hint(2,"DESKEW: finished");
        st = 0;
        r = 0;
    }

    return(r);
}

/****************************** AUTOTHRESHOLD STUFF ******************************/

int pp_thresh()
{
 int x,y; 
 static int st;
 
 {
     if (pm_t != 8) return 128;
     show_hint(2,"computing threshold...");
     st = 2;
     for (x=0; x<=MAXVAL; x++) h0[x]=0;
     for (y=0; y<YRES; y++) 
     for (x=0; x<XRES; x++) 
       h0[*PIX(x,y)]++;
     h_split();H_thr*=pp_dark;
     /*
     for (x=0; x<=MAXVAL; x++) printf("%4d: %8.0f %s\n",x,h0[x],graf(log(1+h0[x]),0,10,60));
     printf("<PP_THRESH: %f,%f,%f>\n", H_dark, H_thr, H_light);
     */
     return(H_thr);
 }

}

/****************************** BALANCE STUFF ******************************/

/* light icon */
int Dx,Dy,Iw,Ih,Rx,Ry;
float M,Q,c;
unsigned char *Icon;

/*

balance:   convert the pixmap to b/n, based on local darkness

*/
void pre_balance()
{
  /* counters */
  int x,y,x0,x1,y0,y1,i,j,dx,dy;
  /* for  linear regression */
  float sn,st,sl,stt,sll,stl;
  /* for balancing */
  float t,l;
  unsigned char *p;

  /* init  linear regression */
  sn=sl=st=sll=stt=stl=0;

  /* init icon */
  /* steps for icon */
  Ry=(LS+LH)*DENSITY/25.4;  Rx=(LW*2)*DENSITY/25.4;
  /* initial corner for icon */
  Dx=((XRES-1)%Rx)/2;   Dy=((YRES-1)%Ry)/2; 
  /* width and height of threshold  map */
  Iw=(XRES-1)/Rx+1;  Ih=(YRES-1)/Ry+1;
  dx=Rx/40+1; dy=Ry/40+1;
  /* temp buffer for icon */
  Icon=malloc(Iw*Ih);

  /* calculate icon */
  for (y=0; y<Ih; y++) {
    for (x=0; x<Iw; x++)  {
      /* populate histogram */
      x0=(Dx+x*Rx-Rx<0 ? 0 : Dx+x*Rx-Rx);
      y0=(Dy+y*Ry-Ry<0 ? 0 : Dy+y*Ry-Ry);
      x1=(Dx+x*Rx+Rx>=XRES ? XRES : Dx+x*Rx+Rx);
      y1=(Dy+y*Ry+Ry>=YRES ? YRES : Dy+y*Ry+Ry);
      for (i=0; i<=MAXVAL; i++) h0[i]=0;
      for(j=y0; j<=y1; j+=dy) 
      for(i=x0,p=PIX(x0,j); i<=x1; i+=dy,p+=2) 
	h0[*p]++;
      h_split();
#if 0
     if (x==0 && y==0) for (i=0;i<MAXVAL; i++) {
       printf("%4d: %8.2f %s\n",i,h0[i],graf(log(1+h0[i]),0,10,60));
     }
     MSG("%3d,%3d: %4.0f,%4.0f,%4.0f,%4.2f\n",x,y,H_dark,H_thr,H_light,(H_thr-H_dark)/(H_light-H_dark));
#endif
      Icon[x+y*Iw]=l=H_light; ;
      t=H_thr;
      if (t<l) sn++,sl+=l,st+=t,stt+=t*t,sll+=l*l,stl+=t*l; /* linear regression*/
    }
  }

  M=(sn*stl-st*sl)/(sn*sll-sl*sl);
  Q=(st-M*sl)/sn;
}
  
void balance()
{
#define FIXP *p=(*p<M*t+Q? 0 : MAXVAL)
#define FIXP *p=TRIM(t,0,MAXVAL)
#define FIXP t=M*l+Q; *p=TRIM( (((float)*p-t)/(l-t)/2+0.5)*MAXVAL , 0 , MAXVAL );

  float r,t,l, *r1, *r2;
  unsigned char *p;
  int x,y,i;

  pre_balance();
  
  r1=alloca(XRES*sizeof(*r1));
  r2=alloca(XRES*sizeof(*r2));

  for (x=0; x<XRES; x++) r1[x]= PIXFI((float)(x-Dx)/Rx,0,Icon,Iw,Ih);
  for (y=0; y<=Dy; y++) {
    for (x=0,p=PIX(0,y); x<XRES; x++,p++) { 
      l=r1[x];
      FIXP;
    }
  }
  for (i=Dy; i+Ry<YRES; i+=Ry) {
    for (x=0; x<XRES; x++) {
      r2[x]= PIXFI((float)(x-Dx)/Rx,(i-Dy)/Ry+1,Icon,Iw,Ih);
    }
    for (y=i+1; y<=i+Ry; y++) {
      r=(float)(y-i)/Ry;
      for (x=0,p=PIX(0,y); x<XRES; x++,p++) {
        l=ITP(r1[x],r2[x],r);
	FIXP;
      }
    }
    memcpy(r1,r2,XRES*sizeof(*r1));
  }
  for (; y<YRES; y++)
  for (x=0,p=PIX(0,y); x<XRES; x++,p++) {
    l=r1[x];
    FIXP;
  }
  redraw_dw=1;
}

/***************************** HQ BINARIZATION STUFF *******************************/
/* memcpy with border! */
#define MC(R,P) memcpy((R)+1,P,XRES), *(R)=*((R)+1), *((R)+XRES+1)=*((R)+XRES);

/* 
  try detect and delete links between symbols
  C= strong (normal 0<=C<=1) 
*/
void avoid_links(float C)
{
  int i,j,bpl,t;
  unsigned char *p1, *r1, *p2, *r2, *p3, *r3,  /* for input on 3 rows */
              *q;  /* for output */
  float v,a,b,dxx,dx,dy,dyy; 

  if (C==0) return;

  C*=100;
  q = pixmap; 
  r1=alloca(XRES+2);
  r2=alloca(XRES+2);
  r3=alloca(XRES+2);
  bpl = (XRES/4) + ((XRES%4) != 0); /* byte per line */

  MC(r2,pixmap);
  MC(r3,pixmap);
  for (j=0; j<YRES; j++) {
    memcpy(r1,r2,XRES+2);
    memcpy(r2,r3,XRES+2);
    if (j+1<YRES) MC(r3,pixmap+(j+1)*XRES);
    q=pixmap+j*XRES; 
    memset(q,0,XRES);
    for (i=0, p1=r1+1,p2=r2+1,p3=r3+1; i<XRES; ++i, ++p1,++p2,++p3,++q) {
      v=*p2; t=0; 
      if (v<=thresh_val) {
        a=(*(p2+1)+*(p3+1)+*(p1+1))/6;
	b=(*(p2-1)+*(p3-1)+*(p1-1))/6;
	dxx=a+b-(*p2+*p3+*p1)/3;
	dx=a-b;
        a=(*(p1+1)+*(p1)+*(p1-1))/6;
	b=(*(p3-1)+*(p3)+*(p3+1))/6;
	dyy=a+b-(*p2+*(p2+1)+*(p2-1))/3;
	dy=a-b;
	if (dyy >=0) {
	   t=MIN(0,dyy/2+dxx);
	   v=MIN(MAXVAL,v-C*t);
	}
      }
      *q=v;
    }
  } 
  return;
}

void test() 
{  
  pp_thresh();
  avoid_links(100);
  hqbin();
  redraw_dw=1;
}

/*
  re-write pixmap in 1-bit format, double resolution (with intepolation)
*/
void hqbin()
{
  int i,j,m,bpl;
  unsigned char *p1, *r1, *p2, *r2, *p3, *r3,  /* for input on 3 rows */
              *q1, *q2;  /* for output on 2 rows*/
  float d;
  
  q1=q2 = pixmap; 
  r1=alloca(XRES+2);
  r2=alloca(XRES+2);
  r3=alloca(XRES+2);
  bpl = (XRES/4) + ((XRES%4) != 0); /* byte per line */

  MC(r2,pixmap);
  MC(r3,pixmap);
  for (j=0; j<YRES; j++) {
    memcpy(r1,r2,XRES+2);
    memcpy(r2,r3,XRES+2);
    if (j+1<YRES) MC(r3,pixmap+(j+1)*XRES);
    q1=pixmap+2*j*bpl; 
    q2=q1+bpl;
    memset(q1,0,2*bpl);
    for (i=0, m=128, p1=r1+1,p2=r2+1,p3=r3+1; i<XRES; ++i, ++p1,++p2,++p3) {
      d=(*p2*3+*(p2-1)+*p1)*3+*(p1-1);
      if (d< thresh_val*16)   *q1 |= m;
      d=(*p2*3+*(p2-1)+*p3)*3+*(p3-1);
      if (d< thresh_val*16)   *q2 |= m;
      m>>=1;
      d=(*p2*3+*(p2+1)+*p1)*3+*(p1+1);
      if (d< thresh_val*16)   *q1 |= m;
      d=(*p2*3+*(p2+1)+*p3)*3+*(p3+1);
      if (d< thresh_val*16)   *q2 |= m;
      if ((m>>=1) == 0) {
	  m = 128;
	  ++q1; 
	  ++q2; 
      }
    }
  }
  pm_t=1;
  XRES*=2;
  YRES*=2;
  DENSITY *= 2;
  /* WARNING: should realloc the pixmap */
}

#undef MC

/************************ ANTI - DISTORSION STUFF ********************/

void adist(int x0, int x1)
{
  int i,y;
  unsigned char *p;

  float A;

  A=(float)(x0-x1)/x0/x0;
  
  for (y=0; y<YRES; y++)
  for (i=0,p=PIX(i,y); i<x0; i++,p++) {
    *p=*PIX((int)(A*i*i)+x1,y);
  }
}


#if 0
/****************************** THRASED STUFF ******************************/


int h_thr_min()
{
  int i,I,L,M,m;

  /*for (i=0; i<=MAXVAL; i++)    h0[i]*=h0[i];*/

  h_stat(h0,0);
  h_smooth(sqrt(h_var)/10,h0,h1);

#if 0
  for (i=0; i<=MAXVAL; i++)
    h0[i]=(h0[i]>=1? 30*log(h0[i]): 0);

  for (i=I=M=0; i<=MAXVAL; i++)
  if (h0[i]+i>M && h0[i]>0) I=i, M=h0[i]+i;
  H_light=I;
  
  for (i=I=0,M=-MAXVAL; i<=MAXVAL; i++)
  if (h0[i]-i>M && h0[i]>0) I=i, M=h0[i]-i;
  H_dark=I;
#else
  for (i=I=M=0; i<=MAXVAL; i++)
  if (h0[i]>M) I=i, M=h0[i];
  L=I;

  for (M=0,I=i=-L/2; L+i+i<=L; i++)
  if ((m=h0[L+i+i]-h0[L+i]) > M) M=m, I=i;  
  I=L+I; 

  if(I<L) {
    for (i=I, M=h0[i]; i>=0; i--)
      if (h0[i]>M) I=i, M=h0[i];
    H_light=L; H_dark=I;
  } else {
    for (i=I, M=h0[i]; i<=MAXVAL; i++)
      if (h0[i]>M) I=i, M=h0[i];
    H_light=I; H_dark=L;
  }
#endif
  
  for (i=H_dark, M=h0[i]; i<H_light; i++)
  if (h0[i]<M) I=i, M=h0[i];
  H_thr=I;
  return(H_thr);
}

#if STAT
  /* statistical graymap : t versus v */
  unsigned int s[256][256];
  long int m;
  memset(s,0,sizeof(s));
  m=0;
#endif


#if STAT
	if (++s[(int)(v+0.5)][(int)(t+0.5)]>m) m++;
#endif      

#if STAT
  for (i=0; i<=MAXVAL; i++){
    for (j=0; j<=MAXVAL; j++)
    s[i][j]=rintf(25*log(s[i][j]));
  }
  printf("P2\n256 256 %d\n",(int)rintf(25*log(m)));
  for (i=0; i<=MAXVAL; i++){
    for (j=0; j<=MAXVAL; j++)
      printf("%d ",s[i][j]);
    printf("\n");
  }
#endif      
/******* END TRASHED ******/
#endif      


