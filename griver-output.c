#include "griver-output.h"
#include "glibconfig.h"

#include <stdbool.h>
#include <stdio.h>
#include <wayland-client-core.h>
#include <wayland-client.h>
#include <wayland-client-protocol.h>

#include "river-layout-v3-client-protocol.h"
#include "river-status-unstable-v1-client-protocol.h"

enum {
  GRIVER_LAYOUT_DEMAND,
  GRIVER_USER_COMMAND,
  GRIVER_OUTPUT_LAST_SIGNAL
};

static guint griver_output_signals[GRIVER_OUTPUT_LAST_SIGNAL] = { 0 };

typedef struct {
	// should we ref-count this object?
	int cmd_tags;
    bool initialized;

	uint32_t uid;

	struct wl_output       *output;
	struct river_layout_v3 *layout;
} GriverOutputPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GriverOutput, g_river_output, G_TYPE_OBJECT);

static void commit_dimensions(GriverOutput *self, const char *layout_name, uint32_t serial);

static void push_view_dimensions (GriverOutput *self, 
		uint32_t view_count, uint32_t width, uint32_t height,
		uint32_t tags, uint32_t serial);

static void output_finalize (GObject *object);


static void output_finalize (GObject *object) {
	GriverOutput *output = GRIVER_OUTPUT(object);
	GriverOutputPrivate *priv = g_river_output_get_instance_private(output);

	if ( priv->layout != NULL )
		river_layout_v3_destroy(priv->layout);
	wl_output_destroy(priv->output);
}

static void g_river_output_init(GriverOutput *output) {
}

static void g_river_output_class_init(GriverOutputClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	gobject_class->finalize = output_finalize;
	klass->push_view_dimensions = push_view_dimensions;
	klass->commit_dimensions = commit_dimensions;

	griver_output_signals[GRIVER_LAYOUT_DEMAND] = g_signal_new ("layout-demand",
			G_TYPE_FROM_CLASS (klass),
			G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
			0,
			NULL,
			NULL,
			NULL,
			G_TYPE_NONE,
			5,
			G_TYPE_UINT,
			G_TYPE_UINT,
			G_TYPE_UINT,
			G_TYPE_UINT,
			G_TYPE_UINT
			);

	griver_output_signals[GRIVER_USER_COMMAND] = g_signal_new ("user-command",
			G_TYPE_FROM_CLASS (klass),
			G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
			0,
			NULL,
			NULL,
			NULL,
			G_TYPE_NONE,
			2,
			G_TYPE_STRING,
			G_TYPE_UINT);
}

static void push_view_dimensions (GriverOutput *self, 
			uint32_t view_count, uint32_t width, uint32_t height,
			uint32_t tags, uint32_t serial)
{
	GriverOutputPrivate *priv = g_river_output_get_instance_private(self);

	river_layout_v3_push_view_dimensions(priv->layout, view_count, width, height,
			tags, serial);
}

static void commit_dimensions (GriverOutput *self, const char *layout_name, uint32_t serial)
{
	GriverOutputPrivate *priv = g_river_output_get_instance_private(self);
	river_layout_v3_commit(priv->layout, layout_name, serial);
}

void
g_river_output_push_view_dimensions (GriverOutput *ctx, 
			uint32_t view_count, uint32_t width, uint32_t height,
			uint32_t tags, uint32_t serial)
{
	return GRIVER_OUTPUT_GET_CLASS(ctx)->push_view_dimensions(
			ctx, view_count, width, height, tags, serial);
}

void
g_river_output_commit_dimensions (GriverOutput *ctx, const char *layout_name, uint32_t serial)
{
	return GRIVER_OUTPUT_GET_CLASS(ctx)->commit_dimensions(
			ctx, layout_name, serial);
}

static void layout_handle_namespace_in_use (void *data, struct river_layout_v3 *river_layout_v3)
{
	// This should just try to gracefully crash?

	// struct Output *output = (struct Output *)data;
	// GriverContextPrivate *priv = g_river_context_get_instance_private (output->ctx);

	// GError *err;
	// g_set_error(err, G_RIVER_ERROR, G_RIVER_ERROR_NAMESPACE_INUSE,
	// 		"Namespace already in use");
	// priv->error = err;
	// fputs("Namespace already in use.\n", stderr);
	// priv->loop = false;

	// do a teardown
}

// these should be a part of output
static void layout_handle_layout_demand (void *data, struct river_layout_v3 *river_layout_v3,
		uint32_t view_count, uint32_t width, uint32_t height, uint32_t tags, uint32_t serial)
{
	GriverOutput *output = GRIVER_OUTPUT(data);

	g_signal_emit (output, griver_output_signals[GRIVER_LAYOUT_DEMAND], 0,
			view_count, width, height, tags, serial);
}

static void layout_handle_user_command (void *data, 
		struct river_layout_v3 *river_layout_manager_v3, const char *command)
{
	GriverOutput *output = GRIVER_OUTPUT(data);
	GriverOutputPrivate *priv = g_river_output_get_instance_private(output);

	g_signal_emit (output, griver_output_signals[GRIVER_USER_COMMAND], 0, command, priv->cmd_tags);
}

void layout_handle_command_tags(void *data,
		struct river_layout_v3 *river_layout_v3,
		uint32_t tags)
{
	GriverOutput *output = GRIVER_OUTPUT(data);
	GriverOutputPrivate *priv = g_river_output_get_instance_private(output);
	priv->cmd_tags = tags;
}

static const struct river_layout_v3_listener layout_listener = {
	.namespace_in_use = layout_handle_namespace_in_use,
	.layout_demand    = layout_handle_layout_demand,
	.user_command     = layout_handle_user_command,
	.user_command_tags = layout_handle_command_tags,
};

uint32_t g_river_output_get_uid(GriverOutput *self)
{
	GriverOutputPrivate *priv = g_river_output_get_instance_private(self);
	
	return priv->uid;
}

void g_river_output_configure (GriverOutput *self, struct river_layout_manager_v3 *layout_manager)
{
	GriverOutputPrivate *priv = g_river_output_get_instance_private(self);
	if (!priv->initialized) {
		priv->initialized = true;

		priv->layout = river_layout_manager_v3_get_layout(layout_manager,
				priv->output, "tile");
		river_layout_v3_add_listener(priv->layout, &layout_listener, priv->output);
	}
}

/**
 * g_river_output_new:
 *
 * Creates a new river context object.
 *
 * Returns: (transfer full): a new river context object.
 **/
GObject *g_river_output_new(struct river_layout_manager_v3 *layout_manager,
		struct wl_output *wl_output, struct river_layout_v3 *layout, 
		uint32_t uid, const char *namespace, bool initalized)
{
	GriverOutput *output = g_object_new(GRIVER_TYPE_OUTPUT, NULL);
	GriverOutputPrivate *priv = g_river_output_get_instance_private(output);

	priv->output = wl_output;
	priv->layout = NULL;
	priv->uid    = uid;

	if (initalized) {
		g_river_output_configure(output, layout_manager);
	}
	return (GObject *)output;
}
