// OreScript.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <ctype.h> //isalnum
#include <varargs.h>

#include "xalloc.h"

#include "vm.h"

#ifdef _WIN32
#include <windows.h>
#endif

typedef enum {
	E_IPR_ERROR_NONE,
	E_IPR_ERROR,
	E_IPR_ERROR_OUT_OF_MEMORY,
	E_IPR_ERROR_UNEXPECTED_TOKEN,
	E_IPR_ERROR_SYNTAX,
	E_IPR_ERROR_DIVIDE_BY_ZERO,
	E_IPR_ERROR_VARIABLE_DOES_NOT_EXIST,
	E_IPR_ERROR_VARIABLE_HAS_NO_VALUE,
	E_IPR_ERROR_EOF,
	E_IPR_ERROR_FUNCTION_ALREADY_EXISTS,
	E_IPR_ERROR_FUNCTION_DOES_NOT_EXISTS,
	E_IPR_ERROR_UNEXPECTED_FUNCTION_DECLARATION,

} e_ipr_e_return_codes;

int ipr_error_f(ipr_t *ipr, int errcode, int line, const char *c_source_filename) {
	ipr->error = errcode;

	if (!ipr->error) {
		sprintf(ipr->errstr, "NO ERROR");
		return ipr->error;
	}
	sprintf_s(ipr->errstr, "Error at line %d in file %s\n", line, c_source_filename);
	return errcode;
}

#define ipr_error(x, y) (ipr_error_f(x,y,__LINE__,__FILE__))

int is_space(unsigned char c) {
	if (c == ' ' || c == '\t')
		return 1;
	return 0;
}

int read_text_file(const char *filename, char **buf, int *filesize) {

	FILE *fp = fopen(filename, "r");

	if (fp == NULL)
		return 1;
	fseek(fp, 0, SEEK_END);
	*filesize = ftell(fp);
	rewind(fp);

	*buf = (char*)xmalloc(*filesize);

	fread(*buf, 1, *filesize, fp);

	fclose(fp);

	return 0;

}

static void stack_push(ipr_t *ipr, intptr_t v) {
	if (!ipr->thrunner) {
		ipr->stack[++ipr->registers[REG_SP]] = v;
		return;
	}

	if (ipr->thrunner->sp + 1 > VM_STACK_SIZE) {
		printf("stack > VM_STACK_SIZE\n");
		__asm int 3
		return;
	}

	ipr->thrunner->stack[++ipr->thrunner->sp] = v;
}

static intptr_t stack_pop(ipr_t *ipr) {
	if (!ipr->thrunner) {
		return ipr->stack[ipr->registers[REG_SP]--];
	}
	return ipr->thrunner->stack[ipr->thrunner->sp--];
}

var_t *ipr_getvarbyname(ipr_t *ipr, const char* n) {
	var_t *v;
	for (int i = 0; i < IPR_MAX_VARIABLES; i++) {
		v = ipr->variables[i];
		if (!v)
			continue;

		if (!strcmp(v->name, n)) {
			return v;
		}
	}
	return NULL;
}

void se_error(ipr_t *ipr, const char *msg, ...) {

	va_list args;

	va_start(args, msg);

	vprintf(msg, args);
	va_end(args);

	ipr->error = E_IPR_ERROR;
}

int se_argc(ipr_t *ipr) {
	return ipr->thrunner->stack[ipr->func_args_sp - 1];
}

varval_t *se_argv(ipr_t *ipr, int n) {
	if (n >= se_argc(ipr))
		return &ipr->varval_null;

	return (varval_t*)ipr->thrunner->stack[ipr->func_args_sp - 1 - se_argc(ipr) + n];
}

int se_vartype(ipr_t *ipr, int n) {
	return se_argv(ipr, n)->type;
}

float varval_to_float(ipr_t *ipr, varval_t *vv) {
	if (!vv)
		return 0;
	switch (vv->type) {

	case IPR_VT_FLOAT:
		return vv->number;

	case IPR_VT_INT:
		return (float)vv->integer;

	case IPR_VT_STRING:
		return atof(ipr->strings[vv->stringindex]->string);
	}
	return 0;
}

const char *varval_to_string(ipr_t *ipr, varval_t* value) {

	static char retstr[128];
	if (!value || value->type == IPR_VT_NULL)
		return "[null] " __FILE__ " at " S__LINE__;
	if (value->type == IPR_VT_CHAR) {
		retstr[0] = value->character;
		retstr[1] = '\0';
		return retstr;
	} else if (value->type == IPR_VT_INT) {
		sprintf(retstr, "%d", value->integer);
		return retstr;
	}
	else if (value->type == IPR_VT_FLOAT) {
		sprintf(retstr, "%f", value->number);
		return retstr;
	}

	return ipr->strings[value->stringindex]->string;
}

const char *se_getstring(ipr_t *ipr, int n) {
	return varval_to_string(ipr, se_argv(ipr, n));
}

float se_getfloat(ipr_t* ipr, int n) {
	varval_t *v = se_argv(ipr, n);

	if (v->type==IPR_VT_NULL) {
		se_error(ipr, "null cast to float\n");
		return 0;
	}

	if (v->type == IPR_VT_INT)
		return (float)v->integer;

	if (v->type != IPR_VT_FLOAT) {
		se_error(ipr, "error not an float\n");
		return 0;
	}
	return v->number;
}

int se_getint(ipr_t *ipr, int n) {
	varval_t *v = se_argv(ipr, n);

	if (v->type==IPR_VT_NULL) {
		se_error(ipr, "null cast to int\n");
		return 0;
	}

	if (v->type == IPR_VT_FLOAT)
		return v->number;

	if (v->type != IPR_VT_INT) {
		se_error(ipr, "error not an int\n");
		return 0;
	}
	return v->integer;
}

void se_addfloat(ipr_t *ipr, float f) {
	varval_t * vv = alloc_scriptvar_value(ipr);
	vv->flags = VF_LOCAL;
	vv->number = f;
	vv->type = IPR_VT_FLOAT;
	stack_push(ipr, (intptr_t)vv);
}

void se_addchar(ipr_t *ipr, int ch) {
	varval_t *vv = alloc_scriptvar_value(ipr);
	vv->flags = VF_LOCAL;
	vv->character = ch;
	vv->type = IPR_VT_CHAR;
	stack_push(ipr, (intptr_t)vv);
}

void se_addint(ipr_t *ipr, int n) {
	varval_t *vv = alloc_scriptvar_value(ipr);
	vv->flags = VF_LOCAL;
	vv->integer = n;
	vv->type = IPR_VT_INT;
	stack_push(ipr, (intptr_t)vv);
}

int isinteger(const char *s) {
	int is = 1;
	for (int i = 1; i < strlen(s); i++) {

		if (!isdigit(s[i])) {
			is = 0;
			break;
		}

	}

	if (*s != '-' && !isdigit(*s))
		return 0;
	return is;
}

var_t *alloc_scriptvar(ipr_t *ipr) {
	int i;
	for (i = 0; i < IPR_MAX_VARIABLES; i++) {
		if (!ipr->variables[i])
			break;
	}

	if (i == IPR_MAX_VARIABLES) {
		printf("MAX VARIABLES REACHED\n");
		return NULL;
	}

	var_t *var = (var_t*)xmalloc(sizeof(var_t));
	memset(var, 0, sizeof(var_t));

	ipr->variables[i] = var;
	++ipr->numvars;
	return var;
}

var_t *alloc_local_scriptvar(ipr_t *ipr, bool add_to_list) {
	int i;
	if (add_to_list) {
		for (i = 0; i < IPR_MAX_VARIABLES; i++) {
			if (!ipr->localvars[i])
				break;
		}

		if (i == IPR_MAX_VARIABLES) {
			printf("MAX LOCAL VARIABLES REACHED\n");
			return NULL;
		}
	}
	var_t *var = (var_t*)xmalloc(sizeof(var_t));
	memset(var, 0, sizeof(var_t));
	var->flags = VF_LOCAL;
	if (add_to_list) {
		ipr->localvars[i] = var;
		++ipr->numlocalvars;
	}
	return var;
}

varval_t *alloc_scriptvar_value_f(ipr_t *ipr, int line) {
	varval_t *val = (varval_t*)xmalloc(sizeof(varval_t));
	memset(val, 0, sizeof(*val));
	//printf("line: %d (val=%02X)\n", line, val);

	return val;
}

