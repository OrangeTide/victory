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
#include <signal.h>
#include <unistd.h>
#include "logger.h"
#include "httpd.h"
#include "net.h"
#include "httpparser.h"
#include "channel.h"
#include "service.h"
#include "module.h"
#include "env.h"

#define HTTPD_METHOD_MAX 16
#define HTTPD_URI_MAX 512

struct httpchannel {
	struct channel channel;
	struct httpparser hp;
	const struct module *module;
	struct data *app_data;
	char method[HTTPD_METHOD_MAX];
	char uri[HTTPD_URI_MAX];
	struct env headers;
};

struct worker {
	pthread_t th;
	struct server *server;
	struct httpchannel httpchannel;
};

struct server {
	struct worker *thread_pool;
	unsigned num_thread_pool;
	struct net_listen listen_handle;
	struct server *next;
	char *desc;
};

static struct server *server_head;
static unsigned pool_size = 5;
static pthread_once_t httpd_init_once = PTHREAD_ONCE_INIT;

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
	Debug("realloc from %zd to %d\n", old, min);
	assert(old <= min);
	*(char**)ptr = realloc(*(char**)ptr, min * elem);
	assert(*(void**)ptr != NULL);
	memset(*(char**)ptr + old, 0, (min - old) * elem);
}

static void httpd_init(void)
{
}

void httpd_response(struct channel *ch, int status_code)
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
	Debug("%s:status_code=%d\n", ch->desc, status_code);
	channel_write(ch, resp, resp_len);
}

void httpd_header(struct channel *ch, const char *name, const char *value)
{
	channel_write(ch, name, strlen(name));
	channel_write(ch, ": ", 2);
	channel_write(ch, value, strlen(value));
	channel_write(ch, "\r\n", 2);
}

void httpd_end_headers(struct channel *ch)
{
	channel_write(ch, "\r\n", 2);
}

static void on_method(void *p, const char *method, const char *uri)
{
	struct httpchannel *hc = p;
	struct channel *ch = &hc->channel;

	snprintf(hc->method, sizeof(hc->method), "%s", method);
	snprintf(hc->uri, sizeof(hc->uri), "%s", uri);
}

static void on_header(void *p, const char *name, const char *value)
{
	struct httpchannel *hc = p;
	struct channel *ch = &hc->channel;

	env_set(&hc->headers, name, value);
}

static void on_header_done(void *p)
{
	struct httpchannel *hc = p;
	struct channel *ch = &hc->channel;
	const struct module *mod;
	const char *host;

	/* check host */
	host = env_get(&hc->headers, "Host");
	if (!host) {
		httpd_response(ch, 400);
		httpd_end_headers(ch);
		channel_done(ch);
		return;
	}
	// TODO: pass Host to service_start
	if (service_start(hc->method, host, hc->uri,
		&hc->module, &hc->app_data)) {
		Error("%s:could not find service or start module\n", ch->desc);
		httpd_response(ch, 404);
		// TODO: write headers
		httpd_end_headers(ch);
		channel_done(ch);
		return;
	}
	Info("%s:connected to service.\n", ch->desc);

	mod = hc->module;

	if (!mod || !mod->on_header_done) {
		Error("%s:could not find service or start module\n", ch->desc);
		httpd_response(ch, 501);
		httpd_end_headers(ch);
		channel_done(ch);
		return;
	}
	mod->on_header_done(ch, hc->app_data, &hc->headers);
}

static void on_data(void *p, size_t len, const void *data)
{
	// ignored
}

static void httpd_process(struct httpchannel *hc)
{
	struct channel *ch = &hc->channel;

	while (!ch->done && channel_fill(ch) > 0) {
		if (httpparser(&hc->hp, ch->buf, ch->buf_cur, hc, on_method,
			on_header, on_header_done, on_data)) {
			Info("%s:parse failure\n", ch->desc);
			httpd_response(ch, 500);
			httpd_end_headers(ch);
			break;
		}
		ch->buf_cur = 0; /* httpparser() consumes 100% of buffer */
	}
}

static void httpchannel_cleanup(struct httpchannel *hc)
{
	data_free(hc->app_data);
	hc->app_data = NULL;
	channel_close(&hc->channel);
}

static void worker_cleanup(void *p)
{
	struct httpchannel *hc = p;

	httpchannel_cleanup(hc);
}

static void httpchannel_init(struct httpchannel *hc, struct net_socket sock,
	const char *desc)
{
	memset(hc, 0, sizeof(*hc));
	httpparser_init(&hc->hp);
	channel_init(&hc->channel, sock, desc);
	env_init(&hc->headers);
}

static int server_accept(struct server *serv, struct httpchannel *hc)
{
	char desc[64];
	struct net_socket new_sock;

	if (net_accept(&serv->listen_handle, &new_sock, sizeof(desc), desc))
		return -1;
	httpchannel_init(hc, new_sock, desc);
	return 0;
}

static void *worker_start(void *p)
{
	struct worker *w = p;
	struct server *serv = w->server;
	struct httpchannel *hc = &w->httpchannel;

	signal(SIGPIPE, SIG_IGN);
	pthread_cleanup_push(worker_cleanup, hc);
	while (1) {
		pthread_testcancel();
		if (server_accept(serv, hc))
			return NULL;
		httpd_process(hc);
		Debug("%s:connection terminated\n", hc->channel.desc);
		httpchannel_cleanup(hc);
	}
	pthread_cleanup_pop(1);
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
		e = pthread_cancel(serv->thread_pool[i].th);
		if (e) {
			perror(serv->desc);
			break;
		}
	}
	grow(&serv->thread_pool, &serv->num_thread_pool, new_size,
		sizeof(*serv->thread_pool));
	/* add threads if we grow */
	for (i = old_size; i < new_size; i++) {
		struct worker *w = &serv->thread_pool[i];

		w->server = serv;
		e = pthread_create(&w->th, NULL, worker_start, w);
		if (e) {
			perror(serv->desc);
			/* the list came up short */
			serv->num_thread_pool = i;
			Warning("limiting thread pool to %d\n",
				serv->num_thread_pool);
			break;
		}
		e = pthread_detach(w->th);
		if (e) {
			perror(serv->desc);
			// TODO: handle the error
		}

	}

}

static void server_add_entry(struct net_listen listen_handle, const char *desc)
{
	struct server *serv;

	serv = calloc(1, sizeof(*serv));
	serv->listen_handle = listen_handle;
	serv->desc = strdup(desc);
	resize_thread_pool(serv, pool_size);
	serv->next = server_head;
	server_head = serv;
}

int httpd_poolsize(int newsize)
{
	pool_size = newsize;
	return 0;
}

int httpd_start(const char *node, const char *service)
{
	char desc[64];
	struct net_listen listen_handle;

	if (net_listen(&listen_handle, node, service, sizeof(desc), desc))
		return -1;
	server_add_entry(listen_handle, desc);
	return 0;
}

int httpd_loop(void)
{
	pthread_once(&httpd_init_once, httpd_init);
	if (server_head) {
		struct worker w;

		memset(&w, 0, sizeof(w));
		w.server = server_head;
		worker_start(&w); /* join the last thread pool */
	}
	return 0;
}
