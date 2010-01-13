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

html.c: HTML generation and parse

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "common.h"
#include "gui.h"

/* HTML left margin */
int HTML_LM = 0;

/*

Buffers for HTML parse (they store the tag name and
attributes). The parse_cml function reads the next tag or text
and feed these buffers with that.

tag is the tag name, and tagsz is the size of the memory buffer
pointed by tag. This must not be confused with strlen(tag).

attribc is the number of attributes (like argc). This must not be
confused with the size of the memory buffer *attrib. The size of
the memory buffer *attrib is stored on listsz. This is also the
size of the memory buffer *val.

attrib are the attribute names (like argv).

val are the attribute values (like argv).

attribsz[i] are the size of the memory buffer attrib[i]. This
must not be confused with strlen(attrib[i]).

valsz[i] are the size of the memory buffer val[i]. This must not
be confused with strlen(val[i]).

*/
int tagsz = 0;
char *tag = NULL;
int tagtxtsz = 0;
char *tagtxt = NULL;
int attribc = 0;
int listsz = 0;
char **attrib = NULL;
char **val = NULL;
int *attribsz = NULL;
int *valsz = NULL;
char *href=NULL,*type,*src,*value,*name,*action,*bgcolor;
int checked,size,cellspacing,oneword=1;

/*

Debug viewable buffer and associated data.

*/
char *dv=NULL;
int dvsz=0,topdv=-1,dvfl;

#include <printf.h>
int hesc_printf_handler(FILE* stream, const struct printf_info *info, const void *const *args) {
	const char* str = *((const char**)(args[0]));
	int rlen = 0;
	const char* ipos = str;
	for (ipos = str; *ipos; ipos++) {
		switch (*ipos) {
			case '<': rlen += fprintf(stream,"%s","&lt;"); break;
			case '>': rlen += fprintf(stream,"%s","&gt;"); break;
			case '&': rlen += fprintf(stream,"%s","&amp;"); break;
			default: rlen += fprintf(stream,"%c",*ipos); break;
		}
	}
	return rlen;
}
int hesc_printf_arginfo(const struct printf_info *info, size_t n, int *argtypes) {
	if (n > 0) {
		argtypes[0] = PA_POINTER;
	}
	return 1;
}

void __attribute__((constructor)) setup_hesc_printf() {
	register_printf_function('H',hesc_printf_handler,hesc_printf_arginfo);
}
/*

Test if the character is a separator.

*/
int htsep(int c)
{
    return((c == ' ') ||
           (c == '\t') ||
           (c == '\r') ||
           (c == '\n'));
}

/*

Get and store next attribute when parsing an HTML tag. Returns 1
if an attribute (with or without value) was read, or 0 if the end
of the tag was reached or -1 if the end of the message or an
invalid syntax was detected.

*/
int get_attrib(char **ht)
{
    char *t,*u,*a,**n,**v;

    t = *ht;

    /* enlarge attrib and val arrays if necessary */
    if (listsz <= attribc) {
        int n,t;

        /* number of entries to add */
        t = attribc - listsz + 16;

        /* pointers to attributes and their values */
        attrib = c_realloc(attrib,(listsz+=t)*sizeof(char *),NULL);
        val = c_realloc(val,listsz*sizeof(char *),NULL);

        /* sizes of attributes and their values */
        attribsz = c_realloc(attribsz,listsz*sizeof(int),NULL);
        valsz = c_realloc(valsz,listsz*sizeof(int),NULL);

        /* initialize pointers and sizes */
        for (n=1; n<=t; ++n) {
            attrib[listsz-n] = NULL;
            val[listsz-n] = NULL;
            attribsz[listsz-n] = 0;
            valsz[listsz-n] = 0;
        }
    }

    /* skip separators */
    for (; *t && htsep(*t); ++t);

    /* find the end of the name */
    for (a=t; *t && !htsep(*t) && (*t!='=') && (*t!='>'); ++t);

    /* end of the tag or name beginning with '=' */
    if (t == a) {
        int r;

        *ht = (*t == 0) ? t : t+1;
        r = *t=='>' ? 0 : -1;
        return(r);
    }

    /* store name */
    n = attrib + attribc;
    if (attribsz[attribc] < t-a+1)
        *n = c_realloc(*n,attribsz[attribc]=t-a+1+512,NULL);
    strncpy(*n,a,t-a);
    (*n)[t-a] = 0;

    /* extract value */
    v = val + attribc;
    if (*t == '=') {

        /* value is enclosed within double quotes */
        if (*(a=++t) == '"') {
            for (++t; *t &&
                      ((*t != '"') || (*(t-1) == '\\')) /* &&
                       (*t!='>') */ ; ++t);
            if (*t != '"')
                return(-1);
            ++t;
        }

        /* search '>' or a separator */
        else {
            for (; *t && !htsep(*t) && (*t!='>'); ++t);
        }

        /* empty value */
        if (t == a) {
            *ht = (*t == 0) ? t : t+1;
            return(-1);
        }

        /* remove delimiters from value */
        if ((a < t-1) && (*a == '"') && (*(t-1) == '"')) {
            ++a;
            u = t-1;
        }

        /* nothing to remove */
        else
            u = t;

        /* copy the value */
        if (valsz[attribc] < u-a+1)
            *v = c_realloc(*v,valsz[attribc]=u-a+1+512,NULL);
        strncpy(*v,a,u-a);
        (*v)[u-a] = 0;
    }

    /* empty value */
    else {
        if (valsz[attribc] < 1)
            *v = c_realloc(*v,512,NULL);
        (*v)[0] = 0;
    }

    /* counter of attributes */
    ++attribc;

    /* success */
    *ht = t;
    return(1);
}

/*

Increase topge by one and enlarge ge if necessary.

*/
void new_ge(void)
{
    edesc *k;

    if (++(topge) >= gesz) {
        int i;

        ge = c_realloc(ge,((gesz)+=250)*sizeof(edesc),NULL);
        for (i=0, k=ge+gesz-1; i<250; ++i, --k) {
            k->tsz = k->argsz = 0;
            k->txt = k->arg = NULL;
        }
    }
}

/*

Convert text to graphic components.

*/
void text2ge(char *t)
{
    int n,o,p,ln,cn,x,y;

    /* clear document */
    topge = -1;
    x = y = 0;

    /* loop all text lines */
    for (p=0,ln=cn=0; t[p]!=0; ++ln) {
        edesc *k;

        /* compute the size (n) of the current line */
        for (n=0; (t[p+n]!='\n') && (t[p+n]!=0); ++n);
        o = p;
        if (t[p+=n] == '\n')
            ++p;
        if (n > cn)
            cn = n;

        /* enlarge array of ge's */
        new_ge();

        /* create a new ge */
        k = ge + topge;
        k->type = GE_TEXT;
        k->l = 0;
        k->r = DFW*n;
        k->t = y;
        k->b = y+DFH;
        k->a = 0;
        if (k->tsz < n+1) {
            k->txt = c_realloc(k->txt,n+1,NULL);
            k->tsz = n+1;
        }
        strncpy(k->txt,t+o,n);
        (k->txt)[n] = 0;

        /* prepare next line */
        y += DFH + VS;
    }

    /* document total size in document pixels */
    GRX = cn * DFW;
    GRY = y;
}

/*

This is the code that implements the BR tag. As it is
currently being used as is by the TR and /TR tags, we've
made it a separate function.

*/
void imp_br(int *x,int *y,int *ch,int *sep)
{
    /* perhaps the maximum line width augmented */
    if (*x > GRX)
        GRX = *x;
    
    /* prepare next line */
    *y += *ch;
    *ch = 0;
    *x = HTML_LM;
    *sep = 0;
}

/*

Get the next ClaraML tag and store it using the buffers.

*/
int get_tag(char **ht)
{
    char *a,*t;
    int r,n,s;

    /* bypass separators */
    t = *ht;
    s = 0;
    while (htsep(*t)) {
        if (*t != '\n')
            s = 1;
        ++t;
    }

    /* separator explicited */
    if (s) {
        r = CML_SEP;
    }

    /* end of message reached */
    else if (*t == 0)
        r = CML_END;

    /* starting a new tag */
    else if (*t == '<') {
        int d;

        /* initialize special pointers */
        href = type = src = value = name = action = bgcolor = "";
        checked = 0;
        size = 0;

        /* default cellspacing */
        cellspacing = 10;

        /* get and store the tag name */
        a = ++t;
        while (*t && (*t != '>') && (!htsep(*t)))
            ++t;
        n = t - a;
        if (tagsz < n+1)
            tag = c_realloc(tag,tagsz=(n+1+512),NULL);
        strncpy(tag,a,n);
        tag[n] = 0;

        /*
            Get and store each attribute and value.
            Unsupported attributes are ignored.
        */
        attribc = 0;
        while ((d=get_attrib(&t)) > 0) {
            if (strcmp("HREF",attrib[attribc-1]) == 0)
                href = val[attribc-1];
            else if (strcmp("TYPE",attrib[attribc-1]) == 0)
                type = val[attribc-1];
            else if (strcmp("SRC",attrib[attribc-1]) == 0)
                src = val[attribc-1];
            else if (strcmp("NAME",attrib[attribc-1]) == 0)
                name = val[attribc-1];
            else if (strcmp("VALUE",attrib[attribc-1]) == 0)
                value = val[attribc-1];
            else if (strcmp("ACTION",attrib[attribc-1]) == 0)
                action = val[attribc-1];
            else if (strcmp("BGCOLOR",attrib[attribc-1]) == 0)
                bgcolor = val[attribc-1];
            else if (strcmp("CHECKED",attrib[attribc-1]) == 0)
                checked = 1;
            else if (strcmp("SIZE",attrib[attribc-1]) == 0)
                size = atoi(val[attribc-1]);
            else if (strcmp("CELLSPACING",attrib[attribc-1]) == 0)
                cellspacing = atoi(val[attribc-1]);
        }
        if (d < 0) {
            fatal(FD,"HTML parse failed at:%s",*ht);
        }
        r = CML_TAG;
    }

    /* ordinary text: read next word */
    else if (oneword) {
        a = t;
        while (*t && !(htsep(*t)) && (*t!='<'))
            ++t;
        n = t-a;
        if (tagtxtsz < n+1)
            tagtxt = c_realloc(tagtxt,tagtxtsz=(n+1+512),NULL);
        strncpy(tagtxt,a,n);
        tagtxt[n] = 0;
        r = CML_TEXT;
    }

    /* ordinary text: read until next tag */
    else {
        a = t;
        while (*t && (*t!='<'))
            ++t;
        n = t-a;
        if (tagtxtsz < n+1)
            tagtxt = c_realloc(tagtxt,tagtxtsz=(n+1+512),NULL);
        strncpy(tagtxt,a,n);
        tagtxt[n] = 0;
        r = CML_TEXT;
    }

    *ht = t;
    return(r);
}

/* (devel)

HTML windows overview
---------------------

Clara is able to read a piece of HTML code, render it, display
the rendered code, and attend events like selection of an anchor,
filling a text field, or submitting a form. Note that anchor
selection and form submission activate internal procedures, and
won't call external customizable CGI programs.

Most windows displayed by Clara are produced using this HTML
support. When the "show HTML source" option on the "View" menu is
selected, Clara will display unrendered HTML, and it will become
easier to identify the HTML windows. Note that all HTML is
produced by Clara itself. Clara won't read HTML from files or
through HTTP.

Perhaps you are asking why Clara implements these things. Clara
does not intend to be a web browser. Clara supports HTML because
we were in need of a forms interface, and the HTML forms is
around there, ready to be used, and extensively proved on
practice as an easy and effective solution.  Note that we're not
trying to achieve completeness. Clara HTML support is
partial. There is only one font available, tables cannot be
nested and most options are unavailable, PBM is the only graphic
format supported, etc. However, it attends our needs, and the
code is surprisingly small.

Let's understand how the HTML windows work. First of all, note
that there is a html flag on the structure that defines a window
(structure dwdesc). For instance, this flag is on for the window
OUTPUT (initializition code at function setview).

When the function redraw is called and the window OUTPUT is
visible on the plate, the service draw_dw will be called
informing OUTPUT through the global variable CDW (Current
Window). However, before making that, redraw will test the flag
RG to check if the HTML contents for the OUTPUT window must be
generated again, calling a function specific to that window. For
instance, when a symbol is trained, this flag must be set in
order to notify asynchronously the need to recompute the window
contents, and render it again.

HTML renderization is performed by the function html2ge. It will
create an array of graphic entities. Each such entity is a
structure informing the geometric position (x,y,width,height) of
something, and this something (a piece of text, a button and its
label and state, a PBM image, etc). Finally, the function
draw_dw will search the elements currently visible on the
portion of the document clipped by the window, and display them.

*/

