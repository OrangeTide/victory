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
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "channel.h"
#include "container_of.h"
#include "httpparser.h"

struct httpd {
	struct channel_entry channel;
	void (*on_connect)(struct httpd *h);
	void (*on_header)(struct httpd *h, const char *name, const char *val);
	struct httpparser *hp;
};

void httpd_on_read(struct channel_entry *ch)
{
	char buf[64]; // TODO: we could move this to a per-channel or per-thread buffer?
	int fd = ch->fd;
	ssize_t cnt;
	struct httpd *h = container_of(ch, struct httpd, channel);
	struct httpparser *hp = h->hp;

	assert(ch != NULL);
	cnt = read(fd, buf, sizeof(buf));
	if (cnt < 0) {
		perror("read()");
		channel_close(ch);
	} else if (!cnt) {
		fprintf(stderr, "closing:%s\n", ch->desc);
		channel_close(ch);
	} else {
		// fprintf(stderr, "read:%s:cnt=%zd\n", ch->desc, cnt);
		if (httpparser(hp, buf, cnt)) {
			fprintf(stderr, "httpd:%s:parser failure\n", ch->desc);
			channel_close(ch);
		}
	}
}

static int response(struct channel_entry *ch, size_t resp_len, const char *resp)
{
	int fd = ch->fd;
	ssize_t cnt;

	cnt = write(fd, resp, resp_len);
	if (cnt < 0) {
		fprintf(stderr, "httpd:%s:i/o error (%d)\n", ch->desc, errno);
		channel_close(ch);
		return -1;
	} else if ((size_t)cnt != resp_len) {
		fprintf(stderr, "httpd:%s:client stalled, closing.\n", ch->desc);
		channel_close(ch);
		return -1;
	} else {
		// TODO: support keep-alive
		channel_close(ch);
		return 0;
	}
}

static void on_method(void *p, const char *method, const char *uri)
{
	struct httpd *h = p;
	struct channel_entry *ch = &h->channel;

	printf("%s():%p:method=\"%s\" uri=\"%s\"\n", __func__, p, method, uri);
	if (strcmp(method, "GET")) {
		const char resp[] =
			"HTTP/1.1 405 Method Not Allowed\r\n"
			"\r\n";
		const ssize_t resp_len = sizeof(resp) - 1;

		response(ch, resp_len, resp);
	}
	// TODO: look up service
	// TODO: bind to service
}

static void on_field(void *p, const char *name, const char *value)
{
	// printf("%s():%p:name=\"%s\" value=\"%s\"\n", __func__, p, name, value);
	if (!strcmp(name, "Host")) {
		printf("HOST=%s\n", value);
	} else if (!strcmp(name, "Connection")) {
		printf("CONNECTION=%s\n", value);
	}
}

static void on_headerfinish(void *p)
{
	struct httpd *h = p;
	struct channel_entry *ch = &h->channel;
	const char resp[] =
		"HTTP/1.1 200 OK\r\n"
		"\r\n";
	const ssize_t resp_len = sizeof(resp) - 1;

	response(ch, resp_len, resp);
}

static void on_data(void *p, size_t len, const void *data)
{
	printf("%s():%p:len=%zd data=%p\n", __func__, p, len, data);
}

struct channel_entry *httpd_alloc(void)
{
	struct httpd *h;

	// TODO: how to pass further parameters?
	h = calloc(1, sizeof(*h));
	h->hp = httpparser_create(h, on_method, on_field, on_headerfinish, on_data);
	return &h->channel;
}

void httpd_free(struct channel_entry *ch)
{
	struct httpd *h = container_of(ch, struct httpd, channel);

	// TODO: how to free the additional parameters?
	httpparser_destroy(h->hp);
	free(h);
}
