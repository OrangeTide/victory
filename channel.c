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
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/select.h>
#include "container_of.h"
#include "channel.h"

struct server {
	void (*client_on_read)(struct channel_entry *ch); // TODO: optoinally handle as part of client_alloc?
	struct channel_entry *(*client_alloc)(void);
	void (*client_free)(struct channel_entry *ch);
	struct channel_entry channel;
};

static struct channel_entry **channels;
static unsigned max_channels;
static fd_set channel_fdset;

/******/

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

/******/

static void channel_ready(struct channel_entry *ch)
{
	assert(ch != NULL);
	FD_SET(ch->fd, &channel_fdset);
}

static void channel_not_ready(struct channel_entry *ch)
{
	assert(ch != NULL);
	FD_CLR(ch->fd, &channel_fdset);
}

static void channel_add_entry(int fd, struct channel_entry *ch)
{
	assert(ch != NULL);
	assert(fd >= 0);
	fprintf(stderr, "%s():fd=%d ch=%p desc=\"%s\"\n",
		__func__, fd, ch, ch->desc);
	grow(&channels, &max_channels, fd + 1, sizeof(*channels));
	assert(channels[fd] == NULL); /* must not be used */
	channels[fd] = ch;
	ch->fd = fd;
}

static void channel_remove_entry(struct channel_entry *ch)
{
	int fd = ch->fd;

	channel_not_ready(ch);
	channels[fd] = NULL;
	if (ch->free)
		ch->free(ch);
	else
		free(ch);
}

void channel_close(struct channel_entry *ch)
{
	int fd = ch->fd;

	close(fd);
	channel_remove_entry(ch);
}

static void make_name(char *buf, size_t buflen, const struct sockaddr *sa, socklen_t salen)
{
	char hostbuf[64], servbuf[32];

	getnameinfo(sa, salen, hostbuf, sizeof(hostbuf), servbuf, sizeof(servbuf), NI_NUMERICHOST | NI_NUMERICSERV);
	snprintf(buf, buflen, "%s:%s", hostbuf, servbuf);
	// TODO: return a value
}

static void channel_freedata(struct channel_entry *ch)
{
	if (!ch)
		return;
	free(ch->desc);
}

static void server_free(struct channel_entry *ch)
{
	struct server *serv = container_of(ch, struct server, channel);

	channel_freedata(ch);
	free(serv);
}

static void server_on_read(struct channel_entry *ch)
{
	struct server *serv = container_of(ch, struct server, channel);
	int fd = ch->fd;
	struct sockaddr_storage addr;
	socklen_t addrlen = sizeof(addr);
	int newfd;
	char desc[64];
	struct channel_entry *newch;

	newfd = accept(fd, (struct sockaddr*)&addr, &addrlen);
	if (newfd < 0) {
		perror("accept()");
		return;
	}

	make_name(desc, sizeof(desc), (struct sockaddr*)&addr, addrlen);
	if (serv->client_alloc)
		newch = serv->client_alloc();
	else
		newch = calloc(1, sizeof(newch));
	newch->desc = strdup(desc);
	newch->on_read = serv->client_on_read;
	newch->free = serv->client_free;
	channel_add_entry(newfd, newch);
	channel_ready(newch);
}

int channel_server_start(const char *node, const char *service,
	void (*client_on_read)(struct channel_entry *ch),
	struct channel_entry *(*client_alloc)(void),
	void (*client_free)(struct channel_entry *ch))
{
	struct addrinfo hints = {
		.ai_flags = AI_NUMERICHOST | AI_PASSIVE,
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
		.ai_protocol = 0, // TODO
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
		struct server *serv;
		struct channel_entry *ch;

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
		e = listen(fd, 6);
		make_name(desc, sizeof(desc), cur->ai_addr, cur->ai_addrlen);
		serv = calloc(1, sizeof(*serv));
		serv->client_on_read = client_on_read;
		serv->client_alloc = client_alloc;
		serv->client_free = client_free;
		ch = &serv->channel;
		ch->desc = strdup(desc);
		ch->on_read = server_on_read;
		ch->free = server_free;
		channel_add_entry(fd, ch);
		channel_ready(ch);
	}
	freeaddrinfo(res);
	return 0;
fail_and_free:
	freeaddrinfo(res);
	return -1;
}

void channel_loop(void)
{
	fd_set rfds;
	int cnt;
	unsigned i;

	while (1) {
		rfds = channel_fdset;
		cnt = select(max_channels, &rfds, NULL, NULL, NULL);
		if (cnt < 0) {
			perror("select()");
			break;
		}

		for (i = 0; cnt && i < max_channels; i++) {
			struct channel_entry *ch = channels[i];

			if (ch && FD_ISSET(i, &rfds)) {
				ch->on_read(ch);
			}
			if (FD_ISSET(i, &rfds))
				cnt--;
		}

		if (cnt)
			fprintf(stderr, "items pending...\n");
	}
}
