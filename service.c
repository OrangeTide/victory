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
#include "module.h"
#include "logger.h"

struct service {
	const struct module *module;
	char *arg;
	// TODO: a description would be useful for debug messages
};

struct service_entry {
	struct service_entry *next;
	const char *host_match;
	const char *uri_match;
	struct service service;
};

static struct service_entry *service_head;

static int match_service(const struct service_entry *curr, const char *host, const char *uri)
{
	return !fnmatch(curr->host_match, host, FNM_NOESCAPE) &&
		!fnmatch(curr->uri_match, uri, FNM_NOESCAPE);
}

const struct service *service_find(const char *host, const char *uri)
{
	struct service_entry *curr;

	// TODO: make this some sort of prefix tree
	for (curr = service_head; curr; curr = curr->next) {
		if (match_service(curr, host, uri))
			return &curr->service;
	}
	return NULL;
}

int service_register(const char *host_match, const char *uri_match,
	const struct module *module, const char *arg)
{
	struct service_entry *se;

	se = calloc(1, sizeof(*se));
	if (!se) {
		perror(__func__);
		return -1;
	}
	se->host_match = strdup(host_match);
	se->uri_match = strdup(uri_match);
	se->service.module = module;
	se->service.arg = arg ? strdup(arg) : NULL;
	se->next = service_head;
	service_head = se;

	return 0;
}

const struct module *service_module(const struct service *service)
{
	return service ? service->module : NULL;
}

const char *service_arg(const struct service *service)
{
	return service ? service->arg : NULL;
}

int service_start(const char *method, const char *host, const char *uri,
	const struct module **module, struct data **app_data)
{
	const struct service *serv;
	const struct module *mod;
	const char *module_arg;

	serv = service_find(host, uri);
	if (!serv) {
		Error("%s:could not find URI path\n", uri);
		return -1;
	}

	mod = service_module(serv);
	if (!mod) {
		Error("%s:no module defined\n", uri);
		return -1;
	}
	module_arg = service_arg(serv);

	*app_data = module_start(mod, method, uri, module_arg);
	*module = mod;
	// TODO: check for error??
	Info("%s:using module %s\n", uri, mod->desc);
	return 0;
}
