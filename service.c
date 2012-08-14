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
#include <stdlib.h>
#include <string.h>
#include <fnmatch.h>
#include "service.h"

struct service_entry {
	struct service_entry *next;
	const char *uri_match;
	struct service service;
};

static struct service_entry *service_head;

const struct service *service_find(const char *uri)
{
	struct service_entry *curr;

	for (curr = service_head; curr; curr = curr->next) {
		if (!fnmatch(curr->uri_match, uri, FNM_PATHNAME | FNM_NOESCAPE))
			return &curr->service;
	}
	return NULL;
}

int service_register(const char *uri_match, const struct service *service)
{
	struct service_entry *se;

	se = calloc(1, sizeof(*se));
	if (!se) {
		perror(__func__);
		return -1;
	}
	se->uri_match = strdup(uri_match);
	se->service = *service;
	se->next = service_head;
	service_head = se;

	return 0;
}