/*

HTML Renderization.

*/
void html2ge(char *ht,int lm)
{

    /* current text position and element type */
    char *t;
    int el;

    /* current element being constructed */
    edesc *k;

    /* current coordinate */
    int x,y;

    /* maximum element height on current row */
    int ch;

    /* anchor mode and first word on anchored text */
    int a,fw;

    /* current margin level for lists */
    int mli;

    /* flags */
    int sep,spc;

    /* the select menu being constructed */
    cmdesc *sm=NULL;

    /* buffer for select option */
    char *si=NULL;
    int sisz=0;

    /* buffer for form action */
    char *fa=NULL;
    int fasz=0;

    /* reading title flag */
    int intitle;

    /*
        Parameters of the table being parsed: first element
        of the table, first element of the row,
        maximum number of columns, number of rows, number
        of cells on current row, and cellspacing.
    */
    int ft,fr,nc,nr,cc,cs;

    /* buffers for computing the width of each column */
    static int *cw=NULL,*ccw=NULL,cwsz=0;

    /* last HREF (get_tag destroys href) */
    static char *last_href=NULL;
    static int lhs=0;

    /* clear document */
    free_earray();

    /* prepare */
    GRX = x = y = 0;
    sep = 0;
    mli = 0;
    ch = 0;
    nc = nr = 0;
    cc = -1;
    a = 0;
    bgcolor = "";
    intitle = 0;
    HTML_LM = lm;

    /* just to avoid compilation warnings */
    fw = ft = fr = cs = 0;

    /* for each HTML item */
    for (t=ht; (el=get_tag(&t)) != CML_END; ) {

        /*
        printf("returned %d from get_tag (tag=%s)\n",el,tag);
        */

        /* just a text piece for the current SELECT menu */
        if ((el == CML_TEXT) && (sm != NULL)) {
            int n;

            n = strlen(si) + strlen(tagtxt) + 1;
            if (n > sisz)
                si = c_realloc(si,sisz=n+512,NULL);
            if (si[0] != 0)
                strcat(si," ");
            strcat(si,tagtxt);
        }

        /* just a text piece for the title */
        else if ((el == CML_TEXT) && (intitle)) {
        }

        /* a piece of text */
        else if (el == CML_TEXT) {
            int n;

            /* enlarge array of ge's */
            new_ge();

            /* add a space */
            if ((x > 0) && (sep)) {
                x += DFW;
            }

            /* initialize the new ge */
            n = strlen(tagtxt);
            k = ge + topge;
            k->type = GE_TEXT;
            k->l = x;
            k->r = k->l + DFW*n;
            k->t = y;
            k->b = y+DFH;
            spc = 0;

            /* the first word of an anchored text */
            if (((k->a=a) == HTA_HREF) && (href!=0)) {
                if (k->argsz <= strlen(href))
                    k->arg = c_realloc(NULL,k->argsz=strlen(href)+1,NULL);
                strcpy(k->arg,href);
                fw = k->iarg = topge;
                a = HTA_HREF_C;
            }

            /* not the first word within anchored text */
            else if (k->a == HTA_HREF_C) {
                k->iarg = fw;

                /* make the space part of the ge */
                if ((x > 0) && (sep)) {
                    ++n;
                    spc = 1;
                    k->l -= DFW;
                    x -= DFW;
                }
            }

            /* word is not within anchored text */
            else {

                /* make the space part of the ge */
                if ((x > 0) && (sep)) {
                    ++n;
                    spc = 1;
                    k->l -= DFW;
                    x -= DFW;
                }
            }

            /* enlarge text buffer */
            if (k->tsz <= n) {
                k->txt = c_realloc(k->txt,n+1,NULL);
                k->tsz = n+1;
            }

            /* copy the word eventually prepending a space */
            if (spc) {
                (k->txt)[0] = ' ';
                strncpy((k->txt)+1,tagtxt,n-1);
            }
            else
                strncpy(k->txt,tagtxt,n);
            (k->txt)[n] = 0;

            x += n * DFW;

            /* current line maximum component height */
            if (k->b - k->t + VS > ch)
                ch = k->b - k->t + VS;

            /* coordinates within table (if any) */
            k->tr = nr;
            k->tc = cc;

            sep = 0;
        }

        /* line break */
        else if ((el == CML_TAG) &&
                 (strcmp(tag,"BR")==0)) {

            /* our BR is always effective... */
            if (ch <= 0)
                ch = DFH + VS;

            imp_br(&x,&y,&ch,&sep);
        }

        /* anchor */
        else if ((el == CML_TAG) &&
                 (strcmp(tag,"A")==0)) {

            a = HTA_HREF;
        }

        /* select begin */
        else if ((el == CML_TAG) &&
                 (strcmp(tag,"SELECT")==0)) {

            sm = addmenu(2,name,0);

            if (sisz <= 0) {
                si = c_realloc(si,sisz=512,NULL);
            }
            si[0] = 0;

            /* enlarge array of ge's */
            new_ge();

            /* add a space */
            if ((x > 0) && (sep)) {
                x += DFW + 4;
            }

            /* initialize the new ge */
            k = ge + topge;
            k->type = GE_SELECT;
            k->l = x;
            k->t = y;
            k->b = y+DFH+4+4;
            spc = 0;
            k->iarg = sm - CM;

            /* current line maximum component height */
            if (k->b - k->t + VS > ch)
                ch = k->b - k->t + VS;

            sep = 0;
        }

        /* select end */
        else if ((el == CML_TAG) &&
                 (strcmp(tag,"/SELECT")==0)) {

            /* finish previous option */
            if (si[0] != 0)
                additem(sm,si,CM_TA,0,-1,"",0);

            /* maximum item width */
            comp_menu_size(sm);
            k = ge + topge;
            k->r = k->l + CW + 2*DFW;
            x = k->r;

            /* finish menu */
            sm = NULL;
        }

        /* select option */
        else if ((el == CML_TAG) &&
                 (strcmp(tag,"OPTION")==0)) {

            /* finish previous option */
            if (si[0] != 0) {
                additem(sm,si,CM_TA,0,-1,"",0);
                si[0] = 0;
            }
        }

        /* form submission button */
        else if ((el == CML_TAG) &&
                 (strcmp(tag,"INPUT")==0) &&
                 (strcmp(type,"SUBMIT")==0)) {
            int n;

            /* enlarge array of ge's */
            new_ge();

            /* add a space */
            if ((x > 0) && (sep)) {
                x += DFW + 4;
            }

            /* initialize the new ge */
            k = ge + topge;
            k->type = GE_SUBMIT;
            k->l = x;
            n = strlen(value);
            k->r = k->l + (n+2)*DFW;
            x = k->r;
            k->t = y;
            k->b = y+DFH+VS+4+VS;
            spc = 0;
            k->iarg = sm - CM;

            /* enlarge text buffer */
            if (k->tsz <= n) {
                k->txt = c_realloc(k->txt,n+1,NULL);
                k->tsz = n+1;
            }

            /* copy the label */
            strncpy(k->txt,value,n);
            (k->txt)[n] = 0;

            /* attach the action to it */
            k->a = HTA_SUBMIT;
            n = strlen(fa);
            if (k->argsz <= n);
                k->arg = c_realloc(NULL,k->argsz=n+1,NULL);
            strcpy(k->arg,fa);

            /* current line maximum component height */
            if (k->b - k->t + VS > ch)
                ch = k->b - k->t + VS;

            sep = 0;
        }

        /* IMG SRC */
        else if ((el == CML_TAG) &&
                 (strcmp(tag,"IMG")==0)) {
            int n,w,h;

            /* enlarge array of ge's */
            new_ge();

            /* add a space */
            if ((x > 0) && (sep)) {
                x += DFW;
            }

            /* initialize the new ge */
            k = ge + topge;
            k->type = GE_IMG;
            k->l = x;
            img_size(src,&w,&h);
            k->r = k->l + w - 1;
            x = k->r;
            k->t = y;
            k->b = y + h;
            spc = 0;

            /*
                Image is anchored
            */
            if (((k->a=a) == HTA_HREF) || (a==HTA_HREF_C)) {
                int a;

                if (k->argsz <= (a=strlen(last_href)))
                    k->arg = c_realloc(NULL,k->argsz=a+1,NULL);
                strcpy(k->arg,last_href);
                fw = k->iarg = topge;
                a = HTA_HREF_C;
            }

            /* enlarge text buffer */
            n = strlen(src);
            if (k->tsz <= n) {
                k->txt = c_realloc(k->txt,n+1,NULL);
                k->tsz = n+1;
            }

            /* copy the image name to the text buffer */
            strncpy(k->txt,src,n);
            (k->txt)[n] = 0;

            /* current line maximum component height */
            if (k->b - k->t + VS > ch) {
                ch = k->b - k->t + VS;
            }

            /* coordinates within table (if any) */
            k->tr = nr;
            k->tc = cc;

            sep = 1;
        }

        /* TEXT INPUT */
        else if ((el == CML_TAG) &&
                 (strcmp(tag,"INPUT")==0) &&
                 ((strcmp(type,"TEXT")==0) || (strcmp(type,"NUMBER")==0))) {

            int n;

            /* left stepper */
            if (type[0] == 'N') {

                /* enlarge array of ge's */
                new_ge();

                /* add a space */
                if ((x > 0) && (sep)) {
                    x += DFW;
                }

                /* initialize the new ge */
                k = ge + topge;
                k->type = GE_STEPPER;
                k->l = x;
                k->r = k->l + DFH;
                x = k->r;
                k->t = y;
                k->b = y+DFH+4;
                spc = 0;

                /* left */
                k->iarg = 9;

                /* current line maximum component height */
                if (k->b - k->t + VS > ch)
                    ch = k->b - k->t + VS;

                /* coordinates within table (if any) */
                k->tr = nr;
                k->tc = cc;

                sep = 0;
            }

            /* enlarge array of ge's */
            new_ge();

            /* add a space */
            if ((x > 0) && (sep)) {
                x += DFW;
            }

            /* initialize the new ge */
            k = ge + topge;
            k->type = GE_INPUT;
            k->l = x;
            if (size == 0)
                k->r = k->l + DFW*MFTL + 4;
            else
                k->r = k->l + DFW*size + 4;
            x = k->r;
            k->t = y;
            k->b = y+DFH+4;
            spc = 0;

            /* enlarge text buffer */
            n = MFTL;
            if (n < strlen(value))
                n = strlen(value) + 1;
            if (k->tsz <= n) {
                k->txt = c_realloc(k->txt,n+1,NULL);
                k->tsz = n+1;
            }

            /* copy the predefined input contents to the text buffer */
            strcpy(k->txt,value);

            /* enlarge arg buffer */
            n = ((name==NULL) ? 0 : strlen(name));
            if (k->argsz <= n) {
                k->arg = c_realloc(k->arg,n+1,NULL);
                k->argsz = n+1;
            }

            /* copy variable name to arg buffer */
            if (name!=NULL)
               strcpy(k->arg,name);
            else
               (k->arg)[0] = 0;

            /* current line maximum component height */
            if (k->b - k->t + VS > ch)
                ch = k->b - k->t + VS;

            /* coordinates within table (if any) */
            k->tr = nr;
            k->tc = cc;

            sep = 0;

            /* right stepper */
            if (type[0] == 'N') {

                /* enlarge array of ge's */
                new_ge();

                /* add a space */
                if ((x > 0) && (sep)) {
                    x += DFW;
                }

                /* initialize the new ge */
                k = ge + topge;
                k->type = GE_STEPPER;
                k->l = x + 2;
                k->r = k->l + DFH;
                x = k->r;
                k->t = y;
                k->b = y+DFH+4;
                spc = 0;

                /* right */
                k->iarg = 3;

                /* current line maximum component height */
                if (k->b - k->t + VS > ch)
                    ch = k->b - k->t + VS;

                /* coordinates within table (if any) */
                k->tr = nr;
                k->tc = cc;

                sep = 0;
            }
        }

        /* CHECKBOX */
        else if ((el == CML_TAG) &&
                 (strcmp(tag,"INPUT")==0) &&
                 ((strcmp(type,"CHECKBOX")==0) ||
                  (strcmp(type,"RADIO")==0))) {

            int n;

            /* enlarge array of ge's */
            new_ge();

            /* add a space */
            if ((x > 0) && (sep)) {
                x += DFW + 4;
            }

            /* initialize the new ge */
            k = ge + topge;
            k->type = (strcmp(type,"CHECKBOX")==0) ? GE_CBOX : GE_RADIO;
            k->l = x;
            k->r = k->l + (DFH-DFD) + 4;
            x = k->r;
            k->t = y;
            k->b = y + (DFH-DFD) + 4;
            spc = 0;
            k->iarg = (checked ? 2 : 0);

            /* enlarge arg buffer */
            n = ((name==NULL) ? 0 : strlen(name));
            if (k->argsz <= n) {
                k->arg = c_realloc(k->arg,n+1,NULL);
                k->tsz = n+1;
            }

            /* copy variable name to arg buffer */
            if (name!=NULL)
               strcpy(k->arg,name);
            else
               (k->arg)[0] = 0;

            /* enlarge txt buffer */
            n = MFTL;
            if (k->tsz <= n) {
                k->txt = c_realloc(k->txt,n+1,NULL);
                k->tsz = n+1;
            }

            /* copy the VALUE argument to the txt buffer */
            strcpy(k->txt,value);

            /* current line maximum component height */
            if (k->b - k->t + VS > ch)
                ch = k->b - k->t + VS;

            /* coordinates within table (if any) */
            k->tr = nr;
            k->tc = cc;

            sep = 1;
        }

        /* close anchor */
        else if ((el == CML_TAG) && (strcmp(tag,"/A")==0)) {

            a = 0;
        }

        /* open list */
        else if ((el == CML_TAG) && (strcmp(tag,"UL")==0)) {

            mli += 2;
        }

        /* close list */
        else if ((el == CML_TAG) && (strcmp(tag,"/UL")==0)) {

            mli -= 2;
        }

        /* open form */
        else if ((el == CML_TAG) && (strcmp(tag,"FORM")==0)) {
            int n;

            /* remember action */
            n = strlen(action);
            if (fasz <= n)
                fa = c_realloc(fa,fasz=n+512,NULL);
            strcpy(fa,action);
        }

        /* close form */
        else if ((el == CML_TAG) && (strcmp(tag,"/FORM")==0)) {

        }

        /* open table */
        else if ((el == CML_TAG) && (strcmp(tag,"TABLE")==0)) {

            ft = fr = topge+1;
            nc = nr = 0;
            cc = -1;
            cs = cellspacing;
        }

        /* close table */
        else if ((el == CML_TAG) && (strcmp(tag,"/TABLE")==0)) {

            if (nr > 0) {
                int i,c,r,w,tw;

                if (cwsz <= nc) {
                    cw = c_realloc(cw,cwsz=(nc+64)*sizeof(int),NULL);
                    ccw = c_realloc(ccw,cwsz*sizeof(int),NULL);
                }

                /* this is just to avoid a compilation warning */
                c = w = 0;

                /* compute maximum width for each column */
                memset(cw,0,nc*sizeof(int));
                for (i=ft, r=-1; i<=topge; ++i) {
                    if ((ge[i].tr > r) || (ge[i].tc > c)) {
                        if ((r>=0) && (w>cw[c]))
                            cw[c] = w;
                        r = ge[i].tr;
                        c = ge[i].tc;
                        w = ge[i].r - ge[i].l;
                    }
                    else {
                        /*
                        printf("summing up %s width %d to %d\n",
                               ge[i].txt,
                               ge[i].r - ge[i].l,w);
                        */
                        w += ge[i].r - ge[i].l;
                    }
                }
                if ((r>=0) && (w>cw[c]))
                    cw[c] = w;

                /* compute cumulative widths */
                memset(ccw,0,nc*sizeof(int));
                for (c=1; c<nc; ++c) {
                    ccw[c] += cw[c-1] + ccw[c-1];
                }

                /* add a default left margin */
                for (i=0; i<nc; ++i)
                    ccw[i] += DFW;

                /* table width including margins */
                tw = ccw[nc-1] + cw[nc-1] + cs*(nc-1) + DFW;
                if (tw > GRX)
                    GRX = tw;

                /* centralize */
                /*
                if (tw < DW)
                    for (i=0; i<nc; ++i)
                        ccw[i] += (DW-tw)/2;
                */

                /* shift right the table cells */
                for (i=ft; i<=topge; ++i) {

                    w = ccw[ge[i].tc];
                    ge[i].l += w + cs*ge[i].tc;
                    if (ge[i].type == GE_RECT) {
                        ge[i].r = ge[i].l + cw[ge[i].tc];
                    }
                    else
                        ge[i].r += w + cs*ge[i].tc;

                    /*
                    printf("element %s tr=%d tc=%d l=%d r=%d\n",
                            ge[i].txt,
                            ge[i].tr,
                            ge[i].tc,
                            ge[i].l,
                            ge[i].r);
                    */
                }
            }

            nc = nr = 0;
            cc = -1;
        }

        /* open table row */
        else if ((el == CML_TAG) && (strcmp(tag,"TR")==0)) {

            cc = -1;
        }

        /* close table row */
        else if ((el == CML_TAG) && (strcmp(tag,"/TR")==0)) {

            if (cc >= nc)
                nc = cc+1;
            if (cc >= 0) {
                int i;

                /* redefine bottom line for each RECT */
                for (i=topge; i>=fr; --i) {
                    if ((ge[i].tr == nr) && (ge[i].type == GE_RECT)) {
                        ge[i].b += ch - VS;
                    }
                }

                imp_br(&x,&y,&ch,&sep);
                ++nr;
                fr = topge + 1;
            }
        }

        /* open table cell */
        else if ((el == CML_TAG) && (strcmp(tag,"TD")==0)) {

            /* prepare */
            ++cc;
            x = 0;
            sep = 0;

            /* enlarge array of ge's */
            new_ge();

            /* add a space */
            if ((x > 0) && (sep)) {
                x += DFW + 4;
            }

            /* initialize the new ge */
            k = ge + topge;
            k->type = GE_RECT;
            k->l = x;
            k->r = x;
            k->t = y;
            k->b = y;
            spc = 0;
            k->iarg = 0;

            /* background color */
            if (strcmp(bgcolor,"WHITE")==0)
                k->bg = WHITE;
            else if (strcmp(bgcolor,"BLACK")==0)
                k->bg = BLACK;
            else if (strcmp(bgcolor,"GRAY")==0)
                k->bg = GRAY;
            else if (strcmp(bgcolor,"DARKGRAY")==0)
                k->bg = DARKGRAY;
            else if (strcmp(bgcolor,"VDGRAY")==0)
                k->bg = VDGRAY;
            else
                k->bg = -1;

            /* coordinates within table */
            k->tr = nr;
            k->tc = cc;

        }

        /* close table cell */
        else if ((el == CML_TAG) && (strcmp(tag,"/TD")==0)) {

        }

        /* add item to current list */
        else if ((el == CML_TAG) && (strcmp(tag,"LI")==0)) {

            y += ch;
            ch = 0;
            x = mli*DFW;
            sep = 0;
        }

        /* ignore this tag by now */
        else if ((el == CML_TAG) && (strcmp(tag,"HTML")==0)) {
        }

        /* ignore this tag by now */
        else if ((el == CML_TAG) && (strcmp(tag,"/HTML")==0)) {
        }

        /* ignore this tag by now */
        else if ((el == CML_TAG) && (strcmp(tag,"HEAD")==0)) {
        }

        /* ignore this tag by now */
        else if ((el == CML_TAG) && (strcmp(tag,"/HEAD")==0)) {
        }

        /* ignore this tag by now */
        else if ((el == CML_TAG) && (strcmp(tag,"TITLE")==0)) {

            intitle = 1;
        }

        /* ignore this tag by now */
        else if ((el == CML_TAG) && (strcmp(tag,"/TITLE")==0)) {

            intitle = 0;
        }

        /* ignore this tag by now */
        else if ((el == CML_TAG) && (strcmp(tag,"BODY")==0)) {
        }

        /* ignore this tag by now */
        else if ((el == CML_TAG) && (strcmp(tag,"/BODY")==0)) {
        }

        /* ignore this tag by now */
        else if ((el == CML_TAG) && (strcmp(tag,"B")==0)) {
        }

        /* ignore this tag by now */
        else if ((el == CML_TAG) && (strcmp(tag,"/B")==0)) {
        }

        /* ignore this tag by now */
        else if ((el == CML_TAG) && (strcmp(tag,"I")==0)) {
        }

        /* ignore this tag by now */
        else if ((el == CML_TAG) && (strcmp(tag,"/I")==0)) {
        }

        /* explicit separator */
        else if (el == CML_SEP) {

            sep = 1;
        }

        else if (el == CML_TAG) {
            printf("UNRECOGNIZED TAG (%ld) %s\n", t - ht, tag);
        }

        else {

            printf("UNRECOGNIZED ELEMENT %d\n",el);
        }

        /* remember href */
        if (href!=NULL) {
            int a;

            /* enlarge buffer */
            if (lhs < (a=strlen(href)+1))
                last_href = c_realloc(last_href,lhs=a+32,NULL);

            strcpy(last_href,href);
        }
    }

    /*
        Compute document total size in document pixels
    */

    /* minimum width is DW*RF */
    if ((GRX > 0) && (GRX < DW*RF))
        GRX = DW*RF;

    /* the minimum height DH*RF */
    GRY = y + ch;
    if ((GRY > 0) && (GRY < DH*RF))
        GRY = DH*RF;

    /*
        When the window contents
        is regenerated and re-renderized, if the total height
        shrinks, we may have display problems, because the
        document position being exhibited may not
        exist anymore, and the positioning routines are
        not smart enough to adapt automatically. A workaround
        tried was adding a final blank area to the
        renderized document (dunno if this is still
        needed).
    */
    else {
        /* GRY += DH*RF-DFH; */
    }
}

