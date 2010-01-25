#include <math.h>
#include "common.h"
#include "gui.h"
#include "docView.h"
#include "clara_marshalers.h"

#define CLARA_DOC_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj),CLARA_TYPE_DOC_VIEW, ClaraDocViewPrivate))

typedef struct _ClaraDocViewPrivate ClaraDocViewPrivate;

// Properties
enum {
        PROP_0,
        PROP_HADJUSTMENT,
        PROP_VADJUSTMENT,
        PROP_ZOOM,
        PROP_ZOOM_ADJUSTMENT,
};

// Signals
enum {
        SIG_SYMBOL_SELECTED,
        SIG_TRANSLIT_GIVEN,
        LAST_SIGNAL,
};

struct _ClaraDocViewPrivate {
        GtkAdjustment *zoom_adj;
        GtkAdjustment *hadjustment;
        GtkAdjustment *vadjustment;

        gdouble oldx, oldy, oldzoom;
        gboolean use_position;
        gboolean keep_position;

        gint curr_sym;
};

typedef struct DblRectangle {
        gdouble x, y, width, height;
} DblRectangle;

typedef unsigned int drawmode_t;        // Search for TAG:DMODE

static void clara_doc_view_init(ClaraDocView *dv);
static void clara_doc_view_class_init(ClaraDocViewClass *klass);
static void copy_bm_to_buffer(guchar *src, guchar *dst, int w, int h,
                              int stride);
static void draw_symbol(cairo_t *cr, drawmode_t mode, sdesc *sym);
static gboolean clara_doc_view_expose(GtkWidget *doc,
                                      GdkEventExpose *event);
static void clara_doc_view_size_request(GtkWidget *widget,
                                        GtkRequisition *req);
static void clara_doc_view_size_allocate(GtkWidget *widget,
                                         GtkAllocation *alloc);
static void clara_doc_view_set_property(GObject *object,
                                        guint property_id,
                                        const GValue *value,
                                        GParamSpec *pspec);
static void clara_doc_view_get_property(GObject *object,
                                        guint property_id, GValue *value,
                                        GParamSpec *pspec);
static void clara_doc_view_zoom_changed_cb(GtkAdjustment *adj,
                                           ClaraDocView *self);
static void clara_doc_view_adjust_position(ClaraDocView *self,
                                           GtkAnchorType anchor);
static void clara_doc_view_set_scroll_adjustments(ClaraDocView *self,
                                                  GtkAdjustment *hpos,
                                                  GtkAdjustment *vpos);
static void clara_doc_view_set_adjustment(ClaraDocView *self,
                                          GtkAdjustment *adj,
                                          GtkOrientation orientation,
                                          gboolean set_values);
static void clara_doc_view_viewport_changed(GtkAdjustment *adj,
                                            ClaraDocView *self);
static void clara_doc_view_calc_viewport(ClaraDocView *self,
                                         DblRectangle *vp, gdouble *zoom);
static gint clara_doc_view_button_press_cb(GtkWidget *widget,
                                           GdkEventButton *evt);
static gint clara_doc_view_key_press_cb(GtkWidget *widget,
                                        GdkEventKey *evt);
static void clara_doc_view_symbol_selected_cb(ClaraDocView *self,
                                              int symNo);
static void clara_doc_view_transliteration_given_cb(ClaraDocView *self,
                                                    int symNo,
                                                    const gchar *translit);


static guint clara_doc_view_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE(ClaraDocView, clara_doc_view, GTK_TYPE_DRAWING_AREA);

// TAG:DMODE

#define MASK(off,bits) ((~((~0)<<(bits))) << (off))

#define DV_BBOX (1<<0)
#define DV_SELECTED (1<<1)

#define DV_HILIGHT__OFF 2
#define DV_HILIGHT__BITS 2
#define DV_HILIGHT__MASK MASK(DV_HILIGHT__OFF,DV_HILIGHT__BITS)

#define DV_HILIGHT_NONE (0 << DV_HILIGHT__OFF)
#define DV_HILIGHT_CLOSURE (1 << DV_HILIGHT__OFF)
#define DV_HILIGHT_SYMBOL (2 << DV_HILIGHT__OFF)
#define DV_HILIGHT_WORD (3 << DV_HILIGHT__OFF)

