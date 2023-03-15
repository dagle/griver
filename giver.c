#include "giver.h"
#include "glibconfig.h"

#include <stdbool.h>
#include <stdio.h>
#include <wayland-client-core.h>
#include <wayland-client.h>
#include <wayland-client-protocol.h>

#include "river-layout-v3-client-protocol.h"
#include "river-status-unstable-v1-client-protocol.h"

// TODO:
// [-] Fix errors, printing to stdout isn't the nicest
// [x] Remove variables that shouldn't need to be global
// [ ] Add a rountrip to make sure things started correctly
// [ ] Fix signals (add output object / name)

typedef struct {
	int intialized;
	int loop;
	int exitcode;
	GError *error;
	GList *outputs;
} GiverContextPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GiverContext, g_river, G_TYPE_OBJECT)

static void run(GiverContext *self, GError **err);
static bool init_wayland (GiverContext *self);
static void finish_wayland (GiverContext *self);

struct wl_registry *wl_registry;
struct wl_callback *sync_callback;
struct river_layout_manager_v3 *layout_manager;
struct wl_display  *wl_display;

enum {
  LAYOUT_DEMAND,
  USER_COMMAND,
  ADD_OUTPUT,
  REMOVE_OUTPUT,
  // IN_USE,
  GIVER_LAST_SIGNAL
};

static guint giver_signals[GIVER_LAST_SIGNAL] = { 0 };

GQuark giver_error_quark;

/**
 * GMIME_ERROR:
 *
 * The GMime error domain GQuark value.
 **/
#define G_RIVER_ERROR gmime_error_quark

/**
 * GMIME_ERROR_IS_SYSTEM:
 * @error: integer error value
 *
 * Decides if an error is a system error (aka errno value) vs. a GMime
 * error.
 *
 * Meant to be used with #GError::code
 **/
#define GMIME_ERROR_IS_SYSTEM(error) ((error) > 0)

/* errno is a positive value, so negative values should be safe to use */
enum {
	G_RIVER_ERROR_NAMESPACE_INUSE             = -1,
	G_RIVER_ERROR_NOT_SUPPORTED               = -2,
	G_RIVER_ALLOCATION_ERROR                  = -3,
	G_RIVER_ERROR_INIT                        = -4,
};
// create an error quarks and levels for G_RIVER using glib

// change this into a class or shomething or a boxed?
// Should we have a backpointer to the class?
// Or how do we get the output from the class?
typedef struct Output {
	// should we ref-count this object?
	GiverContext *ctx;
	int cmd_tags;
    bool initialized;

	uint32_t global_name;

	struct wl_output       *output;
	struct river_layout_v3 *layout;
} Output;

static void layout_handle_namespace_in_use (void *data, struct river_layout_v3 *river_layout_v3)
{
	struct Output *output = (struct Output *)data;
	GiverContextPrivate *priv = g_river_get_instance_private (output->ctx);

	// GError *err;
	// g_set_error(err, G_RIVER_ERROR, G_RIVER_ERROR_NAMESPACE_INUSE,
	// 		"Namespace already in use");
	// priv->error = err;
	fputs("Namespace already in use.\n", stderr);
	priv->loop = false;
}

static void layout_handle_layout_demand (void *data, struct river_layout_v3 *river_layout_v3,
		uint32_t view_count, uint32_t width, uint32_t height, uint32_t tags, uint32_t serial) {

	struct Output *output = (struct Output *)data;

	g_signal_emit (output->ctx, giver_signals[LAYOUT_DEMAND], 0, view_count, width, height, serial);
}
static void layout_handle_user_command (void *data, struct river_layout_v3 *river_layout_manager_v3,
		const char *command) {
	struct Output *output = (struct Output *)data;

	g_signal_emit (output->ctx, giver_signals[USER_COMMAND], 0, command, output->cmd_tags);
}

void layout_handle_command_tags(void *data,
		struct river_layout_v3 *river_layout_v3,
		uint32_t tags) {
	struct Output *output = (struct Output *)data;
	output->cmd_tags = tags;
}

static const struct river_layout_v3_listener layout_listener = {
	.namespace_in_use = layout_handle_namespace_in_use,
	.layout_demand    = layout_handle_layout_demand,
	.user_command     = layout_handle_user_command,
	.user_command_tags = layout_handle_command_tags,
};

