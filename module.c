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
#include "module.h"
#include "logger.h"

struct module_entry {
	struct module_entry *next;
	char *modname;
	struct module module;
};

static struct module_entry *module_head;

const struct module *module_find(const char *modname)
{
	struct module_entry *curr;

	for (curr = module_head; curr; curr = curr->next) {
		if (!strcasecmp(curr->modname, modname))
			return &curr->module;
	}
	return NULL;
}

int module_register(const char *modname, const struct module *module)
{
	struct module_entry *me;

	me = calloc(1, sizeof(*me));
	if (!me) {
		perror(__func__);
		return -1;
	}
	me->modname = strdup(modname);
	me->module = *module;
	me->next = module_head;
	module_head = me;

	return 0;
}