#define DV_CLOSURE__OFF (DV_HILIGHT__OFF + DV_HILIGHT__BITS)
#define DV_CLOSURE__BITS 2
#define DV_CLOSURE__MASK MASK(DV_CLOSURE__OFF,DV_CLOSURE__BITS)
#define DV_CLOSURE_INVISIBLE (0 << DV_CLOSURE__OFF)
#define DV_CLOSURE_UNCERTAIN (1 << DV_CLOSURE__OFF)
#define DV_CLOSURE_KNOWN     (2 << DV_CLOSURE__OFF)




static void clara_doc_view_class_init(ClaraDocViewClass *klass) {
        GtkWidgetClass *widget_class;
        GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

        klass->symbol_selected = clara_doc_view_symbol_selected_cb;
        klass->transliteration_given =
            clara_doc_view_transliteration_given_cb;

        widget_class = GTK_WIDGET_CLASS(klass);
        widget_class->expose_event = clara_doc_view_expose;
        widget_class->size_request = clara_doc_view_size_request;
        widget_class->size_allocate = clara_doc_view_size_allocate;
        widget_class->button_press_event = clara_doc_view_button_press_cb;
        widget_class->key_press_event = clara_doc_view_key_press_cb;

        g_type_class_add_private(gobject_class,
                                 sizeof(ClaraDocViewPrivate));

        gobject_class->set_property = clara_doc_view_set_property;
        gobject_class->get_property = clara_doc_view_get_property;
        GParamSpec *pspec;

        pspec = g_param_spec_double("zoom",
                                    "Zoom level",
                                    "Zoom level. 1.0 is pixel-for-pixel. Shortcut for the current value of zoom-adjustment",
                                    0.0, 2.0, 1.0,
                                    G_PARAM_READWRITE |
                                    G_PARAM_STATIC_STRINGS);
        g_object_class_install_property(gobject_class, PROP_ZOOM, pspec);

        pspec = g_param_spec_object("zoom-adjustment",
                                    "An adjustment representing the zoom level",
                                    "Stores the current zoom level. 1.0 is pixel-for-pixel.",
                                    GTK_TYPE_ADJUSTMENT,
                                    G_PARAM_READABLE |
                                    G_PARAM_STATIC_STRINGS);
        g_object_class_install_property(gobject_class,
                                        PROP_ZOOM_ADJUSTMENT, pspec);

        pspec = g_param_spec_object("hadjustment",
                                    "The horizontal position",
                                    "The horizontal position",
                                    GTK_TYPE_ADJUSTMENT,
                                    G_PARAM_READWRITE |
                                    G_PARAM_STATIC_STRINGS);
        g_object_class_install_property(gobject_class, PROP_HADJUSTMENT,
                                        pspec);

        pspec = g_param_spec_object("vadjustment",
                                    "The vertical position",
                                    "The vertical position",
                                    GTK_TYPE_ADJUSTMENT,
                                    G_PARAM_READWRITE |
                                    G_PARAM_STATIC_STRINGS);
        g_object_class_install_property(gobject_class, PROP_VADJUSTMENT,
                                        pspec);

        klass->set_scroll_adjustments =
            clara_doc_view_set_scroll_adjustments;

        widget_class->set_scroll_adjustments_signal =
            g_signal_new(g_intern_static_string("set-scroll-adjustments"),
                         G_OBJECT_CLASS_TYPE(gobject_class),
                         G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                         G_STRUCT_OFFSET(ClaraDocViewClass,
                                         set_scroll_adjustments), NULL,
                         NULL, g_cclosure_user_marshal_VOID__OBJECT_OBJECT,
                         G_TYPE_NONE, 2, GTK_TYPE_ADJUSTMENT,
                         GTK_TYPE_ADJUSTMENT);

        clara_doc_view_signals[SIG_SYMBOL_SELECTED] =
            g_signal_new("symbol-selected",
                         G_OBJECT_CLASS_TYPE(gobject_class),
                         G_SIGNAL_RUN_FIRST,
                         G_STRUCT_OFFSET(ClaraDocViewClass,
                                         symbol_selected), NULL, NULL,
                         g_cclosure_user_marshal_VOID__INT, G_TYPE_NONE, 1,
                         G_TYPE_INT);

        clara_doc_view_signals[SIG_TRANSLIT_GIVEN] =
            g_signal_new("transliteration-given",
                         G_OBJECT_CLASS_TYPE(gobject_class),
                         G_SIGNAL_RUN_FIRST,
                         G_STRUCT_OFFSET(ClaraDocViewClass,
                                         transliteration_given), NULL,
                         NULL, g_cclosure_user_marshal_VOID__INT_STRING,
                         G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_STRING);
}