void delete_scriptvar_value(ipr_t *ipr, var_t *v) {
	if (!v->value)
		return;
	xfree(v->value);
	v->value = NULL;
}

void delete_scriptvalue(ipr_t *ipr, varval_t *vv) {
	if (!vv)
		return;
	xfree(vv);
}

int ipr_pushtostr(ipr_t *ipr, int c) {

	if (!c)
		return 0;

	if (ipr->stringindex >= sizeof(ipr->string)) {
		printf("keyword string index overflow\n");
		return 0;
	}
	ipr->string[ipr->stringindex++] = c;
	return 1;
}

#define next (ipr->curpos >= ipr->scriptsize ? 0 : ipr->scriptbuf[ipr->curpos++])
#define next_chk(x) ( ( (ipr->scriptbuf[ipr->curpos] == x) ? ipr->scriptbuf[ipr->curpos++] : ipr->scriptbuf[ipr->curpos] ) == x)

int ipr_getstring(ipr_t *ipr) {

	memset(ipr->string, 0, sizeof(ipr->string));
	ipr->stringindex = 0;

	int ch;

	while ((ch = next) != '"' && ch != '\'') {
		if (ch == '\\') { //maybe fix this with actual stuff?

			switch (ch = next) {
			case 'n':
				ch = '\n';
				break;
			case 't':
				ch = '\t';
				break;
			}
		}
		if (!ipr_pushtostr(ipr, ch))
			break;
	}

	return TK_STRING;
}

const char *ipr_get_string_from_index(ipr_t *ipr, int i) {
	return ipr->strings[i]->string;
}

int ipr_getident(ipr_t *ipr) {

	memset(ipr->string, 0, sizeof(ipr->string));
	ipr->stringindex = 0;

	--ipr->curpos;

	int ch;

	while (isalpha((ch = next)) || isdigit(ch) || ch == '_') {
		if (!ipr_pushtostr(ipr, tolower(ch)))
			break;
	}
	--ipr->curpos;

	if (!strcmp(ipr->string, "if"))
		return TK_IF;
	else if (!strcmp(ipr->string, "break"))
		return TK_R_BREAK;
	else if (!strcmp(ipr->string, "else"))
		return TK_R_ELSE;
	else if (!strcmp(ipr->string, "foreach"))
		return TK_R_FOREACH;
	else if (!strcmp(ipr->string, "while"))
		return TK_WHILE;
	else if (!strcmp(ipr->string, "for"))
		return TK_R_FOR;
	else if (!strcmp(ipr->string, "var"))
		return TK_VAR;
	else if (!strcmp(ipr->string, "return"))
		return TK_RETURN;
	else if (!strcmp(ipr->string, "thread"))
		return TK_R_THREAD;
	else if (!strcmp(ipr->string, "notify"))
		return TK_NOTIFY;
	else if (!strcmp(ipr->string, "endon"))
		return TK_ENDON;
	else if (!strcmp(ipr->string, "waittill"))
		return TK_WAITTILL;
	else if (!strcmp(ipr->string, "wait"))
		return TK_R_WAIT;
	else if (!strcmp(ipr->string, "true"))
		return TK_R_TRUE;
	else if (!strcmp(ipr->string, "false"))
		return TK_R_FALSE;
	else if (!stricmp(ipr->string, "null"))
		return TK_R_NULL;

	return TK_IDENT;
}

int ipr_getnumber(ipr_t *ipr) {
	char str[128] = { 0 };
	int stri = 0;
	int ch;

	--ipr->curpos;

	bool is_int = true;

	while ( (ch = next) ) {
		if (!isdigit(ch) && ch != '.')
			break;
		if (ch == '.')
			is_int = false;
		str[stri++] = ch;
	}

	if (is_int)
		ipr->integer = atoi(str);
	else {
		ipr->number = atof(str);
		//ipr->integer = ipr->number;//truncate to int anyway
	}
	--ipr->curpos;

	if (!is_int)
		return TK_FLOAT;
	return TK_INT;
}

int ipr_symbol(ipr_t *ipr) {
	int ch;

scan:

	switch (ch = next) {

	case '\t':
	case ' ':
		goto scan;

	case '\r':
	case '\n':
		++ipr->lineno;
		goto scan;
		break;

	case '(': return TK_LPAREN;
	case ')': return TK_RPAREN;
	case '=':
		if (next_chk('='))
			return TK_EQUALS;
		return TK_ASSIGN;

	case '>': 
		if (next_chk('='))
			return TK_GEQUAL;
		return TK_GREATER;
	case '<': 
		if (next_chk('='))
			return TK_LEQUAL;
		return TK_LESS;

	case '+':
		if (next_chk('='))
			return TK_PLUS_ASSIGN;
		else if (next_chk('+'))
			return TK_PLUS_PLUS;
		return TK_PLUS;

	case '-':
#if 0
		if (isdigit(ipr->scriptbuf[ipr->curpos + 1]))
			goto get_num;
#endif
		if (next_chk('='))
			return TK_MINUS_ASSIGN;
		else if (next_chk('-'))
			return TK_MINUS_MINUS;
		else if (next_chk('>'))
			return TK_MEMBER_SEPERATOR;
		return TK_MINUS;

	case '&':
		if (next_chk('&'))
			return TK_AND_AND;
		return TK_AND;

	case '|':
		if (next_chk('|'))
			return TK_OR_OR;
		return TK_OR;

	case '%': return TK_MODULO;
	case ',': return TK_COMMA;
	case ':': return TK_COLON;
	case ';': return TK_SEMICOLON;
	case '.': 
		if (isdigit(ipr->scriptbuf[ipr->curpos + 1]))
			goto get_num;
		return TK_DOT;
	case '[': return TK_LBRACK;
	case ']': return TK_RBRACK;
	case '{': return TK_LBRACE;
	case '}': return TK_RBRACE;
	case '!': 
		if (next_chk('='))
			return TK_NOTEQUAL;
		return TK_NOT;

	case '$': return TK_DOLLAR;

	case '/': 
		if(next_chk('/')) {
			while (ch && ch != '\n') {
				ch = next;
			}
			goto scan;

		} else if (next_chk('='))
			return TK_DIVIDE_ASSIGN;
		return TK_DIVIDE;

	case '*': 
		if (next_chk('='))
			return TK_MULTIPLY_ASSIGN;
		return TK_MULTIPLY;

	case '"':
	case '\'':
		return ipr_getstring(ipr);

	default:
		if (isdigit(ch)) {

			get_num:

			int tk = ipr_getnumber(ipr);

			return tk;
		}
		else if (isalpha(ch)) {
			int ident = ipr_getident(ipr);

			if (ident == TK_R_TRUE || ident == TK_R_FALSE) {
				ipr->integer = (ident == TK_R_TRUE) ? 1 : 0;
				return TK_INT;
			}
			return ident;
		}

		return TK_ILLEGAL;

		break;

	case 0:
		return TK_EOF;

	}
	return TK_ILLEGAL;
}

int ipr_next_symbol(ipr_t *ipr) {
	int tk = ipr_symbol(ipr);
	ipr->token = tk;
	return tk;
}

int ipr_accept(ipr_t *ipr, int s) {
	int cur = ipr->curpos;

	int tk = ipr_symbol(ipr);
	ipr->token = tk;
	//printf("TOKEN: accept(%s), got(%s)\n", lex_token_strings[s], lex_token_strings[tk]);

	if (s == tk) {
		return 1;
	}
	ipr->curpos = cur;
	return 0;
}

int ipr_expect(ipr_t *ipr, int s) {
	if (ipr_accept(ipr, s))
		return 1;

	ipr->error = 1;
	printf("error: unexpected symbol! expected %s, got %s\n", lex_token_strings[s], lex_token_strings[ipr->token]);
	return 0;
}

void delete_scriptvar(ipr_t *ipr, var_t *var) {
	for (int i = 0; i < IPR_MAX_VARIABLES; i++) {
		var_t **v;
		v = &ipr->variables[i];
		--ipr->numvars;
		
		if (*v == var) {
			*v = NULL;
			break;
		}
	}
	if (var->value)
		delete_scriptvar_value(ipr, var);
	xfree(var);
}

