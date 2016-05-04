#pragma once
#include <stdint.h> //intptr_t

typedef struct ipr_s ipr_t;

#define VM_STACK_SIZE 2048

typedef enum {
	REG_A,
	REG_B,
	REG_C,
	REG_D,
	REG_E,
	REG_F,
	REG_IP,
	REG_SP,

	e_vm_reg_len
} e_vm_registers;

static const char *e_vm_reg_names[] = {
	"A","B","C","D","E","F","IP","SP",NULL
};


typedef enum {
	PUSH,
	ADD,
	POP,
	HALT,
	SET,
	EQ,
	DEC,
	/* jmp would be SET, IP, <to>, */
	BRK,
	NOP,
	PRINT,
} e_opcodes;

#define IPR_MAX_VARIABLES 1024
#define IPR_MAX_ARGS 256

typedef enum {
	IPR_VT_INT,
	IPR_VT_FLOAT,
	IPR_VT_CHAR,
	IPR_VT_STRING,
	IPR_VT_ARRAY,
	IPR_VT_OBJECT,
	IPR_VT_NULL
} vartypes_t;

static const char *vartypestrings[] = {
	"int",
	"float",
	"char",
	"string (indexed)",
	"array",
	"object",
	"null",
	NULL
};

#define VF_LOCAL (1<<0)

typedef struct varval_s varval_t;

typedef struct {
	int key;
	varval_t *value;
} scriptkvp_t;

#define MAX_SCRIPT_KEY_VALUE_PAIRS 1024

typedef struct {
	int size;
	scriptkvp_t *pairs;
} scriptarray_t;

struct varval_s {
	vartypes_t type;
	int flags;
	union {
		char character;
		int integer;
		intptr_t ptr;
		int index; //more generic
		int stringindex;
		float number;
	};
};

static void copy_varval(varval_t *to, varval_t *from) {
	if (!from) {
		to->flags = 0;
		to->type = IPR_VT_NULL;
		to->ptr = 0;
		return;
	}

	if (!to)
		return;

	*to = *from;
}

typedef struct {
	char name[256];
	int flags;
	varval_t *value;
	int refs;
} var_t;

typedef struct {
	var_t *arguments;
	int size;
} argset_t;

typedef struct {
	const char *name;
	int(*call)(ipr_t*);
} stockfunction_t;

typedef struct {
	int name;
	int codepos;
	int size;
	argset_t *argset;
} function_t;

typedef struct {
	intptr_t stack[VM_STACK_SIZE]; //local thread stack ish? idk
	int sp; //stack ptr ofc
	int numargs; //hihihi could just push on stack but meh and the first variables are always arguments
	function_t *func;
	int wait;
	int position; //not naming curpos might get confused with ipr->curpos
	bool done;
} scriptthread_t;

typedef struct {
	int index;
	char string[256];
} scriptstring_t;

typedef enum {
	TK_ILLEGAL,
	TK_EOF,
	TK_VAR,

	TK_R_ELSE,
	TK_R_FOREACH,
	TK_R_BREAK,
	TK_IF, //if
	TK_WHILE, //while
	TK_R_FOR, //for
	TK_RETURN, //return

	TK_R_NULL,
	TK_R_TRUE,
	TK_R_FALSE,
	TK_DOLLAR,
	TK_R_THREAD,
	TK_WAITTILL,
	TK_R_WAIT,
	TK_ENDON,
	TK_NOTIFY,

	TK_MODULO, //%
	TK_DIVIDE, ///
	TK_MULTIPLY, //*

	TK_LPAREN, //(
	TK_RPAREN, //)
	TK_EQUALS, //==
	TK_ASSIGN, //=
	TK_LBRACK, //[
	TK_RBRACK, //]
	TK_LBRACE, //{
	TK_RBRACE, //}
	TK_COMMA, //,
	TK_COLON, //:
	TK_SEMICOLON, //;

	TK_AND,
	TK_OR,
	TK_AND_AND, //&&
	TK_OR_OR, //||
	TK_NOTEQUAL, //!=

	TK_GEQUAL, //>=
	TK_LEQUAL, //<=

	TK_PLUS_ASSIGN, //+=
	TK_MINUS_ASSIGN, //-=

	TK_DIVIDE_ASSIGN,
	TK_MULTIPLY_ASSIGN,

	TK_MEMBER_SEPERATOR, //->

	TK_LESS, //<
	TK_GREATER, //>
	TK_PLUS_PLUS, //++
	TK_MINUS_MINUS, //--
	TK_PLUS, //+
	TK_MINUS, //-
	TK_DOT, //.
	TK_NOT, //!

	TK_IDENT, //test
	TK_STRING, //"test"
	TK_INT, //1
	TK_FLOAT, //1.2
	TK_END_OF_LIST
} e_lex_tokens;