/*

Convert origin to origin name.

*/
char *origname(int o)
{
    if (o == REVISION)
        return("REVISION");
    else if (o == SHAPE)
        return("SHAPE");
    else if (o == SPELLING)
        return("SPELLING");
    else if (o == COMPOSITION)
        return("COMPOSITION");
    else
        return("UNKNOWN");
}

/*

Converts type to type name.

*/
char *typename(int t)
{
    if (t == ANON)
        return("ANON");
    else if (t == TRUSTED)
        return("TRUSTED");
    else if (t == ARBITER)
        return("ARBITER");
    else
        return("UNKNOWN");
}

/*

Converts class to class name.

*/
char *classname(int c)
{
    if (c == UNDEF)
        return("UNDEF");
    if (c == DOT)
        return("DOT");
    if (c == COMMA)
        return("COMMA");
    if (c == ACCENT)
        return("ACCENT");
    if (c == CHAR)
        return("CHAR");
    if (c == PUNCT)
        return("PUNCT");
    if (c == FRAG)
        return("FRAG");
    if (c == NOISE)
        return("NOISE");
    if (c == SCHAR)
        return("SCHAR");
    return("?");
}

/*

Convert window to window name.

*/
char *dwname(int s)
{
    static char p[25];

    if (s == PAGE) {

        if (*cm_d_rs != ' ') {
            snprintf(p,25,"page 1:%d",dw[PAGE].rf);
            p[24] = 0;
            return(p);
        }
        else
            return("page");
    }
    else if (s == PAGE_OUTPUT)
        return("page (output)");
    else if (s == PAGE_LIST)
        return("page (list)");
    else if (s == PAGE_FATBITS)
        return("page (fatbits)");
    else if (s == PAGE_SYMBOL)
        return("page (symbol)");
    else if (s == PAGE_DOUBTS)
        return("page (doubts)");
    else if (s == PATTERN)
        return("pattern");
    else if (s == PATTERN_LIST)
        return("pattern (list)");
    else if (s == PATTERN_TYPES)
        return("pattern (types)");
    else if (s == PATTERN_ACTION)
        return("pattern (action)");
    else if (s == WELCOME)
        return("welcome");
    else if (s == GPL)
        return("gpl");
    else if (s == TUNE)
        return("tune");
    else if (s == TUNE_SKEL)
        return("tune (skel)");
    else if (s == TUNE_PATTERN)
        return("tune (pattern)");
    else if (s == TUNE_ACTS)
        return("tune (acts)");
    else if (s == PAGE_MATCHES)
        return("page (matches)");
    return("?");
}

/*

Convert alphabet to alphabet name

*/
char *aname(int a)
{
    if (a == LATIN)
        return("latin");
    else if (a == GREEK)
        return("greek");
    else if (a == CYRILLIC)
        return("cyrillic");
    else if (a == HEBREW)
        return("hebrew");
    else if (a == ARABIC)
        return("arabic");
    else if (a == KANA)
        return("kana");
    else if (a == OTHER)
        return("other");
    else if (a == NUMBER)
        return("number");
    else if (a == IDEOGRAM)
        return("ideogram");
    else
        return("?");
}

/*

Pushes the current contents of text array (remark: the stack size is
1).

*/
void push_text(void)
{
    if (ptextsz < (topt+1))
        ptext = c_realloc(ptext,ptextsz=topt+1024,NULL);
    if (topt >= 0)
        memcpy(ptext,text,topt+1);
    ptopt = topt;
}

/*

Pops the current contents of text array (remark: the stack size is
1).

*/
void pop_text(void)
{
    if (textsz < (ptopt+1))
        fatal(IE,"inconsistent size of text buffers");
    if (ptopt >= 0)
        memcpy(text,ptext,ptopt+1);
    topt = ptopt;
}

/*

Concatenate string to the array.

*/
void to(char **t,int *top,int *sz,const char *fmt, ...)
{
    va_list args;
    int n=0,f;

    for (f=0; f==0; ) {

        /* try to write */
        va_start(args, fmt);
        n = vsnprintf(*t+*top+1,*sz-*top-1,fmt,args);
        va_end(args);

        /*
            Some implementations of vsnprintf return -1 when
            the buffer is too small.
        */
        if (n < 0) {
            *t = c_realloc(*t,*sz+=15000,NULL);
        }

        /*
            Others return the required size for the buffer.
        */
        else if (n+1 > *sz-*top-1) {
            *t = c_realloc(*t,
                           *sz+=(n+1-(*sz-*top-1)+15000),
                           NULL);
        }

        /* success */
        else {
            f = 1;
        }
    }
    *top += n;
}

/*

Concatenate string to the text array.

*/
void totext(const char *fmt, ...)
{
    va_list args;
    int n=0,f;

    for (f=0; f==0; ) {

        /* try to write */
        va_start(args, fmt);
        n = vsnprintf(text+topt+1,textsz-topt-1,fmt,args);
        va_end(args);

        /*
            Some implementations of vsnprintf return -1 when
            the buffer is too small.
        */
        if (n < 0) {
            text = c_realloc(text,textsz+=15000,NULL);
        }

        /*
            Others return the required size for the buffer.
        */
        else if (n+1 > textsz-topt-1) {
            text = c_realloc(text,
                             textsz+=(n+1-(textsz-topt-1)+15000),
                             NULL);
        }

        /* success */
        else {
            f = 1;
        }
    }
    topt += n;
}

/*

Poor man's headline.

*/
void pm_hl(char *s)
{
    totext("<BR><TABLE><TR>\n");
    totext("<TD BGCOLOR=WHITE>%H</TD>\n",s);
    totext("</TR></TABLE>\n");
}

/*

Poor man's checkbox.

*/
void pm_cbox(char *name,int st,char *t)
{
    char *C;

    C = (st) ? "CHECKED" : "";
    totext("<INPUT TYPE=CHECKBOX %H NAME=%H>%H\n",C,name,t);
}

/*

Poor man's radio.

*/
void pm_radio(char *name,int st,int v,char *t)
{
    char *C;

    C = (st) ? "CHECKED" : "";
    totext("<INPUT TYPE=RADIO %H NAME=%H VALUE=%d>%H\n",C,name,v,t);
}

/*

Poor man's integer field.

*/
void pm_int(char *name,char *t,int v)
{
    totext("<BR><INPUT TYPE=TEXT SIZE=8 NAME=%H VALUE=%d> %H\n",name,v,t);
}

/*

Poor man's conditional float field.

*/
void pm_fl(char *name,char *t,float v,int c)
{
    if (c)
        totext("%H <INPUT TYPE=TEXT SIZE=8 NAME=%H VALUE=%3.2f>\n",t,name,v);
    else
        totext("%H %3.2f\n",t,v);
}

/*

Poor man's table line with integer input field.

*/
void pm_tl(char *n,char *t,int v,int e)
{
    totext("<TR><TD BGCOLOR=DARKGRAY>%H</TD><TD>",t);
    if (e)
        totext("<INPUT TYPE=TEXT SIZE=8 VALUE=%d NAME=%H>",v,n);
    else
        totext("%d",v);
    totext("</TD></TR>\n");
}

/*

Poor man's table line with text input field.

*/
void pm_tt(char *n,char *t,char *v,int e)
{
    totext("<TR><TD BGCOLOR=DARKGRAY>%H</TD><TD>",t);
    if (e)
        totext("<INPUT TYPE=TEXT SIZE=20 VALUE=\"%H\" NAME=%H>",v,n);
    else
        totext("%H",v);
    totext("</TD></TR>\n");
}

/*

Poor man's table line with float input field.

*/
void pm_tf(char *n,char *t,float v,int e)
{
    totext("<TR><TD BGCOLOR=DARKGRAY>%H</TD><TD>",t);
    if (e)
        totext("<INPUT TYPE=TEXT SIZE=8 VALUE=%3.2f NAME=%H>",v,n);
    else
        totext("%3.2f",v,n);
    totext("</TD></TR>\n");
}

/*

Poor man's table line with conditional checkbox.

*/
void pm_tlc(char *n,char *t,int v,int e)
{
    char *C;

    C = (v) ? "CHECKED" : "";
    totext("<TR><TD BGCOLOR=DARKGRAY>%H</TD><TD>",t);
    if (e)
        totext("<INPUT TYPE=CHECKBOX %H NAME=%H>",C,n);
    else
        totext(v ? "yes" : "no");
    totext("</TD></TR>\n");
}

/*

Poor man's table line with conditional integer input.

*/
void pm_tli(char *n,char *t,int v,int e)
{
    totext("<TR><TD BGCOLOR=DARKGRAY>%H</TD><TD>",t);
    if (e)
        totext("<INPUT TYPE=NUMBER SIZE=8 NAME=%H VALUE=%d>",n,v);
    else
        totext("%d",v);
    totext("</TD></TR>\n");
}

/*

Poor man's table line with conditional float input.

*/
void pm_tlf(char *n,char *t,float v,int e)
{
    totext("<TR><TD BGCOLOR=DARKGRAY>%H</TD><TD>",t);
    if (e)
        totext("<INPUT TYPE=NUMBER SIZE=8 NAME=%H VALUE=%3.2f>",n,v);
    else
        totext("%3.2f",v);
    totext("</TD></TR>\n");
}

