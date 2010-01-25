--k-and-r-style

-nbbo           /* break after boolean ops */
-nbc            /* no blanks after commas */
-nfca -nfc1     /* don't format comments */
-i8             /* indent 8 spaces */
-nut            /* no tabs */
-brf
-npsl

// typedefs
-T FILE

// glib
-T gchar
-T gdouble
-T guchar
-T GValue
-T GParamSpec

// GObject
-T GObject
-T GClosure

// gtk
-T GtkAllocation
-T GtkWidget
-T GdkEventKey
-T GdkEventExpose
-T GdkEventButton
-T GtkAdjustment
-T GtkRequisition
-T GtkMenuItem
-T GtkTreeViewColumn
-T GtkCellRenderer
-T GtkTreeModel
-T GtkTreeIter
-T GtkTreeSelection
-T cairo_t

// clara
-T DblRectangle
-T ClaraDocViewClass
-T ClaraDocView
-T ClaraDocViewPrivate
-T drawmode_t
-T sdesc
-T cldesc
-T ddesc
-T cmdesc
-T ptdesc
-T bdesc
-T edesc
-T adesc
-T alpharec
-T flag_t
-T vdesc
-T trdesc
-T pdesc
-T lndesc
-T wdesc
-T point
-T lkdesc
-T dwdesc
