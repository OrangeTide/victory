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
#include <ctype.h>
#include <string.h>
#include "csv.h"

/* add a single character to the buffer */
static inline int tok_add(char *buf, unsigned *cur, unsigned max, char ch)
{
	unsigned i = *cur;

	if (i < (max - 1)) {
		buf[i] = ch;
		(*cur)++;
		return 0;
	}
	return -1;
}

/* null terminate the buffer */
static inline const char *tok_close(char *buf, unsigned *cur, unsigned max)
{
	unsigned i = *cur;

	if (i < max) {
		buf[i] = 0;
		(*cur)++;
		return buf;
	}
	return NULL;
}

static inline int buf_add(struct csv *csv, char ch)
{
	return tok_add(csv->buf, &csv->buf_cur, csv->buf_max, ch);
}

static inline void buf_reset(struct csv *csv)
{
	csv->buf_cur = 0;
}

static inline const char *buf_close(struct csv *csv)
{
	return tok_close(csv->buf, &csv->buf_cur, csv->buf_max);
}

static int data_ready(struct csv *csv)
{
	int (*on_data)(void *ptr, unsigned row, unsigned col,
		size_t len, const char *data) = csv->on_data;

	if (on_data) {
		int res = on_data(csv->user_ptr, csv->row, csv->col,
			csv->buf_cur, buf_close(csv));
		if (res) {
			fprintf(stderr, "%d:%d:processing error before this point.\n",
				csv->debug_line, csv->debug_ofs);
			return res;
		}
	}
	return 0;
}

static void row_end(struct csv *csv)
{
	void (*on_row_end)(void *user_ptr, unsigned row) = csv->on_row_end;

#if 0
	if (csv->col <= 0) /* skip empty rows */
		return;
#endif
	if (on_row_end)
		on_row_end(csv->user_ptr, csv->row);
}

int csv_push(struct csv *csv, size_t len, const char *buf)
{
	while (len > 0) {
		char ch = *buf;

		buf++;
		len--;
		csv->debug_ofs++;
		if (ch == '\n') {
			csv->debug_line++;
			csv->debug_ofs = 0;
		}

		switch(csv->state) {
		case CSV_S_0:
			if (ch == '"') {
				csv->state = CSV_S_QUOTED;
				buf_reset(csv);
			} else if (ch == ',') {
				csv->state = CSV_S_0;
				csv->col++;
			} else if (ch == '\n') {
				csv->state = CSV_S_0;
				row_end(csv);
				csv->row++;
				csv->col = 0;
			} else if (isspace(ch)) {
				csv->state = CSV_S_0;
				/* ignored */
			} else {
				csv->state = CSV_S_FIELD;
				buf_reset(csv);
				buf_add(csv, ch);
			}
			break;
		case CSV_S_QUOTED:
			if (ch == '"') {
				csv->state = CSV_S_QUOTE_CHECK;
			} else {
				buf_add(csv, ch);
			}
			break;
		case CSV_S_QUOTE_CHECK:
			if (ch == '"') {
				csv->state = CSV_S_QUOTED;
				buf_add(csv, ch);
			} else if (ch == ',') {
				csv->state = CSV_S_0;
				if (data_ready(csv))
					return -1;
				csv->col++;
			} else if (ch == '\n') {
				csv->state = CSV_S_0;
				if (data_ready(csv))
					return -1;
				row_end(csv);
				csv->row++;
				csv->col = 0;
			} else if (isspace(ch)) {
				csv->state = CSV_S_0;
			} else {
				goto parse_error;
			}
			break;
		case CSV_S_FIELD:
			if (ch == '"') {
				buf_add(csv, ch);
			} else if (ch == ',') {
				csv->state = CSV_S_0;
				if (data_ready(csv))
					return -1;
				csv->col++;
			} else if (ch == '\n') {
				csv->state = CSV_S_0;
				if (data_ready(csv))
					return -1;
				row_end(csv);
				csv->row++;
				csv->col = 0;
			} else if (ch == '\r') {
				/* ignored */
			} else {
				buf_add(csv, ch);
			}
			break;
		}
	}
	return 0;
parse_error:
	fprintf(stderr, "%d:%d:parse error\n", csv->debug_line, csv->debug_ofs);
	return -1;
buffer_overflow:
	fprintf(stderr, "buffer overflow\n");
	return -1;
not_implemented:
	fprintf(stderr, "not implemented\n");
	return -1;
}

int csv_eol(struct csv *csv)
{
	switch (csv->state) {
	case CSV_S_0:
		return 0;
	case CSV_S_QUOTED:
		break;
	case CSV_S_QUOTE_CHECK:
		csv->state = CSV_S_0;
		if (data_ready(csv))
			return -1;
		row_end(csv);
		return 0;
	case CSV_S_FIELD:
		csv->state = CSV_S_0;
		if (data_ready(csv))
			return -1;
		row_end(csv);
		return 0;
	}
	fprintf(stderr, "%d:%d:parse error at EOL\n",
		csv->debug_line, csv->debug_ofs);
	return -1;
}