static void clara_doc_view_init(ClaraDocView *self) {
        ClaraDocViewPrivate *priv = CLARA_DOC_VIEW_GET_PRIVATE(self);
        priv->zoom_adj = g_object_ref_sink(gtk_adjustment_new(1.0, 0.1, 2.0,    //val,  min, max
                                                              0, 0, 0));        // step-inc, page-inc, page-size
        g_signal_connect(priv->zoom_adj, "value-changed",
                         G_CALLBACK(clara_doc_view_zoom_changed_cb), self);
        clara_doc_view_set_adjustment(self, NULL,
                                      GTK_ORIENTATION_HORIZONTAL, FALSE);
        clara_doc_view_set_adjustment(self, NULL, GTK_ORIENTATION_VERTICAL,
                                      FALSE);

        GTK_WIDGET_SET_FLAGS(self, GTK_CAN_FOCUS);
        gtk_widget_add_events(GTK_WIDGET(self),
                              GDK_BUTTON_PRESS_MASK | GDK_KEY_PRESS_MASK);
        priv->curr_sym = -1;
}

GtkWidget *clara_doc_view_new(void) {
        return g_object_new(CLARA_TYPE_DOC_VIEW, NULL);
}

// Event handlers (except drawing)


static void clara_doc_view_set_adjustment(ClaraDocView *self,
                                          GtkAdjustment *adj,
                                          GtkOrientation orientation,
                                          gboolean set_values) {
        ClaraDocViewPrivate *priv = CLARA_DOC_VIEW_GET_PRIVATE(self);

        GtkAdjustment **aptr = ((orientation == GTK_ORIENTATION_HORIZONTAL)
                                ? &priv->hadjustment : &priv->vadjustment);

        // disconnect the old, if necessary
        if (*aptr != NULL) {
                g_signal_handlers_disconnect_by_func(*aptr,
                                                     clara_doc_view_viewport_changed,
                                                     self);
                g_object_unref(*aptr);
                *aptr = NULL;
        }
        if (adj == NULL)
                adj = GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 0, 0, 0, 0));
        *aptr = adj;
        g_object_ref_sink(adj);
        g_signal_connect(adj, "value-changed",
                         G_CALLBACK(clara_doc_view_viewport_changed),
                         self);
        if (set_values) {
                priv->keep_position = FALSE;
                clara_doc_view_adjust_position(self,
                                               GTK_ANCHOR_NORTH_WEST);
        }
}

static void clara_doc_view_set_property(GObject *object,
                                        guint property_id,
                                        const GValue *value,
                                        GParamSpec *pspec) {
        ClaraDocView *self = CLARA_DOC_VIEW(object);
        ClaraDocViewPrivate *priv = CLARA_DOC_VIEW_GET_PRIVATE(self);

        switch (property_id) {
        case PROP_HADJUSTMENT:
                clara_doc_view_set_adjustment(self,
                                              g_value_get_object(value),
                                              GTK_ORIENTATION_HORIZONTAL,
                                              TRUE);
                break;
        case PROP_VADJUSTMENT:
                clara_doc_view_set_adjustment(self,
                                              g_value_get_object(value),
                                              GTK_ORIENTATION_VERTICAL,
                                              TRUE);
                break;

        case PROP_ZOOM_ADJUSTMENT:
                G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);  // can't set
        case PROP_ZOOM:
                gtk_adjustment_set_value(priv->zoom_adj,
                                         g_value_get_double(value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id,
                                                  pspec);
                break;
        }
}

