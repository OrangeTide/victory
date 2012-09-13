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

int net_listen(struct net_listen *handle, const char *node, const char *service,
	size_t desc_len, char *desc)
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

	e = getaddrinfo(node, service, &hints, &res);
	if (e) {
		Error("%s\n", gai_strerror(e));
		return -1;
	}
	for (cur = res; cur; cur = cur->ai_next) {
		int fd;
		const int yes = 1;

		fd = socket(cur->ai_family, cur->ai_socktype, cur->ai_protocol);
		if (fd < 0) {
			perror("socket()");
			goto fail_and_free;
		}
		fcntl(fd, F_SETFD, FD_CLOEXEC); /* this is a race */
		e = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
		if (e) {
			perror("SO_REUSEADDR");
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
		if (desc)
			make_name(desc, desc_len,
				cur->ai_addr, cur->ai_addrlen);
		handle->fd = fd;
		break; // TODO: repeat for each entry we have found
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
	} while (newfd >= 0 || errno == EINTR);
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

