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
#ifndef CHANNEL_H
#define CHANNEL_H
#include <stddef.h>

#define CHUNK_SIZE 256

struct channel {
	int fd;
	char *desc;
	size_t buf_max;
	size_t buf_cur;
	int done;
	char buf[CHUNK_SIZE * 16];
};

void channel_init(struct channel *ch, int fd, const char *desc);
void channel_done(struct channel *ch);
void channel_close(struct channel *ch);
int channel_fill(struct channel *ch);
int channel_write(struct channel *ch, const void *buf, size_t count);
void channel_printf(struct channel *ch, const char *fmt, ...);
#endif
