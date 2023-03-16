#ifndef __GRIVER_CONTEXT_H__
#define __GRIVER_CONTEXT_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GRIVER_TYPE_CONTEXT (g_river_context_get_type())

G_DECLARE_DERIVABLE_TYPE(GriverContext, g_river_context, GRIVER, CONTEXT, GObject)

struct _GriverContextClass {
	GObjectClass parent_class;

	void (*run) (GriverContext *self, GError **err);
	// void (*push_view_dimensions) (GriverContext *self, 
	// 		uint32_t view_count, uint32_t width, uint32_t height,
	// 		uint32_t tags, uint32_t serial);
	// void (*commit_dimensions) (GriverContext *self, const char *layout_name, uint32_t serial);
	// gpointer padding[12];
};

void g_river_context_run(GriverContext *self, GError **err);

GObject *g_river_context_new(const char *str);

G_END_DECLS

#endif /* __GRIVER_CONTEXT_H__ */