scriptarray_t *alloc_scriptarray(ipr_t *ipr) {
	scriptarray_t *arr = (scriptarray_t*)xmalloc(sizeof(scriptarray_t));
	arr->size = 0;
	arr->pairs = NULL;
	return arr;
}

void delete_scriptarray(ipr_t *ipr, scriptarray_t *arr) {
	xfree(arr->pairs);
	xfree(arr);
}

void scriptarray_set_index(scriptarray_t *arr, int index, varval_t *new_val) {
	++arr->size;

	arr->pairs = (scriptkvp_t*)xrealloc(arr->pairs, sizeof(scriptkvp_t) * arr->size);

	if (!arr->pairs) {
		printf("failed to reallocate memory for array values\n");
		xfree(arr->pairs);
		arr->pairs = NULL;
		//should really fatal fail here, just crash is best lol
		return;
	}

	arr->pairs[arr->size - 1].key = index;
	arr->pairs[arr->size - 1].value = new_val;
}

//takes the values from the stack (might be better)
void scriptarray_push(ipr_t *ipr, scriptarray_t *arr) {

	++arr->size;

	arr->pairs = (scriptkvp_t*)xrealloc( arr->pairs, sizeof(scriptkvp_t) * arr->size );

	if (!arr->pairs) {
		printf("failed to reallocate memory for array values\n");
		xfree(arr->pairs);
		arr->pairs = NULL;
		return;
	}

	varval_t *vv = (varval_t*)stack_pop(ipr);

	arr->pairs[arr->size - 1].key = arr->size - 1;
	arr->pairs[arr->size - 1].value = vv; //maybe add refs here (decrease later)
}

scriptstring_t *alloc_scriptstring(ipr_t *ipr, const char *s) {
	scriptstring_t *ss = (scriptstring_t*)xmalloc(sizeof(scriptstring_t));
	ss->index = ipr->numstrings;

	strncpy(ss->string, s, sizeof(ss->string) - 1);
	ss->string[sizeof(ss->string) - 1] = 0;
	ipr->strings[ipr->numstrings++] = ss;
	//printf("allocated scriptstring '%s' at %d\n", s, ipr->numstrings - 1);
	return ss;
}

scriptstring_t *ipr_findscriptstring(ipr_t *ipr, const char *s) {
	for (int i = 0; i < ipr->numstrings; i++) {
		if (!strcmp(ipr->strings[i]->string, s)) {
			return ipr->strings[i];
		}
	}
	return NULL;
}

void se_addstring(ipr_t *ipr, const char *s) {
	varval_t * vv = alloc_scriptvar_value(ipr);
	vv->flags = VF_LOCAL;

	scriptstring_t *ss = ipr_findscriptstring(ipr, s);
	if (ss == NULL) {

		scriptstring_t *ns = alloc_scriptstring(ipr, s);
		vv->stringindex = ns->index;
	}
	else
		vv->stringindex = ss->index;
	vv->type = IPR_VT_STRING;
	stack_push(ipr, (intptr_t)vv);
}

stockfunction_t *ipr_getstockfunctionbyname(ipr_t *ipr, const char *s) {

	for (int ii = 0; ii < ipr->numstockfunctionsets; ii++) {
		stockfunction_t *sfs = ipr->stockfunctionsets[ii];

		for (int i = 0; sfs[i].name; i++) {
			if (!strcmp(sfs[i].name, s))
				return &sfs[i];
		}
	}
	return NULL;
}

function_t *ipr_getfunctionbyname(ipr_t *ipr, const char *s) {

	scriptstring_t *ss = ipr_findscriptstring(ipr, s);

	if (!ss)
		return NULL;

	for (int i = 0; i < ipr->numfuncs; i++) {
		function_t *fp = ipr->functions[i];
		if (fp->name == ss->index)
			return fp;
	}
	return NULL;
}

#define expect(t) if(!ipr_expect(ipr, t)) { return; }	

varval_t *ipr_getscopedvarval(ipr_t *ipr, const char *str, var_t **out_var) {

	if (out_var)
		*out_var = NULL;

#if 0
	if (ipr->infunction) {

		int return_addr = ipr->stack[ipr->registers[REG_SP]];
		int numargs = ipr->stack[ipr->registers[REG_SP] - 1];
		//printf("numargs=%d\n",numargs);
		function_t *fp = (function_t*)ipr->stack[ipr->registers[REG_SP] - 2 - numargs];
		if (!fp) {
			ipr->error = 1;
			printf("nullpointer function_t*!\n");
			return NULL;
		}

		for (int i = 0; i < fp->argset->size; i++) {

			if (!strcmp(fp->argset->arguments[i].name, str))
				return (varval_t*)ipr->stack[ipr->registers[REG_SP] - 2 - i];

		}

	}
#endif

	if (ipr->curfunc) {
		argset_t *set = ipr->curfunc->argset;
		for (int i = 0; i < set->size; i++) {
			if (!strcmp(set->arguments[i].name, str)) {
				//if (i >= se_argc(ipr))
					//return NULL;
				varval_t *v = se_argv(ipr, i);
				return v;
			}
		}
	}

	//search for globals

	for (int i = 0; i < IPR_MAX_VARIABLES; i++) {
		var_t *v = ipr->variables[i];

		if (!v)
			continue;

		if (!strcmp(v->name, str)) {
			if (out_var)
				*out_var = v;
			return v->value;
		}

	}

	return NULL;
}

int ipr_function_call(ipr_t *ipr) {
	int start_tk_pos = ipr->curpos;

#define stmt_expect(tk) if(!ipr_expect(ipr,tk)) { \
								return ipr_error(ipr, E_IPR_ERROR_UNEXPECTED_TOKEN); \
							}
	if (ipr_accept(ipr, TK_IDENT)) {

		if (ipr_accept(ipr, TK_LPAREN)) { //can only be function call

			char func_name[128] = { 0 };
			sprintf_s(func_name, "%s", ipr->string);

			function_t *func = ipr_getfunctionbyname(ipr, func_name);
			stockfunction_t *sf = NULL;

			if (!func) {

				sf = ipr_getstockfunctionbyname(ipr, func_name);

				if (!sf) {
					printf("function '%s' not found\n", func_name);
					return ipr_error(ipr, E_IPR_ERROR_FUNCTION_DOES_NOT_EXISTS);
				}
			}

			bool got_rparen = false;

			int numargs = 0;

			do {
				if (ipr_accept(ipr, TK_EOF)) {
					ipr_pop_function_arguments_from_stack(ipr, numargs);
					return ipr_error(ipr, E_IPR_ERROR_EOF);
				}
				else if (ipr_accept(ipr, TK_RPAREN)) {
					got_rparen = true;
					break;
				}
				else {
					++numargs;

					{

						if (ipr_expression(ipr))
							return ipr->error;
					}

					int tk = ipr->token;

					varval_t *vv = alloc_scriptvar_value(ipr);

					copy_varval(vv, &ipr->lval);
					//printf("pushed %d > %d", numargs, ipr->lval.integer);
					stack_push(ipr, (intptr_t)vv);
				}
			} while (ipr_accept(ipr, TK_COMMA));

			//printf("numargs=%d\n", numargs);
			stack_push(ipr, numargs);
			stack_push(ipr, ipr->func_args_sp); //save :D:D:D:D
			ipr->func_args_sp = ipr->thrunner->sp;
			stack_push(ipr, (intptr_t)ipr->curfunc);
			stack_push(ipr, 0xbad); //tmp

			int save_pos_tmp = ipr->curpos;

			int tk = ipr_next_symbol(ipr);
			while (tk != TK_SEMICOLON) {
				//printf("tk = %s\n", lex_token_strings[tk]);
				if (tk == TK_EOF) {
					ipr_pop_function_arguments_from_stack(ipr, -1);
					return ipr_error(ipr, E_IPR_ERROR_EOF);
				}
				tk = ipr_next_symbol(ipr);
			}

			stack_pop(ipr); //pop bad
			stack_push(ipr, ipr->curpos + 1);

			ipr->curpos = save_pos_tmp;

			if (func != NULL) {

				if (numargs != func->argset->size) {
					ipr_pop_function_arguments_from_stack(ipr,-1);
					printf("arguments and parameter size don't match up!\n");
					return ipr_error(ipr, E_IPR_ERROR);
				}

				if (ipr_function(ipr, func))
					return ipr->error;

			} else if(sf != NULL) {
				varval_t *vv;

				ipr->lastcalledfunc = NULL;

				if (sf->call(ipr)) {
					vv = (varval_t*)stack_pop(ipr);
					ipr->func_return_value = vv;
				}
			}

			int ret_addr = ipr_pop_function_arguments_from_stack(ipr, -1);

			if (ipr->error)
				return ipr->error;

			if (!got_rparen)
				stmt_expect(TK_RPAREN);

			ipr->curpos = ret_addr;
		}
	}

	return E_IPR_ERROR_NONE;
}

