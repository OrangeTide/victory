#include <assert.h>
#include <string.h>
#include "env.h"

static int env_load(struct env *env, unsigned ofs, char **name,
	size_t *name_len, char **val, size_t *val_len, unsigned *next)
{
	char *heap = env->heap;
	unsigned name_start, val_start;
	unsigned _name_len, _val_len;

	// TODO: skip over leading \0 characters.

	name_start = ofs;
	assert(name_start < env->cur);
	if (name_start >= env->cur)
		return -1;
	_name_len = strlen(&heap[name_start]) + 1;

	val_start = ofs + _name_len;
	if (val_start >= env->cur)
		return -1;
	_val_len = strlen(&heap[val_start]) + 1;

	if (name)
		*name = env->heap + name_start;
	if (name_len)
		*name_len = _name_len;
	if (val)
		*val = env->heap + val_start;
	if (val_len)
		*val_len = _val_len;
	if (next)
		*next = val_start + _val_len;

	return 0;
}

static int env_find(struct env *env, const char *name, unsigned *ofs, unsigned *len)
{
	unsigned i, next;

	for (i = 0; i < env->cur; i = next) {
		char *cur_name, *cur_val;

		if (env_load(env, i, &cur_name, NULL, &cur_val, NULL, &next))
			return 0; /* weird/corrupt data in buffer */
		if (!strcmp(name, cur_name)) {
			if (ofs)
				*ofs = i;
			if (len)
				*len = next - i; // TODO: check this
			return 1;
		}
	}
	return 0; /* not found */
}

int env_delete(struct env *env, const char *name)
{
	unsigned ofs, len;

	if (env_find(env, name, &ofs, &len)) {
		env->cur -= len;
		memmove(&env->heap[ofs], &env->heap[ofs + len], env->cur);
		return 0;
	}
	return 0;
}

const char *env_get(struct env *env, const char *name)
{
	unsigned ofs;
	char *val;

	if (env_find(env, name, &ofs, NULL)) {
		if (!env_load(env, ofs, NULL, NULL, &val, NULL, NULL))
			return val;
	}
	return NULL;
}

int env_set(struct env *env, const char *name, const char *value)
{
	size_t name_len = strlen(name) + 1;
	size_t val_len = strlen(value) + 1;
	unsigned cur;

	env_delete(env, name);

	cur = env->cur;
	if (cur + name_len + val_len > env->max)
		return -1;

	memcpy(&env->heap[cur], name, name_len);
	cur += name_len;
	memcpy(&env->heap[cur], value, val_len);
	cur += val_len;
	env->cur = cur;

	return 0;
}

int env_next(struct env *env, struct env_iter *iter,
	const char **name, const char **value)
{
	char *_name, *_val;
	unsigned next;

	if (iter->i >= env->cur)
		return 0;

	if (env_load(env, iter->i, &_name, NULL, &_val, NULL, &next))
		return 0; /* weird error - stop */

	iter->i = next;
	if (name)
		*name = _name;
	if (value)
		*value = _val;
	return 1;
}