static void clara_doc_view_get_property(GObject *object,
                                        guint property_id, GValue *value,
                                        GParamSpec *pspec) {
        ClaraDocView *self = CLARA_DOC_VIEW(object);
        ClaraDocViewPrivate *priv = CLARA_DOC_VIEW_GET_PRIVATE(self);

        switch (property_id) {
        case PROP_HADJUSTMENT:
                g_value_set_object(value, priv->hadjustment);
                break;
        case PROP_VADJUSTMENT:
                g_value_set_object(value, priv->vadjustment);
                break;
        case PROP_ZOOM_ADJUSTMENT:
                g_value_set_object(value, priv->zoom_adj);
                break;
        case PROP_ZOOM:
                g_value_set_float(value,
                                  gtk_adjustment_get_value
                                  (priv->zoom_adj));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id,
                                                  pspec);
                break;
        }
}

static void clara_doc_view_set_scroll_adjustments(ClaraDocView *self,
                                                  GtkAdjustment
                                                  *hadjustment,
                                                  GtkAdjustment
                                                  *vadjustment) {
        g_object_set(self, "hadjustment", hadjustment, "vadjustment",
                     vadjustment, NULL);
}

static void clara_doc_view_size_request(GtkWidget *widget,
                                        GtkRequisition *req) {
        ClaraDocView *self = CLARA_DOC_VIEW(widget);
        ClaraDocViewPrivate *priv = CLARA_DOC_VIEW_GET_PRIVATE(self);

        gdouble zoom = gtk_adjustment_get_value(priv->zoom_adj);
        req->width = XRES * zoom;
        req->height = YRES * zoom;
}

static void clara_doc_view_size_allocate(GtkWidget *widget,
                                         GtkAllocation *alloc) {
        ClaraDocView *self = CLARA_DOC_VIEW(widget);
        widget->allocation = *alloc;
        if (GTK_WIDGET_REALIZED(widget))
                gdk_window_move_resize(widget->window,
                                       alloc->x, alloc->y,
                                       alloc->width, alloc->height);

        clara_doc_view_adjust_position(self, GTK_ANCHOR_NORTH_WEST);

        if (GTK_WIDGET_MAPPED(widget))
                gdk_window_invalidate_rect(widget->window, NULL, FALSE);

}

void clara_doc_view_new_page(ClaraDocView *self) {
        ClaraDocViewPrivate *priv = CLARA_DOC_VIEW_GET_PRIVATE(self);
        priv->keep_position = FALSE;
        gtk_widget_queue_resize(GTK_WIDGET(self));
}

static void clara_doc_view_zoom_changed_cb(GtkAdjustment *adj,
                                           ClaraDocView *self) {
        ClaraDocViewPrivate *priv = CLARA_DOC_VIEW_GET_PRIVATE(self);
        priv->keep_position = TRUE;
        priv->use_position = FALSE;
        clara_doc_view_adjust_position(self, GTK_ANCHOR_CENTER);
        gtk_widget_queue_draw(GTK_WIDGET(self));
}