/*

Prepare to display and edit the symbol c.

*/
void mk_page_symbol(int c)
{
    int i,d,n,tv;
    sdesc *m;
    trdesc *t;
    vdesc *v;
    adesc *a;

    /* clear the array */
    topt = -1;
    text[0] = 0;

    if (c < 0) {
        return;
    }

    /* current word and symbol transliteration */
    m = mc + (d=mc[c].sw);
    if (d < 0)
        d = c;

    /*
    if ((mc[c].tr != NULL) && (mc[c].tr->t != NULL))
        strcpy(ibf[2].b,(mc[c].tr->t));
    else
        strcpy(ibf[2].b,"");
    */

    /* prepare input field and bitmaps */
    /*
    if (editing_doubts)
        snprintf(mba,MMB,"symbol %d from word %d (ESC to skip)",c,d);
    else
    */
        snprintf(mba,MMB,"symbol %d from word %d",c,d);
    show_hint(0,mba);

    if (*cm_v_wclip == 'X')
        wrmc8(curr_mc,NULL,NULL);

    /* HTML headings */
    totext("<HTML><HEAD><TITLE>Symbol Browser</TITLE></HEAD><BODY>\n");

    /* symbol main properties */
    m = mc + c;
    if (*cm_v_wclip == 'X')
        totext("<IMG SRC=internal><BR>\n");
    if ((m->tr == NULL) || (((m->tr)->t) == NULL))
        totext("<INPUT TYPE=TEXT VALUE=\"\"><BR>\n");
    else
        totext("<INPUT TYPE=TEXT VALUE=\"%H\"><BR>\n",(m->tr)->t);
    totext("Symbol %d, %d component(s)\n",c,m->ncl);
    totext("<UL>\n");

    /* basic properties */
    if (m->bm >= 0)
        totext("<LI>best match <A HREF=edit/%d>%d</A>\n",
               m->bm,id2idx(m->bm));
    totext("<LI>width %d height %d zone %d\n",m->r-m->l+1,m->b-m->t+1,m->c);
    totext("<LI>alignment %d\n",m->va);
    totext("<LI>class %H\n",classname(m->tc));
    totext("<LI>shape status %d (best match %d)\n",C_ISSET(m->f,F_SS),m->bm);
    totext("<LI>start of word = %d\n",C_ISSET(m->f,F_ISW));
    totext("<LI>ispreferred = %d\n",C_ISSET(m->f,F_ISP));

    /* list of accents */
    totext("<LI>Accents:");
    for (i=m->sl; i>=0; i=mc[i].sl)
        totext(" %d",i);

    if (m->sw >= 0) {
        wdesc *w;

        w = word + m->sw;
        totext("<LI>word %d (%d,%d), line %d\n",m->sw,w->W,w->E,w->tl);
    }
    else
        totext("<LI>word %d\n",m->sw);

    totext("<LI>top %d bottom %d right %d left %d\n",m->t,m->b,m->r,m->l);

    /* symbol transliterations */
    for (t = m->tr, n=0; t != NULL; t = t->nt, ++n) {

        /* transliteration main properties */
        for (v=t->v, tv=0; v!=NULL; v=(vdesc *)(v->nv), ++tv);
        totext("<LI>Transliteration \"%H\" (%d votes)\n",(t->t),tv);
        totext("<LI>Alphabet %H\n",aname(t->a));
        totext("<UL>\n");
        totext("<LI>preference = %d\n",t->pr);
        totext("<LI>flags =%H%H%H\n",
            (t->f&F_ITALIC) ? " italic" : "",
            (t->f&F_BOLD) ? " bold" : "",
            (t->f) ? "" : " none");
        /*totext("<LI>propagability = %d\n",t->p);*/

        /* votes */
        for (v=t->v; v!=NULL; v=(vdesc *)(v->nv)) {
            if ((0<=v->act) && (v->act<=topa)) {
                a = act + v->act;
                totext("<UL>\n");
                totext("<LI>%H quality %d (act %d)\n",
                        origname(v->orig),v->q,v->act);
                totext("<LI>by %H (%H) ",a->r,typename(a->rt));
                /*
                if (v->orig == REVISION)
                    totext("[<A HREF=nullify/%d>nullify</A>]\n",v->act);
                */
                totext("<LI>from %H at %H",a->sa,ctime(&(a->dt)));
                totext("</UL>\n");
            }
        }
        totext("</UL>\n");
    }

    /* list of closures */
    totext("<LI>Closures:");
    for (i=0; i<m->ncl; ++i)
        totext(" %d",m->cl[i]);
    totext("\n</UL><BR>\n");

    /* by default this mc is not a doubt */
    /* editing_doubts = 0; */

    /* HTML footings */
    totext("</BODY></HTML>\n");
}

/*

Classify and display the doubts.

*/
void mk_page_doubts(void)
{
    int i;
    unsigned char *w;
    int a,dirty,unres,dr;
    wdesc *W;
    sdesc *m;

    w = alloca(tops+1);

    /* search words with dubious chars */
    for (i=dr=0; i<=topw; ++i) {

        /* prepare */
        unres = dirty = 0;
        W = word + i;

        /* search things around W not in W */
        list_s(W->l,W->t,W->r-W->l+1,W->r-W->l+1);
        for (a=0; a<list_s_sz; ++a) {
            m = mc + list_s_r[a];
            if (m->sw != i) {
                ++dirty;
            }
        }

        for (a=word[i].F; (a>=0); a=mc[a].E) {

            if (uncertain(mc[a].tc)) {
                ++unres;
            }
        }

        w[i] = 0;
        if (dirty)
            w[i] += 1;
        if (unres)
            w[i] += 10;

        if ((w[i]==10) || (w[i]==11))
            ++dr;
    }

    /*
        Make page
    */

    /* clear the array */
    topt = -1;
    text[0] = 0;

    /* HTML headings */
    totext("<HTML><HEAD><TITLE>Doubts</TITLE></HEAD><BODY>\n");

    /*
    totext("Dirty words:");

    for (i=0; i<=topw; ++i) {
        if (w[i] == 1) {
            totext("<BR><A HREF=wfocus/%d><IMG SRC=word/%d></A>\n",i,i);
        }
    }
    */

    totext("<BR>Dirty or unresolved words (%d):",dr);

    for (i=0; i<=topw; ++i) {
        if ((w[i] == 10) || (w[i] == 11)) {
            totext("<BR><A HREF=wfocus/%d><IMG SRC=word/%d></A>\n",i,i);
            if (word[i].f & F_ITALIC)
                totext(" (italic)\n");
            if (word[i].f & F_BOLD)
                totext(" (bold)\n");
        }
    }

    /* add isolated unresolved non-SCHARs */
    /*
    {
        int j;

        totext("<BR>UNRESOLVED");

        for (i=0; i<=topps; ++i) {
            j = ps[i];
            if (uncertain(mc[j].tc) && (mc[j].sw < 0)) {
                totext("<BR><IMG SRC=symbol/%d>\n",j);
            }
        }
    }
    */

    /* HTML footings */
    totext("</BODY></HTML>\n");
}

/*

Generate the OCR output, that is, copy to the array "text" the
OCR result, formatted as HTML.

encap drives the output format:

  0 full html with headings, footings and per-symbol links
  1 "encapsulated" html (html without headings or footings)
  2 pure text
  3 djvu text frame (includes per-word and per-line coordinates)

BUG: won't escape double quotes inside strings when generating
     djvu output.

Obs. comment by Berend Reitsma:

Another bug I found is that in my text I had an end-of-line dot right
above another letter i. Somehow this dot was linked to the i below it
resulting in having the diaeresis case in the function mk_page_output().

*/
void mk_page_output(int encap)
{
    int i,a,s,f,w,nw;
    char nc;

    /* clear the array */
    topt = -1;
    text[0] = 0;

    /* HTML headings */
    if (encap == 0) {
        totext("<HTML><HEAD><TITLE>OCR Output</TITLE></HEAD><BODY>\n");
    }

    /* djvu headings */
    else if (encap == 3) {
        totext("select 1; set-txt\n");
        totext("  (page 0 0 %d %d\n",XRES,YRES);
    }

    /* reset stats */
    words = symbols = doubts = 0;

    /* copy the OCR result to the array text */
    for (i=0; i<=topln; ++i) {
        int ll,lr,lt,lb,pass;

        f = 0;
        nc = 0;

        /* djvu line descriptor */
        if (encap == 3) {
            int f;

            f = line[i].f;
            ll = word[f].l;
            lr = word[f].r;
            lt = word[f].t;
            lb = word[f].b;
            for (w=word[f].E, s=-1; w>=0; w=word[w].E) {
                if (ll > word[w].l)
                    ll = word[w].l;
                if (lr < word[w].r)
                    lr = word[w].r;
                if (lt > word[w].t)
                    lt = word[w].t;
                if (lb < word[w].b)
                    lb = word[w].b;
            }
            totext("   (line %d %d %d %d\n",ll,YRES-lb,lr,YRES-lt);
        }

        /* make the line output on a per-word basis */
        for (w=line[i].f, s=-1, pass=1; w>=0; ) {

            /* djvu */
            if ((pass==1) && (encap == 3)) {
                totext("    (word %d %d %d %d \"",word[w].l,YRES-lb,
                                                  word[w].r,YRES-lt);
            }

            nw = 1;

            for (a=word[w].F; a>=0; a=mc[a].E) {

                /* count symbols and doubts */
                if ((pass == 1) && (mc[a].tc != NOISE)) {
                    ++symbols;
                    doubts += uncertain(mc[a].tc);
                }

                /* no output for non-CHARs */
                /*
                if (mc[a].tc != CHAR)
                    continue;
                */

                /*
                   Add a space before start of word,
                   unless the "word" is a period or comma
                */
                if ((pass==1) && (nw) && (mc[a].tc!=DOT) && (mc[a].tc!=COMMA)) {
                    if (encap != 3)
                        totext(" ");
                    if (encap < 2) {
                        if (word[w].f & F_ITALIC)
                            totext("<I>");
                        if (word[w].f & F_BOLD)
                            totext("<B>");
                    }
                    nw = 0;
                    ++words;
                }

                /* send to output the ID of this symbol */
                if (mc[a].tr == NULL) {
                    if (pass == 2) {
                    }
                    else if (encap == 0)
                        totext("[<A HREF=symb/%d>%d</A>]",a,a);
                    else if (encap != 3)
                        totext("[%d]",a);
                }

                /* concatenate the transliteration of this symbol */
                else {
                    int k,ns;
                    char *sig=NULL,*sig2=NULL;

                    /* ISO-8859 encoding (Latin-only by now) */
                    for (ns=0, k=mc[a].sl; k>=0; k = mc[k].sl, ++ns) {
                        if (mc[k].tr != NULL) {
                            if (ns == 0)
                                sig = mc[k].tr->t;
                            else
                                sig2 = mc[k].tr->t;
                        }
                    }
                    if ((ns == 1) &&
                        /* (mc[a].tr->a == LATIN) && */
                        ((sig!=NULL) && (strlen(sig)==1)) &&
                        (strlen(mc[a].tr->t) == 1)) {
                        int l,c;

                        l = ((unsigned char *)sig)[0];
                        c = ((unsigned char *)(mc[a].tr->t))[0];
                        compose(&l,c);

                        if (pass == 2)
                            totext("(char %d %d %d %d %d \"%c\")\n",mc[a].l,YRES-lb,mc[a].r,YRES-lt,mc[c].bm,l);
                        else if ((l == '"') && (encap == 3))
                            totext("\\042");
                        else if (encap)
                            totext("%c",l);
                        else
                            totext("<A HREF=symb/%d>%c</A>",a,l);
                    }

                    else if ((ns == 1) &&
                        (sig!=NULL) && (strlen(sig)==1) && (sig[0]=='.') &&
                        (strlen(mc[a].tr->t) == 1)) {
                        int c;

                        c = ((unsigned char *)(mc[a].tr->t))[0];

                        /* dot over dot -> colon (GL) */
                        if (c == '.')
                            c = ':';

                        /* dot over comma -> semicolon (GL) */
		        else if (c == ',')
                            c = ';';

                        if (pass == 2)
                            totext("(char %d %d %d %d %d \"%c\")\n",mc[a].l,YRES-lb,mc[a].r,YRES-lt,mc[c].bm,c);
                        else if (encap)
                            totext("%c",c);
                        else
                            totext("<A HREF=symb/%d>%c</A>",a,c);
                    }

                    /* diaeresis */
                    else if ((ns == 2) &&
                        (sig!=NULL) && (strlen(sig)==1) && (sig[0]=='.') &&
                        (sig2!=NULL) && (strlen(sig2)==1) && (sig2[0]=='.') &&
                        (strlen(mc[a].tr->t) == 1)) {
                        int l,c;

                        l = '"';
                        c = ((unsigned char *)(mc[a].tr->t))[0];
                        compose(&l,c);

                        if (pass == 2)
                            totext("(char %d %d %d %d %d \"%c\")\n",mc[a].l,YRES-lb,mc[a].r,YRES-lt,mc[c].bm,l);
                        else if (encap)
                            totext("%c",l);
                        else
                            totext("<A HREF=symb/%d>%c</A>",a,l);
                    }

                    else {

                        /* first the signals */
                        for (k=mc[a].sl; k>=0; k = mc[k].sl) {
                            if (mc[k].tr != NULL) {
                                if (pass == 2)
                                    totext("(char %d %d %d %d %d \"%H\")\n",mc[k].l,YRES-lb,mc[k].r,YRES-lt,mc[k].bm,mc[k].tr->t);
                                else if (encap)
                                    totext("\\%H",mc[k].tr->t);
                                else
                                    totext("<A HREF=symb/%d>\\%H</A>",k,mc[k].tr->t);
                            }
                        }

                        /* now the symbol */
                        if (pass == 2)
                            totext("(char %d %d %d %d %d \"%H\")\n",mc[a].l,YRES-lb,mc[a].r,YRES-lt,mc[a].bm,mc[a].tr->t);
                        else if ((strcmp(mc[a].tr->t,"\"") == 0) && (encap == 3))
                            totext("\\\"");
                        else if (encap)
                            totext("%H",mc[a].tr->t);
                        else
                            totext("<A HREF=symb/%d>%H</A>",a,mc[a].tr->t);
                    }
                }

                ++nc;
            }

            if (pass == 2) {
            }

            /* djvu */
            else if (encap == 3) {
                if (word[w].E >= 0)
                    totext("\")\n");
                else
                    totext("\"))\n");
            }

            else if ((encap != 2) && (!nw)) {
                if (word[w].f & F_ITALIC)
                    totext("</I>");
                if (word[w].f & F_BOLD)
                    totext("</B>");
            }

            if ((pass == 1) && (encap == 3) && (djvu_char)) {
                pass = 2;
            }
            else {
                w = word[w].E;
                pass = 1;
            }
        }

        /* break only non-empty lines */
        if (nc > 0) {
            /* totext(" {{%d}}<BR>\n",line[i].ln); */
            if (encap)
                totext("\n");
            else
                totext("<BR>\n");
        }

    }

    /* djvu */
    if (encap == 3)
        totext(")\n");

    /* HTML footings */
    else if (encap == 0) {
        totext("</BODY></HTML>\n");
    }

    /* OOPS.. temporary workaround */
    /*
    doubts = topps + 1 - (symbols - doubts);
    symbols = topps + 1;
    */

    /* remember stats */
    dl_ne[cpage] = symbols;
    dl_db[cpage] = doubts;
    dl_w[cpage] = words;
}

/*

Decide if pattern i precedes pattern j using the flags set
by the user.

*/
int cmp_pattern(int i,int j)
{
    pdesc *c,*d;
    int r;

    c = pattern + i;
    d = pattern + j;

    /* first criterion is the source */
    if (*cm_e_sp != ' ') {
        if (c->d == NULL) {
            if (d->d != NULL)
                return(-1);
        }
        else {
            if (d->d == NULL)
                return(1);
            else if ((r=strcmp(c->d,d->d)) != 0)
                return(r);
        }
    }

    /* second criterion is the number of matches */
    if (*cm_e_sm != ' ') {
        if (c->cm > d->cm)
            return(-1);
        else if (c->cm < d->cm)
            return(1);
    }

    /* third criterion is the transliteration */
    if (*cm_e_st != ' ') {
        if (c->tr == NULL) {
            if (d->tr != NULL)
                return(-1);
        }
        else {
            if (d->tr == NULL)
                return(1);
            else if ((r=strcmp(c->tr,d->tr)) != 0)
                return(r);
        }
    }

    /* third criterion is the number of pixels */
    if (*cm_e_sn != ' ') {
        if (c->bp < d->bp)
            return(-1);
        else if (c->bp > d->bp)
            return(1);
    }

    /* fourth criterion is the width */
    if (*cm_e_sw != ' ') {
        if (c->bw < d->bw)
            return(-1);
        else if (c->bw > d->bw)
            return(1);
    }

    /* fifth criterion is the height */
    if (*cm_e_sh != ' ') {
        if (c->bh < d->bh)
            return(-1);
        else if (c->bh > d->bh)
            return(1);
    }

    /* last criterion is the index */
    if (i < j)
        return(-1);
    else if (i > j)
        return(1);

    /* no difference */
    return(0);
}

