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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <netdb.h>
#include <unistd.h>
#include "httpd.h"
#include "httpparser.h"
#include "channel.h"

struct server {
	pthread_t *thread_pool;
	unsigned num_thread_pool;
	int fd;
	struct server *next;
	char *desc;
};

struct httpchannel {
	struct channel channel;
	struct httpparser hp;
};

static struct server *server_head;
static const unsigned pool_size = 200;

static void grow(void *ptr, unsigned *max, unsigned min, size_t elem)
{
	size_t old = *max * elem;

	if (min <= old)
		return;

	min += sizeof(intptr_t);
	min--;
	min |= min >> 1;
	min |= min >> 2;
	min |= min >> 4;
	min |= min >> 8;
	min |= min >> 16;
	min++;
	min -= sizeof(intptr_t);
	*max = min;
#ifndef NDEBUG
	fprintf(stderr, "realloc from %zd to %d\n", old, min);
#endif
	assert(old <= min);
	*(char**)ptr = realloc(*(char**)ptr, min * elem);
	assert(*(void**)ptr != NULL);
	memset(*(char**)ptr + old, 0, (min - old) * elem);
	assert((intptr_t)*(void**)ptr >= 4096);
}

static void make_name(char *buf, size_t buflen, const struct sockaddr *sa, socklen_t salen)
{
	char hostbuf[64], servbuf[32];

	getnameinfo(sa, salen, hostbuf, sizeof(hostbuf), servbuf, sizeof(servbuf), NI_NUMERICHOST | NI_NUMERICSERV);
	snprintf(buf, buflen, "%s:%s", hostbuf, servbuf);
	// TODO: return a value
}

static void response(struct channel *ch, int status_code)
{
	const char resp200[] = "HTTP/1.1 200 OK\r\n";
	const size_t resp200_len = sizeof(resp200) - 1;
	const char resp400[] = "HTTP/1.1 400 Bad Request\r\n";
	const size_t resp400_len = sizeof(resp400) - 1;
	const char resp500[] = "HTTP/1.1 500 Internal Server Error\r\n";
	const size_t resp500_len = sizeof(resp500) - 1;
	const char resp501[] = "HTTP/1.1 501 Not Implemented\r\n";
	const size_t resp501_len = sizeof(resp501) - 1;
	const char resp502[] = "HTTP/1.1 502 Bad Gateway\r\n";
	const size_t resp502_len = sizeof(resp502) - 1;
	const char resp503[] = "HTTP/1.1 503 Service Unavailable\r\n";
	const size_t resp503_len = sizeof(resp503) - 1;
	const char resp504[] = "HTTP/1.1 504 Gateway Timeout\r\n";
	const size_t resp504_len = sizeof(resp504) - 1;
	const char resp505[] = "HTTP/1.1 505 HTTP Version Not Supported\r\n";
	const size_t resp505_len = sizeof(resp505) - 1;
	const char *resp;
	size_t resp_len;

	switch (status_code) {
	case 200: resp = resp200; resp_len = resp200_len; break;
	case 400: resp = resp400; resp_len = resp400_len; break;
	default:
	case 500: resp = resp500; resp_len = resp500_len; break;
	case 501: resp = resp501; resp_len = resp501_len; break;
	case 502: resp = resp502; resp_len = resp502_len; break;
	case 503: resp = resp503; resp_len = resp503_len; break;
	case 504: resp = resp504; resp_len = resp504_len; break;
	case 505: resp = resp505; resp_len = resp505_len; break;
	}
	fprintf(stderr, "%s:status_code=%d\n", ch->desc, status_code);
	channel_write(ch, resp, resp_len);
}

static void end_headers(struct channel *ch)
{
	channel_write(ch, "\r\n", 2);
}

static void on_method(void *p, const char *method, const char *uri)
{
	// TODO: check method
	// TODO: look up URI
}

static void on_header(void *p, const char *name, const char *value)
{
	// TODO: process headers
}

static void on_headerfinish(void *p)
{
	struct channel *ch = p;
	const char msg[] = "Hello World\r\n";

	response(ch, 200);
	// TODO: write headers
	end_headers(ch);
	// TODO: output file/stream/etc
	channel_write(ch, msg, sizeof(msg) - 1);

	channel_done(ch);
}

