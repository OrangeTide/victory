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
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include "httpparser.h"

enum state {
	S_REQUESTLINE_METHOD = 1000,
	S_REQUESTLINE_URI_FIRSTCHAR,
	S_REQUESTLINE_URI,
	S_REQUESTLINE_HTTPVERSION,
	S_REQUESTLINE_END,
	S_REQUESTHEADER_FIELDNAME_FIRSTCHAR,
	S_REQUESTHEADER_FIELDNAME,
	S_REQUESTHEADER_FIELDVALUE_FIRSTCHAR,
	S_REQUESTHEADER_FIELDVALUE,
	S_REQUESTHEADER_EOL,
	S_REQUESTHEADER_BLANKLINE,
	S_DATA,
};

struct httpparser {
	enum state state;
	unsigned debug_ofs;
	char tok[4096];
	unsigned cur_tok;
	unsigned second_tok; /* offset of second token */
	void *p;
	void (*report_method)(void *p, const char *method, const char *uri);
	void (*report_field)(void *p, const char *name, const char *value);
	void (*report_headerfinish)(void *p);
	void (*report_data)(void *p, size_t len, const void *data);
};

static inline int tok_add(char *buf, unsigned *cur, unsigned max, char ch)
{
	if (*cur < max) {
		buf[*cur] = ch;
		(*cur)++;
		return 0;
	}
	return -1;
}

static inline void tok_rewind(unsigned *cur)
{
	*cur = 0;
}

void httpparser_reset(struct httpparser *hp)
{
	hp->state = S_REQUESTLINE_METHOD;
	tok_rewind(&hp->cur_tok);
	hp->debug_ofs = 0;
}

struct httpparser *httpparser_create(void *p,
	void (*report_method)(void *p, const char *method, const char *uri),
	void (*report_field)(void *p, const char *name, const char *value),
	void (*report_headerfinish)(void *p),
	void (*report_data)(void *p, size_t len, const void *data))
{
	struct httpparser *hp;

	hp = calloc(1, sizeof(*hp));
	hp->p = p;
	hp->report_method = report_method;
	hp->report_field = report_field;
	hp->report_headerfinish = report_headerfinish;
	hp->report_data = report_data;
	httpparser_reset(hp);
	return hp;
}

void httpparser_destroy(struct httpparser *hp)
{
	free(hp);
}