int ipr_factor(ipr_t *ipr) {
	int start_tk_pos = ipr->curpos;

	if (ipr_accept(ipr, TK_LBRACK)) {
		printf("DEFINING ARRAY!! //TODO");
		stmt_expect(TK_RBRACK);
	} else if (ipr_accept(ipr, TK_IDENT)) {

		if (ipr_accept(ipr, TK_DOT)) {

			varval_t *val = ipr_getscopedvarval(ipr, ipr->string, NULL);

			if (!val)
				return ipr_error(ipr, E_IPR_ERROR_VARIABLE_DOES_NOT_EXIST);

			if (!ipr_accept(ipr, TK_IDENT))
				return ipr_error(ipr, E_IPR_ERROR_UNEXPECTED_TOKEN);

			if (val->type != IPR_VT_STRING) {
				printf("can only be used on strings for now.\n");
				return ipr_error(ipr, E_IPR_ERROR);
			}

			if (!strcmp(ipr->string, "length")) {
				ipr->lval.type = IPR_VT_INT;
				ipr->lval.integer = strlen(varval_to_string(ipr, val));
			}
			else
				return ipr_error(ipr, E_IPR_ERROR);

		} else if (ipr_accept(ipr, TK_LBRACK)) {
			varval_t *val = ipr_getscopedvarval(ipr, ipr->string, NULL);

			if (!val)
				return ipr_error(ipr, E_IPR_ERROR_VARIABLE_DOES_NOT_EXIST);

			if(ipr_factor(ipr))
				return ipr_error(ipr, E_IPR_ERROR_UNEXPECTED_TOKEN);

			if (ipr->lval.type != IPR_VT_INT && ipr->lval.type!=IPR_VT_FLOAT) {
				printf("only numeric indexes\n");
				return ipr_error(ipr, E_IPR_ERROR);
			}

			if (val->type != IPR_VT_STRING) {
				printf("can only be used on strings for now.\n");
				return ipr_error(ipr, E_IPR_ERROR);
			}

			const char *valstr = varval_to_string(ipr, val);
			int idx = ipr->lval.type==IPR_VT_INT?ipr->lval.integer:(int)ipr->lval.number;
			if (idx < 0 || idx > strlen(valstr)) {
				printf("out of bounds!!!\n");
				return ipr_error(ipr, E_IPR_ERROR);
			}

			ipr->lval.type = IPR_VT_CHAR;
			ipr->lval.character = valstr[idx];

			stmt_expect(TK_RBRACK);

		} else if (ipr_accept(ipr, TK_LPAREN)) {
			ipr->curpos = start_tk_pos;

			if (ipr_function_call(ipr))
				return ipr->error;

			if (!ipr->func_return_value) {
				printf("function does not return with a value!!!\n");
				return ipr_error(ipr, E_IPR_ERROR);
			}
			copy_varval(&ipr->lval, ipr->func_return_value);
			if (ipr->func_return_value) {
				delete_scriptvalue(ipr, ipr->func_return_value);
				ipr->func_return_value = NULL;
			}
		}
		else {
			varval_t *val = ipr_getscopedvarval(ipr, ipr->string, NULL);
			copy_varval(&ipr->lval,val);
		}
	}
	else if (ipr_accept(ipr, TK_STRING)) {
		scriptstring_t *ss = ipr_findscriptstring(ipr, ipr->string);
		ipr->lval.type = IPR_VT_STRING;
		if (ss == NULL) {

			scriptstring_t *ns = alloc_scriptstring(ipr, ipr->string);
			ipr->lval.stringindex = ns->index;
		}
		else
			ipr->lval.stringindex = ss->index;
	} else if(ipr_accept(ipr,TK_FLOAT)) {
		ipr->lval.type = IPR_VT_FLOAT;
		ipr->lval.number = ipr->number;
	} else if (ipr_accept(ipr, TK_INT)) {

		ipr->lval.type = IPR_VT_INT;
		ipr->lval.integer = ipr->integer;
	} else if (ipr_accept(ipr, TK_LPAREN)) {
		if (ipr_condition(ipr))
			return ipr->error;

		if (!ipr_expect(ipr, TK_RPAREN))
			return ipr_error(ipr, E_IPR_ERROR_SYNTAX);
	} else {
		printf("factor: syntax error got %s\n", lex_token_strings[ipr->token]);
		return ipr_error(ipr, E_IPR_ERROR_SYNTAX);
	}
	return ipr_error(ipr, E_IPR_ERROR_NONE);
}

int ipr_term(ipr_t *ipr) {
	float total = 1;

	if (ipr_accept(ipr, TK_MINUS))
		total *= -1;

	if (ipr_factor(ipr))
		return ipr->error;

	total *= (ipr->lval.type == IPR_VT_INT ? (float)ipr->lval.integer : ipr->lval.number);

#if 1
	while (ipr_accept(ipr,TK_DIVIDE) || ipr_accept(ipr,TK_MULTIPLY)) {
		int tk = ipr->token;

		if (ipr_accept(ipr, TK_MINUS))
			total *= -1;

		if (ipr_factor(ipr))
			return ipr->error;

		if (tk == TK_MULTIPLY)
			total *= varval_to_float(ipr, &ipr->lval);
		else if (tk == TK_DIVIDE) {
			if(!ipr->lval.integer)
				return ipr_error(ipr, E_IPR_ERROR_DIVIDE_BY_ZERO);
			total /= varval_to_float(ipr, &ipr->lval);
		}
		//fix this automatically turns everything to floats rofl
		ipr->lval.type = IPR_VT_FLOAT;
		ipr->lval.number = total;
	}
#endif
	return ipr_error(ipr, E_IPR_ERROR_NONE);
}

int ipr_expression(ipr_t *ipr) {

	float total = 1;

	while (ipr_accept(ipr, TK_PLUS))
		total *= 1;
	while (ipr_accept(ipr, TK_MINUS))
		total *= -1;

	if (ipr_term(ipr))
		return ipr->error;

#define test_is_num(x) ((x)->type == IPR_VT_INT || (x)->type == IPR_VT_FLOAT)

	total *= (ipr->lval.type == IPR_VT_INT ? (float)ipr->lval.integer : ipr->lval.number); //for numbers
	/* above wont make sense for strings will be some negative or positive stringindex hehe */

	while (1) {
		varval_t copy;
		copy_varval(&copy, &ipr->lval);

		if (!ipr_accept(ipr, TK_PLUS) && !ipr_accept(ipr, TK_MODULO) && !ipr_accept(ipr, TK_MINUS))
			break;

		int tk = ipr->token;

		{
			if (ipr_term(ipr))
				return ipr->error;

			bool is_num = !(copy.type == IPR_VT_STRING || ipr->lval.type == IPR_VT_STRING);

			if (!is_num) {
				const char *str1 = varval_to_string(ipr, &copy);
				//const char *str1 = ipr->strings[copy.stringindex]->string;
				//nope if the first isnt a string hehe
				const char *str2 = varval_to_string(ipr, &ipr->lval);
				int new_str_len = strlen(str1) + strlen(str2) + 1;
				char *new_str = (char*)xmalloc(new_str_len);

				snprintf(new_str, new_str_len, "%s%s", str1, str2);
				//printf("new string = %s\n", new_str);

				scriptstring_t *ns = alloc_scriptstring(ipr, new_str);
				ipr->lval.stringindex = ns->index;
				ipr->lval.type = IPR_VT_STRING;
				xfree(new_str); //free the result
			}
			else {
				float nn = (ipr->lval.type == IPR_VT_INT ? (float)ipr->lval.integer : ipr->lval.number); //for numbers

				switch (tk) {
				case TK_PLUS:
					total += nn;
					break;

				case TK_MINUS:
					total -= nn;
					break;

				case TK_MODULO:
					total = (int)total % (int)nn; //not sure maybe fix this?? or change
					break;
				}
			}

			/* for now all becomes a float rofl */
			if (is_num) {
				//printf("total=%f\n", total);
				ipr->lval.number = total;
				ipr->lval.type = IPR_VT_FLOAT;
			}
		}
	}
	return E_IPR_ERROR_NONE;
}