/*

Generate the HTML contents of the window PATTERN (list).

*/
void mk_pattern_list(void)
{
    pdesc *d;
    int i,k,n;

    /* clear the array */
    topt = -1;
    text[0] = 0;

    /* HTML headings */
    totext("<HTML><HEAD><TITLE>List of Patterns</TITLE></HEAD><BODY>\n");

    /* names of the fields */
    totext("<BR><TABLE><TR>\n");
    totext("<TD BGCOLOR=WHITE>pos</TD>\n");
    totext("<TD BGCOLOR=WHITE>frag</TD>\n");
    totext("<TD BGCOLOR=WHITE>img</TD>\n");
    totext("<TD BGCOLOR=WHITE>id</TD>\n");
    totext("<TD BGCOLOR=WHITE>transliteration</TD>\n");
    totext("<TD BGCOLOR=WHITE>matches</TD>\n");
    totext("<TD BGCOLOR=WHITE>size</TD>\n");
    totext("<TD BGCOLOR=WHITE>type</TD>\n");
    totext("<TD BGCOLOR=WHITE>symbol</TD>\n");
    totext("<TD BGCOLOR=WHITE>source</TD></TR>\n");

    /* sort patterns before displaying */
    if (pattern != NULL) {
        if (slistsz <= topp)
            slist = c_realloc(slist,(slistsz=topp+256)*sizeof(int),NULL);
        for (i=0; i<=topp; ++i)
            slist[i] = i;
        qsf(slist,0,topp,0,cmp_pattern);
    }

    /*
        per-pattern properties
    */
    for (k=0; k<=topp; ++k) {

        /* ignore fragments, if requested */
        if ((*cm_v_of != ' ') && (pattern[k].f & F_FRAG))
            continue;

        d = pattern + slist[k]; 

        /* index */
        totext("<TR><TD>%d</TD>\n",k);
        /*
        totext("<TD><INPUT TYPE=CHECKBOX NAME=FRAG%d VALUE=B></TD>\n",d->id);
        */

        /* is fragment flag */
        if (d->f & F_FRAG)
            totext("<TD>x</TD>\n");
        else
            totext("<TD></TD>\n");

        /* bitmap */
        totext("<TD><IMG SRC=pattern/%d></TD>\n",d->id);

        /* link to edit */
        totext("<TD><A HREF=edit/%d>%d</A></TD>",slist[k],d->id);

        /*
            Editable transliteration. This feature is currently
            disabled. We're displaying a non-editable transliteration
            instead. The problem concerns the submission of the
            transliteration (it's not implemented).
        */
        /*
        totext("<TD><INPUT TYPE=TEXT NAME=TR%d VALUE=\"%s\"></TD>\n",
                d->id,((d->tr==NULL) ? "" : d->tr));
        */

        /* non editable transliteration */
        totext("<TD BGCOLOR=WHITE>%H\n",((d->tr==NULL) ? "" : d->tr));

        /* matches and size */
        totext("<TD>%d</TD><TD>%dx%dx%d</TD>\n",
                d->cm,d->bw,d->bh,d->bp);

        /* type and symbol */
        totext("<TD>%d</TD><TD>%d</TD>\n",d->pt,d->e);

        /* page */
        n = pagenbr(d->d);
        if (n >= 0)
            totext("<TD><A HREF=load/%d>%H</A>",n,d->d);
        totext("</TD></TR>\n");
    }
    totext("</TABLE>\n");

    /* HTML footings */
    totext("</BODY></HTML>\n");
}

/*

Generate HTML account list for alphabet a.

*/
void acc_alpha(int a)
{
    int i,j,T;
    int m[256];
    unsigned char *t;
    alpharec *A;

    for (i=0; i<256; ++i)
        m[i] = 0;

    /*
        Account samples for each alphabet element.
        BUG: macros (like \alpha) are not accounted.
    */    
    for (i=0; i<=topp; ++i) {
        if ((pattern[i].a == a) && (pattern[i].pt == cpt)) {
            t = (unsigned char *) (pattern[i].tr);
            if ((t != NULL) && (strlen(t) == 1))
                ++(m[*t]);
        }
    }
    
    /* Decimal digits */
    if (a == NUMBER) {
        T = number_sz;
        A = number;
    }

    /* Latin letters */
    else if (a == LATIN) {
        T = latin_sz;
        A = latin;
    }

    /* Greek letters */
    else if (a == GREEK) {
        T = greek_sz;
        A = greek;
    }

    /* unsupported (by now) */
    else
        return;

    totext("<BR><BR><TABLE>\n");

    /* lower */
    if (a != NUMBER) {
        totext("<TR><TD BGCOLOR=WHITE>SYMB</TD><TD BGCOLOR=WHITE>QTY</TD>");

        /* names of the classification methods */
        totext("<TD BGCOLOR=WHITE>sk</TD>");
        totext("<TD BGCOLOR=WHITE>bm</TD>");
        totext("<TD BGCOLOR=WHITE>pd</TD>");
    }

    /* capitals */
    totext("<TD BGCOLOR=WHITE>SYMB</TD><TD BGCOLOR=WHITE>QTY</TD>");

    /* names of the classification methods */
    totext("<TD BGCOLOR=WHITE>sk</TD>");
    totext("<TD BGCOLOR=WHITE>bm</TD>");
    totext("<TD BGCOLOR=WHITE>pd</TD></TR>\n");

    for (i=0; i<T; ++i) {
        int c,s;
        char *C;

        /* lower */
        if (a != NUMBER) {
            c = A[i].sc;
            s = pt[cpt].sc[c];

            totext("<TR><TD>%H:</TD><TD>%d</TD>",A[i].ln,m[c]);

            for (j=CL_BOT; j<=CL_TOP; ++j) {
                C = (s & (1<<(j-1))) ? " CHECKED" : "";
                totext("<TD><INPUT TYPE=CHECKBOX NAME=%d_%d%H></TD>",j,c,C);
            }
        }
        else
            totext("<TR>");

        /* capital */
        c = A[i].cc;
        s = pt[cpt].sc[c];

        totext("<TD>%H:</TD><TD>%d</TD>",A[i].LN,m[c]);

        for (j=CL_BOT; j<=CL_TOP; ++j) {
            C = (s & (1<<(j-1))) ? " CHECKED" : "";
            totext("<TD><INPUT TYPE=CHECKBOX NAME=%d_%d%H></TD>",j,c,C);
        }

        totext("</TR>\n");

    }
    totext("</TABLE>\n");
}

/*

Generate the HTML contents of the window PATTERN (types).

*/
void mk_pattern_types(void)
{
    ptdesc *d;

    /* clear the array */
    topt = -1;
    text[0] = 0;

    /* HTML headings */
    totext("<HTML><HEAD><TITLE>Pattern types</TITLE></HEAD><BODY>\n");

    if ((1 <= cpt) && (cpt <= toppt)) {
        char a[18];

        snprintf(a,17,"PATTERN TYPE %d",cpt);
        a[17] = 0;
        pm_hl(a);

        /* open FORM */
        totext("<FORM ACTION=upd>\n");

        d = pt + cpt;

        totext("<BR><TABLE>\n");

        pm_tlc("AT","Auto-tune",d->at,1);
        pm_tlc("HB","is bold",C_ISSET(d->f,F_BOLD),!d->at);
        pm_tlc("HI","is italic",C_ISSET(d->f,F_ITALIC),!d->at);

        pm_tt("N","font name",d->n,!d->at);
        pm_tl("A","Ascent",d->a,!d->at);
        pm_tl("D","Descent",d->d,!d->at);
        pm_tl("XH","X_height",d->xh,!d->at);
        pm_tl("DD","Dot diameter",d->dd,!d->at);
        pm_tl("LS","Line separation",d->ls,!d->at);
        pm_tl("SS","Symbol separation",d->ss,!d->at);
        pm_tl("WS","Word separation",d->ws,!d->at);
        pm_tl("BD","Baseline displacement",d->bd,!d->at);

        totext("</TABLE>\n");

        /* consistency facility */
        totext("<BR><INPUT TYPE=SUBMIT VALUE=CONSIST>\n");

        /* accounting */
        {
            int a;

            totext("<BR>");
            pm_hl("ACCOUNTING");

            totext("<BR>");
            a = pt[cpt].acc;
            pm_cbox("AA",a&1,"Account accented letters<BR>");
            pm_cbox("AD",a&(1<<NUMBER),"Account Decimal digits<BR>");
            pm_cbox("AL",a&(1<<LATIN),"Account Latin letters<BR>");
            pm_cbox("AG",a&(1<<GREEK),"Account Greek letters<BR>");

            if (a&(1<<NUMBER)) {
                acc_alpha(NUMBER);
            }

            if (a&(1<<LATIN)) {
                acc_alpha(LATIN);
            }

            if (a&(1<<GREEK)) {
                acc_alpha(GREEK);
            }
        }

        /* close form */
        totext("</FORM>\n");

    }

    /*
        This is a first tentative to create a tool for manual
        adjustments of symbol geometry.
    */
    {
        pdesc *p;
        int i,j;

        totext("<BR>");
        pm_hl("BASELINES (first tentative, please ignore)");

        totext("<BR><TABLE CELLSPACING=2><TR>\n");
        for (i=0, j=0; (j<10) && (i<=topp); ++i) {
            p = pattern + i;
            if (p->pt == cpt) {
                totext("<TD><IMG SRC=apatt/%d></TD>\n",p->id);
                ++j;
            }
        }
        totext("</TR></TABLE>\n");
    }

    /* HTML footings */
    totext("<BR></BODY></HTML>\n");
}

/*

Generate the HTML contents of the window PATTERN (PROPS) to edit
the properties of the current pattern.

Includes fixes by John Wehle.

*/
void mk_pattern_props(void)
{
    pdesc *s;
    int n;
    int tp;

    /* clear the array */
    topt = -1;
    text[0] = 0;

    if ((cdfc<0) || (topp<cdfc))
        return;

    /* "the pattern is from this page" flag */
    s = pattern + cdfc;
    tp = ((s->d!=NULL) && (strcmp(s->d,pagename) == 0));

    /* HTML headings */
    totext("<HTML><HEAD><TITLE>Pattern Props</TITLE></HEAD><BODY>\n");

    /* begin the form */
    n = pagenbr(s->d);
    totext("Pattern %d (id %d):<BR>",cdfc,s->id);

    /* include the web clip */
    if (*cm_v_wclip != ' ') {
        /*
        if (tp) {
            wrmc8(pattern[cdfc].e,NULL,NULL);
            totext("<IMG SRC=internal><BR>\n");
        }
        else if (n >= 0) {
            totext("<BR>[To see this clip, load]<BR>");
            totext("[the page <A HREF=load/%d>%s</A>]<BR><BR>\n",n,s->d);
        }
        else */ {
            totext("<BR>[The clip for patterns is]<BR>");
            totext("[no longer available]<BR>\n");
        }
    }

    /* include the pattern bitmap */
    totext("<TD><IMG SRC=pattern/%d></TD>\n",s->id);

    /* open FORM */
    totext("<FORM ACTION=upd>\n");

    /*
        The transliteration. Edition is available only
        if the page from which the pattern came from is
        currently loaded.
    */
    if (tp) {
        if (s->tr == NULL)
            totext("<INPUT TYPE=TEXT VALUE=\"\"><BR>");
        else
            totext("<INPUT TYPE=TEXT VALUE=\"%H\"><BR>",s->tr);
    }
    else {
        if (s->tr == NULL)
            totext("(untransliterated)<BR>");
        else
            totext("Transliteration: %H<BR>",s->tr);
    }

    /* other details */
    totext("Alphabet: %H<BR>\n",aname(s->a));
    totext("Type: %d<BR>\n",s->pt);

    /*
       now this stuff is being read from the buttons,
       but someday this code may be used elsewhere.
    */
    /*
    totext("Alphabet: <SELECT NAME=alphabet>\n");
    totext("<OPTION VALUE=web>Latin\n");
    totext("<OPTION VALUE=news>Numeric\n");
    totext("<OPTION VALUE=news>Greek\n");
    totext("<OPTION VALUE=news>Hebrew\n");
    totext("<OPTION VALUE=news>Arabic\n");
    totext("</SELECT NAME=alphabet><BR>\n");
    totext("<INPUT TYPE=CHECKBOX NAME=BOLD VALUE=B> Bold<BR>\n");
    totext("<INPUT TYPE=CHECKBOX NAME=ITALIC %sVALUE=I> Italic<BR>\n",
            (s->f & F_ITALIC) ? "CHECKED " : "");
    totext("<INPUT TYPE=CHECKBOX NAME=ULINE VALUE=U> Uline<BR>\n");
    if (strcmp(s->d,pagename) == 0) {
        totext("<INPUT TYPE=SUBMIT VALUE=update><BR>\n");
    }
    */

    /* close form */
    totext("</FORM>\n");

    /* the page name */
    totext("%d matches<BR>",s->cm);
    totext("From ");

    if (s->d != NULL) {
        if (n >= 0)
            totext("<A HREF=load/%d>%H</A><BR>\n",n,s->d);
        else
            totext("%H<BR>\n",s->d);
    }

    /* "Nullify and remove" link */
    {
        if (s->act >= 0) {
            totext("From <A HREF=act/%d>act %d</A><BR>",s->act,s->act);
        }
        totext("<A HREF=rs/%d>Nullify and remove</A><BR>",cdfc);
    }

    /* HTML footings */
    totext("</BODY></HTML>\n");
}

