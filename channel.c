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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include "logger.h"
#include "httpparser.h"
#include "channel.h"

static size_t buf_check(size_t *buf_max, size_t buf_cur, size_t count)
{
	size_t max = *buf_max;

	// TODO: we could grow the buf
	assert(buf_cur <= max);
	return (buf_cur + count < max) ? count : max - buf_cur;
}

static void buf_commit(size_t buf_max, size_t *buf_cur, size_t count)
{
	assert((*buf_cur) + count <= buf_max);
	(*buf_cur) += count;
}

void ch_init(struct channel *ch, struct net_socket sock, const char *desc)
{
	memset(ch, 0, sizeof(*ch));
	ch->buf_max = sizeof(ch->buf);
	ch->done = 0;
	ch->sock = sock;
	ch->desc = desc ? strdup(desc) : NULL;
}

static ch_is_connected(struct channel *ch)
{
	return ch->sock.fd != -1;
}

void ch_done(struct channel *ch)
{
	ch->done = 1;
}

void ch_close(struct channel *ch)
{
	if (!ch)
		return;
	if (ch->sock.fd != -1)
		if (close(ch->sock.fd))
			perror(ch->desc);
	ch->sock.fd = -1;
	free(ch->desc);
	ch->desc = NULL;
}

int ch_fill(struct channel *ch)
{
	ssize_t res;
	size_t count;

	count = buf_check(&ch->buf_max, ch->buf_cur, CHANNEL_CHUNK_SIZE);
	assert(count != 0);
	res = read(ch->sock.fd, ch->buf + ch->buf_cur, count);
	Debug("%s:read %zd bytes (asked for %zd bytes)\n",
		ch->desc, res, count);
	Debug("%s:cur=%zd max=%zd\n", ch->desc, ch->buf_cur, ch->buf_max);
	if (res <= 0) {
		if (res < 0)
			perror(ch->desc);
		return -1;
	}
	buf_commit(ch->buf_max, &ch->buf_cur, res);
	return 1;

}

int ch_write(struct channel *ch, const void *buf, size_t count)
{
	while (count > 0) {
		ssize_t res;

		if (ch->done)
			return -1;
		res = write(ch->sock.fd, buf, count);
		if (res < 0) {
			perror(ch->desc);
			ch_done(ch);
			return res;
		}
		count -= res;
		buf += res;
	}

	return 0;
}

int ch_printf(struct channel *ch, const char *fmt, ...)
{
	va_list ap;
	int res;

	if (!ch_is_connected(ch))
		return -1;
	va_start(ap, fmt);
	res = vdprintf(ch->sock.fd, fmt, ap);
	va_end(ap);
	return res;
}

int ch_puts(struct channel *ch, const char *str)
{
	size_t cur = 0, len = strlen(str);
	ssize_t res;

	if (!ch_is_connected(ch))
		return -1;
	do {
		res = send(ch->sock.fd, str + cur, len - cur, 0);
		if (res < 0) {
			SysError();
			break;
		}
		assert(res != 0);
		cur += res;
	} while(cur < len);
	return len;
}
