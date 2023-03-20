#include "griver-output.h"
#include "glibconfig.h"

#include <stdbool.h>
#include <stdint.h>
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
	int cmd_tags;
    bool initialized;

	uint32_t uid;

	struct wl_output       *output;
	struct river_layout_v3 *layout;
} GriverOutputPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GriverOutput, g_river_output, G_TYPE_OBJECT);

static void commit_dimensions(GriverOutput *out, const char *layout_name, uint32_t serial);

static void push_view_dimensions (GriverOutput *out, 
			uint32_t x, uint32_t y, uint32_t width, uint32_t height,
			uint32_t serial);

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

	/**
	 * GriverContext::layout-demand:
	 * @out: The [class@Griver.GriverOutput] instance.
	 * @view_count: Number of views.
	 * @width: The width of the output.
	 * @height: The height of the output.
	 * @tags: Which tags are visible.
	 * @serial: A serial used to push and commit dimensions
	 *
	 * River wants us to arrange views.
	 *
	 **/
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

	/**
	 * GriverContext::user-command:
	 * @out: The [class@Griver.GriverOutput] instance.
	 * @command: The command being requested.
	 * @tags: Tags are effected by the command.
	 *
	 * A user ran a command, changing some setting.
	 *
	 **/
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

static void push_view_dimensions (GriverOutput *out, 
			uint32_t x, uint32_t y, uint32_t width, uint32_t height,
			uint32_t serial)
{
	GriverOutputPrivate *priv = g_river_output_get_instance_private(out);

	river_layout_v3_push_view_dimensions(priv->layout, x, y,  width, height,
			serial);
}

static void commit_dimensions (GriverOutput *out, const char *layout_name, uint32_t serial)
{
	GriverOutputPrivate *priv = g_river_output_get_instance_private(out);
	river_layout_v3_commit(priv->layout, layout_name, serial);
}

/**
 * g_river_output_push_view_dimensions:
 * @out: A #GriverOut to push dimensions.
 * @x: x cordinate of view
 * @y: y cordinate of the view
 * @width: The width of the view.
 * @height: The height of the view.
 * @serial: A serial used to identify the roundtrip.
 *
 * Push a dimension of a view to the output
 * The x and y coordinates are relative to the usable area of the output,
 * with (0,0) as the top left corner.
 *
 **/
void
g_river_output_push_view_dimensions (GriverOutput *out, 
			uint32_t x, uint32_t y, uint32_t width, uint32_t height,
			uint32_t serial)
{
	return GRIVER_OUTPUT_GET_CLASS(out)->push_view_dimensions(
			out, x, y, width, height, serial);
}

/**
 * g_river_output_commit_dimensions:
 * @out: A #GriverOut to push dimensions.
 * @layout_name: What we call the layout, for example "[]="
 * @serial: A serial used to identify the roundtrip.
 *
 * We are done with dimensions and we want river to start rendering
 *
 **/
