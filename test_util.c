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
#include <string.h>
#include "util.h"
#include "logger.h"

#ifndef ARRAY_SIZE
# define ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))
#endif

static int test(void)
{
	struct {
		char *test;
		char *result;
		int negative_test;
	} data[] = {
		{ "../foo/bar/../baz", "foo/bar/baz", 0, },
		{ "../foo/../bar/../baz", "foo/bar/baz", 0, },
		{ "a/b/c/d", "a/b/c/d", 0, },
		{ "foo/bar/../baz", "foo/bar/baz", 0, },
		{ "/foo/bar/../baz", "/foo/bar/baz", 0, },
		{ "/../foo/bar/../baz", "/foo/bar/baz", 0, },
		{ "foo../bar", "foo../bar", 0, },
		{ "/somepath/foo../bar", "/somepath/foo../bar", 0, },
		{ "", "", 0, },
		{ "/././././", "/", 0, },
		{ "/some/really/long/path/that/won't/fit/in/buffer/", "/", 1, },
	};
	char buf[20];
	unsigned i;
	int e;

	for (i = 0; i < ARRAY_SIZE(data); i++) {
		e = util_fixpath(buf, sizeof(buf), data[i].test);
		Debug("test=%s dest=%s%s\n", data[i].test, buf,
			data[i].negative_test ? " (negative)" : "");
		if (data[i].negative_test && e == 0) {
			fprintf(stderr,
				"%s:util_fixpath should have failed\n",
				__FILE__);
			return -1;
		} else if (data[i].negative_test && e) {
			/* passed */
		} else if (!data[i].negative_test && e) {
			fprintf(stderr, "%s:failed util_fixpath (e=%d)\n",
				__FILE__, e);
			return e;
		} else if (!data[i].negative_test && e == 0) {
			if (strcmp(buf, data[i].result)) {
				fprintf(stderr, "%s:result incorrect for \"%s\"\n",
					__FILE__, data[i].test);
				return -1;
			}
		} else {
			fprintf(stderr, "%s:impossible situation, test is broken for \"%s\"\n",
				__FILE__, data[i].test);
			return -1;
		}
	}

	return 0;
}

int main()
{
	if (test()) {
		printf("%s:Test Failure\n", __FILE__);
		return 1;
	}

	printf("%s:Test Success\n", __FILE__);
	return 0;
}