int ipr_condition(ipr_t *ipr) {

	int cond = 0;

	do {
		int prev_cond = cond;
		int utk = ipr->token;

		if (ipr_accept(ipr, TK_EOF))
			return ipr_error(ipr, E_IPR_ERROR_EOF);

		if (ipr_accept(ipr, TK_RPAREN))
			break;

			if (ipr_accept(ipr, TK_NOT)) {
				if (ipr_expression(ipr))
					return ipr->error;
				cond = (varval_to_float(ipr,&ipr->lval) == 0);
			} else {
				if (ipr_expression(ipr))
					return ipr->error;

				float cmp = varval_to_float(ipr, &ipr->lval);
				//printf("token=%s\n", lex_token_strings[ipr->token]);

				if (ipr_accept(ipr, TK_NOTEQUAL) || ipr_accept(ipr, TK_GEQUAL) || ipr_accept(ipr, TK_LEQUAL) || ipr_accept(ipr, TK_EQUALS) || ipr_accept(ipr, TK_LESS) || ipr_accept(ipr, TK_GREATER)) {
					int tk = ipr->token;

					if (ipr_expression(ipr))
						return ipr->error;
					//printf("cmp=%d,factor=%d,token=%s\n", cmp, ipr->lval.integer,lex_token_strings[ipr->token]);
					
					float cmp_val = varval_to_float(ipr, &ipr->lval);

					if (tk == TK_GEQUAL)
						cond = (cmp >= cmp_val);
					else if (tk == TK_LEQUAL)
						cond = (cmp <= cmp_val);
					else if (tk == TK_EQUALS)
						cond = (cmp == cmp_val);
					else if (tk == TK_GREATER)
						cond = (cmp > cmp_val);
					else if (tk == TK_LESS)
						cond = (cmp < cmp_val);
					else if (tk == TK_NOTEQUAL)
						cond = (cmp != cmp_val);
					
				} else {

					return E_IPR_ERROR_NONE;
#if 0
					if (ipr->token != TK_RPAREN) {
						printf("condition: invalid operator (%s)", lex_token_strings[ipr->token]);
						return ipr_error(ipr, E_IPR_ERROR_SYNTAX);
					}
#endif
				}
			}

			if (utk == TK_AND_AND)
				cond = (cond&&prev_cond);
			else if (utk == TK_OR_OR)
				cond = (cond || prev_cond);
	} while (ipr_accept(ipr,TK_AND_AND)||ipr_accept(ipr,TK_OR_OR));
	ipr->lval.type = IPR_VT_INT;
	ipr->lval.integer = cond;

	return E_IPR_ERROR_NONE;
}

int ipr_assign(ipr_t *ipr) {

	var_t *var = NULL;

	varval_t *vv = ipr_getscopedvarval(ipr, ipr->string, &var);

	if (ipr_accept(ipr, TK_ASSIGN)) {
		if (!var && !vv) {
			var = alloc_scriptvar(ipr);

			if (!var)
				return ipr_error(ipr, E_IPR_ERROR_OUT_OF_MEMORY);

			varval_t *new_val = NULL;

			new_val = alloc_scriptvar_value(ipr);
			if (!new_val) {
				delete_scriptvalue(ipr, new_val);
				return ipr_error(ipr, E_IPR_ERROR_OUT_OF_MEMORY);
			}

			strncpy(var->name, ipr->string, sizeof(var->name) - 1);
			var->name[sizeof(var->name) - 1] = '\0';

			{

				if (ipr_expression(ipr)) {
					delete_scriptvalue(ipr, new_val); //mem leak fixed
					return ipr->error;
				}

				if (var->value)
					delete_scriptvar_value(ipr, var);
				var->value = new_val;
				copy_varval(var->value, &ipr->lval);

				//printf("invalid assignment\n");
				//return ipr_error(ipr, E_IPR_ERROR_SYNTAX);
			}
		} else {

			//set existing local variable ;)
			if (ipr_expression(ipr))
				return ipr->error;
			copy_varval(vv, &ipr->lval);
		}
		return ipr_error(ipr, E_IPR_ERROR_NONE);
	}

	if (!vv)
		return ipr_error(ipr, E_IPR_ERROR_VARIABLE_HAS_NO_VALUE);

	if (vv->type != IPR_VT_INT && vv->type!=IPR_VT_FLOAT) {
		printf("not allowed to use these operators on anything other than numbers.\n");
		return ipr_error(ipr, E_IPR_ERROR_SYNTAX);
	}

	vv->number = vv->type==IPR_VT_INT ? (float)vv->integer : vv->number;
	vv->type = IPR_VT_FLOAT; //idk for now let's just calculate all in floats

	float fac = 0.0f;

	int tk_savepos = ipr->curpos;

	int tk = ipr_next_symbol(ipr);

	if (tk == TK_PLUS_PLUS || tk == TK_MINUS_MINUS) {
		fac = (vv->number + ((tk == TK_PLUS_PLUS) ? 1 : -1));
		vv->integer = (int)fac;
		vv->type = IPR_VT_INT;
		return E_IPR_ERROR_NONE;
	}

	if (ipr_expression(ipr))
		return ipr->error;

	float lval_num = ipr->lval.type==IPR_VT_FLOAT ? ipr->lval.number : (float)ipr->lval.integer;

	if (tk == TK_DIVIDE_ASSIGN || tk == TK_MULTIPLY_ASSIGN || tk == TK_PLUS_ASSIGN || tk == TK_MINUS_ASSIGN) {
		if (tk == TK_DIVIDE_ASSIGN) {
			if (!lval_num) {
				printf("DIVIDE BY ZERO\n");
				return ipr_error(ipr, E_IPR_ERROR_DIVIDE_BY_ZERO);
			}

			vv->number /= lval_num;
		}
		else if (tk == TK_MULTIPLY_ASSIGN)
			vv->number *= lval_num;
		else if (tk == TK_PLUS_ASSIGN)
			vv->number += lval_num;
		else if (tk == TK_MINUS_ASSIGN)
			vv->number -= lval_num;
		else {
			ipr->curpos = tk_savepos;
		}

	}
	return E_IPR_ERROR_NONE;
}

function_t *alloc_scriptfunction(ipr_t *ipr) {

	function_t *fp = (function_t*)xmalloc(sizeof(function_t));
	if (!fp)
		return fp;
	memset(fp, 0, sizeof(function_t));
	return fp;
}

void delete_scriptfunction(ipr_t *ipr, function_t *fp) {
	if (!fp)
		return;
	var_t *vv;
	for (int i = 0; i < fp->argset->size; i++) {
		vv = &fp->argset->arguments[i];

		if (!vv)
			continue;

		if (vv->value)
			delete_scriptvar_value(ipr, vv);
	}

	if(fp->argset->arguments)
		xfree(fp->argset->arguments); //not sure maybe cuz realloc vs (x)free //TODO add own reimplementation of (x)realloc?

	xfree(fp->argset);

	xfree(fp);
}

#define MAX_PROGRAM_SIZE 65000

unsigned char program[MAX_PROGRAM_SIZE];
int programindex = 0;

void pg_addbyte(unsigned char b) {
	if (programindex + 1 >= MAX_PROGRAM_SIZE) {
		printf("error max program\n");
		exit(-1);
	}
	program[programindex++] = b;
}

void pg_addint(int i) {
	if (programindex + sizeof(int) >= MAX_PROGRAM_SIZE) {
		printf("max progam error\n");
		exit(-1);
	}
	program[programindex++] = i & 0xff;
	program[programindex++] = (i >> 8) & 0xff;
	program[programindex++] = (i >> 16) & 0xff;
	program[programindex++] = (i >> 24) & 0xff;
}

