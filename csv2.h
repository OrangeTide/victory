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
#ifndef CSV2_H
#define CSV2_H
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
	int (*field)(void *p, unsigned row, unsigned col, size_t len,
		const char *str);
	int (*row_end)(void *p, unsigned row);
};

int csv_init(struct csv_parser *cp, char *fieldbuf, size_t fieldbuf_max, void *p,
	int (*field)(void *p, unsigned row, unsigned col, size_t len, const char *str),
	int (*row_end)(void *p, unsigned row));
int csv(struct csv_parser *cp, const char *buf, size_t len);
int csv_eof(struct csv_parser *cp);
int csv_load(const char *filename, void *p,
	int (*field)(void *p, unsigned row, unsigned col, size_t len, const char *str),
	int (*row_end)(void *p, unsigned row));
#endif
