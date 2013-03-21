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
#ifndef ENV_H
#define ENV_H
struct env {
	unsigned cur, max;
	char heap[2048 - (2 * sizeof(unsigned))];
};

struct env_iter {
	unsigned i;
};

static inline void env_init(struct env *env)
{
	env->cur = 0;
	env->max = sizeof(env->heap);
}

static inline void env_iter(struct env_iter *iter)
{
	iter->i = 0;
}

const char *env_get(struct env *env, const char *name);
int env_set(struct env *env, const char *name, const char *value);
int env_delete(struct env *env, const char *name);
int env_next(struct env *env, struct env_iter *iter,
	const char **name, const char **value);
#endif