void
g_river_output_commit_dimensions (GriverOutput *out, const char *layout_name, uint32_t serial)
{
	return GRIVER_OUTPUT_GET_CLASS(out)->commit_dimensions(
			out, layout_name, serial);
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

uint32_t g_river_output_get_uid(GriverOutput *out)
{
	GriverOutputPrivate *priv = g_river_output_get_instance_private(out);
	
	return priv->uid;
}

void g_river_output_configure (GriverOutput *out, struct river_layout_manager_v3 *layout_manager,
		const char *namespace)
{
	GriverOutputPrivate *priv = g_river_output_get_instance_private(out);
	if (!priv->initialized) {
		priv->initialized = true;

		priv->layout = river_layout_manager_v3_get_layout(layout_manager,
				priv->output, namespace);
		river_layout_v3_add_listener(priv->layout, &layout_listener, priv->output);
	}
}

/**
 * g_river_output_tall_layout:
 * @out: A #GriverOut to tile.
 * @view_count: number of views
 * @width: width of the usable area
 * @height: height of the usable area
 * @main_count: number of views in the main
 * @view_padding: the padding between views.
 * @outer_padding: the outer padding
 * @ratio: ratio between master and secondary
 * @rotation: Where the master should be.
 * @serial: A serial used to push and commit dimensions
 * 
 * Tiles the output in the classic dwm style but with different rations.
 * Doesn't call commit.
 *
 **/
void g_river_output_tall_layout(GriverOutput *out, uint32_t view_count, uint32_t width,
		uint32_t height, uint32_t main_count, uint32_t view_padding, uint32_t outer_padding, 
		double ratio, GriverRotation rotation, uint32_t serial) {

	if (view_count <= 0) {
		return;
	}

	uint32_t lmain_count = MIN(main_count, view_count);
	uint32_t secondary_count = view_count - lmain_count;

	uint32_t usable_width;
	switch (rotation) {
		case LEFT:
		case RIGHT:
			usable_width = width - 2 * outer_padding;
			break;
		case TOP:
		case BOT:
			usable_width = height - 2 * outer_padding;
			break;
	}

	uint32_t usable_height;
	switch (rotation) {
		case LEFT:
		case RIGHT:
			usable_height = height - 2 * outer_padding;
			break;
		case TOP:
		case BOT:
			usable_height = width - 2 * outer_padding;
			break;
	}

	uint32_t main_width, main_height, main_height_rem;

	uint32_t secondary_width, secondary_height, secondary_height_rem;
	if (secondary_count > 0 ) {
		main_width = (uint32_t) (ratio * usable_width);
		main_height = usable_height / main_count;
		main_height_rem = usable_height % main_count;

		secondary_width = (uint32_t) (ratio * usable_width);
		secondary_height = usable_height / secondary_count;
		secondary_height_rem = usable_height % secondary_count;
	} else {
		main_width = usable_width;
		main_height = usable_height / main_count;
		main_height_rem = usable_height % main_count;
	}

	for (int i = 0; i < view_count; i++) {
		uint32_t x, y, lwidth, lheight;

		if (i < main_count) {
			x = 0;
			y = i * main_height; 
			if (i > 0) { 
				y += main_height_rem;
			}
			lwidth = main_width;
			lheight = main_height; 
			if (i == 0) {
				lheight += main_height_rem;
			}
		} else {
	            x = main_width;
	            y = (i - main_count) * secondary_height; 
				if (i > main_count) {
					y += secondary_height_rem;
				}
	            lwidth = secondary_width;
	            lheight = secondary_height;
				if (i == main_count) {
					lheight += secondary_height_rem;
				}
	        }

	        x += view_padding;
	        y += view_padding;
	        lwidth -= 2 * view_padding;
	        lheight -= 2 * view_padding;

			switch (rotation) {
				case LEFT:
					g_river_output_push_view_dimensions(out,
							x + outer_padding,
							y + outer_padding,
							lwidth,
							lheight,
							serial
							);
					break;
				case RIGHT:
					g_river_output_push_view_dimensions(out,
							width - lwidth - x + outer_padding,
							y + outer_padding,
							lwidth,
							lheight,
							serial
							);
					break;
				case TOP:
					g_river_output_push_view_dimensions(out,
							y + outer_padding,
							x + outer_padding,
							lheight,
							lwidth,
							serial
							);
					break;
				case BOT:
					g_river_output_push_view_dimensions(out,
							y + outer_padding,
							width - lwidth - x + outer_padding,
							lheight,
							lwidth,
							serial
							);
					break;
			}
	}
}

/**
 * g_river_output_new: (skip)
 * @layout_manager: A layout manager to get the layout from
 * @wl_output: A wayland output
 * @uid: an identifier
 * @namespace: namespace, if ou have multiple layouts
 * @initalized:
 *
 * Creates a new output.
 *
 * Returns: (transfer full): a new river context object.
 **/
GObject *g_river_output_new(struct river_layout_manager_v3 *layout_manager,
		struct wl_output *wl_output, uint32_t uid,
		const char *namespace, bool initalized)
{
	GriverOutput *output = g_object_new(GRIVER_TYPE_OUTPUT, NULL);
	GriverOutputPrivate *priv = g_river_output_get_instance_private(output);

	priv->output = wl_output;
	priv->layout = NULL;
	priv->uid    = uid;

	if (initalized) {
		g_river_output_configure(output, layout_manager, namespace);
	}
	return (GObject *)output;
}
