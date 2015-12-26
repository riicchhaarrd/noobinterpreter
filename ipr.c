#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <ctype.h> //isalnum

int is_space(unsigned char c) {
	if(c == ' ' || c == '\n' || c == '\r' || c == '\t')
		return 1;
	return 0;
}

int is_alnum(unsigned char c) {
	if(is_space(c))
		return 0;
	if(c == '-' || c == ',')
		return 1;
	return isalnum(c);
}

int read_text_file(const char *filename, char **buf, int *filesize) {
	
	FILE *fp = fopen(filename, "r");
	
	if(fp==NULL)
		return 1;
	fseek(fp, 0, SEEK_END);
	*filesize = ftell(fp);
	rewind(fp);
	
	*buf = (char*)malloc(*filesize);
	
	fread(*buf, 1, *filesize, fp);
	
	fclose(fp);
	
	return 0;
	
}

typedef enum {
	VT_INT,
	VT_FLOAT,
	VT_STRING,
	VT_NULL
} vartypes_t;

#define VF_LOCAL (1<<0)

typedef struct {
	char name[256];
	int type, flags;
	union {
		int integer;
		int stringindex;
		float number;
	};
} var_t;

typedef struct {
	const char *name;
	int (*call)();
} function_t;

int sf_print();

function_t scriptfunctions[] = {
	{"print", sf_print},
	{NULL, 0}
};

typedef struct {
	int index;
	char string[256];
} scriptstring_t;

typedef struct {
	size_t curpos, scriptsize;
	char *scriptbuf;
	char string[256];
	int stringindex;
	
	var_t *variables[256];
	int numvars;
	
	scriptstring_t *strings[2048];
	int numstrings;
	
	var_t *args[256]; /* add arg_t struct?? and replace this??*/
	int argcount;
} ipr_t;

ipr_t ipr;	

int ipr_getnext(ipr_t* ipr) {
	if(ipr == NULL)
		return -1;
	if(ipr->curpos >= ipr->scriptsize) {
		return -1;
	}
	return ipr->scriptbuf[ipr->curpos++];
}

int ipr_getnextnonspace(ipr_t* ipr) {
	int ch = ipr_getnext(ipr);
	while(is_space(ch))
		ch = ipr_getnext(ipr);
	return ch;
}

const char *ipr_getstring(ipr_t *ipr) {
	
	memset(ipr->string, 0, sizeof(ipr->string));
	ipr->stringindex = 0;	
	
	int ch = ipr->scriptbuf[ipr->curpos - 1];
	
	while(1) {
		
		if(!is_alnum(ch)) {
			if(!*ipr->string)
				break;
			ipr->curpos--;
			//printf("[%s]", ipr->string);
			break;
		}
		
		if(ipr->stringindex >= sizeof(ipr->string)) {
			printf("keyword string index overflow\n");
			break;
		}
		ipr->string[ipr->stringindex++] = ch;
		ch = ipr_getnext(ipr);
	}
	
	return ipr->string;
	
}

const char *ipr_getstringnspaces(ipr_t *ipr) {
	
	memset(ipr->string, 0, sizeof(ipr->string));
	ipr->stringindex = 0;	
	
	int ch = ipr->scriptbuf[ipr->curpos - 1];
	
	while(1) {
		
		if(!is_alnum(ch) && !is_space(ch)) {
			if(!*ipr->string)
				break;
			ipr->curpos--;
			break;
		}
		
		if(ipr->stringindex >= sizeof(ipr->string)) {
			printf("keyword string index overflow\n");
			break;
		}
		ipr->string[ipr->stringindex++] = ch;
		ch = ipr_getnext(ipr);
	}
	
	return ipr->string;
	
}

var_t *ipr_getvar(ipr_t *ipr, const char* n) {
	var_t *v;
	for(int i = 0; i < ipr->numvars; i++) {
		v = ipr->variables[i];
		
		if(!strcmp(v->name, n)) {
			return v;
		}
	}
	return NULL;
}

const char *se_getstring(int n) {
	
	if(n >= ipr.argcount)
		return "";
	var_t *v = ipr.args[n];
	
	static char retstr[128];
	
	if(v->type == VT_INT) {
		sprintf(retstr, "%d", v->integer);
		return retstr;
	} else if(v->type == VT_FLOAT) {
		sprintf(retstr, "%f", v->number);
		return retstr;
	} else if(v->type == VT_NULL) {
		return "[null]";
	}
	
	return ipr.strings[ipr.args[n]->stringindex]->string;
}

