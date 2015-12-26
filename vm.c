#include <stdio.h> include <stdbool.h>
bool isRunning = true;
#define stack_push(x) (stack[++registers[SP]] = x) define STACK_SIZE 256
int stack[STACK_SIZE]; typedef enum {
	A,
	B,
	C,
	D,
	E,
	F,
	IP,
	SP,
	
	e_reg_len
} e_registers;
const char *e_reg_names[] = {
	"A","B","C","D","E","F","IP","SP",NULL
};
int registers[e_reg_len]; typedef enum {
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
const unsigned char program[] = {
	PRINT, 3,
	HALT,
	
	't','e','s','t','\n',0,
};
void print_registers() {
	int i;
	for(i = 0; i < e_reg_len; i++)
		printf("%s => %d\n", e_reg_names[i], registers[i]);
}
int get_opcode() {
    return program[registers[IP]];
}
void vm_execute(int instr) {
    switch (instr) {
        case HALT: {
            isRunning = false;
            printf("HALT\n");
        }
		break;
		
        case PUSH: {
    	    registers[SP]++;
	        stack[registers[SP]] = program[++registers[IP]];
        }
		break;
		
        case POP: {
	        int val_popped = stack[registers[SP]--];
	        printf("popped %d\n", val_popped);
	    }
		break;
		
	    case ADD: {
	        int a = stack[registers[SP]--];
	        int b = stack[registers[SP]--];
	        int result = b + a;
	        registers[SP]++;
	        stack[registers[SP]] = result;
	    }
		break;
		
		case PRINT: {
			int str_offset = program[++registers[IP]];
			printf("%s", &program[str_offset]);
		}
		break;
		
		case NOP:
			printf("no-operation\n");
		break;
		
		case BRK: {
			#if 0
			printf("Break at %d\n", registers[IP]);
			print_registers();
			getchar();
			#endif
		}
		break;
		
		case EQ: {
			int *reg1 = 
&registers[program[++registers[IP]]];
			int *reg2 = 
&registers[program[++registers[IP]]];
			int loc = program[++registers[IP]];
			
			if(*reg1 == *reg2) {
				registers[IP] = loc;
				//printf("EQ to %d\n", registers[IP]);
			} else {
				//printf("reg1 != reg2, reg1 = %d, reg2 
= %d\n", *reg1, *reg2);
			}
		}
		break;
		
		case DEC: {
			int *reg = &registers[program[++registers[IP]]];
			*reg -= 1;
			//printf("DEC %s = %d\n", 
e_reg_names[program[registers[IP] - 1]], *reg);
		}
		break;
		
		case SET: {
			int reg = program[++registers[IP]];
			int val = program[++registers[IP]];
			
			registers[reg] = val;
			//printf("SET register '%s' to %d\n", 
e_reg_names[reg], val);
		}
		break;
    }
}
int main2(int argc, char **argv) {
	
	printf("program size = %d\n", sizeof(program) / 
sizeof(*program));
	
	if(argc > 1) {
		
		printf("argc > 1\n");
		return 0;
	}
	
	while (isRunning) {
		vm_execute(get_opcode());
        ++registers[IP];
    }
	return 0;
}
