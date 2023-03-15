#ifndef __GIVERTILE_H__
#define __GIVERTILE_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GIVER_TYPE_CONTEXT (g_river_get_type())

G_DECLARE_DERIVABLE_TYPE(GiverContext, g_river, GIVER, CONTEXT, GObject)
// G_DECLARE_FINAL_TYPE(GiverContext, g_river, GIVER, CONTEXT, GObject)

struct _GiverContextClass {
	void (*run) (GiverContext *self, GError **err);
	gpointer padding[12];
};

void g_river_run(GiverContext *self, GError **err);

GObject *g_river_new(void);

G_END_DECLS

#endif /* __GIVERTILE_H__ */