/*

Generate the HTML contents of the window PATTERN (action) to ask
the user about the action to perform.

*/
void mk_pattern_action(void)
{
    /* clear the array */
    topt = -1;
    text[0] = 0;

    /* HTML headings */
    totext("<HTML><HEAD><TITLE>Pattern Action</TITLE></HEAD><BODY>\n");

    /* title */
    totext("<BR><TABLE>\n");
    totext("<TR><TD BGCOLOR=WHITE>Warning</TD>");
    totext("<TD>the symbol being trained was</TD></TR>");
    totext("<TR><TD></TD><TD>previously found similar to one pattern</TD>\n");
    totext("</TABLE>");

    totext("<BR><TABLE>\n");

    totext("<TR><TD><IMG SRC=pattern/%d></TD>\n",r_tr_patt);
    totext("<TD BGCOLOR=WHITE>'%H' (pattern)</TD></TR>\n",r_tr_old);

    totext("<TR><TD><IMG SRC=symbol/%d></TD>\n",r_tr_symb);
    totext("<TD BGCOLOR=WHITE>'%H' (symbol)</TD></TR>\n",r_tr_new);

    totext("</TABLE>\n");

    /* open form */
    totext("<FORM action=action>\n");

    totext("<BR>Select the best alternative:<BR>\n");
    {
        char s[81];

        snprintf(s,80,"Both pattern and symbol are '%H'<BR>",r_tr_old);
        s[80] = 0;
        pm_radio("AP",action_type==1,1,s);
        snprintf(s,80,"Both pattern and symbol are '%H'<BR>",r_tr_new);
        pm_radio("AP",action_type==3,3,s);
        snprintf(s,80,"The symbol is '%H', the pattern is '%H'<BR>",r_tr_new,r_tr_old);
        pm_radio("AP",action_type==2,2,s);
    }

    /* consistency facility */
    totext("<BR><INPUT TYPE=SUBMIT VALUE=OK>\n");

    /* close form */
    totext("</FORM>");

    /* HTML footings */
    totext("</BODY></HTML>\n");
}

/*

Generate the TEXT contents of the window DEBUG.

*/
void mk_debug(void)
{
    /* clear the array */
    topt = -1;
    text[0] = 0;

    /* attach vocabulary */
    if (*cm_g_vocab != ' ') {
        load_vocab("vocab.txt");
    }

    /* attach debug messages */
    else if (*cm_g_log != ' ') {
        if (dbm != NULL)
            totext(dbm);
    }
}

/*

Human time. Converts seconds to a representation more
convenient for humans. The size of the representation
won't exceed 22 bytes (twice the maximum integer plus
two characters) plus the string terminator 0.

*/
char *ht(int t)
{
    static char hts[64];
    int d,h,m,s;

    if (t < 3600) {
        m = t / 60;
        s = t % 60;
        if (m > 0)
            sprintf(hts,"%dm%ds",m,s);
        else
            sprintf(hts,"%ds",s);
    }
    else if (t < 24*3600) {
        h = t / 3600;
        m = (t % 3600) / 60;
        sprintf(hts,"%dh%dm",h,m);
    }
    else {
        d = t / (24*3600);
        h = (t % (24*3600)) / 3600;
        sprintf(hts,"%dd%dh",d,h);
    }
    return(hts);
}

/* (book)


Analysing the statistics
------------------------

The "page (list)" tab offers recognition statistics on a per-page
basis. The contents of each column on this tab is described
below:

POS: The sequential position on the list. The current page
is informed by an asterisk on this column.

FILE: The name of the file that contains the PBM image of the
document.

RUNS: The number of OCR runs on this page. Partial OCR runs,
like classification (started by the "classify" button also count
as one run.

TIME: Total CPU time wasted with OCR operations on this
page. I/O time (reading and saving session files) is not
included.

WORDS: Current number of words on this page. This variable is
updated by the "build" step.

SYMBOLS: Current number of symbols on this page. This variable
is updated by the "build" step.

DOUBTS: Current number of untransliterated CHAR symbols on
this page. This variable is updated by the "build" step.

CLASSES: Current number of classes on this page.

FACT: Quotient between the number of symbols and the number of
classes.

RECOG: Quotient between (symbols-doubts) and symbols, where
"symbols" is the number of symbols and "doubts" is the number of
doubts as defined above.

PROGRESS: difference between the current recog rate and the
recog rate for the previous run.

*/

/*

Generate the HTML contents of PAGE (list) window (the list of
pages with various statistics).

*/
void mk_page_list(void)
{
    /* totals */
    int tr,tt,ts,td,tw,tc;
    float mr,mp,mf;

    /* open td */
    char *ot = "<TD BGCOLOR=WHITE>";

    /* other */
    int j,n;
    float r,p,f;

    /* clear the array */
    topt = -1;
    text[0] = 0;

    /* reset the totals */
    tr = tt = ts = td = tw = tc = 0;
    mr = mp = mf = 0.0;

    if (npages <= 0) {
        totext("No pages found");
        return;
    }

    /* HTML headings */
    totext("<HTML><HEAD><TITLE>List of Pages</TITLE></HEAD><BODY>\n");

    totext("<BR><TABLE><TR>\n");
    totext("%spos</TD>\n",ot);
    totext("%sfile</TD>\n",ot);
    totext("%sruns</TD>\n",ot);
    totext("%stime</TD>\n",ot);
    totext("%swords</TD>\n",ot);
    totext("%ssymbols</TD>\n",ot);
    totext("%sdoubts</TD>\n",ot);
    totext("%sclasses</TD>\n",ot);
    totext("%sfact</TD>\n",ot);
    totext("%srecog</TD>\n",ot);
    totext("%sprogress</TD></TR>\n",ot);

    for (n=j=0; n<npages; ) {

        totext("<TR>\n");
        if (n == cpage)
            totext("<TD>%d *</TD>",n);
        else
            totext("<TD>%d</TD>",n);
        totext("<TD><A HREF=load/%d>%H</A></TD>\n",n,pagelist+j);
        totext("<TD>%d</TD><TD>%s</TD>\n",dl_r[n],ht(dl_t[n]));
        totext("<TD>%d</TD>\n",dl_w[n]);
        totext("<TD>%d</TD><TD>%d</TD>\n",dl_ne[n],dl_db[n]);
        totext("<TD>%d</TD>\n",dl_c[n]);

        if (dl_ne[n] > 0)
            r = ((float) dl_ne[n] - dl_db[n]) / dl_ne[n];
        else
            r = 0.0;
        if (dl_c[n] > 0)
            f = ((float) dl_ne[n]) / dl_c[n];
        else
            f = 1.0;
        p = ((float)dl_lr[n]) / 100;

        totext("<TD>%1.2f</TD>\n",f);
        totext("<TD>%1.2f</TD><TD>%1.2f</TD>\n",r,r-p);
        totext("</TR>\n");

        tr += dl_r[n];
        tw += dl_w[n];
        tt += dl_t[n];
        ts += dl_ne[n];
        td += dl_db[n];
        tc += dl_c[n];
        mr += r;
        mp += (r-p);
        mf += f;

        /* to next filename */
        if (++n < npages)
            while (pagelist[j++] != 0);

    }

    if (npages > 1) {
        mr /= npages;
        mp /= npages;
        mf /= npages;
    }

    totext("<TR>\n");
    totext("%s</TD>%sTOTAL</TD>\n",ot,ot);
    totext("%s%d</TD>%s%s</TD>\n",ot,tr,ot,ht(tt));
    totext("%s%d</TD>\n",ot,tw);
    totext("%s%d</TD>%s%d</TD>\n",ot,ts,ot,td);
    totext("%s%d</TD>\n",ot,topp+1);

    if (topp >= 0)
        f = ((float) ts) / (topp + 1);
    else
        f = 1.0;

    totext("%s%1.2f</TD>\n",ot,f);
    totext("%s%1.2f</TD>%s%1.2f</TD>\n",ot,mr,ot,mp);
    totext("</TR>\n");

    totext("</TABLE>\n");

    /* HTML footings */
    totext("</BODY></HTML>\n");
}

/*

Revision operation name.

*/
char *opname(int o)
{
    if (o == REV_TR)
        return("transliteration");
    else if (o == REV_MERGE)
        return("symbol merge");
    else if (o == REV_SLINK)
        return("symbol linkage");
    else if (o == REV_ALINK)
        return("accent linkage");
    else if (o == REV_DIS)
        return("symbol disassemble");
    else if (o == REV_PATT)
        return("pattern transliteration");
    else
        return("unknown");
}

/*

Generates the HTML contents for window HISTORY (the properties of
revision act n).

*/
void mk_history(int n)
{
    adesc *a;
    char *t = "<TR><TD BGCOLOR=WHITE>";

    /* clear the array */
    topt = -1;
    text[0] = 0;

    /* the act to display */
    if ((n<0) || (actsz<=n)) {
        text[0] = 0;
        return;
    }
    a = act + n;

    /* HTML headings */
    totext("<HTML><HEAD><TITLE>Acts Browser</TITLE></HEAD><BODY>\n");

    pm_hl("ACTS TUNING");

    totext("<BR><TABLE>\n");

    /* preamble */
    totext("%sRevision act</TD><TD>%d</TD></TR>\n",t,n);
    totext("%sfrom page</TD><TD>%H</TD></TR>\n",t,a->d);
    totext("%snullificated ?</TD><TD>%H</TD></TR>\n",
        t,(C_ISSET(a->f,F_ISNULL) ? "yes":"no"));
    totext("%spropagability</TD><TD>%d</TD></TR>\n",
        t,(C_ISSET(a->f,F_PROPAG)));
    totext("%stype</TD><TD>%H</TD></TR>\n",t,opname(a->t));

    /* transliteration */
    if ((a->t == REV_TR) || (a->t == REV_PATT)) {
        if (a->t == REV_TR)
            totext("%ssymbol</TD><TD>%d</TD></TR>\n",t,a->mc);
        else
            totext("%spattern</TD><TD>%d</TD></TR>\n",t,a->mc);
        totext("%salphabet</TD><TD> %H</TD></TR>\n",t,aname(a->a));
        totext("%stransliteration</TD><TD> %H</TD></TR>\n",t,a->tr);
        totext("%sflags</TD><TD> %d</TD></TR>\n",t,a->f);
    }

    /* merged symbols */
    else if (a->t == REV_MERGE) {
        totext("%scomponents</TD><TD>%d and %d</TD></TR>\n",t,a->a1,a->a2);
    }

    /* symbol linkage */
    else if (a->t == REV_SLINK) {
        totext("%Hleft symbol</TD><TD>%d</TD></TR>\n",t,a->mc);
        totext("%Hright symbol</TD><TD>%d</TD></TR>\n",t,a->a1);
    }

    /* symbol linkage */
    else if (a->t == REV_ALINK) {
        totext("%Hbase symbol</TD><TD>%d</TD></TR>\n",t,a->mc);
        totext("%Haccent</TD><TD>%d</TD></TR>\n",t,a->a1);
    }

    /* symbol disassemble */
    else if (a->t == REV_DIS) {
        totext("%Hsymbol</TD><TD>%d</TD></TR>\n",t,a->mc);
    }

    /* reviewer data */
    totext("%Hsubmitted at</TD><TD>%H</TD></TR>\n",t,ctime(&(a->dt)));
    totext("%Hreceived from host</TD><TD>%H</TD></TR>\n",t,a->sa);
    totext("%Hreviewer</TD><TD>%H (%H)</TD></TR>\n",t,a->r,typename(a->rt));

    /* original revision data */
    if (a->ob != NULL)
        totext("%Horiginal book</TD><TD>%H</TD></TR>\n",t,a->ob);
    if (a->om >= 0)
        totext("%Horiginal symbol</TD><TD>%d</TD></TR>\n",t,a->om);
    if (a->od > 0)
        totext("%Horiginal submission</TD><TD>%H</TD></TR>\n",t,ctime(&(a->od)));
    if (a->or != NULL)
        totext("%Horiginal reviewer</TD><TD> %H</TD></TR>\n",t,a->or);

    totext("</TABLE>\n");

    /* HTML footings */
    totext("</BODY></HTML>\n");
}

/*

Generates the HTML contents for the TUNE form.

*/
void mk_tune(void)
{
    char *C;

    /* clear the array */
    topt = -1;
    text[0] = 0;

    /* HTML headings */
    totext("<HTML><HEAD><TITLE>OCR Tuning</TITLE></HEAD><BODY>\n");

    /* title */
    pm_hl("BASIC TUNING");

    /* be careful! */
    /*
    totext("<BR>Warning: most tunings must be worked as\n");
    totext("<BR>preliminary steps before OCR.\n");
    */

    /* open form */
    totext("<FORM action=tune><BR>\n");

    /* preprocessing */
    pm_hl("1. Preprocessing");
    totext("<BR>");
    pm_cbox("PPD",pp_deskew,"Use deskewer (set number of columns, see n. 6)<BR>");
    pm_cbox("PPDA",pp_deskew_accurate,"Accurate deskew<BR>");
    pm_cbox("PPB",pp_bal,"Apply balance (check to change thres. factor)<BR>");

    /* segmentation */
    pm_hl("2. Binarization/segmentation");
    totext("<BR>");
    pm_cbox("DR",pp_double,"Double resolution<BR>");
    pm_radio("TT",bin_method==1,1,"Use manual global thresholder<BR>");
    pm_radio("TT",bin_method==2,2,"Use histogram-based global thresholder<BR>");
    pm_radio("TT",bin_method==3,3,"Use local binarizer<BR>");
    pm_radio("TT",bin_method==4,4,"Strong mode (unfinished - don't try)<BR>");
    totext("<TABLE>\n"); 
    pm_tf("PPBD","threshold factor",pp_dark,pp_bal);
    pm_tf("PPAL","Try avoid links",pp_avoid_links,1);
    totext("</TABLE>\n"); 

    /* classifier type */
    pm_hl("3. Classification");
    C = (classifier==CL_SKEL) ? "CHECKED" : "";
    totext("<BR><INPUT TYPE=RADIO NAME=C %H VALUE=1>Skeleton-based classifier\n",C);
    C = (classifier==CL_BM) ? "CHECKED" : "";
    totext("<BR><INPUT TYPE=RADIO NAME=C %H VALUE=2>Border mapping classifier (experimental)\n",C);
    C = (classifier==CL_PD) ? "CHECKED" : "";
    totext("<BR><INPUT TYPE=RADIO NAME=C %H VALUE=3>Pixel distance classifier\n",C);
    totext("<BR>\n");

    /* other options */
    pm_cbox("PM",p_match,"Try partial matching<BR>");
    pm_cbox("BF",bf_auto,"Build bookfont automatically<BR>");

    /* merge of fragments */
    pm_hl("4. Fragments merging (not operational yet)");
    totext("<BR>");
    pm_cbox("MI",mf_i,"Merge internal fragments<BR>");
    pm_cbox("MB",mf_b,"Merge pieces on one same box<BR>");
    pm_cbox("MC",mf_c,"Merge close fragments<BR>");
    pm_cbox("MR",mf_r,"Recognition merging<BR>");
    pm_cbox("ML",mf_l,"Learned merging<BR>");

    /* magic numbers */
    pm_hl("5. Parameters for words and lines (DD = Dot Diameter)");

    totext("<BR><TABLE>\n");
    pm_tl("MWD","max word distance as percentage of x_height",m_mwd,1);
    pm_tl("MSD","max symbol distance as percentage of x_height",m_msd,1);
    pm_tf("DD","dot diameter (mm)",DD,1);
    pm_tl("MAE","max alignment error as percentage of DD",m_mae,1);
    pm_tl("DS","descent (relative to baseline) as percentage of DD",m_ds,1);
    pm_tl("AS","ascent (relative to baseline) as percentage of DD",m_as,1);
    pm_tl("XH","x_height (relative to baseline) as percentage of DD",m_xh,1);
    pm_tl("FS","steps required to complete the unity",m_fs,1);
    totext("<BR></TABLE>\n");

    /* page geometry */
    pm_hl("6. Page Geometry (mostly unnefective by now)");

    totext("<BR>\n");
    pm_cbox("A",(PG_AUTO),"auto-tune");

    totext("<BR><TABLE>\n");

    /* resolution */    
    pm_tl("R","resolution in dots per inch",DENSITY,!PG_AUTO);

    /* typical symbol dimension */
    pm_tf("LW","letter width (mm)",LW,!PG_AUTO);
    pm_tf("LH","letter height (mm)",LH,!PG_AUTO);
    
    /* Vertical separation between lines */
    pm_tf("LS","line separation (mm)",LS,!PG_AUTO);
    
    /* Number of text columns */
    pm_tl("TC","number of text columns",abs(TC),!PG_AUTO);
    pm_tlc("S","columns separated by vertical line",(SL!=0),!PG_AUTO);

    /* Page margins */
    pm_tf("LM","left margin (mm)",LM,!PG_AUTO);
    pm_tf("RM","right margin (mm)",RM,!PG_AUTO);
    pm_tf("TM","top margin (mm)",TM,!PG_AUTO);
    pm_tf("BM","bottom margin (mm)",BM,!PG_AUTO);
    
    totext("<BR></TABLE>\n");

    /* consistency facility */
    totext("<INPUT TYPE=SUBMIT VALUE=CONSIST>\n");

    /* close form */
    totext("</FORM>");

    /* HTML footings */
    totext("</BODY></HTML>\n");
}

