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
#include <errno.h>

#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "logger.h"
#include "container_of.h"
#include "httpd.h"
#include "module.h"
#include "ext.h"
#include "mod_static_files.h"

struct mod_static_file_info {
	struct data app_data;
	int fd;
	struct stat stat_buf;
	const char *content_type;
	const char *base;
	const char *uri;
};

static void mod_free(struct data *app_data)
{
	struct mod_static_file_info *info = container_of(app_data,
		struct mod_static_file_info, app_data);

	Debug("free %p (info=%p)\n", app_data, info);
	assert(app_data != NULL);
	close(info->fd);
	free(info);
}

static int open_path(struct mod_static_file_info *info,
	const char *base, const char *uri)
{
	int fd;
	char path[PATH_MAX];
	int e;
	size_t base_len = strlen(base);

	/* fix up absolute paths */
	while (uri[0] == '/')
		uri++;

	/* check that there is room for the string, plus an extra '/' */
	if (base_len > sizeof(path) - 2)
		return -1;
	memcpy(path, base, base_len);

	/* append a '/' if one is not found */
	if (base_len && path[base_len - 1] != '/') {
		path[base_len++] = '/';
		path[base_len] = 0;
	}

	e = util_fixpath(path + base_len, sizeof(path) - base_len, uri);
	if (e) {
		Error("%s:buffer overflow prevented\n", uri);
		return -1;
	}

	Debug("Using path=\"%s\" (%s/%s)\n", path, base, uri);

	fd = open(path, O_RDONLY);
	if (fd < 0)
		goto failure;
	if (fstat(fd, &info->stat_buf))
		goto close_and_fail;
	info->fd = fd;
	info->content_type = ext_content_type(uri);
	return 0;
close_and_fail:
	close(fd);
failure:
	Error("%s:%s\n", path, strerror(errno));
	return -1;
}

static struct data *mod_start(const char *method, const char *uri,
	const char *arg)
{
	struct mod_static_file_info *info;

	info = calloc(1, sizeof(*info));
	if (!info) {
		perror(uri);
		return NULL;
	}
	info->fd = -1;
	info->app_data.free_data = mod_free;
	info->uri = uri;
	info->base = arg; /* TODO: parse multiple options in arg */

	Debug("module start (arg=\"%s\" uri=\"%s\"\n", arg, uri);
	return &info->app_data;
}

static void on_header_done(struct channel *ch, struct data *app_data,
	struct env *headers)
{
	struct mod_static_file_info *info = container_of(app_data,
		struct mod_static_file_info, app_data);
	char length_str[20];
	char buf[256]; /* TODO: use a bigger buffer size */
	off_t total_sent;

	if (open_path(info, info->base, info->uri)) {
		httpd_response(ch, 404);
		httpd_end_headers(ch);
		ch_done(ch);
		return;
	}

	httpd_response(ch, 200);

	if (info->content_type)
		httpd_header(ch, "Content-Type", info->content_type);
	else
		httpd_header(ch, "Content-Type", "text/plain");

	snprintf(length_str, sizeof(length_str), "%lu",
		(long)info->stat_buf.st_size);
	httpd_header(ch, "Content-Length", length_str);

	httpd_end_headers(ch);

	/* stream file out */
	total_sent = 0;
	while (total_sent < info->stat_buf.st_size) {
		ssize_t res;

		res = read(info->fd, buf, sizeof(buf));
		if (res < 0) {
			perror(__func__);
			ch_done(ch);
			return;
		}
		if (total_sent + res > info->stat_buf.st_size)
			res = info->stat_buf.st_size - total_sent;
		ch_write(ch, buf, res);
		total_sent += res;
	}

	/* TODO: support persistent */
	ch_done(ch);
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
