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
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define HT_RESPONSE_200 "HTTP/1.1 200 OK\r\n"
#define HT_RESPONSE_400 "HTTP/1.1 400 Bad Request\r\n"

#define sys_error() fprintf(stderr, "%s():%d:%s\n", \
	__func__, __LINE__, strerror(errno));

#define ht_response(ch, status) ch_puts((ch), HT_RESPONSE_##status)

struct listen {
	int fd;
	struct listen *next;
};

struct channel {
	int fd;
	bool connected;
};

struct buffer {
	unsigned cur, max;
	char *data;
};

struct env {
	struct buffer data;
	char area[2048];
};

struct methodline_state {
	bool done;
	unsigned state;
};

struct headers_state {
	bool done;
	unsigned state;
};

struct work_info {
	pthread_t thr;
	struct listen *li;
	bool active;
};

static struct listen *listen_head;
static struct work_info **work;
static unsigned work_cur, work_max;

static void ch_init(struct channel *ch, int fd)
{
	ch->connected = true;
	ch->fd = fd;
}

static int net_listen(int *fd, struct addrinfo *ai)
{
	int res;
	int _fd;
	const int yes = 1;

	_fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
	if (_fd < 0) {
		sys_error();
		return -1;
	}
	/* disable IPv4-to-IPv6 mapping */
	if (ai->ai_family == AF_INET6)  {
		res = setsockopt(_fd, IPPROTO_IPV6, IPV6_V6ONLY,
			&yes, sizeof(yes));
		if (res)
			goto error_and_close;
	}
	res = setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
	if (res)
		goto error_and_close;
	res = bind(_fd, ai->ai_addr, ai->ai_addrlen);
	if (res)
		goto error_and_close;
	res = listen(_fd, 6);
	if (res)
		goto error_and_close;
	if (fd)
		*fd = _fd;
	return 0;
error_and_close:
	sys_error();
	close(_fd);
	return -1;
}

static int li_add(int fd)
{
	struct listen *li;

	li = calloc(1, sizeof(*li));
	if (!li) {
		sys_error();
		return -1;
	}

	li->fd = fd;
	li->next = listen_head;
	listen_head = li;

	return 0;
}

static int ht_listen(const char *node, const char *service)
{
	struct addrinfo hints;
	struct addrinfo *res, *curr;
	int e;
	int ret = 0;

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_UNSPEC;
	hints.ai_protocol = 0;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_next = NULL;

	e = getaddrinfo(node, service, &hints, &res);
	if (e) {
		fprintf(stderr, "%s():%s\n", __func__, gai_strerror(e));
		return -1;
	}

	for (curr = res; curr; curr = curr->ai_next) {
		int fd;
		char host[40], serv[16];

		/* currently we only support IPv4 and IPv6 */
		if (curr->ai_family != AF_INET && curr->ai_family != AF_INET6)
			continue;

		getnameinfo(curr->ai_addr, curr->ai_addrlen, host, sizeof(host),
			serv, sizeof(serv), NI_NUMERICHOST | NI_NUMERICSERV);

		if (net_listen(&fd, curr)) {
			fprintf(stderr, "%s():failed to add %s:%s\n", __func__,
				host, serv);
			ret = -1;
		} else if (li_add(fd)) {
			close(fd);
		} else {
			fprintf(stderr, "%s():listen on %s:%s\n", __func__,
				host, serv);
		}
	}

	freeaddrinfo(res);

	return ret;
}

static int ch_puts(struct channel *ch, const char *str)
{
	size_t cur = 0, len = strlen(str);
	ssize_t res;

	if (!ch->connected)
		return -1;
	do {
		res = send(ch->fd, str + cur, len - cur, 0);
		if (res < 0) {
			sys_error();
			break;
		}
		assert(res != 0);
		cur += res;
	} while(cur < len);
	return len;
}

static int ch_printf(struct channel *ch, const char *fmt, ...)
{
	va_list ap;
	int res;

	if (!ch->connected)
		return -1;
	va_start(ap, fmt);
	res = vdprintf(ch->fd, fmt, ap);
	va_end(ap);
	return res;
}