int se_getint(int n) {
	if(n >= ipr.argcount)
		return 0;
	
	return ipr.args[n]->integer;
}

int se_argc() {
	return ipr.argcount;
}

var_t *se_argv(int n) {
	if(n >= ipr.argcount)
		return NULL;
	return ipr.args[n];
}

int sf_print() {
	
	for(int i = 0; i < se_argc(); i++)
		printf("%s\n", se_getstring(i));
	
	return 0;
}

int isinteger(const char *s) {
	int is = 1;
	for(int i = 1; i < strlen(s); i++) {
		
		if(!isdigit(s[i])) {
			is = 0;
			break;
		}
		
	}
	
	if(*s != '-' && !isdigit(*s))
		return 0;
	return is;
}

var_t *alloc_script_var() {
	var_t *var = (var_t*)malloc(sizeof(var_t));
	memset(var,0,sizeof(var_t));
	var->type=VT_NULL;
	return var;
}

var_t *ipr_convert_to_var(ipr_t *ipr, char *str) {
	/* first check if it's a number */
	var_t *v = NULL;
	
	if(isinteger(str)) {
		v = alloc_script_var();
		v->flags |= VF_LOCAL;
		v->type = VT_INT;
		v->integer = atoi(str);
		return v;
	} else {
	
		int v_found = 0;
		
		for(int i = 0; i < ipr->numvars; i++) {
		
			if(!strcmp(ipr->variables[i]->name, str)) {
				v=ipr->variables[i];
				v_found=1;
				break;
			}
			//printf("ipr->variables[i]->name = %s CMP str = %s\n", ipr->variables[i]->name, str);
		}
		
	}
	
	return v;
}

