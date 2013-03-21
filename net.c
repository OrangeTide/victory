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
#include <stdio.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include "logger.h"
#include "net.h"

static void make_name(char *buf, size_t buflen,
	const struct sockaddr *sa, socklen_t salen)
{
	char hostbuf[64], servbuf[32];

	getnameinfo(sa, salen, hostbuf, sizeof(hostbuf), servbuf,
		sizeof(servbuf), NI_NUMERICHOST | NI_NUMERICSERV);
	snprintf(buf, buflen, "%s:%s", hostbuf, servbuf);
	// TODO: return a value
}

int net_listen(void (*create_server)(void *p, struct net_listen sock,
	size_t desc_len, const char *desc), void *p,
	const char *node, const char *service)
{
	struct addrinfo hints = {
		.ai_flags = AI_PASSIVE,
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
		.ai_protocol = 0,
		.ai_next = NULL,
	};
	struct addrinfo *res, *cur;
	int e;
	struct net_listen sock;
	char desc[40];

	if (!create_server) {
		Error("create_server callback is NULL.\n");
		return -1;
	}
	Debug("listen (%s:%s)\n", node, service);
	e = getaddrinfo(node, service, &hints, &res);
	if (e) {
		Error("%s (%s:%s)\n", gai_strerror(e), node, service);
		return -1;
	}
	Debug("res=%p\n", res);
	for (cur = res; cur; cur = cur->ai_next) {
		int fd;
		const int yes = 1;

		make_name(desc, sizeof(desc),
			cur->ai_addr, cur->ai_addrlen);
		Debug("cur=%p (name=%s family=%d socktype=%d proto=%d flags=%x addrlen=%ld)\n",
			cur, desc, cur->ai_family, cur->ai_socktype, cur->ai_protocol,
			cur->ai_flags, (long)cur->ai_addrlen);
		fd = socket(cur->ai_family, cur->ai_socktype, cur->ai_protocol);
		if (fd < 0) {
			Error("socket():%s (%s:%s)\n", strerror(errno),
				node, service);
			goto fail_and_free;
		}
		fcntl(fd, F_SETFD, FD_CLOEXEC); /* this is a race */
		e = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
		if (e) {
			Error("SO_REUSEADDR:%s (%s:%s)\n", strerror(errno),
				node, service);
			goto fail_and_free;
		}
		e = bind(fd, cur->ai_addr, cur->ai_addrlen);
		if (e) {
			Error("bind():%s (%s:%s)\n", strerror(errno),
				node, service);
			goto fail_and_free;
		}
		e = listen(fd, SOMAXCONN);
		if (e) {
			Error("listen():%s (%s:%s)\n", strerror(errno),
				node, service);
			goto fail_and_free;
		}
		sock.fd = fd;
		Debug("create server... %s\n", desc);
		create_server(p, sock, strlen(desc), desc);
	}
	freeaddrinfo(res);
	return 0;
fail_and_free:
	freeaddrinfo(res);
	return -1;
}

int net_accept(struct net_listen *listen_handle, struct net_socket *socket,
	size_t desc_len, char *desc)
{
	struct sockaddr_storage addr;
	socklen_t addrlen = sizeof(addr);
	int newfd;

	do {
		newfd = accept(listen_handle->fd,
			(struct sockaddr*)&addr, &addrlen);
		pthread_testcancel();
	} while (newfd < 0 && errno == EINTR);
	if (newfd < 0) {
		perror("accept()");
		return -1;
	}
	fcntl(newfd, F_SETFD, FD_CLOEXEC); /* a race, but better than nothing */

	if (desc)
		make_name(desc, desc_len, (struct sockaddr*)&addr, addrlen);
	socket->fd = newfd;
	return 0;
}