static int net_accept(struct listen *li, struct channel *ch)
{
	struct sockaddr_storage ss;
	socklen_t ss_len = sizeof(ss);
	int newfd;

	do {
		newfd = accept(li->fd, (struct sockaddr*)&ss, &ss_len);
	} while (newfd < 0 && errno == EINTR);
	if (newfd < 0) {
		sys_error();
		return -1;
	}
	ch_init(ch, newfd);
	return 0;
}

static void ch_close(struct channel *ch)
{
	if (!ch->connected)
		return;
	assert(ch->fd != -1);
	close(ch->fd);
	ch->fd = -1;
	ch->connected = false;
}

#define buffer_wrap(str) { 0, sizeof(str), (str) }

static void buffer_reset(struct buffer *bu)
{
	bu->cur = 0;
	bu->data[0] = 0;
}

static void buffer_init(struct buffer *bu, char *data, size_t max)
{
	assert(data != NULL && max > 0);
	bu->data = data;
	bu->max = max;
	buffer_reset(bu);
}

static void buffer_consume(struct buffer *bu, unsigned count)
{
	assert(count <= bu->cur);
	if (count <= bu->cur)
		memmove(bu->data, bu->data + count, bu->cur - count);
	bu->cur -= count;
}

static void env_init(struct env *env)
{
	memset(env->area, 'X', sizeof(env->area));
	buffer_init(&env->data, env->area, sizeof(env->area));
}

static int env_find(const struct env *env, const char *name,
	const char **n, const char **v)
{
	const struct buffer *bu = &env->data;
	unsigned rem, len;
	const char *s;

	for (rem = bu->cur, s = bu->data; rem > 0; rem -= len, s += len) {
		assert(rem != 0);
		len = strlen(s) + 1;
		assert(s[len - 1] == 0);
		assert(len <= rem);
		if (!strcasecmp(name, s)) {
			if (n)
				*n = s;
			if (v)
				*v = s + len;
			return 0;
		}
	}
	return -1; /* not found */
}

static void env_del(struct env *env, const char *name)
{
	struct buffer *bu = &env->data;
	const char *n, *v;
	unsigned nlen, vlen;

	if (env_find(env, name, &n, &v))
		return;
	nlen = strlen(n) + 1;
	vlen = strlen(v) + 1;
	/* cheat to turn a const into non-const */
	memmove(bu->data + (n - bu->data), v + vlen, nlen + vlen);
}

static const char *env_get(struct env *env, const char *name)
{
	const char *v;

	if (!env_find(env, name, NULL, &v))
		return v;
	return NULL;
}

static int env_set(struct env *env, const char *n, const char *v)
{
	struct buffer *bu = &env->data;
	unsigned nlen, vlen;
	char *s;

	env_del(env, n);

	nlen = strlen(n) + 1;
	vlen = strlen(v) + 1;
	if (bu->cur + nlen + vlen > bu->max)
		return -1;
	s = bu->data + bu->cur;

	memcpy(s, n, nlen);
	s += nlen;
	assert(s[-1] == 0);

	memcpy(s, v, vlen);
	s += vlen;
	assert(s[-1] == 0);

	bu->cur = s - bu->data;
	assert(bu->cur <= bu->max);
	return 0;
}

static int fill(struct channel *ch, struct buffer *bu)
{
	ssize_t res;

	if (!ch->connected)
		return -1;
	if (bu->max == bu->cur)
		return -1;
	if (bu->cur)
		return 0;

	do {
		assert(ch->fd > 0);
		assert(bu->cur < bu->max);
		res = recv(ch->fd, bu->data + bu->cur, bu->max - bu->cur, 0);
	} while (res < 0 && errno == EINTR);

	if (res <= 0) {
		if (res < 0)
			sys_error();
		ch_close(ch);
		return -1;
	}

	bu->cur += res;
	return 0;
}

static void ms_init(struct methodline_state *ms)
{
	memset(ms, 0, sizeof(*ms));
	ms->done = false;
}

static void hs_init(struct headers_state *hs)
{
	memset(hs, 0, sizeof(*hs));
	hs->done = false;
}

static void append(struct buffer *bu, char ch)
{
	if (bu->cur + 1 >= bu->max)
		return;
	bu->data[bu->cur++] = ch;
	bu->data[bu->cur] = 0;
}

static int methodline_parse(struct buffer *bu, struct methodline_state *ms,
	struct buffer *method, struct buffer *uri, struct buffer *version)
{
	const char *s = bu->data;
	unsigned rem = bu->cur;
	char ch;