const static char *lex_token_strings[] = {
	"TK_ILLEGAL",
	"TK_EOF",
	"TK_VAR",

	"TK_R_ELSE",
	"TK_R_FOREACH",
	"TK_R_BREAK",
	"TK_IF",
	"TK_WHILE",
	"TK_R_FOR",
	"TK_RETURN",

	"TK_R_NULL",
	"TK_R_TRUE",
	"TK_R_FALSE",
	"TK_DOLLAR",
	"TK_THREAD",
	"TK_WAITTILL",
	"TK_R_WAIT",
	"TK_ENDON",
	"TK_NOTIFY",

	"TK_MODULO",
	"TK_DIVIDE",
	"TK_MULTIPLY",

	"TK_LPAREN",
	"TK_RPAREN",
	"TK_EQUALS",
	"TK_ASSIGN",
	"TK_LBRACK",
	"TK_RBRACK",
	"TK_LBRACE",
	"TK_RBRACE",
	"TK_COMMA",
	"TK_COLON",
	"TK_SEMICOLON",
	
	"TK_AND",
	"TK_OR",
	"TK_AND_AND",
	"TK_OR_OR",
	"TK_NOTEQUAL",
	
	"TK_GEQUAL",
	"TK_LEQUAL",
	"TK_PLUS_ASSIGN",
	"TK_MINUS_ASSIGN",
	"TK_DIVIDE_ASSIGN",
	"TK_MULTIPLY_ASSIGN",

	"TK_MEMBER_SEPERATOR",

	"TK_LESS",
	"TK_GREATER",
	"TK_PLUS_PLUS",
	"TK_MINUS_MINUS",
	"TK_PLUS",
	"TK_MINUS",
	"TK_DOT",
	"TK_NOT",

	"TK_IDENT",
	"TK_STRING",
	"TK_INT",
	"TK_FLOAT",
	"TK_END_OF_LIST",
};

struct ipr_s {
	size_t curpos, scriptsize;
	int lineno;
	char *scriptbuf;
	int integer;
	float number;
	char string[2048];
	int stringindex;
	int error;
	int eof;
	char errstr[256];

	stockfunction_t *stockfunctionsets[255]; //max of 255 sets seems fair

	int numstockfunctionsets;

	var_t *variables[IPR_MAX_VARIABLES];
	int numvars;

	int token;

#define IPR_MAX_SCRIPT_STRINGS 16384

	scriptstring_t *strings[IPR_MAX_SCRIPT_STRINGS];
	int numstrings;

	intptr_t stack[VM_STACK_SIZE];
	intptr_t registers[e_vm_reg_len];

#define IPR_MAX_FUNCTIONS 1024

	function_t *functions[IPR_MAX_FUNCTIONS];
	int numfuncs;

#define IPR_MAX_SCRIPT_THREADS 1024
	scriptthread_t *threadrunners[IPR_MAX_SCRIPT_THREADS];
	int numthreadrunners;

	scriptthread_t *thrunner; //current thread runner

	function_t *curfunc, *lastcalledfunc;
	varval_t lval;
	union {
		varval_t *func_return_value;
	};
	int func_args_sp;

	var_t *localvars[IPR_MAX_VARIABLES];
	int numlocalvars;
	int blockno;
	int factor;

	bool loopbreak;

	varval_t varval_null;

#define FRAMETIME (1000/20)
	unsigned long current_time;
};

static void ipr_register_stockfunction_set(ipr_t *ipr, stockfunction_t *set) {
	ipr->stockfunctionsets[ipr->numstockfunctionsets++] = set;
}

int ipr_expression(ipr_t*);
int ipr_run_block(ipr_t*, int, int);
int ipr_block(ipr_t*);
int ipr_function(ipr_t*, function_t*);
int ipr_condition(ipr_t*);

int ipr_pop_function_arguments_from_stack(ipr_t *ipr,int);

varval_t *alloc_scriptvar_value_f(ipr_t*, int);
#define alloc_scriptvar_value(x) alloc_scriptvar_value_f(x, __LINE__)

/* some standard script functions stuff */
void se_addchar(ipr_t*, int);
#define se_addbool(a,b) se_addint(a,(b)==true?1:0)
void se_addstring(ipr_t*, const char*);
void se_addfloat(ipr_t*, float);
void se_addint(ipr_t*,int);
int se_getint(ipr_t*, int);
float se_getfloat(ipr_t*, int);
const char *se_getstring(ipr_t*, int);
int se_argc(ipr_t*);
varval_t *se_argv(ipr_t*, int);
int se_vartype(ipr_t*, int);

#define dprintf

#define S(x) #x
#define S_(x) S(x)
#define S__LINE__ S_(__LINE__)