/*
 * Copyright (c) 2012 Jon Mayo
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <stddef.h>
#include "httpd.h"
#include "daemonize.h"
#include "service.h"

static void *app_start(const char *method, const char *uri)
{
}

static void app_free(void *app_ptr)
{
}

static void on_header(void *app_ptr, const char *name, const char *value)
{
}

static void on_header_done(void *app_ptr)
{
}

static void on_data(void *app_ptr, size_t len, const void *data)
{
}


int main()
{

	const struct service myapp = {
		app_start, app_free, on_header, on_header_done, on_data,
	};

	service_register("/*", &myapp);

	httpd_poolsize(100);
	if (httpd_start(NULL, "8080"))
		return 1;
	httpd_loop();
	// daemonize();
	return 0;
}
