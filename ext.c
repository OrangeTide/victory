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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fnmatch.h>
#include "util.h"
#include "ext.h"
#include "logger.h"
#include "csv.h"

struct ext_info {
	char *content_type;
	char *ext_match;
	struct ext_info *next;
};

static struct ext_info *ext_head;
static char *default_content_type;

/* finds an entry with the same pattern */
static struct ext_info *find_entry(const char *match_pattern)
{
	struct ext_info *curr;

	for (curr = ext_head; curr; curr = curr->next) {
		if (!strcmp(match_pattern, curr->ext_match))
			return curr;
	}
	return NULL;
}

const char *ext_content_type(const char *path)
{
	const char *fn = util_basename(path);
	struct ext_info *curr;

	for (curr = ext_head; curr; curr = curr->next) {
		if (!fnmatch(curr->ext_match, fn, FNM_PATHNAME | FNM_NOESCAPE))
			return curr->content_type;
	}
	return default_content_type; /* not found */
}

/* create a new entry, or replace an entry that has an identical pattern */
int ext_register(const char *match, const char *content_type)
{
	struct ext_info *ext;

	ext = find_entry(match);
	if (!ext) {
		ext = calloc(1, sizeof(*ext));
		if (!ext) {
			perror(__func__);
			return -1;
		}
		ext->next = ext_head;
		ext_head = ext;
	}
	free(ext->content_type);
	ext->content_type = strdup(content_type);
	free(ext->ext_match);
	ext->ext_match = strdup(match);
	Debug("registered mime type %s (%s)\n",
		ext->content_type, ext->ext_match);
	return 0;
}

void ext_default_content_type(const char *content_type)
{
	free(default_content_type);
	default_content_type = strdup(content_type);
}

struct ext_config_info {
	char content_type[256];
	char pattern[256];
};

static void on_row_end(void *user_ptr, unsigned row)
{
	struct ext_config_info *info = user_ptr;

	if (row == 0)
		return; /* ignore first row */
	ext_register(info->pattern, info->content_type);
	memset(&info, 0, sizeof(info));
}

static int on_data(void *user_ptr, unsigned row, unsigned col,
	size_t len, const char *data)
{
	struct ext_config_info *info = user_ptr;

	assert(info != NULL);
	if (row == 0)
		return 0; /* ignore first row */
	switch (col) {
	case 0:
		/* content-type */
		snprintf(info->content_type, sizeof(info->content_type), "%.*s",
			(int)len, data);
		break;
	case 1:
		/* pattern */
		snprintf(info->pattern, sizeof(info->pattern), "%.*s",
			(int)len, data);
		break;
	default:
		return -1;
	}
	return 0;
}

/* read a CSV file */
int ext_config_load(const char *filename)
{
	FILE *f;
	char buf[256];
	size_t len;
	struct csv csv;
	struct ext_config_info info;

	memset(&info, 0, sizeof(info));
	f = fopen(filename, "rb");
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
