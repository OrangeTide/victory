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
#include "httpparser.h"

static const char testdata1[] =
	"GET /this/is/a/test HTTP/1.1\r\n"
	"User-Agent: curl/7.19.7 (x86_64-pc-linux-gnu) libcurl/7.19.7 OpenSSL/0.9.8k zlib/1.2.3.3 libidn/1.15\r\n"
	"Host: localhost:8080\r\n"
	"Accept: */*\r\n"
	"\r\n"
	"some data follows...\r\n"
	"more data\r\n"
	"\r\n"
	"\r\n"
	"\r\n";
static size_t testdata1_len = sizeof(testdata1) - 1;

static const char testdata2[] =
	"GET /this/is/a/test HTTP/1.1\r\n"
	"Host: localhost:8080\r\n"
	"User-Agent: Mozilla/5.0 (X11; U; Linux x86_64; en-US; rv:1.9.2.24) Gecko/20111107 Ubuntu/10.04 (lucid) Firefox/3.6.24\r\n"
	"Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
	"Accept-Language: en-us,en;q=0.5\r\n"
	"Accept-Encoding: gzip,deflate\r\n"
	"Accept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.7\r\n"
	"Keep-Alive: 115\r\n"
	"Connection: keep-alive\r\n"
	"\r\n";
static size_t testdata2_len = sizeof(testdata2) - 1;

static void on_method(void *p, const char *method, const char *uri)
{
	printf("%p:method=\"%s\" uri=\"%s\"\n", p, method, uri);
}

static void on_header(void *p, const char *name, const char *value)
{
	printf("%p:name=\"%s\" value=\"%s\"\n", p, name, value);
}

static void on_headerfinish(void *p)
{
	printf("%p:HEADER DONE!\n", p);
}

static void on_data(void *p, size_t len, const void *data)
{
	printf("%p:len=%zd data=%p\n", p, len, data);
}

int main()
{
	struct httpparser hp;

	httpparser_init(&hp);
	httpparser(&hp, testdata1, testdata1_len, NULL, on_method, on_header,
		on_headerfinish, on_data);

	httpparser_init(&hp);
	httpparser(&hp, testdata2, testdata2_len, NULL, on_method, on_header,
		on_headerfinish, on_data);
	return 0;
}

