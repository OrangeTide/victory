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
#include <stdarg.h>
#include <unistd.h>
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

void channel_init(struct channel *ch, int fd, const char *desc)
{
	memset(ch, 0, sizeof(*ch));
	ch->buf_max = sizeof(ch->buf);
	ch->done = 0;
	ch->fd = fd;
	ch->desc = desc ? strdup(desc) : NULL;
}

void channel_done(struct channel *ch)
{
	ch->done = 1;
}

void channel_close(struct channel *ch)
{
	if (!ch)
		return;
	if (close(ch->fd))
		perror(ch->desc);
	ch->fd = -1;
	free(ch->desc);
	ch->desc = NULL;
}

int channel_fill(struct channel *ch)
{
	ssize_t res;
	size_t count;

	count = buf_check(&ch->buf_max, ch->buf_cur, CHUNK_SIZE);
	assert(count != 0);
	res = read(ch->fd, ch->buf + ch->buf_cur, count);
	fprintf(stderr, "%s:read %zd bytes (asked for %zd bytes)\n",
		ch->desc, res, count);
	fprintf(stderr, "%s:cur=%zd max=%zd\n", ch->desc, ch->buf_cur, ch->buf_max);
	if (res <= 0) {
		if (res < 0)
			perror(ch->desc);
		return -1;
	}
	buf_commit(ch->buf_max, &ch->buf_cur, res);
	return 1;

}

int channel_write(struct channel *ch, const void *buf, size_t count)
{
	while (count > 0) {
		ssize_t res = write(ch->fd, buf, count);
		if (res < 0) {
			perror(ch->desc);
			// channel_close(ch);
			// TODO: longjmp out of here
			return res;
		}
		count -= res;
		buf += res;
	}

	return 0;
}

void channel_printf(struct channel *ch, const char *fmt, ...)
{
	va_list ap;
	char buf[128];

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	channel_write(ch, buf, strlen(buf));
}