void ipr_initialize(ipr_t *ipr) {

	memset(ipr, 0, sizeof(*ipr));
#if 0
	ipr->scriptbuf = NULL;
	ipr->curpos = ipr->scriptsize = ipr->stringindex = 0;
#endif //already memsetting
	memset(ipr->string, 0, sizeof(ipr->string));

	memset(&ipr->lval, 0, sizeof(ipr->lval));
	ipr->lval.type = IPR_VT_NULL;

	for (int i = 0; i < sizeof(ipr->strings) / sizeof(ipr->strings[i]); i++)
		ipr->strings[i] = NULL;
	ipr->numstrings = 0;

	for (int i = 0; i < IPR_MAX_VARIABLES; i++)
		ipr->variables[i] = NULL;
	ipr->numvars = 0;


	/* add all the builtin funcs here for now */

	extern stockfunction_t scriptfunctions[];
	extern stockfunction_t scriptfunctions_graphics[];

	ipr_register_stockfunction_set(ipr, scriptfunctions);
	ipr_register_stockfunction_set(ipr, scriptfunctions_graphics);
}

argset_t *ipr_build_argset_from_parentheses(ipr_t *ipr) {
	argset_t *set = (argset_t*)xmalloc(sizeof(argset_t));
	if (!set) {
		printf("out of memory\n");
		return set; //returning NULL memory error;
	}
	set->arguments = NULL;
	set->size = 0;

	if (ipr_accept(ipr, TK_RPAREN))
		return set;

	do {
		if (ipr_accept(ipr, TK_IDENT)) {

			++set->size;

			set->arguments = (var_t*)realloc(set->arguments, sizeof(var_t) * set->size);

			if (!set->arguments) {
				xfree(set->arguments);
				return NULL;
			}

			var_t *sv = &set->arguments[set->size - 1];
			sv->flags = 0;
			sv->value = NULL;
			strncpy(sv->name, ipr->string, sizeof(sv->name) - 1);
			sv->name[sizeof(sv->name) - 1] = '\0';
		}
		else {
			//do else? or just leave huehe
			printf("invalid argument definition, got token(%s)\n", lex_token_strings[ipr->token]);
			return NULL;
		}

	} while (ipr_accept(ipr, TK_COMMA));

	return set;
}

int ipr_script_user_functions(ipr_t *ipr) {

	if (ipr->error)
		return ipr_error(ipr, E_IPR_ERROR);

	if (!ipr_accept(ipr, TK_IDENT))
		return ipr_error(ipr, E_IPR_ERROR_UNEXPECTED_TOKEN);

	if (!ipr_accept(ipr, TK_LPAREN))
		return ipr_error(ipr, E_IPR_ERROR_UNEXPECTED_TOKEN);

	char func_name[128] = { 0 };
	snprintf(func_name, sizeof(func_name) - 1, "%s", ipr->string); //unsafe??

	int savepos = ipr->curpos; //save current position to restore later on

	int rparen = ipr_accept(ipr, TK_RPAREN);

	while (!rparen) {
		if (ipr->token == TK_EOF) {
			printf("unexpected EOF\n");
			return ipr_error(ipr, E_IPR_ERROR_UNEXPECTED_TOKEN);
		}
		++ipr->curpos;
		rparen = ipr_accept(ipr, TK_RPAREN);
	}
	//replace someday?? unneeded anymore?

	int is_func_declaration = 0;

	if (ipr_accept(ipr, TK_LBRACE))
		is_func_declaration = 1;

	ipr->curpos = savepos; //restore the position to before the function ish

	if (!is_func_declaration)
		return ipr_error(ipr, E_IPR_ERROR_UNEXPECTED_FUNCTION_DECLARATION); //in open file we would only accept function declarations riteee :d we're not javascript m8 here

	function_t *fp = ipr_getfunctionbyname(ipr, func_name);

	if (NULL != fp)
		return ipr_error(ipr, E_IPR_ERROR_FUNCTION_ALREADY_EXISTS); //function already exists.

	fp = alloc_scriptfunction(ipr);

	if (!fp)
		return ipr_error(ipr, E_IPR_ERROR_OUT_OF_MEMORY);

	scriptstring_t *ss = alloc_scriptstring(ipr, func_name);

	if (!ss) {
		delete_scriptfunction(ipr, fp);
		return ipr_error(ipr, E_IPR_ERROR_OUT_OF_MEMORY);
	}

	fp->name = ss->index;

	argset_t *set = ipr_build_argset_from_parentheses(ipr); //fix this someday maybe too bloaty? just simple names should be fine?

	if (!set) {
		delete_scriptfunction(ipr, fp);
		return ipr_error(ipr, E_IPR_ERROR);
	}

	fp->argset = set;

#define user_func_expect(tk) if(!ipr_expect(ipr,tk)) { \
								delete_scriptfunction(ipr,fp); \
								return ipr_error(ipr, E_IPR_ERROR_UNEXPECTED_TOKEN); \
							}
	if (set->size) {
		user_func_expect(TK_RPAREN);
	}

	user_func_expect(TK_LBRACE);

	int num_braces = 1;

	fp->codepos = ipr->curpos - 1;

	int num_rbraces = 0;

	while (num_rbraces < num_braces) {
		if (ipr->token == TK_EOF) {
			delete_scriptfunction(ipr, fp);
			return ipr_error(ipr, E_IPR_ERROR_UNEXPECTED_TOKEN);
		}

		if (ipr_accept(ipr, TK_LBRACE)) {
			++num_braces;
		}

		++ipr->curpos;

		int rbrace = ipr_accept(ipr, TK_RBRACE);
		if (rbrace) {
			++num_rbraces;
		}
	}

	fp->size = ipr->curpos - fp->codepos;
#if 0
	printf("FUNCTION start = %c, end = %c\n", ipr->scriptbuf[fp->codepos], ipr->scriptbuf[fp->codepos + fp->size]);
	printf("current tk = %s\n", lex_token_strings[ipr->token]);
#endif
	ipr->functions[ipr->numfuncs++] = fp;

	return ipr_error(ipr, E_IPR_ERROR_NONE);
}

void ipr_print_current_line(ipr_t *ipr) {
	return;//tmp
	char line[1024] = { 0 };
	int linen = 0;

	int cur = ipr->curpos;

	while (cur > 0) {
		if (ipr->scriptbuf[cur] == '\n') {
			break;
		}
		--cur;
	}

	int start = cur + 1;

	cur = ipr->curpos;

	while (1) {
		if (!ipr->scriptbuf[cur])
			break;

		if (ipr->scriptbuf[cur] == '\n')
			break;

		++cur;
	}
	int end = cur - 1;

	if (start > end) {
		int tt = end;
		end = start;
		start = tt;
	}

	//printf("start=%d,end=%d\n", start, end);

#define _MASK_CUR_POS

	//let's just hope the line isn't over > 1023
	for (int i = start; i < end; i++) {
#ifdef _MASK_CUR_POS
		if (i == ipr->curpos)
			line[linen++] = '@';
#endif

		line[linen++] = ipr->scriptbuf[i];

#ifdef _MASK_CUR_POS
		if (i == ipr->curpos)
			line[linen++] = '@';
#endif
	}

	printf("LINE = %s\n", line);
}

int ipr_get_block_end(ipr_t* ipr, int *block_end) {
#define texpect(x) if(!ipr_expect(ipr,x)) return ipr_error(ipr, E_IPR_ERROR_SYNTAX);

	int save_pos = ipr->curpos;

	//texpect(TK_LBRACE)

	ipr_accept(ipr, TK_LBRACE); //optional i suppose

	int num_lbraces = 1;
	int num_rbraces = 0;

	while (num_rbraces < num_lbraces) {
		if (ipr_accept(ipr, TK_EOF))
			return ipr_error(ipr, E_IPR_ERROR_EOF);
		else if (ipr_accept(ipr, TK_LBRACE))
			++num_lbraces;
		else if (ipr_accept(ipr, TK_RBRACE))
			++num_rbraces;
		else
			++ipr->curpos;
	}

	*block_end = ipr->curpos;
	ipr->curpos = save_pos;

	return E_IPR_ERROR_NONE;
}

int ipr_pop_function_arguments_from_stack(ipr_t *ipr, int numargs) {
	int ret_addr = 0;
	if (numargs == -1) {
		ret_addr = stack_pop(ipr); //ret address
		ipr->curfunc = (function_t*)stack_pop(ipr);
		ipr->func_args_sp = stack_pop(ipr); //prev args sp
		numargs = stack_pop(ipr); //numargs
	}
	for (int i = 0; i < numargs; i++) {
		varval_t *v = (varval_t*)stack_pop(ipr);
		delete_scriptvalue(ipr, v);
	}
	return ret_addr;
}

