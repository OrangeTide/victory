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
#define HT_RESPONSE_403 "HTTP/1.1 403 Forbidden\r\n"
#define HT_RESPONSE_404 "HTTP/1.1 404 Not Found\r\n"

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

struct module {
	char *name;
	// TODO: implement this
	struct module *next;
};

struct service {
	char *prefix;
	struct module *module;
	struct service *next;
};

/* domain is an internal identifier for one or more interfaces */
struct domain {
	char *name;
	struct service *service_head;
};

struct host_alias {
	char *host;
	struct domain *domain;
	struct host_alias *next;
};

enum csv_state {
	CSV_START,
	CSV_ESCAPED,
	CSV_ESCAPED_DQUOTE,
	CSV_UNESCAPED,
	CSV_NEWLINE,
	CSV_ERROR,
};

struct csv_parser {
	enum csv_state s;
	unsigned row, col;
	struct buffer buf;
	void *p;
	int (*field)(void *p, unsigned row, unsigned col, size_t len, const char *str);
	int (*row_end)(void *p, unsigned row);
};

static struct listen *listen_head;
static struct work_info **work;
static unsigned work_cur, work_max;
static struct host_alias *host_alias_head;
static struct module *module_head;

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

static int buffer_addch(struct buffer *bu, char ch)
{
	if (bu->cur + 1 >= bu->max)
		return -1;
	bu->data[bu->cur++] = ch;
	bu->data[bu->cur] = 0;
	return 0;
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
				buffer_addch(method, ch);
			break;
		case 1: /* uri */
			if (ch == ' ')
				ms->state = 2;
			else
				buffer_addch(uri, ch);
			break;
		case 2: /* version */
			if (ch == '\r')
				ms->state = 3;
			else if (isspace(ch))
				goto parse_error;
			else
				buffer_addch(version, ch);
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
				buffer_addch(name, ch);
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
				buffer_addch(name, ch);
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
				buffer_addch(value, ch);
				hs->state = 4;
			}
			break;
		case 3: /* LWS before value */
			if (ch == ' ' || ch == '\t') {
				/* ignore */
			} else if (isspace(ch)) {
				goto parse_error;
			} else {
				buffer_addch(value, ch);
				hs->state = 4;
			}
			break;
		case 4: /* value */
			if (ch == '\r')
				hs->state = 5;
			else
				buffer_addch(value, ch);
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

static int ha_add(struct domain *dom, const char *alias)
{
	struct host_alias *ha;

	ha = calloc(1, sizeof(*ha));
	if (!ha) {
		sys_error();
		return -1;
	}

	ha->host = strdup(alias);
	if (!ha->host) {
		sys_error();
		free(ha);
		return -1;
	}
	ha->next = host_alias_head;
	ha->domain = dom;
	host_alias_head = ha;

	return 0;
}

/* find a hosting domain by a host alias. */
static struct domain *ha_find_dom(const char *host_alias)
{
	struct host_alias *cu;

	for (cu = host_alias_head; cu; cu = cu->next) {
		if (!strcasecmp(host_alias, cu->host)) {
			return cu->domain;
		}
	}
	return NULL;
}

static struct domain *dom_find(const char *name)
{
	struct host_alias *cu;
	/* must walk the host aliases to find all the hosting domains.
	 * there will be many duplicate comparisons this way. */

	for (cu = host_alias_head; cu; cu = cu->next) {
		if (!strcasecmp(name, cu->domain->name)) {
			return cu->domain;
		}
	}
	return NULL;
}

/* create a hosting domain, if it does not already exist */
static struct domain *dom_create(const char *name)
{
	struct domain *dom;

	assert(name != NULL);
	dom = dom_find(name);
	if (dom)
		return dom;
	dom = calloc(1, sizeof(*dom));
	if (!dom) {
		sys_error();
		return NULL;
	}
	dom->name = strdup(name);
	if (!dom->name) {
		sys_error();
		free(dom);
		return NULL;
	}
	dom->service_head = NULL;
	return dom;
}

static struct module *mo_find(const char *module)
{
	struct module *cu;

	for (cu = module_head; cu; cu = cu->next) {
		if (!strcasecmp(module, cu->name)) {
			return cu;
		}
	}
	return NULL;
}

/* match a subpath. */
static int match_subpath(const char *prefix, const char *path, const char **sub)
{
	char prev = 0;
	while (*prefix && *prefix == *path) {
		prefix++;
		prev = *path++;
	}
	if (sub)
		*sub = path;
	return *prefix == 0 && ( *path == 0 || prev == '/');
}

static struct service *se_find(struct domain *dom,
	const char *path, const char **sub)
{
	struct service *cu;

	for (cu = dom->service_head; cu; cu = cu->next) {
		if (match_subpath(cu->prefix, path, sub)) {
			return cu;
		}
	}
	return NULL;
}

