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
struct httpparser;
void httpparser_reset(struct httpparser *hp);
struct httpparser *httpparser_create(
	void *p,
	void (*report_method)(void *p, const char *method, const char *uri),
	void (*report_field)(void *p, const char *name, const char *value),
	void (*report_headerfinish)(void *p),
	void (*report_data)(void *p, size_t len, const void *data));
void httpparser_destroy(struct httpparser *hp);
int httpparser(struct httpparser *hp, const char *buf, size_t len);
#endif
