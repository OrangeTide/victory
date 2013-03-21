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
#include <stdlib.h>
#include <stdio.h>
#include "csv.h"

const char test1[] =
	"This is a test,1234,4567,\r\n"
	"a,\"\"\"\",,\r\n"
	"b,,\",\",\r\n"
	"c,,\"\"\"\"\"\",\r\n"
	"d,e,f,this is the end";

const char test2[] = "d,e,f,\"too short";

static int on_data(void *user_ptr, unsigned row, unsigned col,
	size_t len, const char *data)
{
	fprintf(stderr, "[%d,%d]='%.*s'\n", row, col, (int)len, data);
	return 0;
}

static int test_csv_file(FILE *f)
{
	char buf[6];
	size_t len;
	struct csv csv;

	csv_init(&csv, NULL, on_data, NULL);
	do {
		len = fread(buf, 1, sizeof(buf), f);
		if (csv_push(&csv, len, buf))
			return -1;
	} while (!feof(f));

	if (csv_eol(&csv))
		return -1;

	return 0;
}

static int test_csv_buffer(size_t len, const char *data)
{
	const char *buf = data;
	size_t chunk_len;
	struct csv csv;

	csv_init(&csv, NULL, on_data, NULL);
	while (len > 0) {
		chunk_len = rand() % len;
		if (!chunk_len)
			chunk_len = len;
		if (csv_push(&csv, chunk_len, buf))
			return -1;
		buf += chunk_len;
		len -= chunk_len;
	}
	if (csv_eol(&csv))
		return -1;
	return 0;
}

int main(int argc, char **argv)
{
	struct {
		const char *ptr;
		size_t len;
		int negative_test;
	} tests[] = {
		{ test1, sizeof(test1) - 1, 0 },
		{ test2, sizeof(test2) - 1, 1 },
	};
	unsigned i;

	for (i = 0; i < sizeof(tests)/sizeof(*tests); i++) {
		int res;

		fprintf(stderr, "%s:Test #%u%s\n", argv[0], i + 1,
			tests[i].negative_test ? " (negative)" : "");
		res = test_csv_buffer(tests[i].len, tests[i].ptr);
		if ((!tests[i].negative_test && res)
			|| (tests[i].negative_test && !res)) {
			printf("%s:test failure\n", argv[0]);
			return 1;
		}
	}
	printf("%s:all tests passed\n", argv[0]);
	return 0;
}
