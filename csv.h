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
#ifndef CSV_H
#define CSV_H
#include <stddef.h>

enum csv_state {
	CSV_S_0 = 0,
	CSV_S_QUOTED,
	CSV_S_QUOTE_CHECK,
	CSV_S_FIELD,
};

struct csv {
	enum csv_state state;
	unsigned row, col;
	int debug_ofs, debug_line;
	unsigned buf_max, buf_cur;
	int (*on_data)(void *ptr, unsigned row, unsigned col,
		size_t len, const char *data);
	void (*on_row_end)(void *user_ptr, unsigned row);
	void *user_ptr;
	char buf[1024];
};

static inline void csv_init(struct csv *csv, void *user_ptr,
	int (*on_data)(void *ptr, unsigned row, unsigned col,
		size_t len, const char *data),
	void (*on_row_end)(void *user_ptr, unsigned row))
{
	csv->state = 0;
	csv->row = 0;
	csv->col = 0;
	csv->debug_ofs = 0;
	csv->debug_line = 1;
	csv->buf_max = sizeof(csv->buf);
	csv->buf_cur = 0;
	csv->user_ptr = user_ptr;
	csv->on_data = on_data;
	csv->on_row_end = on_row_end;
}

int csv_push(struct csv *csv, size_t len, const char *buf);
int csv_eol(struct csv *csv);
#endif
