#include "logger.h"
#include "httpd.h"
#include "module.h"
#include "mod_static_files.h"

static void mod_free(struct data *app_data)
{
	Debug("free %p\n", app_data);
	// TODO: clean up the data structure
}

static struct data *mod_start(const char *method, const char *uri,
	const char *arg)
{
	// TODO: allocate this instead of using a static.
	static struct data app_data;

	app_data.free_data = mod_free;
	return &app_data;
}

static void on_header_done(struct channel *ch, struct data *app_data,
	struct env *headers)
{
	const char msg[] = "Hello World\r\n";

	httpd_response(ch, 200);
	// TODO: write headers
	httpd_header(ch, "Content-Type", "text/plain");
	httpd_end_headers(ch);
	// TODO: output file/stream/etc
	channel_write(ch, msg, sizeof(msg) - 1);
	channel_done(ch);
}

static void on_data(struct channel *ch, struct data *app_data, size_t len,
	const void *data)
{
}

const struct module mod_static_files = {
	.desc = __FILE__,
	.start = mod_start,
	.on_header_done = on_header_done,
	.on_data = on_data,
};