	while (rem > 0) {
		ch = *s;
		s++;
		rem--;

		switch (ms->state) {
		case 0: /* method */
			if (ch == ' ')
				ms->state = 1;
			else
				append(method, ch);
			break;
		case 1: /* uri */
			if (ch == ' ')
				ms->state = 2;
			else
				append(uri, ch);
			break;
		case 2: /* version */
			if (ch == '\r')
				ms->state = 3;
			else if (isspace(ch))
				goto parse_error;
			else
				append(version, ch);
			break;
		case 3: /* CR */
			if (ch == '\n')
				goto complete;
			else
				goto parse_error;
			break;
		default:
			assert(0);
		}
	}

	buffer_consume(bu, s - bu->data);
	return 0;
complete:
	ms->done = true;
	buffer_consume(bu, s - bu->data);
	return 0;
parse_error:
	fprintf(stderr, "%s():parse failure (%.*s)\n", __func__, bu->cur, bu->data);
	buffer_consume(bu, s - bu->data);
	fprintf(stderr, "%s():parse failure @ %d (state=%d)\n", __func__, bu->cur, ms->state);
	return -1;
}

static int ht_method(struct channel *ch, struct buffer *bu,
	size_t method_len, char *method_str, size_t uri_len, char *uri_str,
	size_t version_len, char *version_str)
{
	struct methodline_state ms;
	struct buffer method, uri, version;

	buffer_init(&method, method_str, method_len);
	buffer_init(&uri, uri_str, uri_len);
	buffer_init(&version, version_str, version_len);
	ms_init(&ms);
	while (!fill(ch, bu)) {
		if (methodline_parse(bu, &ms, &method, &uri, &version)) {
			ch_close(ch);
			return -1;
		}
		if (ms.done)
			break;
	}
	return 0;
}

static int headers_parse(struct buffer *bu, struct headers_state *hs,
	struct buffer *name, struct buffer *value, struct env *env)
{
	const char *s = bu->data;
	unsigned rem = bu->cur;
	char ch;

	// TODO: 413 Entity Too Large
	while (rem > 0) {
		ch = *s;
		s++;
		rem--;

		switch (hs->state) {
		case 0: /* first char */
			if (ch == '\r') {
				if (name->cur)
					env_set(env, name->data, value->data);
				hs->state = 6; /* CR/LF - end of headers */
			} else if (ch == ' ' || ch == '\t') { /* indented continuation line */
				// TODO: this is completely untested .. device a test for it!
				// TODO: insert single SP for these leading LWS
				hs->state = 4; /* append to existing value data */
			} else {
				if (name->cur)
					env_set(env, name->data, value->data);
				buffer_reset(name);
				buffer_reset(value);
				append(name, ch);
				hs->state = 11;
			}
			break;
		case 11: /* name */
			if (ch == ' ' || ch == '\t') { /* LWS after name */
				hs->state = 1;
			} else if (ch == ':') {
				hs->state = 2;
			} else if (isspace(ch)) {
				goto parse_error;
			} else {
				append(name, ch);
			}
			break;
		case 1: /* LWS after name */
			if (ch == ' ' || ch == '\t')
				; /* ignore */
			else if (ch == ':')
				hs->state = 2;
			else
				goto parse_error;
			break;
		case 2: /* ':' */
			if (ch == ' ' || ch == '\t') {
				hs->state = 3;
			} else {
				append(value, ch);
				hs->state = 4;
			}
			break;
		case 3: /* LWS before value */
			if (ch == ' ' || ch == '\t') {
				/* ignore */
			} else if (isspace(ch)) {
				goto parse_error;
			} else {
				append(value, ch);
				hs->state = 4;
			}
			break;
		case 4: /* value */
			if (ch == '\r')
				hs->state = 5;
			else
				append(value, ch);
			break;
		case 5: /* CR after value */
			if (ch == '\n')
				hs->state = 0;
			else
				goto parse_error;
			break;
		case 6: /* CR on first column */
			if (ch == '\n')
				goto complete;
			else
				goto parse_error;
			break;
		default:
			assert(0);
		}
	}