static void clara_doc_view_adjust_position(ClaraDocView *self,
                                           GtkAnchorType anchor) {
        ClaraDocViewPrivate *priv = CLARA_DOC_VIEW_GET_PRIVATE(self);
        GtkAllocation alloc;
        gtk_widget_get_allocation(GTK_WIDGET(self), &alloc);

        gdouble zoom = gtk_adjustment_get_value(priv->zoom_adj);

        gdouble page_width = MIN(XRES, alloc.width / zoom),
            page_height = MIN(YRES, alloc.height / zoom);
        gboolean conf_triggered = FALSE;

        int xanc, yanc;         // 0 for left/top, 1 for center, 2 for right/bottom

        switch (anchor) {
        case GTK_ANCHOR_NORTH_WEST:
                xanc = 0;
                yanc = 0;
                break;
        case GTK_ANCHOR_NORTH:
                xanc = 1;
                yanc = 0;
                break;
        case GTK_ANCHOR_NORTH_EAST:
                xanc = 2;
                yanc = 0;
                break;
        case GTK_ANCHOR_WEST:
                xanc = 0;
                yanc = 1;
                break;
        case GTK_ANCHOR_CENTER:
                xanc = 1;
                yanc = 1;
                break;
        case GTK_ANCHOR_EAST:
                xanc = 2;
                yanc = 1;
                break;
        case GTK_ANCHOR_SOUTH_WEST:
                xanc = 0;
                yanc = 2;
                break;
        case GTK_ANCHOR_SOUTH:
                xanc = 1;
                yanc = 2;
                break;
        case GTK_ANCHOR_SOUTH_EAST:
                xanc = 2;
                yanc = 2;
                break;
        default:
                g_warning("Invalid anchor %d\n", anchor);
        }

        if (priv->hadjustment != NULL) {

                gdouble xpos = gtk_adjustment_get_value(priv->hadjustment);
                gdouble hsize =
                    gtk_adjustment_get_page_size(priv->hadjustment);
                gtk_adjustment_configure(priv->hadjustment,
                                         (priv->keep_position ?
                                          CLAMP(xpos +
                                                xanc * (page_width -
                                                        hsize) / 2.0, 0,
                                                XRES - page_width)
                                          : 0), 0, XRES, 25,
                                         page_width / 3.0, page_width);
                conf_triggered = TRUE;
        }
        if (priv->vadjustment != NULL) {
                gdouble ypos = gtk_adjustment_get_value(priv->vadjustment);
                gdouble vsize =
                    gtk_adjustment_get_page_size(priv->vadjustment);

                gtk_adjustment_configure(priv->vadjustment,
                                         (priv->keep_position
                                          ? CLAMP(ypos +
                                                  yanc * (page_height -
                                                          vsize) / 2.0, 0,
                                                  YRES - page_height)
                                          : 0), 0, YRES, 25,
                                         page_height / 3.0, page_height);
                conf_triggered = TRUE;
        }

        if (!conf_triggered)
                gtk_widget_queue_draw(GTK_WIDGET(self));
}



// copy src (in bitmap format) to dst (as a grayscale).
// dst will have the given stride.
static void copy_bm_to_buffer(guchar *src, guchar *dst,
                              int w, int h, int stride) {
        int i, j;
        int src_stride = ((w + 8) >> 3);
        for (j = 0; j < h; j++) {
                for (i = 0; i < w; i++) {
                        // high bit is leftmost
                        dst[j * stride + i] =
                            (src[j * src_stride + (i >> 3)] &
                             (128 >> (i % 8))) ? 0xff : 0;
                }
        }
}

static void cairo_ellipse(cairo_t *cr, gdouble x, gdouble y,
                          gdouble width, gdouble height) {
        cairo_save(cr);
        cairo_translate(cr, x + width / 2., y + height / 2.);
        cairo_scale(cr, width / 2., height / 2.);
        cairo_arc(cr, 0., 0., 1.25, 0., 2 * M_PI);
        cairo_restore(cr);
}

