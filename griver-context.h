#ifndef __GRIVER_CONTEXT_H__
#define __GRIVER_CONTEXT_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GRIVER_TYPE_CONTEXT (g_river_context_get_type())

G_DECLARE_DERIVABLE_TYPE(GriverContext, g_river_context, GRIVER, CONTEXT, GObject)

struct _GriverContextClass {
	GObjectClass parent_class;

	gboolean (*run) (GriverContext *ctx, GError **err);
};

gboolean g_river_context_run(GriverContext *ctx, GError **err);

GObject *g_river_context_new(const char *str);

G_END_DECLS

#endif /* __GRIVER_CONTEXT_H__ */