static void configure_output (struct Output *output, GiverContextPrivate *priv)
{
	if (priv->intialized) {
		output->layout = river_layout_manager_v3_get_layout(layout_manager,
				output->output, "griver");
		river_layout_v3_add_listener(output->layout, &layout_listener, output);
	}
}

static bool create_output (GiverContext *self, struct wl_output *wl_output, uint32_t global_name)
{
	GiverContextPrivate *priv = g_river_get_instance_private (self);

	// don't use calloc?
	struct Output *output = calloc(1, sizeof(struct Output));
	if ( output == NULL )
	{
		// GError *err;
		// g_set_error(err, G_RIVER_ERROR, G_RIVER_ALLOCATION_ERROR,
		// 		"Namespace already in use");
		// priv->error = err;
		fputs("Failed to allocate.\n", stderr);
		return false;
	}

	output->output      = wl_output;
	output->layout      = NULL;
	output->global_name = global_name;
	output->ctx = self;

	configure_output(output, priv);

	priv->outputs = g_list_append(priv->outputs, output);
	return true;
}

static void g_river_class_init(GiverContextClass *klass){
	// GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	klass->run = run;

	giver_signals[LAYOUT_DEMAND] = g_signal_new ("layout-demand",
			G_TYPE_FROM_CLASS (klass),
			G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
			0,
			NULL,
			NULL,
			NULL,
			G_TYPE_NONE,
			5,
			// GIVER_TYPE_OUTPUT,
			G_TYPE_UINT,
			G_TYPE_UINT,
			G_TYPE_UINT,
			G_TYPE_UINT,
			G_TYPE_UINT
			);
	giver_signals[USER_COMMAND] = g_signal_new ("user-command",
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
	giver_signals[ADD_OUTPUT] = g_signal_new ("output-add",
			G_TYPE_FROM_CLASS (klass),
			G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
			0,
			NULL,
			NULL,
			NULL,
			G_TYPE_NONE,
			0
			// GIVER_TYPE_OUTPUT
			);
	giver_signals[REMOVE_OUTPUT] = g_signal_new ("output-remove",
			G_TYPE_FROM_CLASS (klass),
			G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
			0,
			NULL,
			NULL,
			NULL,
			G_TYPE_NONE,
			0
			// GIVER_TYPE_OUTPUT // maybe change this to string, easier to free that way
			);
}

static void registry_handle_global (void *data, struct wl_registry *registry,
		uint32_t name, const char *interface, uint32_t version)
{
	GiverContext *self = GIVER_CONTEXT(data);
	GiverContextPrivate *priv = g_river_get_instance_private (self);
	if ( strcmp(interface, river_layout_manager_v3_interface.name) == 0 )
		layout_manager = wl_registry_bind(registry, name,
				&river_layout_manager_v3_interface, 1);
	else if ( strcmp(interface, wl_output_interface.name) == 0 )
	{
		struct wl_output *wl_output = wl_registry_bind(registry, name,
				&wl_output_interface, 3);
		if (!create_output(self, wl_output, name))
		{
			priv->loop = false;
			priv->exitcode = EXIT_FAILURE;
			return;
		}
		g_signal_emit (self, giver_signals[ADD_OUTPUT], 0);
	}
}

static struct Output *output_from_global_name (GiverContextPrivate *priv, uint32_t name)
{
	GList *list = priv->outputs;
	while (list) {
		struct Output *output = (struct Output *) list->data;
		if (output->global_name == name)
			return output;
		list = list->next;
	}
	return NULL;
}

static void destroy_output (GiverContextPrivate *priv, struct Output *output)
{
	if ( output->layout != NULL )
		river_layout_v3_destroy(output->layout);
	wl_output_destroy(output->output);
	priv->outputs = g_list_remove(priv->outputs, output);
	free(output);
}

static void registry_handle_global_remove (void *data, struct wl_registry *registry, uint32_t name)
{
	GiverContext *self = GIVER_CONTEXT(data);
	GiverContextPrivate *priv = g_river_get_instance_private (self);
	struct Output *output = output_from_global_name(priv, name);
	if ( output != NULL ){
		g_signal_emit (self, giver_signals[REMOVE_OUTPUT], 0);
		destroy_output(priv, output);
	}
}

static const struct wl_registry_listener registry_listener = {
	.global        = registry_handle_global,
	.global_remove = registry_handle_global_remove,
};

static void sync_handle_done (void *data, struct wl_callback *wl_callback,
		uint32_t irrelevant)
{
	GiverContext *self = GIVER_CONTEXT(data);
	GiverContextPrivate *priv = g_river_get_instance_private (self);

	wl_callback_destroy(wl_callback);
	sync_callback = NULL;

	/* When this function is called, the registry finished advertising all
	 * available globals. Let's check if we have everything we need.
	 */
	if ( layout_manager == NULL )
	{
		// GError *err;
		// g_set_error(err, G_RIVER_ERROR, G_RIVER_ERROR_NOT_SUPPORTED,
		// 		"Namespace already in use");
		// priv->error = err;
		fputs("Wayland compositor does not support river-layout-v3.\n", stderr);
		priv->exitcode = EXIT_FAILURE;
		priv->loop = false;
		return;
	}

	/* If outputs were registered before the river_layout_manager is
	 * available, they won't have a river_layout, so we need to create those
	 * here.
	 */
	GList *list = priv->outputs;
	while (list) {
		struct Output *output = (struct Output *) list->data;
		configure_output(output, priv);
		list = list->next;
	}
}

// maybe we need this, dunno
static const struct wl_callback_listener sync_callback_listener = {
	.done = sync_handle_done,
};

static bool init_wayland (GiverContext *self)
{
	GiverContextPrivate *priv = g_river_get_instance_private (self);
	/* We query the display name here instead of letting wl_display_connect()
	 * figure it out itself, because libwayland (for legacy reasons) falls
	 * back to using "wayland-0" when $WAYLAND_DISPLAY is not set, which is
	 * generally not desirable.
	 */
	const char *display_name = g_getenv("WAYLAND_DISPLAY");
	if ( display_name == NULL )
	{
		// g_set_error(err, G_RIVER_ERROR, G_RIVER_ERROR_INIT,
		// 		"WAYLAND_DISPLAY is not set");
		g_printerr("WAYLAND_DISPLAY is not set.\n");
		return false;
	}

	wl_display = wl_display_connect(display_name);
	if ( wl_display == NULL )
	{
		// g_set_error(err, G_RIVER_ERROR, G_RIVER_ERROR_INIT,
		// 		"Can not connect to Wayland server");
		g_printerr("Can not connect to Wayland server.\n");
		return false;
	}

	priv->outputs = NULL;

	wl_registry = wl_display_get_registry(wl_display);
	wl_registry_add_listener(wl_registry, &registry_listener, self);
	priv->intialized = true;

	sync_callback = wl_display_sync(wl_display);
	wl_callback_add_listener(sync_callback, &sync_callback_listener, NULL);

	return true;
}

static void destroy_all_outputs (GiverContext *self)
{
	GiverContextPrivate *priv = g_river_get_instance_private (self);
	GList *list = priv->outputs;
	while (list) {
		struct Output *output = (struct Output *) list->data;
		destroy_output(priv, output);
		list = list->next;
	}
}

static void finish_wayland (GiverContext *self)
{  
	if ( wl_display == NULL ) {
		return;
	}

	destroy_all_outputs(self);
	// if ( sync_callback != NULL )
	// 	wl_callback_destroy(sync_callback);

	if ( layout_manager != NULL ) {
		river_layout_manager_v3_destroy(layout_manager);
	}

	wl_registry_destroy(wl_registry);
	wl_display_disconnect(wl_display);
}

static void 
run (GiverContext *self, GError **error) {
	GiverContextPrivate *priv = g_river_get_instance_private(self);
	if (init_wayland(self)) {
	}

	priv->exitcode = EXIT_SUCCESS;
	while(priv->loop) {
		if (wl_display_dispatch(wl_display) < 0) {
			priv->exitcode = EXIT_FAILURE;
			*error = priv->error;
			break;
		}
	} 
	finish_wayland(self);
}

void 
g_river_run (GiverContext *ctx, GError **error) {
	g_return_if_fail (GIVER_IS_CONTEXT(ctx));
	
	return GIVER_CONTEXT_GET_CLASS(ctx)->run(ctx, error);
}

static void g_river_init(GiverContext *ctx) {
}

/**
 * g_river_new:
 *
 * Creates a new river context object.
 *
 * Returns: (transfer full): a new river context object.
 **/
GObject *g_river_new(){
	return g_object_new(GIVER_TYPE_CONTEXT, NULL);
}
