#include "httpd.h"
#include "module.h"
#include "mod_static_files.h"

static void *mod_start(const char *method, const char *uri, const char *arg)
{
	// TODO: allocate information necessary for this.
	return NULL;
}

static void mod_free(void *app_ptr)
{
}

static void on_header(struct channel *ch, void *app_ptr, const char *name,
	const char *value)
{
}

static void on_header_done(struct channel *ch, void *app_ptr)
{
	const char msg[] = "Hello World\r\n";

	httpd_response(ch, 200);
	// TODO: write headers
	httpd_end_headers(ch);
	// TODO: output file/stream/etc
	channel_write(ch, msg, sizeof(msg) - 1);
	channel_done(ch);
}

static void on_data(struct channel *ch, void *app_ptr, size_t len,
	const void *data)
{
}

const struct module mod_static_files = {
	.desc = __FILE__,
	.start = mod_start,
	.free = mod_free,
	.on_header = on_header,
	.on_header_done = on_header_done,
	.on_data = on_data,
};
