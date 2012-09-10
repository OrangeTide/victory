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
