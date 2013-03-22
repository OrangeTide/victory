/* psserver.c : publish-subscribe message server */
/*
 * Copyright (c) 2013 Jon Mayo <jon@rm-f.net>
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <fcntl.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#include <ev.h>

#include "httpparser.h"
#include "net.h"
#include "container_of.h"
#include "logger.h"

#define PSSERVER_PORT "8088"

/* http server connection */
struct ht_conn {
	struct net_socket sock;
	ev_io io;
};

/* server listen data structure */
struct li {
	struct net_listen sock;
	ev_io io;
};

static void ht_close(EV_P_ struct ht_conn *ht)
{
	if (ht->sock.fd < 0)
		return;
	ev_io_stop(EV_A_ &ht->io);
	close(ht->sock.fd);
	ht->sock.fd = -1;
}

/* HTTP connection callback */
static void ht_cb(EV_P_ ev_io *w, int revents)
{
	struct ht_conn *ht = container_of(w, struct ht_conn, io);
	char buf[4];
	ssize_t len;

	Debug("Reading from socket...\n");
	len = recv(ht->sock.fd, buf, sizeof(buf), 0);
	if (len < 0) {
		Error("recv():%s\n", strerror(errno));
		ht_close(EV_A_ ht);
		return;
	} else if (len == 0) {
		Debug("Connection close\n");
		ht_close(EV_A_ ht);
		return;
	}

	// TODO: process some data
	ht_close(EV_A_ ht);
}

static struct ht_conn *ht_new(struct net_socket sock)
{
	struct ht_conn *ht;

	ht = calloc(1, sizeof(*ht));
	ht->sock = sock;
	ev_io_init(&ht->io, ht_cb, ht->sock.fd, EV_READ);
	return ht;
}

/* listener callback */
static void li_cb(EV_P_ ev_io *w, int revents)
{
	struct li *li = container_of(w, struct li, io);
	struct ht_conn *ht;
	struct net_socket sock;
	char desc[40];

	while (1) {
		Debug("fd=%d\n", li->sock.fd);
		net_accept(&li->sock, &sock, sizeof(desc), desc);
		ht = ht_new(sock);
		ev_io_start(EV_A_ &ht->io);
	}
}

static void _li_create(void *p, struct net_listen sock, size_t desc_len, const char *desc)
{
	struct li *li;
#if EV_MULTIPLICITY
	EV_P = p;
#endif

	li = calloc(1, sizeof(*li));
	li->sock = sock;
	ev_io_init(&li->io, li_cb, li->sock.fd, EV_READ);
	ev_io_start(EV_A_ &li->io);
	Info("Server created: %s\n", desc);
}

static void li_listen(EV_P_ const char *node, const char *service)
{
#if EV_MULTIPLICITY
	void *p = EV_A;
#else
	void *p = NULL;
#endif
	net_listen(_li_create, p, node, service);

}

int main()
{
	struct li *li;

	EV_P = ev_default_loop(0);
	li_listen(EV_A_ NULL, PSSERVER_PORT);
	ev_loop(EV_A_ 0);
	// TODO: close all servers
	return 0;
}