static void on_data(void *p, size_t len, const void *data)
{
	// ignored
}

static void httpd_process(struct httpchannel *hc)
{
	struct channel *ch = &hc->channel;

	while (!ch->done && channel_fill(ch) > 0) {
		if (httpparser(&hc->hp, ch->buf, ch->buf_cur, ch, on_method,
			on_header, on_headerfinish, on_data)) {
			fprintf(stderr, "%s:parse failure\n", ch->desc);
			response(ch, 500);
			end_headers(ch);
			break;
		}
		ch->buf_cur = 0; /* httpparser() consumes 100% of buffer */
	}
}

static void httpchannel_init(struct httpchannel *hc, int fd, const char *desc)
{
	httpparser_init(&hc->hp);
	channel_init(&hc->channel, fd, desc);
}

static int server_accept(struct server *serv, struct httpchannel *hc)
{
	struct sockaddr_storage addr;
	socklen_t addrlen = sizeof(addr);
	int newfd;
	char desc[64];

	newfd = accept(serv->fd, (struct sockaddr*)&addr, &addrlen);
	if (newfd < 0) {
		perror("accept()");
		return -1;
	}
	make_name(desc, sizeof(desc), (struct sockaddr*)&addr, addrlen);
	httpchannel_init(hc, newfd, desc);
	return 0;
}

static void *worker_start(void *p)
{
	struct server *serv = p;
	struct httpchannel hc;

	while (1) {
		if (server_accept(serv, &hc))
			return NULL;
		httpd_process(&hc);
		fprintf(stderr, "%s:connection terminated\n", hc.channel.desc);
		channel_close(&hc.channel);
	}
	return NULL;
}

static void resize_thread_pool(struct server *serv, unsigned new_size)
{
	unsigned i;
	int e;
	unsigned old_size = serv->num_thread_pool;

	/* remove threads if we shrink */
	// TODO: prefer idle/sleep threads
	for (i = new_size; i < old_size; i++) {
		e = pthread_cancel(serv->thread_pool[i]);
		if (e) {
			perror(serv->desc);
			break;
		}
	}
	grow(&serv->thread_pool, &serv->num_thread_pool, new_size, sizeof(*serv->thread_pool));
	/* add threads if we grow */
	for (i = old_size; i < new_size; i++) {
		e = pthread_create(&serv->thread_pool[i], NULL, worker_start, serv);
		if (e) {
			perror(serv->desc);
			/* the list came up short */
			serv->num_thread_pool = i;
			break;
		}
		e = pthread_detach(serv->thread_pool[i]);
		if (e) {
			perror(serv->desc);
			// TODO: handle the error
		}

	}

}

static void server_add_entry(int fd, const char *desc)
{
	struct server *serv;

	serv = calloc(1, sizeof(*serv));
	serv->fd = fd;
	serv->desc = strdup(desc);
	resize_thread_pool(serv, pool_size);
	serv->next = server_head;
	server_head = serv;
}

int httpd_start(const char *node, const char *service)
{
	struct addrinfo hints = {
		.ai_flags = AI_NUMERICHOST | AI_PASSIVE,
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
		.ai_protocol = 0,
		.ai_next = NULL,
	};
	struct addrinfo *res, *cur;
	int e;
	char desc[64];

	e = getaddrinfo(node, service, &hints, &res);
	if (e) {
		fprintf(stderr, "Error:%s\n", gai_strerror(e));
		return -1;
	}
	for (cur = res; cur; cur = cur->ai_next) {
		int fd;

		fd = socket(cur->ai_family, cur->ai_socktype, cur->ai_protocol);
		if (fd < 0) {
			perror("socket()");
			goto fail_and_free;
		}
		e = bind(fd, cur->ai_addr, cur->ai_addrlen);
		if (e) {
			perror("bind()");
			goto fail_and_free;
		}
		e = listen(fd, SOMAXCONN);
		if (e) {
			perror("listen()");
			goto fail_and_free;
		}
		make_name(desc, sizeof(desc), cur->ai_addr, cur->ai_addrlen);
		server_add_entry(fd, desc);
	}
	freeaddrinfo(res);
	return 0;
fail_and_free:
	freeaddrinfo(res);
	return -1;
}

int httpd_loop(void)
{
	if (server_head)
		worker_start(server_head); /* join the last thread pool */
	return 0;
}