static void draw_symbol(cairo_t *cr, drawmode_t mode, sdesc *sym) {
        if (mode & DV_BBOX) {
                cairo_rectangle(cr,
                                sym->l, sym->t,
                                sym->r - sym->l, sym->b - sym->t);

                cairo_set_source_rgb(cr, 0, 0, 0);
                cairo_stroke(cr);
        }
        int cn;
        if ((mode & DV_CLOSURE__MASK)) {
                switch (mode & DV_CLOSURE__MASK) {
                case DV_CLOSURE_UNCERTAIN:
                        cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
                        break;
                case DV_CLOSURE_KNOWN:
                        cairo_set_source_rgb(cr, 0, 0, 0);
                        break;
                default:
                        g_print("Unknown closure mask %x\n",
                                mode & DV_CLOSURE__MASK);
                        cairo_set_source_rgb(cr, 1, 0, 0);
                }
                for (cn = 0; cn < sym->ncl; cn++) {
                        cldesc *closure = cl + sym->cl[cn];
                        int w = closure->r - closure->l,
                            h = closure->b - closure->t,
                            x = closure->l, y = closure->t;
                        int stride =
                            cairo_format_stride_for_width(CAIRO_FORMAT_A8,
                                                          w);
                        guchar *buffer = g_malloc(h * stride);
                        copy_bm_to_buffer(closure->bm, buffer, w, h,
                                          stride);
                        cairo_surface_t *surface =
                            cairo_image_surface_create_for_data(buffer,
                                                                CAIRO_FORMAT_A8,
                                                                w, h,
                                                                stride);
                        cairo_mask_surface(cr, surface, x, y);
                        cairo_surface_destroy(surface);
                        g_free(buffer);

                }
        }

        if (mode & DV_SELECTED) {
                cairo_ellipse(cr, sym->l, sym->t,
                              sym->r - sym->l, sym->b - sym->t);
                cairo_set_source_rgb(cr, 0, 0, 1);
                cairo_stroke(cr);
        }


        for (cn = 0; cn < sym->ncl; cn++) {
                char buf[50];
                cldesc *closure = cl + sym->cl[cn];
                int w = closure->r - closure->l,
                    h = closure->b - closure->t,
                    x = closure->l, y = closure->t;
                cairo_set_source_rgba(cr, 0, 1, 0, 0.5);
                cairo_rectangle(cr, x, y, w, h);
                cairo_stroke(cr);
                cairo_set_source_rgb(cr, 1, 0, 0);
                cairo_move_to(cr, x, y);
                snprintf(buf, 50, "%d", sym->cl[cn]);
                cairo_show_text(cr, buf);
        }

}

static void clara_doc_view_viewport_changed(GtkAdjustment *adj,
                                            ClaraDocView *self) {
        ClaraDocViewPrivate *priv = CLARA_DOC_VIEW_GET_PRIVATE(self);
        GtkWidget *widget = GTK_WIDGET(self);
        if (GTK_WIDGET_REALIZED(self)) {
                DblRectangle rect;
                gdouble zoom;
                clara_doc_view_calc_viewport(self, &rect, &zoom);
#define SCR(x) ((int)((x) * zoom))
                // floating point equality is actually CORRECT here. 
                // If it changed, even by F_EPSILON, we might need a redraw. Determining for sure involves actually redrawing, so...
                if (priv->use_position && priv->oldzoom == zoom) {
                        gint dx = SCR(priv->oldx) - SCR(rect.x),
                            dy = SCR(priv->oldy) - SCR(rect.y);
#undef SCR
                        if (dx != 0 || dy != 0)
                                gdk_window_scroll(widget->window, dx, dy);
                        gdk_window_process_updates(widget->window, TRUE);

                } else {
                        gtk_widget_queue_draw(GTK_WIDGET(self));
                }

                priv->keep_position = TRUE;
                priv->oldx = rect.x;
                priv->oldy = rect.y;
        }

}

static gboolean clara_doc_view_expose(GtkWidget *doc,
                                      GdkEventExpose *event) {
        ClaraDocView *self = CLARA_DOC_VIEW(doc);
        ClaraDocViewPrivate *priv = CLARA_DOC_VIEW_GET_PRIVATE(self);
        DblRectangle vp;
        gdouble zoom;
        cairo_t *cr;

        priv->use_position = TRUE;

        // figure out what we want to draw...
        clara_doc_view_calc_viewport(self, &vp, &zoom);
        priv->oldzoom = zoom;
        cr = gdk_cairo_create(doc->window);
        cairo_rectangle(cr,
                        event->area.x, event->area.y,
                        event->area.width, event->area.height);
        cairo_clip(cr);

        cairo_scale(cr, zoom, zoom);
        cairo_translate(cr, -vp.x, -vp.y);

        cairo_rectangle(cr, 0, 0, XRES, YRES);
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_fill_preserve(cr);
        cairo_set_source_rgb(cr, 0, 0, 0);
        cairo_stroke(cr);

        //int id, p;
        // get the active class
        //if ((curr_mc >= 0) && ((id = mc[curr_mc].bm) >= 0)) {
        //        p = id2idx(id);
        //}

        int i;
        for (i = 0; i < topps; ++i) {
                // for each symbol...
                int c;
                c = ps[i];
                sdesc *sym = mc + c;
                if ((sym->l <= vp.x + vp.width) &&
                    (sym->r >= vp.x) &&
                    (sym->t <= vp.y + vp.height) && (sym->b >= vp.y)) {
                        guint options = DV_BBOX;
                        if (curr_mc == c)
                                options |= DV_SELECTED;

                        if (uncertain(mc[c].tc))
                                options |= DV_CLOSURE_UNCERTAIN;
                        else
                                options |= DV_CLOSURE_KNOWN;
                        draw_symbol(cr, options, mc + c);
                }
        }
        cairo_destroy(cr);
        return FALSE;
}

