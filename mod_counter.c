/*
 * Copyright (c) 2012-2013 Jon Mayo
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
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "logger.h"
#include "container_of.h"
#include "httpd.h"
#include "module.h"
#include "mod_counter.h"

static long counter = 0; /* TODO: protect from concurrent access */

struct mod_counter_info {
	struct data app_data;
	const char *content_type;
	const char *base;
	const char *uri;
};

static void mod_free(struct data *app_data)
{
	struct mod_counter_info *info = container_of(app_data,
		struct mod_counter_info, app_data);

	Debug("free %p (info=%p)\n", app_data, info);
	assert(app_data != NULL);
	free(info);
}

static struct data *mod_start(const char *method, const char *uri,
	const char *arg)
{
	struct mod_counter_info *info;

	info = calloc(1, sizeof(*info));
	if (!info) {
		perror(uri);
		return NULL;
	}
	info->app_data.free_data = mod_free;
	info->uri = uri;
	info->base = arg; /* TODO: parse multiple options in arg */

	Debug("module start (arg=\"%s\" uri=\"%s\"\n", arg, uri);
	return &info->app_data;
}

static void on_header_done(struct channel *ch, struct data *app_data,
	struct env *headers)
{
	struct mod_counter_info *info = container_of(app_data,
		struct mod_counter_info, app_data);
	char length_str[20];
	char buf[256]; /* TODO: use a bigger buffer size */
	size_t buf_len;

	httpd_response(ch, 200);

	httpd_header(ch, "Content-Type", "text/plain");
	snprintf(buf, sizeof(buf), "%lu\r\n", counter++);
	buf_len = strlen(buf);

	snprintf(length_str, sizeof(length_str), "%lu", (unsigned long)buf_len);
	httpd_header(ch, "Content-Length", length_str);

	httpd_end_headers(ch);

	ch_write(ch, buf, buf_len);

	/* TODO: support persistent */
	ch_done(ch);
}

static void on_data(struct channel *ch, struct data *app_data, size_t len,
	const void *data)
{
}

const struct module mod_counter = {
	.desc = __FILE__,
	.start = mod_start,
	.on_header_done = on_header_done,
	.on_data = on_data,
};
