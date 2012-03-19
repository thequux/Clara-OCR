#include "common.h"

static GPtrArray *closures = NULL;

static void cldesc_destroy(gpointer ptr) {
	cldesc* p = (cldesc*)ptr;
	g_free(p->bm);
	g_free(p->sup);
	g_slice_free(cldesc, ptr);
}

cldesc* closure_get(int i) {
	g_assert (i >= 0 && i < closures->len);
	return (cldesc*)g_ptr_array_index(closures, i);
}

cldesc* closure_new() {
	cldesc *c = g_slice_new0(cldesc);
	c->id = closures->len;
	g_ptr_array_add(closures, c);
	g_assert((cldesc*)g_ptr_array_index(closures, c->id) == c);
	return c;
}
int closure_count() {
	return closures->len;
}
void clear_closures() {
	if (closures != NULL)
		g_ptr_array_free(closures, TRUE);
	closures = g_ptr_array_new_with_free_func(cldesc_destroy);
}
	