int ipr_statement(ipr_t *ipr) {

	if (ipr->loopbreak || (ipr->curfunc && ipr->func_return_value)) {
		//printf("curfunc=%s,return_val=%s\n", ipr->strings[ipr->curfunc->name]->string, vartypestrings[ipr->func_return_value->type]);
		int tk = ipr_next_symbol(ipr);
		while (tk != TK_SEMICOLON) {
			//printf("tk=%s\n", lex_token_strings[tk]);
			tk = ipr_next_symbol(ipr);
		}
		//ipr_next_symbol(ipr);
		return E_IPR_ERROR_NONE; //just ignore
	}

	if (ipr->error)
		return ipr->error;

	int start_tk_pos = ipr->curpos; //for while and longer texts

#define stmt_expect(tk) if(!ipr_expect(ipr,tk)) { \
								return ipr_error(ipr, E_IPR_ERROR_UNEXPECTED_TOKEN); \
							}

	
	if(ipr_accept(ipr, TK_R_BREAK)) {
		stmt_expect(TK_SEMICOLON);
		ipr->loopbreak = true;
	}
	else if (ipr_accept(ipr, TK_RETURN)) {

		if (ipr_expression(ipr))
			return ipr->error;

		varval_t *vv = alloc_scriptvar_value(ipr);
		copy_varval(vv, &ipr->lval);
		ipr->func_return_value = vv;
		stmt_expect(TK_SEMICOLON);
	} else if(ipr_accept(ipr, TK_R_WAIT)) {
		if (ipr_factor(ipr))
			return ipr->error;

		ipr->thrunner->wait += (((ipr->lval.type == IPR_VT_INT) ? ipr->lval.integer : ipr->lval.number) * 1000);
		//printf("wait=%f\n", ipr->thrunner->wait);
		stmt_expect(TK_SEMICOLON);
		ipr->thrunner->position = ipr->curpos;
	} else if (ipr_accept(ipr, TK_WAITTILL)) {
		if (ipr_factor(ipr))
			return ipr->error;

		Sleep(ipr->lval.integer);

		stmt_expect(TK_SEMICOLON);
	}
	else if (ipr_accept(ipr, TK_IF) || ipr_accept(ipr, TK_WHILE) || ipr_accept(ipr, TK_R_FOR)) {

		int tk = ipr->token;
		int loop_start_pos = ipr->curpos;

		int block_end = 0;

#define SKIP_TILL_END_OF_STATEMENT { \
			int tk = ipr_next_symbol(ipr); \
			while (tk != TK_SEMICOLON && tk != TK_RPAREN) { \
				tk = ipr_next_symbol(ipr); \
			} \
		}

		int post_statement_pos = 0;

	loop_begin:
		ipr->curpos = loop_start_pos;

		stmt_expect(TK_LPAREN)

			if (tk == TK_R_FOR) {
				if (block_end) { //only initialize once
					SKIP_TILL_END_OF_STATEMENT
				}
				else {
					if (ipr_statement(ipr))
						return ipr->error;
				}
			}

		if (ipr_condition(ipr))
			return ipr_error(ipr, E_IPR_ERROR);

		if (tk == TK_R_FOR) {
			stmt_expect(TK_SEMICOLON)
		}

		int cond = (ipr->lval.integer);

		if (tk == TK_R_FOR) {
			post_statement_pos = ipr->curpos;
			SKIP_TILL_END_OF_STATEMENT
				--ipr->curpos; //eat up the next rparen
		}

		stmt_expect(TK_RPAREN)

			if (!block_end) {
				if (ipr_get_block_end(ipr, &block_end))
					return ipr->error;
			}

		if (cond) {
			if (ipr_run_block(ipr, ipr->curpos, block_end))
				return ipr->error;
			if (tk == TK_WHILE || tk == TK_R_FOR) {
				if (!ipr->loopbreak) {
					//do the post statement (on for)
					if (post_statement_pos) {
						ipr->curpos = post_statement_pos;
						if (ipr_statement(ipr))
							return ipr->error;
					}
					goto loop_begin;
				}
				ipr->loopbreak = false;
			}
		}

		ipr->curpos = block_end;

		if (tk == TK_IF && ipr_accept(ipr, TK_R_ELSE)) {
			if (ipr_get_block_end(ipr, &block_end))
				return ipr->error;
			if (!cond) {
				if (ipr_run_block(ipr, ipr->curpos, block_end))
					return ipr->error;
			}
			ipr->curpos = block_end;
		}
	} else if(ipr_accept(ipr, TK_R_THREAD)) {
		start_tk_pos = ipr->curpos;
		ipr_accept(ipr, TK_IDENT);
		goto do_func_call;
	} else {

		do {
			if (ipr_accept(ipr, TK_LBRACE)) {
				--ipr->curpos;

				if (ipr_block(ipr))
					return ipr->error;
			} else if (ipr_accept(ipr, TK_IDENT)) {
				do_func_call:
				if (ipr_accept(ipr, TK_LPAREN)) {
					ipr->curpos = start_tk_pos;
					if (ipr_function_call(ipr))
						return ipr->error;

					copy_varval(&ipr->lval, ipr->func_return_value);
					if (ipr->func_return_value) {
						delete_scriptvalue(ipr, ipr->func_return_value);
						ipr->func_return_value = NULL;
					}
				} else {
					if (ipr_assign(ipr))
						return ipr->error;
				}
			} else {
				//ipr_print_current_line(ipr);
				//__asm int 3
				printf("statement: unexpected token %s\n", lex_token_strings[ipr->token]);
				return ipr_error(ipr, E_IPR_ERROR_UNEXPECTED_TOKEN);
			}
		} while (ipr_accept(ipr, TK_COMMA));
	}
	ipr_accept(ipr, TK_SEMICOLON);

	return ipr_error(ipr, E_IPR_ERROR_NONE);
}

int ipr_block(ipr_t *ipr) {
	if (ipr_accept(ipr, TK_LBRACE)) {

		while (!(ipr->thrunner->wait > 0)) {

			if (ipr_accept(ipr, TK_EOF))
				return ipr_error(ipr, E_IPR_ERROR_EOF);

			if (ipr_accept(ipr, TK_RBRACE))
				break;

			int stmt = ipr_statement(ipr);

			if (stmt)
				return stmt;

		}
	}
	else
		return ipr_error(ipr, E_IPR_ERROR_SYNTAX);

	return ipr_error(ipr, E_IPR_ERROR_NONE);
}

int ipr_run_block(ipr_t *ipr, int start, int end) {

	int save_pos = ipr->curpos;

	ipr->curpos = start;
#if 0
	printf("===================SCOPED BLOCK========================\n");
	for (int i = start; i < end; i++) {
		fputc(ipr->scriptbuf[i], stdout);
	}
	printf("\n=====================================================\n");
#endif
	while (ipr->curpos < end) {
		int block = ipr_block(ipr);

		if (block)
			return block;
	}

	ipr->curpos = save_pos;

	return ipr_error(ipr, E_IPR_ERROR_NONE);

}

int ipr_function(ipr_t *ipr, function_t *func) {

	ipr->curfunc = func;
	ipr->lastcalledfunc = func;
	int save_pos = ipr->curpos;

	int start = func->codepos;

	ipr->curpos = start;
	int end = start + func->size;
#if 0
	printf("==================FUNCTION=============================\n");
	for (int i = start; i < end; i++) {
		fputc(ipr->scriptbuf[i], stdout);
	}
	printf("\n=====================================================\n");
#endif
	while (!(ipr->thrunner->wait > 0) && ipr->curpos < end) {
		int block = ipr_block(ipr);

		if (block)
			return block;
	}

	ipr->curpos = save_pos;

	return ipr_error(ipr, E_IPR_ERROR_NONE);
}

scriptthread_t *alloc_scriptthread(ipr_t *ipr, function_t *func) {

	scriptthread_t *thr = (scriptthread_t*)xmalloc(sizeof(scriptthread_t));
	thr->func = func;
	thr->position = 0;
	thr->wait = 0;
	thr->done = true;
	memset(thr->stack, 0, sizeof(thr->stack));
	thr->sp = 0;

	int i;

	//find free spot

	for (i = 0; i < IPR_MAX_SCRIPT_THREADS; i++) {
		if (!ipr->threadrunners[i]) {
			ipr->threadrunners[i] = thr;
			++ipr->numthreadrunners;
			break;
		}
	}

	if (i == IPR_MAX_SCRIPT_THREADS) {

		//full
		printf("MAX SCRIPT THREADS REACHED\n");
		xfree(thr);
		thr = NULL;
	}

	return thr;
}


