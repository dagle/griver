#include "griver-output.h"
#include "griver-context.h"
#include <stdio.h>

GriverOutput *outputs = NULL;

uint32_t main_count = 1;
uint32_t view_padding = 1;
uint32_t outer_padding = 1;
double ratio = 0.6;
GriverRotation rotation = GRIVER_LEFT;

void
tile(GriverOutput *output, uint32_t view_count, uint32_t width,
		uint32_t height, uint32_t tags, uint32_t serial)
{
	g_river_output_tall_layout(output, view_count, width, height, main_count, view_padding, outer_padding, ratio, rotation, serial);
	g_river_output_commit_dimensions(output, "[]=", serial);
}

void
user_command(GriverOutput *output, const char *cmd, uint32_t tags)
{
	printf("Got command: %s\n", cmd);
	// TODO: Do some parsing.
}

void add_output(GriverContext *ctx, GriverOutput *output, gpointer user_data)
{
	g_signal_connect(output, "layout-demand", G_CALLBACK(tile), NULL);
	g_signal_connect(output, "user-command", G_CALLBACK(user_command), NULL);
}

void output_delete(GriverContext *ctx, GriverOutput *output, gpointer user_data)
{
	printf("output deleted");
}

int main(int argc, char *argv[])
{
	GriverContext *ctx;
	GError *error = NULL;

	ctx = (GriverContext *)g_river_context_new("rivertile");

	g_signal_connect(ctx, "output-add", G_CALLBACK(add_output), NULL);
	g_signal_connect(ctx, "output-remove", G_CALLBACK(output_delete), NULL);
	gboolean ret = g_river_context_run(ctx, &error);
	printf("Ret: %d\n", ret);

	return 0;
}