/*

Generates the HTML contents for the TUNE (skel) form.

*/
void mk_tune_skel(void)
{
    int q;

    /* clear the array */
    topt = -1;
    text[0] = 0;

    spcpy(SP_GLOBAL,cdfc);

    /* HTML headings */
    totext("<HTML><HEAD><TITLE>Skeleton Tuning</TITLE></HEAD><BODY>\n");

    /* title */
    pm_hl("SKELETON TUNING");

    /* be careful! */
    /* totext("<BR>Warning: skeleton tuning\n"); */
    /* totext("<BR>must be worked before OCR.\n"); */

    /* open form */
    totext("<FORM action=tune>\n");

    totext("<BR><TABLE>\n");

    pm_tlc("SAT","Auto-tune",st_auto,1);

    pm_tli("SA","SA",SA,!st_auto);

    if (SA <= 2) {
        pm_tlf("RR","RR",RR,!st_auto);
        pm_tlf("MA","MA",MA,!st_auto);
        pm_tli("MP","MP",MP,!st_auto);
    }

    if (SA == 1)
        pm_tlf("ML","ML",ML,!st_auto);

    if (SA == 2)
        pm_tli("MB","MB",MB,!st_auto);

    if ((SA != 3) && (SA != 6) && (SA != 7))
        pm_tli("RX","RX",RX,!st_auto);

    if ((SA != 4) && (SA != 6) && (SA != 7))
        pm_tli("BT","BT",BT,!st_auto);

    spcpy(SP_GLOBAL,SP_DEF);

    if ((0<=cdfc) && (cdfc<=topp)) {
        q = skel_quality(cdfc);
        totext("<TR><TD BGCOLOR=DARKGRAY>Quality</TD><TD>");
        totext("%d (%dx%d pixels)",q,pattern[cdfc].bp,pattern[cdfc].sp);
        totext("</TD></TR>\n");
    }

    /* close table */
    totext("</TABLE>\n");

    /* start auto-tune */
    totext("<BR><INPUT TYPE=SUBMIT VALUE=TUNE>\n");

    /* close form */
    totext("</FORM>");

    /* last best quality computed (if any) */
    if (st_bq > 0.0) {
        totext("<BR>Best mean quality %3.2f\n",st_bq);
    }

    /* HTML footings */
    totext("</BODY></HTML>\n");
}

/*

Compare top and left limits of the specified ge's.

*/
int cmp_gep(int a,int b)
{
    if (ge[a].t < ge[b].t)
        return(-1);
    else if (ge[a].t > ge[b].t)
        return(1);
    else if (ge[a].l < ge[b].l)
        return(-1);
    else if (ge[a].l > ge[b].l)
        return(1);
    return(0);
}

/*

Convert ge array to plain text.

TODO: The test to check if two pieces of text will be put on
the same output line must be relaxed.

*/
void ge2txt(void)
{
    int *a,i,c,col,m,n;
    char w[81],*t;

    /* clear the array */
    topt = -1;
    text[0] = 0;

    /* count text ge's */
    for (i=c=0; i<=topge; ++i)
        c += (ge[i].type == GE_TEXT);

    /* list text ge's */
    a = alloca(c*sizeof(int));
    for (i=c=0; i<=topge; ++i)
        if (ge[i].type == GE_TEXT)
            a[c++] = i;

    /* sort by top and left limits */
    qsf(a,0,c-1,0,cmp_gep);

    /* white line */
    memset(w,' ',80);

    /* generates plain text output */
    for (i=0, col=0; i<c; ++i) {

        /* break last line */
        if ((i > 0) && (ge[a[i-1]].t < ge[a[i]].t)) {
            totext("\n");
            col = 0;
        }

        /* column adjustment */
        n = (ge[a[i]].l - col*DFW) / DFW;
        while (n > 0) {
            if ((m=n) > 80)
                m = 80;
            w[m] = 0;
            totext("%H",w);
            w[m] = ' ';
            col += m;
            n -= m;
        }

        /* send text to output */
        if ((t=ge[a[i]].txt) != NULL) {
            totext("%H",t);
            col += strlen(t);
        }
    }

    /* break last line */
    if (col > 0)
        totext("\n");
}

/*

Processing of the submitted form.

*/
void submit_form(int hti)
{
    int i;

    /*
        Submit the HTML form from the PAGE_SYMBOL
        window.
    
        This code assumes that there exists only one
        input field on the PAGE_SYMBOL window
    */
    if (CDW == PAGE_SYMBOL) {
        int i;

        CDW = PAGE_SYMBOL;
        to_tr[0] = 0;
        start_ocr(-2,OCR_REV,0);
        for (i=0; i<=topge; ++i) {
            if (ge[i].type == GE_INPUT) {
                strncpy(to_tr,ge[i].txt,MFTL);
                i = topge;
            }
        }
        to_tr[MFTL] = 0;
        to_rev = REV_TR;
    }

    /*
        Submit the HTML form from the PATTERN
        window.
    
        This code assumes that there exists only one
        input field on the PATTERN window
    */
    else if (dw[PATTERN].v) {
        pdesc *s;
        int i;
    
        if ((0 <= cdfc) && (cdfc <= topp)) {
            s = pattern + cdfc;
	
            /*
                Remark: we initialize the transliteration buffer to_tr
                after the start_ocr call becase start_ocr clear the
                buffer to_tr.
            */
            start_ocr(-2,OCR_REV,0);
            to_tr[0] = 0;
            for (i=0; i<=topge; ++i) {
                if (ge[i].type == GE_INPUT) {
                    strncpy(to_tr,ge[i].txt,MFTL);
                    i = topge;
                }
            }
            to_tr[MFTL] = 0;
            to_rev = REV_PATT;
        }
    }
    
    /*
        Submit the HTML form from the PATTERN_LIST
        window.
    
        By now the PATTERN_LIST windows does not contain
        a form anymore, but someday in the future
        we'll allow pattern edition on that window
        again...
    */
    /*
    if (CDW == PATTERN_LIST) {
        int i;
    
        for (i=0; i<=topge; ++i) {
            if (ge[i].type == GE_CBOX) {
                printf("%s has input type CHECKBOX and is ",ge[i].arg);
                if (ge[i].iarg == 2)
                    printf("CHECKED\n");
                else
                    printf("NOT CHECKED\n");
            }
            else if (ge[i].type == GE_INPUT) {
                printf("%s has input type TEXT and it's value is %s\n",
                       ge[i].arg,ge[i].txt);
            }
        }
    }
    */

    /*
        Submit the TUNE_SKEL form.
    */
    else if (dw[TUNE_SKEL].v) {

        CDW = TUNE_SKEL;

        /* submission through TUNE button: start auto-tune */
        if (hti >= 0) {

            if (!ocring) {
                tskel_ready = 0;
                start_ocr(-2,OCR_OPT,0);
            }
        }

        else {
            for (i=0; (i<=topge); ++i) {

                if (ge[i].type == GE_INPUT) {
                    if (strcmp(ge[i].arg,"SA") == 0)
                        SA = atoi(ge[i].txt);
                    else if (strcmp(ge[i].arg,"RR") == 0)
                        RR = atof(ge[i].txt);
                    else if (strcmp(ge[i].arg,"MA") == 0)
                        MA = atof(ge[i].txt);
                    else if (strcmp(ge[i].arg,"MP") == 0)
                        MP = atoi(ge[i].txt);
                    else if (strcmp(ge[i].arg,"ML") == 0)
                        ML = atof(ge[i].txt);
                    else if (strcmp(ge[i].arg,"MB") == 0)
                        MB = atoi(ge[i].txt);
                    else if (strcmp(ge[i].arg,"RX") == 0)
                        RX = atoi(ge[i].txt);
                    else if (strcmp(ge[i].arg,"BT") == 0)
                        BT = atoi(ge[i].txt);
                }

                else if (ge[i].type == GE_CBOX) {
                    if (strcmp(ge[i].arg,"SAT") == 0) {
                        st_auto = (ge[i].iarg == 2);
                    }
                }
            }
        }

        consist_skel();
#ifdef PATT_SKEL
        spcpy(cdfc,SP_GLOBAL);
        spcpy(SP_GLOBAL,SP_DEF);
#else
        spcpy(SP_DEF,SP_GLOBAL);
#endif
        dw[TUNE_PATTERN].rg = 1;
        dw[TUNE_SKEL].rg = 1;
        redraw_dw = 1;
        font_changed = 1;

        /* invalidate all skeletons */
        {
            int i;

            for (i=0; i<=topp; ++i)
                pattern[i].sw = -1;
        }
    }

    /*
        Submit the PATTERN_ACTION form.
    */
    else if (CDW == PATTERN_ACTION) {

        /* submission through OK button: get out from waiting */
        if (hti >= 0) {

            /* simulate ENTER effects */
            waiting_key = 0;
            key_pressed = 1;
            redraw_stline = 1;
        }

        {
            for (i=0; (i<=topge); ++i) {

                if ((ge[i].type == GE_RADIO) && (ge[i].iarg == 2)) {

                    /* selection of action to perform */
                    if (strcmp(ge[i].arg,"AP") == 0) {

                        action_type = atoi(ge[i].txt);
                    }
                }
            }
        }
    }

    /*
        Submit the TUNE form.
    */
    else if (CDW == TUNE) {
        int new_toppt = -1;

        for (i=0; (i<=topge); ++i) {

            /* radios */
            if ((ge[i].type == GE_RADIO) && (ge[i].iarg == 2)) {

                /* selection of classifier */
                if (strcmp(ge[i].arg,"C") == 0) {
                    classifier = atoi(ge[i].txt);
                    if ((classifier < CL_BOT) || (CL_TOP < classifier))
                        classifier = CL_SKEL;
                }

                /* selection of binarizer */
                else if (strcmp(ge[i].arg,"TT") == 0) {
                    bin_method = atoi(ge[i].txt);
                }
            }

            /* text input fields */
            if (ge[i].type == GE_INPUT) {

                /* resolution */
                if (strcmp(ge[i].arg,"R") == 0)
                    DENSITY = atoi(ge[i].txt);

                /* dark correction */
                else if (strcmp(ge[i].arg,"PPBD") == 0)
                    pp_dark = atof(ge[i].txt);

                /* avoid links */
                else if (strcmp(ge[i].arg,"PPAL") == 0)
                    pp_avoid_links = atof(ge[i].txt);

                /* letter width and height */
                else if (strcmp(ge[i].arg,"LW") == 0)
                    LW = atof(ge[i].txt);
                else if (strcmp(ge[i].arg,"LH") == 0)
                    LH = atof(ge[i].txt);

                /* dot diameter */
                else if (strcmp(ge[i].arg,"DD") == 0)
                    DD = atof(ge[i].txt);

                /* vertical line separation */
                else if (strcmp(ge[i].arg,"LS") == 0)
                    LS = atof(ge[i].txt);

                /* number of text columns */
                else if (strcmp(ge[i].arg,"TC") == 0)
                    TC = atoi(ge[i].txt);

                /* left, right, top and bottom  margins */
                else if (strcmp(ge[i].arg,"LM") == 0)
                    LM = atof(ge[i].txt);
                else if (strcmp(ge[i].arg,"RM") == 0)
                    RM = atof(ge[i].txt);
                else if (strcmp(ge[i].arg,"TM") == 0)
                    TM = atof(ge[i].txt);
                else if (strcmp(ge[i].arg,"BM") == 0)
                    BM = atof(ge[i].txt);

                /* magic numbers */
                else if (strcmp(ge[i].arg,"MWD") == 0)
                    m_mwd = atoi(ge[i].txt);
                else if (strcmp(ge[i].arg,"MSD") == 0)
                    m_msd = atoi(ge[i].txt);
                else if (strcmp(ge[i].arg,"MAE") == 0)
                    m_mae = atoi(ge[i].txt);
                else if (strcmp(ge[i].arg,"DS") == 0)
                    m_ds = atoi(ge[i].txt);
                else if (strcmp(ge[i].arg,"AS") == 0)
                    m_as = atoi(ge[i].txt);
                else if (strcmp(ge[i].arg,"XH") == 0)
                    m_xh = atoi(ge[i].txt);
                else if (strcmp(ge[i].arg,"FS") == 0)
                    m_fs = atoi(ge[i].txt);
            }

            /* the checkboxes... */
            else if (ge[i].type == GE_CBOX) {

                /* use Giulio Lunati's deskewer */
                if (strcmp(ge[i].arg,"PPD") == 0)
                    pp_deskew = (ge[i].iarg == 2);

                /* use Giulio Lunati's deskewer */
                else if (strcmp(ge[i].arg,"PPDA") == 0)
                    pp_deskew_accurate = (ge[i].iarg == 2);

                /* double resolution */
                else if (strcmp(ge[i].arg,"DR") == 0)
                    pp_double = (ge[i].iarg == 2);

                /* apply balance */
                else if (strcmp(ge[i].arg,"PPB") == 0)
                    pp_bal = (ge[i].iarg == 2);

                /* try partial matching */
                else if (strcmp(ge[i].arg,"PM") == 0)
                    p_match = (ge[i].iarg == 2);

                /* build bookfont automatically */
                else if (strcmp(ge[i].arg,"BF") == 0)
                    bf_auto = (ge[i].iarg == 2);

                /* merge internal fragments */
                else if (strcmp(ge[i].arg,"MI") == 0)
                    mf_i = (ge[i].iarg == 2);

                /* merge pieces on one same box */
                else if (strcmp(ge[i].arg,"MB") == 0)
                    mf_b = (ge[i].iarg == 2);

                /* merge close fragments */
                else if (strcmp(ge[i].arg,"MC") == 0)
                    mf_c = (ge[i].iarg == 2);

                /* recognition merging */
                else if (strcmp(ge[i].arg,"MR") == 0)
                    mf_r = (ge[i].iarg == 2);

                /* learned merging */
                else if (strcmp(ge[i].arg,"ML") == 0)
                    mf_l = (ge[i].iarg == 2);

                /* page geometry auto-tune */
                else if (strcmp(ge[i].arg,"A") == 0) {
                    int a;

                    a = PG_AUTO;
                    PG_AUTO = (ge[i].iarg == 2);
                    if (a != PG_AUTO) {
                        dw[TUNE].rg = 1;
                        redraw_dw = 1;
                    }
                }

                /* columns separated by vertical line */
                else if (strcmp(ge[i].arg,"S") == 0) {
                    SL = (ge[i].iarg == 2);
                }
            }
        }

        /* enlarge pattern types array if needed */
        if (new_toppt >= 0)
            enlarge_pt(new_toppt);

        /* consist fields */
        consist_density();
        consist_pg();
        consist_pp();
        consist_magic();
        dw[TUNE].rg = 1;
        redraw_dw = 1;
    }

    /*
        Submit the PATTERN (types) form.
    */
    else if (CDW == PATTERN_TYPES) {
        ptdesc *d;

        d = pt + cpt;

        /* no accounting by default */
        d->acc = 0;

        for (i=0; (i<=topge); ++i) {

            /* text input fields */
            if (ge[i].type == GE_INPUT) {

                if (strcmp(ge[i].arg,"N") == 0) {
                    strncpy(d->n,ge[i].txt,MFTL);
                    d->n[MFTL-1] = 0;
                }

                if (strcmp(ge[i].arg,"A") == 0)
                    d->a = atoi(ge[i].txt);

                if (strcmp(ge[i].arg,"D") == 0)
                    d->d = atoi(ge[i].txt);

                if (strcmp(ge[i].arg,"XH") == 0)
                    d->xh = atoi(ge[i].txt);

                if (strcmp(ge[i].arg,"DD") == 0)
                    d->dd = atoi(ge[i].txt);

                if (strcmp(ge[i].arg,"LS") == 0)
                    d->ls = atoi(ge[i].txt);

                if (strcmp(ge[i].arg,"SS") == 0)
                    d->ss = atoi(ge[i].txt);

                if (strcmp(ge[i].arg,"WS") == 0)
                    d->ws = atoi(ge[i].txt);

                if (strcmp(ge[i].arg,"BD") == 0)
                    d->bd = atoi(ge[i].txt);
            }

            /* the checkboxes... */
            else if (ge[i].type == GE_CBOX) {
                char *g;

                g = ge[i].arg;

                /* auto-tune */
                if (strcmp(g,"AT") == 0) {
                    int a;

                    a = d->at;
                    d->at = (ge[i].iarg == 2);
                    if (a != d->at) {
                        dw[PATTERN_TYPES].rg = 1;
                        redraw_dw = 1;
                    }
                }

                /* is bold */
                else if (strcmp(g,"HB") == 0) {
                    if (ge[i].iarg == 2)
                        C_SET(d->f,F_BOLD);
                    else
                        C_UNSET(d->f,F_BOLD);
                }

                /* is italic */
                else if (strcmp(g,"HI") == 0) {
                    if (ge[i].iarg == 2)
                        C_SET(d->f,F_ITALIC);
                    else
                        C_UNSET(d->f,F_ITALIC);
                }

                /* account accented letters */
                else if (strcmp(g,"AA") == 0) {
                    if (ge[i].iarg == 2)
                        d->acc |= 1;
                }

                /* account decimal digits */
                else if (strcmp(g,"AD") == 0) {
                    if (ge[i].iarg == 2)
                        d->acc |= (1 << NUMBER);
                }

                /* account Latin letters */
                else if (strcmp(g,"AL") == 0) {
                    if (ge[i].iarg == 2)
                        d->acc |= (1 << LATIN);
                }

                /* account Greek letters */
                else if (strcmp(g,"AG") == 0) {
                    if (ge[i].iarg == 2)
                        d->acc |= (1 << GREEK);
                }

                /* per-symbol per-method flag */
                else if (('0' <= g[0]) && (g[0] <= '9')) {
                    int j,m,c,l;

                    l = strlen(g); 
                    for (j=1; j<l && (g[j] != '_'); ++j);
                    if (j >= l) {
                        fatal(IE,"unexpected PATTERN TYPES field name");
                    }
                    m = atoi(g);
                    c = atoi(g+j+1);
                    if (ge[i].iarg == 2) {
                        (d->sc)[c] |= (1 << (m-1));
                    }
                    else {
                        (d->sc)[c] &= (0x0fffffff ^ (1 << (m-1)));
                    }
                }
            }
        }

        dw[PATTERN_TYPES].rg = 1;
        redraw_dw = 1;
        font_changed = 1;
    }

    form_changed = 0;
}