void ipr_remove_thread_runner(ipr_t *ipr, scriptthread_t *thr) {
#if 0
	for (int i = 0; i < IPR_MAX_SCRIPT_THREADS; i++) {
		if (!ipr->threadrunners[i])
			continue;
		if (ipr->threadrunners[i] == thr) {
			ipr->threadrunners[i]->done = true;
			--ipr->numthreadrunners;
			break;
		}
	}
#endif


	scriptthread_t *tmp = ipr->thrunner;

	ipr->thrunner = thr;

	if (ipr->func_return_value) {
		delete_scriptvalue(ipr, ipr->func_return_value);
		ipr->func_return_value = NULL;
	}
	if (!thr->done)
		ipr_pop_function_arguments_from_stack(ipr, -1); //clean the stack and free local vars

	ipr->thrunner = tmp;

	--ipr->numthreadrunners;
	thr->done = true;
}

void ipr_free_thread_handle(ipr_t *ipr, scriptthread_t *thr) {
	ipr_remove_thread_runner(ipr, thr); //remove if not already

	xfree(thr);
}

scriptthread_t *ipr_create_thread_handle(ipr_t *ipr, const char *str) {
	function_t *func = NULL;

	for (int i = 0; i < ipr->numfuncs; i++) {
		function_t *fp = ipr->functions[i];
		const char *func_name = ipr_get_string_from_index(ipr, fp->name);
		if (!strcmp(func_name, str)) {
			func = fp;
			break;
		}
	}

	if (!func) {
		printf("function '%s' was not found!\n", str);
		return NULL;
	}

	scriptthread_t *runner = alloc_scriptthread(ipr, func);
	return runner;
}

int ipr_exec_thread(ipr_t *ipr, scriptthread_t *thr, int numargs) {
	int retcode = E_IPR_ERROR_NONE;

	if (!thr) {
		printf("null script thread\n");
		return ipr_error(ipr, E_IPR_ERROR);
	}

	thr->done = false;
	ipr->thrunner = thr;
	thr->numargs = numargs;


	//get all the stuff from main stack

	for (int i = 0; i < numargs; i++) {
		varval_t *vv = (varval_t*)ipr->stack[ipr->registers[REG_SP]--]; //stack_pop() implementation
		stack_push(ipr, (intptr_t)vv);
	}

	//push the values u want to pass along before this func
	stack_push(ipr, numargs); //numargs
	stack_push(ipr, ipr->func_args_sp);
	ipr->func_args_sp = ipr->thrunner->sp;
	stack_push(ipr, (intptr_t)ipr->curfunc);
	stack_push(ipr, 0); //push curpos supposedly but 0 means = end, no return

	bool finished = true;

	{

		ipr->curfunc = thr->func;
		ipr->lastcalledfunc = thr->func;
		int save_pos = ipr->curpos;

		int start = thr->func->codepos;

		ipr->curpos = start;
		int end = start + thr->func->size;
#if 0
		printf("==================SCRIPT THREAD=============================\n");
		for (int i = start; i < end; i++) {
			fputc(ipr->scriptbuf[i], stdout);
		}
		printf("\n=====================================================\n");
#endif
		while (!(thr->wait > 0) && ipr->curpos < end) {
			int block = ipr_block(ipr);

			if (block) {
				retcode = block;
				ipr_print_current_line(ipr);
				printf("Script Error: %s (errorcode: %d) (token: %s)\n", ipr->errstr, ipr->error, lex_token_strings[ipr->token]);
				break;
			}
		}

		finished = (ipr->curpos == end);

		if (finished || retcode)
			ipr_remove_thread_runner(ipr, thr);

		ipr->curpos = save_pos; //doesnt matter rlly lol
	}
	ipr->thrunner = NULL;

	return retcode;
}

int ipr_run_current_threads(ipr_t *ipr) {
	int retcode = E_IPR_ERROR_NONE;
	
	for (int i = 0; i < IPR_MAX_SCRIPT_THREADS; i++) {
		if (!ipr->threadrunners[i])
			continue;
		scriptthread_t *thr = ipr->threadrunners[i];

		if (thr->done)
			continue;

		if (thr->wait > 0) {
			thr->wait -= FRAMETIME;
			//printf("wait = %d\n", thr->wait);
			continue;
		}

		ipr->thrunner = thr;

		ipr->curpos = thr->position;
		int end = thr->func->codepos + thr->func->size;

		printf("curpos=%d,end=%d,func=%s\n", ipr->curpos, end, ipr->strings[ipr->curfunc->name]->string);

		while (!(thr->wait > 0) && ipr->curpos < end) {

			if (ipr_accept(ipr, TK_EOF)) {
				retcode = E_IPR_ERROR_EOF;
				break;
			}

			if (ipr_accept(ipr, TK_RBRACE))
				break;

			int stmt = ipr_statement(ipr);

			retcode = stmt;
			if (retcode)
				break;
		}

		thr->position = ipr->curpos;

		if (thr->wait > 0)
			continue;

		ipr_remove_thread_runner(ipr, thr);
	}


	if (retcode) {
		ipr_print_current_line(ipr);
		printf("Script Error: %s (errorcode: %d) (token: %s)\n", ipr->errstr, ipr->error, lex_token_strings[ipr->token]);
	}

	return retcode;
}

void ipr_declare_functions(ipr_t *ipr) {

	while (!ipr->eof && !ipr->error) {

		if (ipr_accept(ipr, TK_EOF)) {
			ipr->eof = 1;
			break;
		}

		int e_code = ipr_script_user_functions(ipr);

		if (e_code) {
			printf("errorcode=%d at line %d (token = %s)\n", e_code, ipr->lineno, lex_token_strings[ipr->token]);
			break;
		}
	}
}

void interpret(ipr_t *ipr) {
	ipr_declare_functions(ipr);


	scriptthread_t *cb_main = ipr_create_thread_handle(ipr, "main");

	se_addint(ipr, __argc);

	if (ipr_exec_thread(ipr, cb_main, 1));

#if 1
	while (ipr->numthreadrunners) {
		if (ipr_run_current_threads(ipr))
			break;

		ipr->current_time += FRAMETIME;

		Sleep(FRAMETIME); //.05 frametime
	}
#endif
}

void ipr_cleanup(ipr_t *ipr) {

	for (int i = 0; i < IPR_MAX_SCRIPT_THREADS; i++) {
		if (!ipr->threadrunners[i])
			continue;
		ipr_free_thread_handle(ipr, ipr->threadrunners[i]);
	}

	for (int i = 0; i < ipr->numfuncs; i++) {
		dprintf("freed function '%s'\n", ipr_get_string_from_index(ipr, ipr->functions[i]->name));
		//xfree(ipr->functions[i]);
		delete_scriptfunction(ipr, ipr->functions[i]);
		ipr->functions[i] = NULL;
	}

	//cleanup here
	for (int i = 0; i < IPR_MAX_VARIABLES; i++) {
		if (!ipr->variables[i])
			continue;
		dprintf("freed variable '%s'\n", ipr->variables[i]->name);
		delete_scriptvar(ipr, ipr->variables[i]);
	}

	for (int i = 0; i < ipr->numstrings; i++) {
		dprintf("freed scriptstring '%s'\n", ipr->strings[i]->string);
		xfree(ipr->strings[i]);
	}


	xfree(ipr->scriptbuf);
}

void test_c_output() {

	return;

	printf("======C OUTPUT======\n");


	int i = 0;

	while (i < 5) {
		printf("%d ", rand());
		i++;
	}

	printf("\n======C OUTPUT======\n");
}

int main(int argc, char **argv) {
	const char *file_name = "file.i";

	if (argc >= 2)
		file_name = argv[1];

	srand(time(0));

	test_c_output();

	ipr_t ipr;

	ipr_initialize(&ipr);

	if (read_text_file(file_name, &ipr.scriptbuf, (int*)&ipr.scriptsize)) {
		printf("failed to read file!\n");
		return 0;
	}

	//printf("filesize = %d\n", ipr.scriptsize);

	interpret(&ipr);

	ipr_cleanup(&ipr);

	xmcheck();

	getchar();
}