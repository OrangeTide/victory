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
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "httpd.h"
#include "daemonize.h"
#include "service.h"
#include "csv.h"
#include "mod_static_files.h"
#include "logger.h"

struct service_config_info {
	unsigned current_row;
	char uri_match[256];
	const struct module *module;
	char arg[256];
};

static void on_row_end(void *user_ptr, unsigned row)
{
	struct service_config_info *info = user_ptr;

	if (row == 0)
		return; /* ignore first row */
	Debug("row=%d mod=%p arg=\"%s\"\n", row, info->module, info->arg);
	service_register(info->uri_match, info->module, info->arg);
}

static int on_data(void *user_ptr, unsigned row, unsigned col,
	size_t len, const char *data)
{
	struct service_config_info *info = user_ptr;

	Debug("DATA [%d,%d] = '%s'\n", row, col, data);
	assert(info != NULL);
	if (row != info->current_row) {
		info->arg[0] = 0;
		info->module = NULL;
		info->current_row = row;
	}
	if (row == 0)
		return 0; /* ignore first row */
	// TODO: start new row
	Debug("COL %d\n", col);
	switch (col) {
	case 0:
		// TODO: support "enabled" as 0/1 or no/yes
		break;
	case 1:
		snprintf(info->uri_match, sizeof(info->uri_match), "%.*s",
			(int)len, data);
		break;
	case 2:
		info->module = module_find(data);
		Debug("module=%s (%p)\n", data, info->module);
		break;
	case 3:
		snprintf(info->arg, sizeof(info->arg), "%.*s", (int)len, data);
		break;
	default:
		return -1;
	}
	Debug("ROW %d\n", row);
	Debug("\t[%u]='%.*s'\n", col, (int)len, data);
	// TODO: implement this
	return 0;
}

static int load_services(const char *filename)
{
	FILE *f;
	char buf[6]; // TODO: make this bigger
	size_t len;
	struct csv csv;
	struct service_config_info info = { -1, "", NULL, "" };

#if __GLIBC_PREREQ(2, 7)
	f = fopen(filename, "rbe");
#else
	f = fopen(filename, "rb");
#endif
	if (!f) {
		perror(filename);
		return -1;
	}
	csv_init(&csv, &info, on_data, on_row_end);
	do {
		len = fread(buf, 1, sizeof(buf), f);
		if (!len && ferror(f)) {
			perror(filename);
			return -1;
		}
		if (csv_push(&csv, len, buf))
			goto failure;
	} while (!feof(f));

	if (csv_eol(&csv))
		goto failure;

	fclose(f);
	return 0;
failure:
	fclose(f);
	return -1;
}

int main()
{

	module_register("static_files", &mod_static_files);

	load_services("serv.csv");

	httpd_poolsize(100);
	if (httpd_start(NULL, "8080")) {
		Error("Unable to start -- Terminating\n");
		return 1;
	}
	httpd_loop();
	// daemonize();
	Info("done -- Terminating\n");
	return 0;
}