	buffer_consume(bu, s - bu->data);
	return 0;
complete:
	hs->done = true;
	buffer_consume(bu, s - bu->data);
	return 0;
parse_error:
	fprintf(stderr, "%s():parse failure (%.*s)\n", __func__, bu->cur, bu->data);
	buffer_consume(bu, s - bu->data);
	fprintf(stderr, "%s():parse failure @ %d (ch=%c state=%d)\n", __func__, bu->cur, ch, hs->state);
	return -1;
}


static int ht_headers(struct channel *ch, struct buffer *bu, struct env *env)
{
	struct headers_state hs;
	char name_str[80];
	char value_str[2048];
	struct buffer name = buffer_wrap(name_str);
	struct buffer value = buffer_wrap(value_str);

	buffer_reset(&name);
	buffer_reset(&value);
	env_init(env);
	hs_init(&hs);
	while (!fill(ch, bu)) {
		if (headers_parse(bu, &hs, &name, &value, env)) {
			ch_close(ch);
			return -1;
		}
		if (hs.done)
			break;
	}
	return 0;
}

static int ht_process(struct channel *ch)
{
	char buf[4096];
	struct buffer bu = buffer_wrap(buf);
	struct env env;
	char method[12];
	char uri[512];
	char version[10];
	const char *host;

	if (ht_method(ch, &bu, sizeof(method), method, sizeof(uri), uri,
		sizeof(version), version))
		return -1;
	if (strcasecmp(version, "HTTP/1.1")) /* require HTTP/1.1 */
		return -1;
	if (ht_headers(ch, &bu, &env))
		return -1;

	host = env_get(&env, "Host");
	if (!host) {
		ht_response(ch, 400);
		ch_puts(ch, "Content-Type: text/plain\r\n");
		ch_puts(ch, "Connection: close\r\n");
		ch_puts(ch, "\r\n");
		ch_puts(ch, "require Host in HTTP/1.1 headers.\n");
		return 0;
	}

	printf("method=%s\n", method);
	printf("uri=%s\n", uri);
	printf("version=%s\n", version);
	printf("Host=%s\n", host);

	ht_response(ch, 200);
	/* send some basic headers */
	ch_puts(ch, "Content-Type: text/plain\r\n");
	ch_puts(ch, "Connection: close\r\n");
	ch_puts(ch, "\r\n");
	// TODO: output a real response
	ch_puts(ch, "Hello World!\n");
	ch_printf(ch, "\n\turi=%s\n\thost=%s\n", uri, host);
	return 0;
}

static void *worker_loop(void *p)
{
	struct work_info *wi = p;
	struct channel ch;

	while (wi->active) {
		if (net_accept(wi->li, &ch))
			break;
		ht_process(&ch);
		ch_close(&ch);
	}
	return NULL;
}

static void wi_grow(unsigned count)
{
	unsigned new_cur = work_cur + count;

	if (new_cur > work_max) {
		if (!work_max)
			work_max = 1;
		while (work_max < new_cur)
			work_max *= 2;
		work = realloc(work, sizeof(*work) * work_max);
	}
	if (work_cur < new_cur) {
		memset(work + work_cur, 0,
			sizeof(*work) * (new_cur - work_cur));
	}
}

static void wi_start(unsigned count)
{
	unsigned i;
	struct listen *curr;
	int e;

	for (curr = listen_head; curr; curr = curr->next) {
		wi_grow(count);
		for (i = 0; i < count; i++) {
			struct work_info *wi;

			wi = calloc(1, sizeof(*wi));
			wi->active = true;
			wi->li = curr;
			e = pthread_create(&wi->thr, NULL, worker_loop, wi);
			if (e) {
				sys_error();
				free(wi);
				break;
			}
			e = pthread_detach(wi->thr);
			if (e) {
				sys_error();
				/* TODO: handle this error */
			}
			wi = NULL;
			work[work_cur++] = wi;
		}
	}
	printf("thread count at %d\n", work_cur);
}

static void main_loop(void)
{
	while (1) {
		sigset_t set;
		siginfo_t info;
		int s;

		/* TODO: handle HUP, INT, USR1, USR2, ... */
		sigemptyset(&set);
		s = sigwaitinfo(&set, &info);
		if (s < 0) {
			sys_error();
			break;
		}
		printf("SIGNAL %d\n", s);
	}

}

int main()
{
	if (ht_listen(NULL, "8080"))
		return 1;
	wi_start(100);

	main_loop();
	return 0;
}
