#ifndef __GIVER_OUTPUT_H__
#define __GIVER_OUTPUT_H__

#include <glib-object.h>
#include <stdbool.h>
#include <stdint.h>
#include "river-layout-v3-client-protocol.h"
#include "river-status-unstable-v1-client-protocol.h"

G_BEGIN_DECLS

#define GRIVER_TYPE_OUTPUT (g_river_output_get_type())

G_DECLARE_DERIVABLE_TYPE(GriverOutput, g_river_output, GRIVER, OUTPUT, GObject);

/**
 * GriverRotation:
 * @GRIVER_LEFT: Put the master to the left
 * @GRIVER_RIGHT: Put the master to the right
 * @GRIVER_TOP: Put the master to at the top
 * @GRIVER_BOT: Put the master to at the bot
 *
 * A type to represents the orientation of tiling
 **/
typedef enum {
	GRIVER_LEFT,
	GRIVER_RIGHT,
	GRIVER_TOP,
	GRIVER_BOT,
} GriverRotation;

struct _GriverOutputClass {
	GObjectClass parent_class;
	void (*push_view_dimensions) (GriverOutput *out, 
			uint32_t x, uint32_t y, uint32_t width, uint32_t height,
			uint32_t serial);
	void (*commit_dimensions) (GriverOutput *out, const char *layout_name, uint32_t serial);
	gpointer padding[12];
};

void
g_river_output_push_view_dimensions (GriverOutput *out, 
			uint32_t x, uint32_t y, uint32_t width, uint32_t height,
			uint32_t serial);

void
g_river_output_commit_dimensions (GriverOutput *out, const char *layout_name, uint32_t serial);

void g_river_output_tall_layout(GriverOutput *out, uint32_t view_count, uint32_t width,
		uint32_t height, uint32_t main_count, uint32_t view_padding, uint32_t outer_padding, 
		double ratio, GriverRotation rotation, uint32_t serial);

uint32_t g_river_output_get_uid(GriverOutput *out);

void g_river_output_configure (GriverOutput *out, struct river_layout_manager_v3 *layout_manager,
		const char *namespace);

GObject *g_river_output_new(struct river_layout_manager_v3 *layout_manager,
		struct wl_output *wl_output, uint32_t uid,
		const char *namespace, bool initalized);

G_END_DECLS

#endif /* __GIVER_OUTPUT_H__ */
