#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "env.h"

#ifndef ARRAY_SIZE
# define ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))
#endif

static int test(void)
{
	struct env env;
	int res;
	const char *testnames[] = {
		"carbon", "argon", "oxygen", "helium",
		"hydrogen", "", "this name has spaces", "uranium-235",
	};
	const char *testvalues[] = {
		"cat", "dog", "pigeon", "giraffe",
		"", "monkey", "bear", "rabbit",
	};
	unsigned i, j;

	env_init(&env);

	/* set various combinations */
	for (j = 0; j < ARRAY_SIZE(testvalues); j++) {
		for (i = 0; i < ARRAY_SIZE(testnames); i++) {
			assert(testvalues[j] != NULL);
			res = env_set(&env, testnames[i], testvalues[j]);
			if (res) {
				fprintf(stderr, "%s:env_set() failed\n", __FILE__);
				return res;
			}
		}
	}

	/* set a specific pattern of values */
	assert(ARRAY_SIZE(testnames) == ARRAY_SIZE(testvalues));
	for (i = 0; i < ARRAY_SIZE(testnames); i++) {
		res = env_set(&env, testnames[i], testvalues[i]);
		if (res) {
			fprintf(stderr, "%s:env_set() failed\n", __FILE__);
			return res;
		}
	}

	/* delete every other name */
	for (i = 0; i < ARRAY_SIZE(testnames); i += 2) {
		res = env_delete(&env, testnames[i]);
		if (res) {
			fprintf(stderr, "%s:env_delete() failed\n", __FILE__);
			return res;
		}
	}

	/* print the remaining list */
	for (i = 1; i < ARRAY_SIZE(testnames); i += 2) {
		const char *value = env_get(&env, testnames[i]);

		if (!value) {
			fprintf(stderr, "%s:null value for \"%s\"\n",
				__FILE__, testnames[i]);
			return -1;
		}

		fprintf(stderr, "%s:test result:\"%s\"=\"%s\"\n",
			__FILE__, testnames[i], value);
		/* check that value is correct */
		if (strcmp(value, testvalues[i])) {
			fprintf(stderr, "%s:env_get() failed\n", __FILE__);
			return  -1;
		}
	}

	/* start over */
	env_init(&env);

	/* try to overflow the buffers */
	for (i = 0; i < sizeof(env.heap); i++) {
		char name[64];
		char value[64];
		int e;

		snprintf(name, sizeof(name), "name%u", i);
		snprintf(value, sizeof(value), "value is %u", i);

		e = env_set(&env, name, value);
		if (e) {
			fprintf(stderr, "%s:env_set() returned %d\n", __FILE__, e);
			break;
		}
	}

	return 0;
}

int main()
{
	if (test()) {
		printf("%s:Test Failure\n", __FILE__);
		return 1;
	}

	printf("%s:Test Success\n", __FILE__);
	return 0;
}