/*

Automatic submission of the current form.

The GUI forms do not have a "submit" button. Submission is
automatically performed when the user generates a mouse or
keyboard event. In some cases, events won't submit the current
form (for instance moving the mouse).

*/
void form_auto_submit(void)
{
    int last_dw = CDW;

    /*
        Do not submit PAGE_SYMBOL for invalid symbols.
    */
    if ((dw[PAGE].v) &&
        ((curr_mc < 0) ||
         (tops < curr_mc))) {

        form_changed = 0;
        return;
    }

    /*
       When the plate contains more than one window, we must
       activate the window that contains the form.
    */
    if (dw[PAGE_SYMBOL].v)
        CDW = PAGE_SYMBOL;
    else if (dw[TUNE_SKEL].v)
        CDW = TUNE_SKEL;

    /* submit! */
    if (form_changed &&
       ((CDW == PAGE_SYMBOL) ||
        (CDW == PATTERN) ||
        (CDW == PATTERN_LIST) ||
        (CDW == PATTERN_TYPES) ||
        (CDW == PATTERN_ACTION) ||
        (CDW == TUNE) ||
        (CDW == TUNE_SKEL) ||
        (CDW == TUNE_ACTS)))

        submit_form(-1);

    /* restore window */
    CDW = last_dw;

    /* this event should not happen */
    if (form_changed) {
        db("oops.. unattended form_changed");
        form_changed = 0;
    }
}

/*

Take actions on HTML windows. Because of auto-submission, this is
much like simulating CGI. The actions may be:

    toggle checkbox
    open select menu
    follow HREF link
    submit form
    attend stepper

When the user presses the mouse button 1 on some HTML clickable
element, the mouse event handling routine mactions_b1 calls
html_action informing the pointer coordinates. Note that the
current HTML element is available on curr_hti, so in most cases
the pointer coordinates will be ignored.

*/
void html_action(int x,int y)
{
    char *a;

    /* no current item */
    if (curr_hti < 0)
        return;

    /*
        A checkbox was clicked. Toggle its status
    */
    if (ge[curr_hti].type == GE_CBOX) {
        ge[curr_hti].iarg = 2 - ge[curr_hti].iarg;

        redraw_inp = 1;
        form_changed = 1;
        form_auto_submit();
        return;
    }

    /*
        A radio button was clicked. Toggle its status
        and unchek the other buttons with the same name
    */
    else if (ge[curr_hti].type == GE_RADIO) {
        ge[curr_hti].iarg = 2 - ge[curr_hti].iarg;

        /* must uncheck all other boxes */
        if (ge[curr_hti].iarg == 2) {
            int i;
            char *a = ge[curr_hti].arg;

            for (i=0; i<=topge; ++i) {
                if ((i!=curr_hti) &&
                    (ge[i].type == GE_RADIO) &&
                    (strcmp(ge[i].arg,a)==0)) {

                    ge[i].iarg = 0;
                }
            }
        }

        redraw_inp = 1;
        form_changed = 1;
        form_auto_submit();
        return;
    }

    /*
        A stepper was clicked. Increase or decrease the field
        and submit.
    */
    if (ge[curr_hti].type == GE_STEPPER) {
        edesc *c;
        int f,i,d;

        /* left stepper */
        if ((ge[curr_hti].iarg == 9) && (curr_hti < topge)) {
            c = ge + curr_hti + 1;
            d = -1;
        }

        /* right stepper */
        else if ((ge[curr_hti].iarg == 3) && (curr_hti > 0)) {
            c = ge + curr_hti - 1;
            d = 1;
        }

        /* invalid stepper */
        else {
            c = NULL;
            d = 0;
        }

        if ((c==NULL) || (c->type != GE_INPUT)) {
            fatal(IE,"failure on page renderization");
        }

        /* float field? */
        for (f=0, i=strlen(c->txt); i>=0; --i) {
            f |= ((c->txt)[i] == '.');
        }

        if (f) {
            snprintf(c->txt,c->tsz,"%3.2f",atof(c->txt)+((float)d)/m_fs);
        }
        else {
            snprintf(c->txt,c->tsz,"%d",atoi(c->txt)+d);
        }

        redraw_inp = 1;
        form_changed = 1;
        form_auto_submit();
        return;
    }

    /*
        A SELECT item was clicked. Open the menu
    */
    else if (ge[curr_hti].type == GE_SELECT) {
        CX0 = x;
        CY0 = y;
        cmenu = CM+ge[curr_hti].iarg;
        redraw_menu = 1;
        return;
    }

    /*
        A SUBMIT button was clicked. Submit the form.
    */
    if (ge[curr_hti].a == HTA_SUBMIT) {

        /* per-form processing */
        submit_form(curr_hti);

        return;
    }

    /*
        The only other action supported is "follow hyperlink". So
        we'll try to follow the current link (if any). First try
        to get HREF link.
    */
    if (ge[curr_hti].a == HTA_HREF)
        a = ge[curr_hti].arg;
    else if (ge[curr_hti].a == HTA_HREF_C)
        a = ge[ge[curr_hti].iarg].arg;
    else
        a = NULL;

    /*
        Now follow the link (if any).
    */
    if (a != NULL) {

        /* "load page" link */
        if (strncmp(a,"load/",5) == 0) {
            int l;

            l = atoi(a+5);
            if (l < 0)
                l = 0;
            if (l >= npages)
                l = npages - 1;
            if (cpage != l) {
                start_ocr(l,OCR_LOAD,0);
            }
            else if (!ocring) {
                setview(PAGE);
            }
        }

        /* "edit pattern" link */
        else if (strncmp(a,"edit/",5) == 0) {

            cdfc = atoi(a+5);
            if (cdfc < 0)
                cdfc = 0;
            if (cdfc > topp)
                cdfc = topp;
            edit_pattern(1);
        }

        /* "edit symbol" link */
        else if (strncmp(a,"symb/",5) == 0) {
            int cur_dw;

            curr_mc = atoi(a+5);
            if ((curr_mc < 0) || (tops < curr_mc))
                curr_mc = 0;
            if (curr_mc <= tops) {
                dw[PAGE_SYMBOL].rg = 1;
                cur_dw = CDW;
                CDW = PAGE;
                check_dlimits(1);
                CDW = cur_dw;
                force_redraw();
            }
        }

        /* "nullify act" link */
        else if (strncmp(a,"nullify/",8) == 0) {
            int n;

            n = atoi(a+8);
            nullify(n);
            dw[PAGE_SYMBOL].rg = 1;
            dw[PATTERN].rg = 1;
            force_redraw();
        }

        /* "remove pattern" link */
        else if (strncmp(a,"rs/",3) == 0) {
            int n;

            n = atoi(a+3);
            if (pattern[n].act >= 0) {
                nullify(pattern[n].act);
                dw[PAGE_SYMBOL].rg = 1;
                dw[PATTERN].rg = 1;
            }
            rm_pattern(n);
            if (dw[PATTERN].v) {
                edit_pattern(0);
            }
            force_redraw();
        }

        /* "show act" link */
        else if (strncmp(a,"act/",4) == 0) {
            int n;

            n = atoi(a+4);
            if ((0 <= n) && (n <= topa))
                curr_act = n;
            setview(TUNE_ACTS);
            dw[TUNE_ACTS].rg = 1;
        }

        /* "word focus" link */
        else if (strncmp(a,"wfocus/",7) == 0) {
            int n;

            n = atoi(a+7);
            if ((0 <= n) && (n <= topw)) {

                zones = 1;
                limits[0] = word[n].l;
                limits[1] = word[n].t;
                limits[2] = word[n].l;
                limits[3] = word[n].b;
                limits[4] = word[n].r;
                limits[5] = word[n].b;
                limits[6] = word[n].r;
                limits[7] = word[n].t;

                setview(PAGE);
                X0 = (word[n].l+word[n].r-HR) / 2;
                Y0 = (word[n].t+word[n].b-VR) / 2;
                check_dlimits(0);
            }
        }
    }
}

/*

Copy the pattern bitmap to the cfont buffer.

*/
void p2cf(void)
{
    pdesc *d;
    unsigned char *p;
    int x,y,m;

    if ((0 <= cdfc) && (cdfc <= topp)) {

        /* copy pattern bitmap from b buffer to cfont */
        d = pattern + cdfc;
        for (y=0; y<FS; ++y) {
            p = d->b + BLS*y;
            m = 128;
            for (x=0; x<FS; ++x) {
                cfont[x+y*FS] = ((*p & m) != 0) ? BLACK : WHITE;
                if ((m >>= 1) <= 0) {
                    ++p;
                    m = 128;
                }
            }
        }

        /* copy pattern skeleton from s buffer to cfont */
        for (y=d->sy; y<FS; ++y) {
            p = d->s + BLS*(y-d->sy);
            m = 128;
            for (x=d->sx; x<FS; ++x) {
                if ((*p & m) != 0)
                    cfont[x+y*FS] |= D_MASK;
                if ((m >>= 1) <= 0) {
                    ++p;
                    m = 128;
                }
            }
        }
    }
}

/*

Copy the properties of the current pattern to the display
buffers.

*/
void edit_pattern(int change)
{
    pdesc *d;

/*
    if (CDW != PATTERN)
        setview(PATTERN);
*/

    if (cdfc < 0)
        cdfc = 0;
    if (topp < cdfc)
        cdfc = topp;
    if ((0 <= cdfc) && (cdfc <= topp)) {

        /* copy pattern flags to GUI buttons */
        d = pattern + cdfc;
    }

    else {
        memset(cfont,WHITE,FS*FS);

        /* informative message */
        /* show_hint(1,"No patterns"); */
    }

    redraw_stline = 1;
    redraw_dw = 1;
    redraw_button = -1;
    redraw_inp = 1;

    /*
        Ask generation and renderization of PATTERN.
    */
    dw[PATTERN].rg = 1;

    /* change window, if requested */
    if (change)
        setview(PATTERN);
}

/*

Extract the visible portion of DEBUG window.

*/
void extract_viewable(void)
{
    int old;

    old = CDW;
    CDW = DEBUG;

    /* extract */
    topdv = -1;
    text_switch = 1;
    draw_dw(0,0);
    text_switch = 0;

    CDW = old;
}