static int se_add(struct domain *dom, const char *prefix, const char *module)
{
	struct service *se;

	se = calloc(1, sizeof(*se));
	if (!se) {
		sys_error();
		return -1;
	}

	se->prefix = strdup(prefix);
	if (!se->prefix) {
		sys_error();
		free(se);
		return -1;
	}
	se->module = mo_find(module);
	se->next = dom->service_head;
	dom->service_head = se;

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
	struct service *se;
	struct module *mo;
	struct domain *dom;
	const char *sub;

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

	dom = ha_find_dom(host);
	if (!dom) {
		ht_response(ch, 403);
		ch_puts(ch, "Content-Type: text/plain\r\n");
		ch_puts(ch, "Connection: close\r\n");
		ch_puts(ch, "\r\n");
		ch_printf(ch, "Host %s is unavailable.\n\n", host);
		return 0;
	}

	printf("method=%s\n", method);
	printf("uri=%s\n", uri);
	printf("version=%s\n", version);
	printf("Host=%s (%s)\n", host, dom->name);

	se = se_find(dom, uri, &sub);
	if (!se) {
		ht_response(ch, 404);
		ch_puts(ch, "Content-Type: text/plain\r\n");
		ch_puts(ch, "Connection: close\r\n");
		ch_puts(ch, "\r\n");
		ch_printf(ch, "Request-URI %s is unavailable.\n\n", uri);
		ch_printf(ch, "dom=%s\n", dom->name);
		ch_printf(ch, "host=%s\n", host);
		return 0;
	}

	mo = se->module;

	ht_response(ch, 200);
	/* send some basic headers */
	ch_puts(ch, "Content-Type: text/plain\r\n");
	ch_puts(ch, "Connection: close\r\n");
	ch_puts(ch, "\r\n");
	// TODO: output a real response
	ch_puts(ch, "Hello World!\n");
	ch_printf(ch, "\n\turi=%s\n\thost=%s\n\tmodule=%s\n\tsubpath=%s\n",
		uri, dom->name, mo ? mo->name : "(none)", sub);
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

static void _csv_next_field(struct csv_parser *cp)
{
	if (cp->field) {
		if (cp->field(cp->p, cp->row, cp->col, cp->buf.cur, cp->buf.data)) {
			cp->s = CSV_ERROR;
			return;
		}
	}
	cp->s = CSV_START;
	cp->col++;
	buffer_reset(&cp->buf);
}

static void _csv_next_row(struct csv_parser *cp)
{
	_csv_next_field(cp);
	if (cp->row_end)
		if (cp->row_end(cp->p, cp->row))
			cp->s = CSV_ERROR;
	cp->col = 0;
	cp->row++;
}

static int _csv_escaped_dquote(struct csv_parser *cp)
{
	if (buffer_addch(&cp->buf, '"'))
		return -1;
	cp->s = CSV_ESCAPED;
	return 0;
}

static int csv_init(struct csv_parser *cp, char *fieldbuf, size_t fieldbuf_max,
	void *p,
	int (*field)(void *p, unsigned row, unsigned col, size_t len, const char *str),
	int (*row_end)(void *p, unsigned row))
{
	if (!cp)
		return -1;
	memset(cp, 0, sizeof(*cp));
	buffer_init(&cp->buf, fieldbuf, fieldbuf_max);
	cp->row = 0;
	cp->col = 0;
	cp->s = CSV_START;
	cp->p = p;
	cp->field = field;
	cp->row_end = row_end;
	return 0;
}

static int csv(struct csv_parser *cp, const char *buf, size_t len)
{
	while (len > 0) {
		char ch = *buf;

		buf++;
		len--;
		switch (cp->s) {
		case CSV_START:
			if (ch == '\r')
				cp->s = CSV_NEWLINE;
			else if (ch == '\n')
				_csv_next_row(cp);
			else if (ch == '"')
				cp->s = CSV_ESCAPED;
			else if (ch == ',')
				_csv_next_field(cp);
			else
				goto unescaped;
			break;
		case CSV_ESCAPED:
			if (ch == '"')
				cp->s = CSV_ESCAPED_DQUOTE;
			else if (buffer_addch(&cp->buf, ch))
				return -1;
			break;
		case CSV_ESCAPED_DQUOTE:
			if (ch == '\r')
				cp->s = CSV_NEWLINE;
			else if (ch == '\n')
				_csv_next_row(cp);
			else if (ch == '"') {
				if (_csv_escaped_dquote(cp))
					return -1;
			} else if (ch == ',')
				_csv_next_field(cp);
			else
				goto unescaped; /* deviates from RFC 4180 */
			break;
unescaped:
		cp->s = CSV_UNESCAPED;
		case CSV_UNESCAPED:
			if (ch == '\r')
				cp->s = CSV_NEWLINE;
			else if (ch == '\n')
				_csv_next_row(cp);
			else if (ch == '"')
				cp->s = CSV_ESCAPED; /* deviates from RFC 4180 */
			else if (ch == ',')
				_csv_next_field(cp);
			else if (buffer_addch(&cp->buf, ch))
				return -1;
			break;
		case CSV_NEWLINE:
			if (ch == '\n')
				_csv_next_row(cp);
			else
				return -1;
		case CSV_ERROR:
			return -1;
		}
	}
	return cp->s == CSV_ERROR ? -1 : 0;
}

static int csv_eof(struct csv_parser *cp)
{
	switch (cp->s) {
	case CSV_START:
		return 0;
	case CSV_UNESCAPED:
		_csv_next_field(cp);
		return 0;
	case CSV_ESCAPED:
	case CSV_ESCAPED_DQUOTE:
	case CSV_NEWLINE:
	case CSV_ERROR:
		return -1;
	}
	return -1;
}

static int csv_load(const char *filename, void *p,
	int (*field)(void *p, unsigned row, unsigned col, size_t len, const char *str),
	int (*row_end)(void *p, unsigned row))
{
	char inbuf[1024];
	char fieldbuf[4096];
	int len;
	struct csv_parser cp;
	FILE *f;

	f = fopen(filename, "r");
	if (!f) {
		perror(filename);
		return -1;
	}

	if (csv_init(&cp, fieldbuf, sizeof(fieldbuf), p, field, row_end))
		goto close_file;
	do {
		len = fread(inbuf, 1, sizeof(inbuf), f);
		if (ferror(f)) {
			perror(filename);
			goto close_file;
		}
		if (csv(&cp, inbuf, len)) {
			fprintf(stderr, "%s:parse error (row=%d col=%d)\n",
				filename, cp.row, cp.col);
			goto close_file;
		}
	} while (!feof(f));
	if (csv_eof(&cp)) {
		fprintf(stderr, "%s:parse error:unexpected end of file\n",
			filename);
		goto close_file;
	}
	fclose(f);
	return 0;
close_file:
	fclose(f);
	return -1;
}

/*
static int field(void *p, unsigned row, unsigned col, size_t len, const char *str)
{
	printf("FIELD:%d:%d:\"%.*s\"\n", row, col, len, str);
	return 0;
}

static int row_end(void *p, unsigned row)
{
	printf("ROW:%d\n", row);
	return 0;
}
*/

struct port_info {
	char *domain, *addr, *port, *canonical;
};

static void po_free(struct port_info *pi)
{
	if (!pi)
		return;
	free(pi->domain);
	pi->domain = NULL;
	free(pi->addr);
	pi->addr = NULL;
	free(pi->port);
	pi->port = NULL;
	free(pi->canonical);
	pi->canonical = NULL;
}


static int po_field(void *p, unsigned row, unsigned col,
	size_t len, const char *str)
{
	struct port_info *pi = p;

	assert(pi != NULL);
	if (!row) /* ignore first row */
		return 0;
	switch (col) {
	case 0:
		memset(pi, 0, sizeof(*pi));
		pi->domain = strdup(str);
		break;
	case 1:
		assert(pi->addr == NULL);
		pi->addr = strdup(str);
		break;
	case 2:
		assert(pi->port == NULL);
		pi->port = strdup(str);
		break;
	case 3:
		assert(pi->canonical == NULL);
		pi->canonical = strdup(str);
		break;
	default:
		printf("FIELD:%d:%d:\"%s\":unknown field\n", row, col, str);
		return -1;
	}

	//printf("FIELD:%d:%d:\"%.*s\"\n", row, col, len, str);
	return 0;
}

static int po_row_end(void *p, unsigned row)
{
	struct port_info *pi = p;
	struct domain *dom;

	assert(pi != NULL);
	if (!row) /* ignore first row */
		return 0;

	//printf("ROW:%d\n", row);
	printf("ROW:%d:%s,%s,%s,%s\n", row, pi->domain, pi->canonical, pi->addr, pi->port);
	if (!pi->domain || !pi->canonical)
		return -1;

	dom = dom_create(pi->domain);
	if (!dom)
		return -1;
	ha_add(dom, pi->canonical);

	if (ht_listen(pi->addr, pi->port))
		return -1;

	po_free(pi);
	return 0;
}

static int config(void)
{
	struct port_info pi;
	int ret;

	memset(&pi, 0, sizeof(pi));
	ret = csv_load("ports.csv", &pi, po_field, po_row_end);
	po_free(&pi);
	if (ret)
		return -1;

#if 1 /* test code */
	ret = se_add(dom_find("myhost"), "/", "(unknown)");
	if (ret)
		return -1;
#endif
	return 0;
}

int main()
{
	if (config())
		return 1;
	wi_start(100);

	main_loop();
	return 0;
}