int httpparser(struct httpparser *hp, const char *buf, size_t len)
{
	while (len) {
		char ch = *buf;
		const enum state state = hp->state;

		if (state == S_DATA) {
			printf("S_DATA\n");
			if (hp->report_data)
				hp->report_data(hp->p, len, buf);
			printf("DATA: len=%zd\n", len);
			return 0;
		}
		hp->debug_ofs++;
		buf++;
		len--;
		switch (state) {
		case S_REQUESTLINE_METHOD:
			if (ch == '\r' || ch == '\n') {
				goto terrible_error;
			} else if (ch == ' ') {
				if (tok_add(hp->tok, &hp->cur_tok, sizeof(hp->tok), 0))
					goto buffer_overflow;
				printf("METHOD: '%s'\n", hp->tok);
				hp->state = S_REQUESTLINE_URI_FIRSTCHAR;
			} else {
				if (tok_add(hp->tok, &hp->cur_tok, sizeof(hp->tok), ch))
					goto buffer_overflow;
			}
			break;
		case S_REQUESTLINE_URI_FIRSTCHAR:
			if (ch == ' ' || ch == '\r' || ch == '\n')
				goto terrible_error;
			hp->second_tok = hp->cur_tok;
			if (tok_add(hp->tok, &hp->cur_tok, sizeof(hp->tok), ch))
				goto buffer_overflow;
			hp->state = S_REQUESTLINE_URI;
			break;
		case S_REQUESTLINE_URI:
			if (ch == ' ') {
				if (tok_add(hp->tok, &hp->cur_tok, sizeof(hp->tok), 0))
					goto buffer_overflow;
				if (hp->report_method)
					hp->report_method(hp->p, hp->tok, hp->tok + hp->second_tok);
				printf("URI: '%s'\n", hp->tok + hp->second_tok);
				tok_rewind(&hp->cur_tok);
				hp->state = S_REQUESTLINE_HTTPVERSION;
			} else if (ch == '\r') {
				/* TODO: use HTTP/1.0 mode */
				hp->state = S_REQUESTLINE_END;
			} else if (ch == '\n') {
				goto terrible_error;
			} else {
				if (tok_add(hp->tok, &hp->cur_tok, sizeof(hp->tok), ch))
					goto buffer_overflow;
				hp->state = S_REQUESTLINE_URI;
			}
			break;
		case S_REQUESTLINE_HTTPVERSION:
			// TODO: support parsing this
			printf("S_REQUESTLINE_HTTPVERSION\n");
			if (ch == '\r')
				hp->state = S_REQUESTLINE_END;
			break;
		case S_REQUESTLINE_END:
			printf("S_REQUESTLINE_END\n");
			if (ch != '\n')
				goto terrible_error;
			hp->state = S_REQUESTHEADER_FIELDNAME_FIRSTCHAR;
			break;
		case S_REQUESTHEADER_FIELDNAME_FIRSTCHAR:
			if (ch == '\r') {
				hp->state = S_REQUESTHEADER_BLANKLINE;
			} else if (ch == '\n' || ch == ':') {
				goto terrible_error;
			} else {
				//printf("TODO: request-header add '%c'\n", ch);
				tok_rewind(&hp->cur_tok);
				if (tok_add(hp->tok, &hp->cur_tok, sizeof(hp->tok), ch))
					goto buffer_overflow;
				hp->state = S_REQUESTHEADER_FIELDNAME;
			}
			break;
		case S_REQUESTHEADER_FIELDNAME:
			if (ch == '\r' || ch == '\n') {
				goto terrible_error;
			} else if (ch == ':') {
				// TODO: combine into a single buffer
				if (tok_add(hp->tok, &hp->cur_tok, sizeof(hp->tok), 0))
					goto buffer_overflow;
				printf("FIELDNAME: '%s'\n", hp->tok);
				hp->state = S_REQUESTHEADER_FIELDVALUE_FIRSTCHAR;
			} else {
				//printf("TODO: request-header add '%c'\n", ch);
				if (tok_add(hp->tok, &hp->cur_tok, sizeof(hp->tok), ch))
					goto buffer_overflow;
			}
			break;
		case S_REQUESTHEADER_FIELDVALUE_FIRSTCHAR:
			if (ch == '\r') {
				// TODO: handle EMPTY
			} else if (ch == '\n') {
				goto terrible_error;
			} else if (isspace(ch)) {
				/* ignore linear white space */
			} else {
				hp->second_tok = hp->cur_tok;
				if (tok_add(hp->tok, &hp->cur_tok, sizeof(hp->tok), ch))
					goto buffer_overflow;
				hp->state = S_REQUESTHEADER_FIELDVALUE;
			}
			break;
		case S_REQUESTHEADER_FIELDVALUE:
			if (ch == '\r') {
				if (hp->report_field)
					hp->report_field(hp->p, hp->tok, hp->tok + hp->second_tok);
				printf("FIELDVALUE: '%s'\n", hp->tok + hp->second_tok);
				hp->state = S_REQUESTHEADER_EOL;
			} else if (ch == '\n') {
				goto terrible_error;
			} else {
				if (tok_add(hp->tok, &hp->cur_tok, sizeof(hp->tok), ch))
					goto buffer_overflow;
			}
			break;
		case S_REQUESTHEADER_EOL:
			printf("S_REQUESTHEADER_EOL\n");
			if (ch != '\n')
				goto terrible_error;
			else
				hp->state = S_REQUESTHEADER_FIELDNAME_FIRSTCHAR;
			break;
		case S_REQUESTHEADER_BLANKLINE:
			printf("S_REQUESTHEADER_BLANKLINE\n");
			if (ch != '\n')
				goto terrible_error;
			if (hp->report_headerfinish)
				hp->report_headerfinish(hp->p);
			hp->state = S_DATA;
			break;
		}
	}
	return 0;
terrible_error:
	fprintf(stderr, "error: some terrible error occured (cur=%d)\n", hp->debug_ofs);
	return -1;
buffer_overflow:
	fprintf(stderr, "error: buffer overflow (cur=%d)\n", hp->debug_ofs);
	fprintf(stderr, "\"%.*s\"\n", hp->cur_tok, hp->tok);
	if (hp->second_tok < hp->cur_tok)
		fprintf(stderr, "\"%.*s\"\n", hp->cur_tok - hp->second_tok, hp->tok + hp->second_tok);
	return -1;
}
