#include "griver-context.h"
#include "griver-output.h"
#include "glib.h"

#include <stdbool.h>
#include <stdio.h>
#include <wayland-client-core.h>
#include <wayland-client.h>
#include <wayland-client-protocol.h>

#include "river-layout-v3-client-protocol.h"

typedef struct {
	char *namespace;
	gboolean intialized;
	gboolean loop;
	gboolean exitcode;

	GError *error;
	GList *outputs; // List of Outputs
} GriverContextPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GriverContext, g_river_context, G_TYPE_OBJECT)

static gboolean run (GriverContext *ctx, GError **err);

static bool init_wayland (GriverContext *ctx);
static void finish_wayland (GriverContext *ctx);
static void context_finalize(GObject *object);

// TODO: move this to the private context
struct wl_registry *wl_registry = NULL;
struct wl_callback *sync_callback = NULL;
struct river_layout_manager_v3 *layout_manager = NULL;
struct wl_display  *wl_display = NULL;

enum {
  GRIVER_ADD_OUTPUT,
  GRIVER_REMOVE_OUTPUT,
  GRIVER_CONTEXT_LAST_SIGNAL
};

static guint griver_signals[GRIVER_CONTEXT_LAST_SIGNAL] = { 0 };

GQuark griver_error_quark;

/**
 * GMIME_ERROR:
 *
 * The GMime error domain GQuark value.
 **/
#define GRIVER_ERROR griver_error_quark

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

static void g_river_context_init(GriverContext *ctx) {
	GriverContextPrivate *priv = g_river_context_get_instance_private (ctx);
	priv->namespace = NULL;
	priv->intialized = false;
	priv->loop = true;
	priv->exitcode = true;

	priv->error = NULL;
	priv->outputs = NULL;
}

static void g_river_context_class_init(GriverContextClass *klass){
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	klass->run = run;
	gobject_class->finalize = context_finalize;

	/**
	 * GriverContext::output-add:
	 * @runtime: The [class@Griver.GriverContext] instance.
	 * @out: (transfer full): A newly allocated output
	 *
	 **/
	griver_signals[GRIVER_ADD_OUTPUT] = g_signal_new ("output-add",
			G_TYPE_FROM_CLASS (klass),
			G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
			0,
			NULL,
			NULL,
			NULL,
			G_TYPE_NONE,
			1,
			GRIVER_TYPE_OUTPUT
			);
	/**
	 * GriverContext::output-remove:
	 * @runtime: The [class@Griver.GriverContext] instance.
	 * @out: The output to remove
	 *
	 **/
	griver_signals[GRIVER_REMOVE_OUTPUT] = g_signal_new ("output-remove",
			G_TYPE_FROM_CLASS (klass),
			G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
			0,
			NULL,
			NULL,
			NULL,
			G_TYPE_NONE,
			1,
			GRIVER_TYPE_OUTPUT
			);
}

GriverOutput *create_output (GriverContext *ctx, struct wl_output *wl_output, uint32_t global_name)
{
	GriverContextPrivate *priv = g_river_context_get_instance_private (ctx);

	GriverOutput *output = GRIVER_OUTPUT(
		g_river_output_new(layout_manager, wl_output, global_name, priv->namespace, priv->intialized));
	priv->outputs = g_list_append(priv->outputs, output);
	return output;
}

static void registry_handle_global (void *data, struct wl_registry *registry,
		uint32_t name, const char *interface, uint32_t version)
{
	GriverContext *ctx = GRIVER_CONTEXT(data);
	if ( strcmp(interface, river_layout_manager_v3_interface.name) == 0 )
	{
		layout_manager = wl_registry_bind(registry, name,
				&river_layout_manager_v3_interface, 2);
	}
	else if ( strcmp(interface, wl_output_interface.name) == 0 )
	{
		struct wl_output *wl_output = wl_registry_bind(registry, name,
				&wl_output_interface, 4);

		GriverOutput *output = create_output(ctx, wl_output, name);
		g_signal_emit (ctx, griver_signals[GRIVER_ADD_OUTPUT], 0, output);
	}
}

static GriverOutput *output_from_global_name (GriverContextPrivate *priv, uint32_t uid)
{
	GList *list = priv->outputs;
	while (list) {
		if (list->data) {
			GriverOutput *output = GRIVER_OUTPUT(list->data);
			if (g_river_output_get_uid(output) == uid)
				return output;
		}
		list = list->next;
	}
	return NULL;
}

static void registry_handle_global_remove (void *data, struct wl_registry *registry, uint32_t name)
{
	GriverContext *ctx = GRIVER_CONTEXT(data);
	GriverContextPrivate *priv = g_river_context_get_instance_private (ctx);
	GriverOutput *output = output_from_global_name(priv, name);
	if ( output != NULL ){
		g_signal_emit (ctx, griver_signals[GRIVER_REMOVE_OUTPUT], 0, output);
		priv->outputs = g_list_remove(priv->outputs, output);
		g_object_unref(output);
	}
}

