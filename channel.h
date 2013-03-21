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
#ifndef CHANNEL_H
#define CHANNEL_H
#include <stddef.h>
#include "net.h"

#define CHANNEL_CHUNK_SIZE 256

struct channel {
	struct net_socket sock;
	char *desc;
	size_t buf_max;
	size_t buf_cur;
	int done;
	char buf[CHANNEL_CHUNK_SIZE * 16];
};

void ch_init(struct channel *ch, struct net_socket sock, const char *desc);
void ch_done(struct channel *ch);
void ch_close(struct channel *ch);
int ch_fill(struct channel *ch);
int ch_write(struct channel *ch, const void *buf, size_t count);
int ch_printf(struct channel *ch, const char *fmt, ...);
int ch_puts(struct channel *ch, const char *str);
#endif
