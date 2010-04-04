#include <stdlib.h>
#include "common.h"
#include "gui.h"

extern void init_flags();
extern int pp_only, finish, links;


/*

The program begins here.

*/


static gboolean reviewer_type_cb(const gchar* opt_name, const gchar* opt_value, gpointer data, GError** err) {
        if (g_strcmp0(opt_value, "A") == 0)
                revtype = ARBITER;
        else if (g_strcmp0(opt_value, "T") == 0)
                revtype = TRUSTED;
        else if (g_strcmp0(opt_value, "N") == 0)
                revtype = ANON;
        else {
                g_set_error(err, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE, "Invalid reviewer type %s", opt_value);
                return FALSE;
        }
        return TRUE;
}
static gboolean output_format_cb(const gchar* opt_name, const gchar* opt_value, gpointer data, GError** err) {
        // text,html,djvu
        if (g_strcmp0(opt_value, "text") == 0)
                outp_format = OE_TEXT;
        else if (g_strcmp0(opt_value, "html") == 0)
                outp_format = OE_ENCAP_HTML;
        else if (g_strcmp0(opt_value, "djvu") == 0) {
                g_warning("DJVu output not yet supported");
                outp_format = OE_DJVU;
        } else {
                g_set_error(err, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE, "Invalid output format %s", opt_value);
                return FALSE;
        }
        return TRUE;
}

static gboolean preinit_arg_hook(GOptionContext* context,
                                 GOptionGroup* group,
                                 gpointer data,
                                 GError** err) {
        spcpy(SP_GLOBAL, SP_DEF);

        if (workdir != NULL)
                g_free(workdir);
        if (page_dir != NULL)
                g_free(page_dir);
        workdir = page_dir = NULL;

        return TRUE;
}

static gboolean postinit_arg_hook(GOptionContext* context,
                                  GOptionGroup* group,
                                  gpointer data,
                                  GError** err) {
        /* book font path */
        patterns_file = g_build_filename(workdir, zsession? "patterns.gz":"patterns", NULL);
        acts_file = g_build_filename(workdir, zsession? "acts.gz":"acts", NULL);

        /* doubts dir */
        if (web) {
                doubtsdir = g_build_filename(workdir,doubts, NULL);
        }

        /* no current page by now */
        cpage = -1;

        /* use default skeleton parameters */
        //if (havek == 0)
        skel_parms(",,,,,,,");

        if(page_dir == NULL)
                page_dir = g_strdup("./");
        if(workdir == NULL)
                workdir = g_strdup(page_dir);
        return TRUE;
}

static GOptionEntry optDesc[] = {
        {"batch", 'b', 0, G_OPTION_ARG_NONE, &batch_mode, "Run in batch mode (non-interactive)", NULL },
        {"debug", 'd', 0, G_OPTION_ARG_NONE, &debug, "Enable debugging", NULL },
        {"reviewer", 'r', 0, G_OPTION_ARG_STRING, &reviewer, "Reviewer name", "reviewer"},
        {"reviewer-type", 't', 0, G_OPTION_ARG_CALLBACK, reviewer_type_cb, "Reviewer type", "A|N|T"},
        {"page-dir", 'f', 0, G_OPTION_ARG_FILENAME, &page_dir, "Directory containing page images", "dir"},
        {"output-format", 'o', 0, G_OPTION_ARG_CALLBACK, output_format_cb, "Output format", "text|html|djvu"},
        {"selthresh", 'T', 0, G_OPTION_ARG_NONE, &selthresh, "Don't use session files. Intended for selthresh script", NULL},
        {"trace", 0, 0, G_OPTION_ARG_NONE, &trace, "Enable trace messages", NULL},
        {"verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, "Verbose mode", NULL},
        {"workdir", 'w', 0, G_OPTION_ARG_FILENAME, &workdir, "Work directory", "dir"},
        {"resolution", 'y', 0, G_OPTION_ARG_INT, &DENSITY, "Resolution of input page", "dpi"},
        {"compress",'z', 0, G_OPTION_ARG_NONE, &zsession, "Prefer compressed session files", NULL},
        {},
};

gboolean continue_ocr_thunk(gpointer data);


#ifdef HAVE_SIGNAL
extern void handle_pipe(int);
extern void handle_alrm(int);
#endif

int main(int argc, char *argv[]) {

#ifdef HAVE_SIGNAL
        /* install handlers */
        signal(SIGPIPE, handle_pipe);
        signal(SIGALRM, handle_alrm);
#endif
        if (!g_thread_supported()) g_thread_init(NULL);
        imlib_lock = g_mutex_new();
                
        /*
           Process command-line parameters.
         */

        init_flags();
        {
                GOptionContext *context;
                GError *error = NULL;

                context = g_option_context_new("");
                g_option_context_add_main_entries(context, optDesc, NULL);
                g_option_group_set_parse_hooks(g_option_context_get_main_group(context),
                                               preinit_arg_hook,
                                               postinit_arg_hook);
                g_option_context_add_group(context, gtk_get_option_group(FALSE));

                if (!g_option_context_parse(context, &argc, &argv, &error)) {
                        g_print("option parsing failed: %s\n", error->message);
                        exit(1);
                }
                g_option_context_free(context);
        }


        /*
           Initialize main data structures.
         */
        init_ds();

        /*
           Initializes GUI.
         */
        init_welcome();
        xpreamble();
        // UNPATCHED: setview(WELCOME);

        /*
           A temporary solution for classifier 4. As we've decided to
           handle pre-built fonts just like manually-created fonts,
           the problem now concerns how to use both pre-built and
           manually created fonts simultaneously. This problem was
           not addressed yet. By now, classifier 4 should be used only
           for tests and exclusively and not tryong to train symbols
           manually.
         */
        if (classifier == CL_SHAPE)
                build_internal_patterns();

        /*
           start full OCR or preprocessing automatically on
           all pages when in batch mode.
         */
        if (batch_mode != 0) {
                if (npages <= 0)
                        fatal(DI, "no pages to process!");
                if (pp_only)
                        start_ocr(-1, OCR_PREPROC, 0);
                else if (searchb) {
                        ocr_other = 2;
                        start_ocr(-1, OCR_OTHER, 0);
                } else
                        start_ocr(-1, -1, 0);
        }


        /* just perform a dictionary operation */
        if (dict_op != 0) {

                dict_behaviour();
                exit(0);
        }

        /*

           Now we alternate two "threads", one for handle user-generated
           GUI events, and other to run the various OCR steps (these are
           not true "threads", but ordinary subroutines).

         */
        if (batch_mode == 0) {
                g_idle_add(continue_ocr_thunk, NULL);
                gtk_main();
        } else {
                while (ocring && !finish)
                        continue_ocr();
        }


        if ((batch_mode) && (report)) {
                //mk_page_list();
                //DFW = 6;
                //html2ge(text, 0);
                //write_report("report.txt");
                mk_pattern_list();
                //DFW = 6;
                html2ge(text, 0);
                write_report("classes.txt");
        }

        if (selthresh) {
                printf("bookfont size is %d, %d links\n", topp, links);
        }
        exit(0);
}
