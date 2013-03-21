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
#ifndef HTTPPARSER_H
#define HTTPPARSER_H
#include <stddef.h>

struct httpparser {
	int state;
	unsigned cur_tok;
	unsigned max_tok;
	unsigned debug_ofs;
	unsigned second_tok;
	long long content_length_remaining;
	char tok[4096];
};

static inline void httpparser_init(struct httpparser *hp)
{
	hp->state = 0; /* S_REQUESTLINE_METHOD; */
	hp->second_tok = 0;
	hp->cur_tok = 0;
	hp->max_tok = sizeof(hp->tok);
	hp->debug_ofs = 0;
	hp->content_length_remaining = -1;
}

int httpparser(struct httpparser *hp, const char *buf, size_t len, void *p,
	void (*report_method)(void *p, const char *method, const char *uri),
	void (*report_header)(void *p, const char *name, const char *value),
	void (*report_header_done)(void *p),
	void (*report_data)(void *p, size_t len, const void *data));
#endif
