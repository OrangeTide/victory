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
#include <string.h>

#include "buffer.h"

void buffer_reset(struct buffer *bu)
{
	bu->cur = 0;
	bu->data[0] = 0;
}

void buffer_init(struct buffer *bu, char *data, size_t max)
{
	assert(data != NULL && max > 0);
	bu->data = data;
	bu->max = max;
	buffer_reset(bu);
}

void buffer_consume(struct buffer *bu, unsigned count)
{
	assert(count <= bu->cur);
	if (count <= bu->cur)
		memmove(bu->data, bu->data + count, bu->cur - count);
	bu->cur -= count;
}

int buffer_addch(struct buffer *bu, char ch)
{
	if (bu->cur + 1 >= bu->max)
		return -1;
	bu->data[bu->cur++] = ch;
	bu->data[bu->cur] = 0;
	return 0;
}

