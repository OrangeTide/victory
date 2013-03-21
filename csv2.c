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
#include <ctype.h>
#include <string.h>

#include "buffer.h"
#include "csv2.h"
#include "logger.h"

static void _csv_next_field(struct csv_parser *cp)
{
	if (cp->field) {
		if (cp->field(cp->p, cp->row, cp->col, cp->buf.cur,
			cp->buf.data)) {
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

int csv_init(struct csv_parser *cp, char *fieldbuf, size_t fieldbuf_max,
	void *p,
	int (*field)(void *p, unsigned row, unsigned col,
		size_t len, const char *str),
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

int csv(struct csv_parser *cp, const char *buf, size_t len)
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

int csv_eof(struct csv_parser *cp)
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

int csv_load(const char *filename, void *p,
	int (*field)(void *p, unsigned row, unsigned col,
		size_t len, const char *str),
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
			Error("%s:parse error (row=%d col=%d)\n",
				filename, cp.row, cp.col);
			goto close_file;
		}
	} while (!feof(f));
	if (csv_eof(&cp)) {
		Error("%s:parse error:unexpected end of file\n",
			filename);
		goto close_file;
	}
	fclose(f);
	return 0;
close_file:
	fclose(f);
	return -1;
}
