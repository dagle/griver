#ifndef __GIVER_OUTPUT_H__
#define __GIVER_OUTPUT_H__

#include <glib-object.h>
#include <stdbool.h>
#include <stdint.h>
#include "river-layout-v3-client-protocol.h"
#include "river-status-unstable-v1-client-protocol.h"

G_BEGIN_DECLS

#define GRIVER_TYPE_OUTPUT (g_river_output_get_type())

G_DECLARE_DERIVABLE_TYPE(GriverOutput, g_river_output, GRIVER, OUTPUT, GObject)

struct _GriverOutputClass {
	GObjectClass parent_class;
	void (*push_view_dimensions) (GriverOutput *self, 
			uint32_t view_count, uint32_t width, uint32_t height,
			uint32_t tags, uint32_t serial);
	void (*commit_dimensions) (GriverOutput *self, const char *layout_name, uint32_t serial);
	gpointer padding[12];
};

void
g_river_push_view_dimensions (GriverOutput *ctx, 
			uint32_t view_count, uint32_t width, uint32_t height,
			uint32_t tags, uint32_t serial);

void
g_river_commit_dimensions (GriverOutput *ctx, const char *layout_name, uint32_t serial);

uint32_t g_river_output_get_uid(GriverOutput *self);

void g_river_output_configure (GriverOutput *self, struct river_layout_manager_v3 *layout_manager);

GObject *g_river_output_new(struct river_layout_manager_v3 *layout_manager,
		struct wl_output *wl_output, struct river_layout_v3 *layout, 
		uint32_t uid, const char *namespace, bool initalized);

G_END_DECLS

#endif /* __GIVER_OUTPUT_H__ */
