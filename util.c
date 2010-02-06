#include <stdlib.h>
#include <execinfo.h>
#include <stdio.h>
#include <assert.h>
#include "common.h"

#define DONT_DIE

void real_UNIMPLEMENTED(const char *file, const char *function) {
#ifdef DONT_DIE
        fprintf(stderr, "Unimplemented function: %s:%s\n", file, function);
#else
        void *symlocs[20];
        char **syms;
        int count, i;
        count = backtrace(symlocs, 20);
        fprintf(stderr, "\nFunction not implemented. Backtrace:\n");
        syms = backtrace_symbols(symlocs, count);
        for (i = 0; i < count; i++)
                fprintf(stderr, "\t%3d: %s\n", i, syms[i]);
        fprintf(stderr, "Aborting\n");

        abort();
#endif
}


gboolean flags[FL_NFLAGS];

void set_flag(flag_t fl, gboolean value) {
        assert(fl >= 0 && fl < FL_NFLAGS);
        flags[fl] = value;
}

gboolean get_flag(flag_t fl) {
        assert(fl >= 0 && fl < FL_NFLAGS);
        return flags[fl];
}

void init_flags() {
        // initialize everything to FALSE..
        int i;
        for (i = 0; i < FL_NFLAGS; i++)
                flags[i] = FALSE;
        set_flag(FL_ALPHABET_LATIN, TRUE);
        set_flag(FL_ALPHABET_NUMBERS, TRUE);
        set_flag(FL_ATTACH_LOG, TRUE);
        set_flag(FL_CURRENT_ONLY, TRUE);
        set_flag(FL_OMIT_FRAGMENTS, TRUE);
        set_flag(FL_RESCAN, TRUE);
        set_flag(FL_SHOW_CLASS, TRUE);
}


// generic file opening utilities
typedef struct {
        gchar *extension;
        gpointer (*open)(const gchar* path, const gchar* mode);
        gint (*read)(gpointer handle, gpointer buf, gsize len);
        gint (*write)(gpointer handle, gpointer buf, gsize len);
        void (*flush)(gpointer handle);
        void (*close)(gpointer handle);
} CFileFuncs;

struct _CFile {
        gpointer handle;
        CFileFuncs* vtable;
};



#ifdef HAVE_LIBBZ2
static gpointer cf_bz2_open(const gchar* path, const gchar* mode) {
        return BZ2_bzopen(path,mode);
}
static gint cf_bz2_read(gpointer handle, gpointer buf, gsize len) {
        return BZ2_bzread((BZFILE*)handle, buf, len);
}
static gint cf_bz2_write(gpointer handle, gpointer buf, gsize len) {
        return BZ2_bzwrite((BZFILE*)handle, buf, len);
}
static void cf_bz2_flush(gpointer handle) {
        BZ2_bzflush((BZFILE*)handle);
}
static void cf_bz2_close(gpointer handle) {
        BZ2_bzclose((BZFILE*)handle);
}
#endif


/* uncompressed streams */
static gpointer cf_plain_open(const gchar* path, const gchar* mode) {
        return fopen(path,mode);
}
static gint cf_plain_read(gpointer handle, gpointer buf, gsize len) {
        return fread(buf, len, 1, (FILE*)handle);
}
static gint cf_plain_write(gpointer handle, gpointer buf, gsize len) {
        return fwrite(buf, len, 1, (FILE*)handle);
}
static void cf_plain_flush(gpointer handle) {
        fflush((FILE*)handle);
}
static void cf_plain_close(gpointer handle) {
        fclose((FILE*)handle);
}


static CFileFuncs file_handlers[] = {
#ifdef HAVE_LIBLZMA
        { ".xz", cf_xz_open, cf_lzma_read, cf_lzma_write, cf_lzma_flush, cf_lzma_close },
        { ".lzma", cf_lzma_open, cf_lzma_read, cf_lzma_write, cf_lzma_flush, cf_lzma_close },
#endif
#ifdef HAVE_LIBBZ2
        { ".bz2", cf_bz2_open, cf_bz2_read, cf_bz2_write, cf_bz2_flush, cf_bz2_close },
#endif
#ifdef HAVE_ZLIB
        { ".gz", cf_gz_open, cf_gz_read, cf_gz_write, cf_gz_flush, cf_gz_close },
#endif
        { "", cf_plain_open, cf_plain_read, cf_plain_write, cf_plain_flush, cf_plain_close },
        {}
};

// generic file utils...
CFile* cfopen(const gchar* path, const gchar* mode) {
        gpointer handle = NULL;
        CFileFuncs *vt;
        CFileFuncs *preferred = NULL;
        CFile *ret = NULL;
        for (vt = file_handlers; vt->open != NULL; vt++) {
                if (g_str_has_suffix(path, vt->extension)) {
                        preferred = vt;
                        break;
                }
        }
        assert(preferred != NULL);
        // first try the extension
        if (preferred &&
            (preferred->extension[0] != 0 || mode[0] == 'w')) {
                handle = preferred->open(path, mode);
                if (handle != NULL) {
                        vt = preferred;
                        goto found;
                }
        }
        assert(mode[0] == 'r');
        // try all formats.
        for (vt = file_handlers; vt->open != NULL; vt++) {
                handle = vt->open(path, mode);
                if (handle != NULL)
                        goto found;
        }
        if (handle == NULL)
                goto out;
 found:
        assert(ret->handle != NULL);
        ret = g_new0(CFile, 1);
        ret->handle = handle;
        ret->vtable = vt;
 out:
        return ret;
}
gint cfread(CFile *f, gpointer buf, gsize len) {
        return f->vtable->read(f->handle, buf, len);
}
gint cfwrite(CFile *f, gpointer buf, gsize len) {
        return f->vtable->write(f->handle, buf, len);
}
void cfflush(CFile *f) {
        f->vtable->flush(f->handle);
}
void cfclose(CFile* f) {
        f->vtable->close(f->handle);
        g_free(f);
}