static void clara_doc_view_calc_viewport(ClaraDocView *self,
                                         DblRectangle *vp, gdouble *zoom) {
        ClaraDocViewPrivate *priv = CLARA_DOC_VIEW_GET_PRIVATE(self);
        gdouble zoom_st;
        if (zoom == NULL)
                zoom = &zoom_st;
        *zoom = gtk_adjustment_get_value(priv->zoom_adj);

        GtkAllocation alloc_screen;
        gtk_widget_get_allocation(GTK_WIDGET(self), &alloc_screen);

        DblRectangle alloc_doc;
        alloc_doc.width = alloc_screen.width / *zoom;
        alloc_doc.height = alloc_screen.height / *zoom;

        if (XRES < alloc_doc.width) {
                vp->x = (XRES - alloc_doc.width) / 2;
                vp->width = alloc_doc.width;
        } else {
                vp->x = gtk_adjustment_get_value(priv->hadjustment);
                vp->width =
                    gtk_adjustment_get_page_size(priv->hadjustment);
        }

        if (YRES < alloc_doc.height) {
                vp->y = (YRES - alloc_doc.height) / 2;
                vp->height = alloc_doc.height;
        } else {
                vp->y = gtk_adjustment_get_value(priv->vadjustment);
                vp->height =
                    gtk_adjustment_get_page_size(priv->vadjustment);
        }


}

static void clara_doc_view_symbol_selected_cb(ClaraDocView *self,
                                              int symNo) {
        ClaraDocViewPrivate *priv = CLARA_DOC_VIEW_GET_PRIVATE(self);
        priv->curr_sym = symNo;
        //if ((curr_mc = symNo) >= 1) {
        //      dw[PAGE_SYMBOL].rg = 1;
        //}
        gtk_widget_queue_draw(GTK_WIDGET(self));
}

static void clara_doc_view_transliteration_given_cb(ClaraDocView *self,
                                                    int symNo,
                                                    const gchar *translit) 
{
}

static gint clara_doc_view_button_press_cb(GtkWidget *widget,
                                           GdkEventButton *evt) {
        ClaraDocView *self = CLARA_DOC_VIEW(widget);
        ClaraDocViewPrivate *priv = CLARA_DOC_VIEW_GET_PRIVATE(self);
        DblRectangle vp;
        gdouble zoom;
        if (evt->type == GDK_BUTTON_PRESS) {
                if (!GTK_WIDGET_HAS_FOCUS(widget)) {
                        gtk_widget_grab_focus(widget);
                }
                // translate to document...
                gdouble xr, yr;
                clara_doc_view_calc_viewport(self, &vp, &zoom);
                xr = vp.x + evt->x / zoom;
                yr = vp.y + evt->y / zoom;
                gint k = symbol_at(xr, yr);
                g_signal_emit(self,
                              clara_doc_view_signals[SIG_SYMBOL_SELECTED],
                              0, k);
        }
        return TRUE;
}

static gint clara_doc_view_key_press_cb(GtkWidget *widget,
                                        GdkEventKey *evt) {
        ClaraDocView *self = CLARA_DOC_VIEW(widget);
        ClaraDocViewPrivate *priv = CLARA_DOC_VIEW_GET_PRIVATE(self);

        gchar buf[10] = { 0 };
        guint32 ch = gdk_keyval_to_unicode(evt->keyval);
        if (ch != 0) {
                g_unichar_to_utf8(ch, buf);
                g_signal_emit(self,
                              clara_doc_view_signals[SIG_TRANSLIT_GIVEN],
                              0, priv->curr_sym, buf);
                return FALSE;
        }
        return TRUE;
}