static const struct wl_registry_listener registry_listener = {
	.global        = registry_handle_global,
	.global_remove = registry_handle_global_remove,
};

static void sync_handle_done (void *data, struct wl_callback *wl_callback,
		uint32_t irrelevant)
{
	wl_callback_destroy(wl_callback);
	sync_callback = NULL;

	/* When this function is called, the registry finished advertising all
	 * available globals. Let's check if we have everything we need.
	 */
	GriverContext *ctx = GRIVER_CONTEXT(data);
	GriverContextPrivate *priv = g_river_context_get_instance_private (ctx);
	if ( layout_manager == NULL )
	{
		// GError *err;
		// g_set_error(err, G_RIVER_ERROR, G_RIVER_ERROR_NOT_SUPPORTED,
		// 		"Namespace already in use");
		// priv->error = err;
		fputs("Wayland compositor does not support river-layout-v3.\n", stderr);
		priv->exitcode = false;
		priv->loop = false;
		return;
	}

	/* If outputs were registered before the river_layout_manager is
	 * available, they won't have a river_layout, so we need to create those
	 * here.
	 */
	GList *list = priv->outputs;
	while (list) {
		if (list->data) {
			GriverOutput *output = GRIVER_OUTPUT(list->data);
			g_river_output_configure(output, layout_manager, priv->namespace);
		}
		list = list->next;
	}
}

// maybe we need this, dunno
static const struct wl_callback_listener sync_callback_listener = {
	.done = sync_handle_done,
};

static bool init_wayland (GriverContext *ctx)
{
	GriverContextPrivate *priv = g_river_context_get_instance_private (ctx);
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

	wl_registry = wl_display_get_registry(wl_display);
	wl_registry_add_listener(wl_registry, &registry_listener, ctx);
	priv->intialized = true;

	sync_callback = wl_display_sync(wl_display);
	wl_callback_add_listener(sync_callback, &sync_callback_listener, ctx);

	return true;
}

static void destroy_all_outputs (GriverContext *ctx)
{
	GriverContextPrivate *priv = g_river_context_get_instance_private (ctx);
	GList *list = priv->outputs;
	
	while (list) {
		if (list->data) {
			GriverOutput *output = GRIVER_OUTPUT(list->data);
			g_object_unref(output);
		}
		list = list->next;
	}
	g_list_free(list);
}

static void finish_wayland (GriverContext *ctx)
{  
	if ( wl_display == NULL ) {
		return;
	}

	destroy_all_outputs(ctx);
	if ( sync_callback != NULL )
		wl_callback_destroy(sync_callback);

	if ( layout_manager != NULL ) {
		river_layout_manager_v3_destroy(layout_manager);
	}

	wl_registry_destroy(wl_registry);
	wl_display_disconnect(wl_display);
}

static gboolean 
run (GriverContext *ctx, GError **error) {
	GriverContextPrivate *priv = g_river_context_get_instance_private(ctx);

	if (!init_wayland(ctx)) {
		return false;
	}

	priv->exitcode = true;
	while(priv->loop) {
		if (wl_display_dispatch(wl_display) < 0) {
			priv->exitcode = false;
			*error = priv->error;
			break;
		}
	}

	if (priv->error) {
		printf("Error: %s", priv->error->message);
	}
	finish_wayland(ctx);
	return priv->exitcode;
}

static void context_finalize(GObject *object) {
	GriverContext *ctx = GRIVER_CONTEXT(object);
	GriverContextPrivate *priv = g_river_context_get_instance_private(ctx);

	g_free(priv->namespace);
}

/**
 * g_river_context_run:
 * @ctx: The context to run
 * @err: a #GError
 *
 * Run the river context, until the compositor stops.
 *
 **/
gboolean 
g_river_context_run (GriverContext *ctx, GError **error) {
	g_return_val_if_fail(GRIVER_IS_CONTEXT(ctx), false);
	
	return GRIVER_CONTEXT_GET_CLASS(ctx)->run(ctx, error);
}

/**
 * g_river_context_new:
 *
 * Creates a new river context object.
 *
 * Returns: (transfer full): a new river context object.
 **/
GObject *g_river_context_new(const char *namespace){
	g_return_val_if_fail(namespace, NULL);

	GriverContext *ctx = g_object_new(GRIVER_TYPE_CONTEXT, NULL);
	GriverContextPrivate *priv = g_river_context_get_instance_private(ctx);
	
	priv->namespace = strdup(namespace);
	return (GObject *)ctx;
}
