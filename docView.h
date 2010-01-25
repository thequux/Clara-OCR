
#define CLARA_TYPE_DOC_VIEW             (clara_doc_view_get_type ())
#define CLARA_DOC_VIEW(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLARA_TYPE_DOC_VIEW, ClaraDocView))
#define CLARA_DOC_VIEW_CLASS(obj)       (G_TYPE_CHECK_CLASS_CAST ((obj), CLARA_DOC_VIEW,  ClaraDocViewClass))
#define CLARA_IS_DOC_VIEW(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLARA_TYPE_DOC_VIEW))
#define CLARA_IS_DOC_VIEW_CLASS(obj)    (G_TYPE_CHECK_CLASS_TYPE ((obj), CLARA_TYPE_DOC_VIEW))
#define CLARA_DOC_VIEW_GET_CLASS        (G_TYPE_INSTANCE_GET_CLASS ((obj), CLARA_TYPE_DOC_VIEW, ClaraDocViewClass))



typedef struct _ClaraDocView ClaraDocView;
typedef struct _ClaraDocViewClass ClaraDocViewClass;

struct _ClaraDocView {
        GtkDrawingArea parent;

        /* private */
};

struct _ClaraDocViewClass {
        GtkDrawingAreaClass parent_class;

        /*< protected > */

        void (*symbol_selected) (ClaraDocView *self, int symNo);
        void (*transliteration_given) (ClaraDocView *self, int symNo,
                                       const gchar *translit);

        /*< private > */

        void (*set_scroll_adjustments) (ClaraDocView *, GtkAdjustment *,
                                        GtkAdjustment *);
};

GtkWidget *clara_doc_view_new(void);
void clara_doc_view_new_page(ClaraDocView *dv);
GType clara_doc_view_get_type(void);
