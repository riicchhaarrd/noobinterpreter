#include "vm.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

static int sf_print(ipr_t *ipr) {

	for (int i = 0; i < se_argc(ipr); i++) {
		printf("%s", se_getstring(ipr, i));
	}
	return 0;
}

static int sf_time(ipr_t *ipr) {
	se_addint(ipr, time(0));
	return 1;
}

static int sf_gettime(ipr_t *ipr) {
	se_addint(ipr, ipr->current_time);
	return 1;
}

static int sf_sleep(ipr_t *ipr) {
	int ms = se_getint(ipr, 0);
	Sleep(ms);
	return 0;
}

static int sf_rand(ipr_t *ipr) {
	int rnd = rand();
	se_addint(ipr, rnd);
	return 1;
}

static int sf_getchar(ipr_t *ipr) {
	int ch = getchar();
	se_addchar(ipr, ch);
	return 1;
}

static int sf_isdefined(ipr_t *ipr) {
	if (se_argc(ipr) < 1)
		se_addbool(ipr, false);
	else {
		varval_t *v = se_argv(ipr, 0);
		if (v->type == IPR_VT_NULL)
			se_addbool(ipr, false);
		else
			se_addbool(ipr, true);
	}
	return 1;
}

static int sf_test_args(ipr_t *ipr) {

	for (int i = 0; i < se_argc(ipr); i++) {
		printf("arg %d => %s\n", i, se_getstring(ipr, i));
	}

	return 0;
}

static int sf_internal_get_var_type(ipr_t *ipr) {
	if (se_argc(ipr) == 0 || se_argc(ipr) > 1) {
		se_addstring(ipr, "one parameter please");
		return 1;
	}

	varval_t *v = se_argv(ipr, 0);

	se_addstring(ipr, vartypestrings[v->type]);
	return 1;
}

stockfunction_t scriptfunctions[] = {
	{"gettime", sf_gettime},
	{"getchar",sf_getchar},
	{"isdefined",sf_isdefined},
	{"test_args", sf_test_args},
	{"internal_get_var_type_as_string", sf_internal_get_var_type},
	{ "print", sf_print },
	{ "time", sf_time },
	{ "sleep", sf_sleep },
	{ "rand", sf_rand },
	{ NULL, 0 }
};