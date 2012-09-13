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
#include <string.h>

/* returns a filename */
const char *util_basename(const char *path)
{
	const char *r = strrchr(path, '/');
	if (r)
		return r + 1;
	else
		return path;
}

int util_fixpath(char *dest, size_t dest_len, const char *path)
{
	int st = 0;
	char *out = dest;
	int absolute = path[0] == '/';
	int prev_slash = 1; /* any seperator, either / or start of string. */

	while (out < dest + dest_len) {
		if (!*path) {
			*out = 0;
			return 0; /* success */
		}
		if (prev_slash && path[0] == '.' && path[1] == '.' &&
			(path[2] == '/' || !path[2])) {

			/* consume the "../" or "..\0" */
			if (path[2] == '/')
				path++;
			path += 2;

			/* TODO: back up out to previous dir */
			while (out > dest) {
				out--;
				if (*out == '/')
					break;
			}

			/* make sure relative paths stay relative */
			if (absolute || out != dest)
				*out++ = '/';
		} else if (prev_slash && path[0] == '.' &&
			(path[1] == '/' || !path[1])) {

			/* ignore - just consume the data */
			if (path[1] == '/')
				path++;
			path++;
		} else {
			prev_slash = *path == '/';
			*out = *path;
			out++;
			path++;
		}
	}
	return -1; /* overflow */
}