void interpret(ipr_t *ipr) {
	
	for(int i = 0; i < sizeof(ipr->args) / sizeof(ipr->args[i]); i++)
		ipr->args[i]=NULL;
	ipr->argcount=0;
	
	for(int i = 0; i < sizeof(ipr->strings) / sizeof(ipr->strings[i]); i++)
		ipr->strings[i] = NULL;
	ipr->numstrings = 0;
	
	for(int i = 0; i < sizeof(ipr->variables) / sizeof(ipr->variables[i]); i++)
		ipr->variables[i] = NULL;
	ipr->numvars=0;
	
	memset(ipr->string, 0, sizeof(ipr->string));
	ipr->stringindex = 0;
	int ch;
	while(1) {
		
		ch = ipr_getnext(ipr);
		
		if(ch == -1)
			break;
		
		if(!is_space(ch)) {
			
			if(!strcmp(ipr->string, "var")) {
				const char *varname = ipr_getstring(ipr);
				int next = ipr_getnextnonspace(ipr);
				
				if(next!=';') {
					printf("did u forget a ;\n");
					break;
				}
				int v_exists=0;
				for(int i = 0; i < ipr->numvars; i++) {
					
					if(!strcmp(ipr->variables[i]->name, varname)) {
						printf("variable '%s' already exists!", varname);
						v_exists=1;
						break;
					}
					
					//printf("[%s CMP %s]\n", ipr->variables[i]->name, varname);
					
				}
				if(v_exists)
					break;
				
				var_t *var = (var_t*)malloc(sizeof(var_t));
				memset(var,0,sizeof(var_t));
				var->type=VT_NULL;
				strncpy(var->name, varname, sizeof(var->name) - 1);
				var->name[sizeof(var->name) - 1] = '\0';
				ipr->variables[ipr->numvars] = var;
				
				ipr->numvars++;
				//printf("var declare %s (%c)\n", varname, next);
				
			} else {
				
				if(ch == '=') {
					int v_found=0;
					var_t *v = NULL;
					for(int i = 0; i < ipr->numvars; i++) {
						v = ipr->variables[i];
						
						if(!strcmp(ipr->string, ipr->variables[i]->name)) {
							
							if(ch == '=') {
								int next = ipr_getnextnonspace(ipr);
								
								if(next == '"') {
									//string
									ipr->curpos++;
									
									const char *val = ipr_getstringnspaces(ipr);
									
									//printf("%s equals \"%s\"\n", v->name, val);
									
									
									if(ipr_getnext(ipr) != '"') {
										printf("forgot a \"\n");
										break;
									}
									
									scriptstring_t *ss = (scriptstring_t*)malloc(sizeof(scriptstring_t));
									strncpy(ss->string, val, sizeof(ss->string) - 1);
									ss->string[sizeof(ss->string) - 1] = '\0';
									ss->index = ipr->numstrings;
									
									ipr->strings[ipr->numstrings++] = ss;
									
									v->type = VT_STRING;
									v->stringindex = ss->index;
									
								} else {
									const char *val = ipr_getstring(ipr);
									
									//printf("%s equals %s\n", v->name, val);
									v->type = VT_INT;
									v->integer = atoi(val);
									
								}
								
								next = ipr_getnextnonspace(ipr);
								
								if(next!=';') {
									
									printf("did you forget a ;\n");
									break;
								}
							}
							v_found = 1;
							break;
						}
						
					}
					
					if(!v_found) {
						
						printf("variable '%s' does not exist!\n", ipr->string);
						break;
					}
				} else if(ch == '(') {
					char fname[128] = {0};
					snprintf(fname, sizeof(fname)-1, "%s", ipr->string);
					
					ipr->curpos++;
					
					const char *args = ipr_getstringnspaces(ipr);
					
					char tempstring[128] = {0};
					int tempsize = 0;
					int terr=0;
					for(int i = 0; i < strlen(args); i++) {
						if(is_space(args[i]))
							continue;
						
						if(args[i] == ',') {
							
							var_t *cv = ipr_convert_to_var(ipr, tempstring);
							
							if(cv == NULL) {
								terr=1;
								printf("could not convert string to argument!\n");
								break;
							}
							
							ipr->args[ipr->argcount++] = cv;
							
							memset(tempstring, 0, sizeof(tempstring));
							tempsize = 0;
							continue;
						}
						
						tempstring[tempsize++] = args[i];
					}
					if(terr)
						break;
					
					var_t *cv = ipr_convert_to_var(ipr, tempstring);
					if(cv==NULL)
						break;
					ipr->args[ipr->argcount++] = cv;
					
					for(int i = 0; scriptfunctions[i].name; i++) {
						
						if(!strcmp(scriptfunctions[i].name, fname)) {
							
							scriptfunctions[i].call();
							
						}
						
					}
					
					for(int i = 0; i < ipr->argcount; i++) {
						cv = ipr->args[i];
						if(cv->flags & VF_LOCAL) {
							printf("freed local var %d, %s\n", i, se_getstring(i));
							free(ipr->args[i]);
						}
						ipr->args[i] = NULL;
					}
					
					ipr->argcount=0;
					
					
					int next = ipr_getnextnonspace(ipr);
					int nn = ipr_getnextnonspace(ipr);
					
					if(next != ')' || nn != ';') {
						printf("invalid function call!\n");
						break;
					}
					
				} else if(ch == '+' || ch == '-') {
					
					var_t *v = ipr_getvar(ipr, ipr->string);
					
					if(v == NULL) {
						printf("variable %s does not exist\n", ipr->string);
						break;
					}
					
					int next = ipr->scriptbuf[ipr->curpos];
					
					if(next == '=') {
						ipr->curpos += 3;
						const char *add = ipr_getstring(ipr);
						//printf("add = %s\n", add);
						if(!isinteger(add)) {
							//if u want add string concatenation here
							printf("not a number!!");
							break;
						}
						int n = atoi(add);
						
						if(ch == '+')
							v->integer += n;
						else
							v->integer -= n;
						
						if(ipr_getnext(ipr)!=';')
							break;
						
					}
				}
				
			}
		}
		
		if(!is_alnum(ch))
			continue;
		
		ipr_getstring(ipr);
		
	}
	
	putchar('\n');
	
	//cleanup here
	for(int i = 0; i < ipr->numvars; i++) {
		printf("freed variable '%s'\n", ipr->variables[i]->name);
		free(ipr->variables[i]);
	}
	
	for(int i = 0; i < ipr->numstrings; i++) {
		printf("freed scriptstring '%s'\n", ipr->strings[i]->string);
		free(ipr->strings[i]);
	}
}

int main(int argc, char **argv) {
	
	ipr.scriptbuf=NULL;
	ipr.curpos=ipr.scriptsize=ipr.stringindex=0;
	memset(ipr.string,0,sizeof(ipr.string));
	
	if(read_text_file("file.i", &ipr.scriptbuf, &ipr.scriptsize)) {
		printf("failed to read file!\n");
		return 0;
	}
	
	printf("filesize = %d\n", ipr.scriptsize);
	
	interpret(&ipr);
	
	free(ipr.scriptbuf);
}