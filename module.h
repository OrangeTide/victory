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
#ifndef MODULE_H
#define MODULE_H
#include <stddef.h>
#include "channel.h"
#include "env.h"

struct module {
	char *desc;
	void *(*start)(const char *method, const char *uri, const char *arg);
	void (*free)(void *app_ptr);
	void (*on_header_done)(struct channel *ch, void *app_ptr,
		struct env *headers);
	void (*on_data)(struct channel *ch, void *app_ptr, size_t len,
		const void *data);
};

const struct module *module_find(const char *modname);
int module_register(const char *modname, const struct module *module);
void *module_start(const struct module *module, const char *method,
	const char *uri, const char *arg);
#endif
