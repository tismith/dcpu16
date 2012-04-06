#include <stdio.h>
#include <stdint.h>
#include <string.h>

typedef uint16_t word;
typedef uint64_t double_word;

enum registers {A = 0, B, C, X, Y, Z, I, J, K};
typedef struct dcpu16 {
	/* Special */
	word PC;
	word SP;
	word O;
	
	/* Registers */
	word reg[9];

	/* Memory */
	word memory[0x10000];

	double_word cycles;
} dcpu16;
dcpu16 cpu;

/* This feels a bit kludgey, but my get_value needs to return a ptr, at least
 * assigments to these should be ignored ... */
word literal[0x3F-0x20+1];
word PC_literal;

/* returns number of following instructions to skip */
typedef int (* basic_instruction_ptr) (word a, word b);
#define BASIC_INSTRUCTION(name) int name (word a, word b)
typedef struct basic_op_code {
	word op_code;
	basic_instruction_ptr instruction;
} basic_op_code;

/* returns number of following instructions to skip */
typedef int (* nonbasic_instruction_ptr) (word a);
#define NONBASIC_INSTRUCTION(name) int name (word a)
typedef struct nonbasic_op_code {
	word op_code;
	nonbasic_instruction_ptr instruction;
} nonbasic_op_code;

#define INSTRUCTION_MAP(opcode, name) \
	{ \
		.op_code = opcode, \
		.instruction = name \
	}

word * get_value(word a) {
	/* register accesses */
	if (a <= 0x07) {
		/* register */
		return &(cpu.reg[a-0x00]);
	} else if (a <= 0x0F) {
		/* [register] */
		return &(cpu.memory[cpu.reg[a-0x08]]);
	} else if (a <= 0x17) {
		/* [next word + register] */
		cpu.cycles++;
		return &(cpu.memory[cpu.PC++] + cpu.reg[a-0x10]);
	} else if (a <= 0x1f) {
		switch (a) {
		case 0x18:
			return &(cpu.memory[cpu.SP++]);
			/* POP / [SP++] */
		case 0x19:
			/* PEEK / [SP] */
			return &(cpu.memory[cpu.SP]);
		case 0x1a:
			/* PUSH / [--SP] */
			return &(cpu.memory[--cpu.SP]);
		case 0x1b:
			/* SP */
			return &(cpu.SP);
		case 0x1c:
			/* PC */
			return &(cpu.PC);
		case 0x1d:
			/* O */
			return &(cpu.O);
		case 0x1e:
			/* [next word] */
			cpu.cycles++;
			return &(cpu.memory[cpu.PC++]);
		case 0x1f:
			/* next word (literal) */
			cpu.cycles++;
			PC_literal = cpu.PC++;
			return &PC_literal;
		}
	} else if (a <= 0x3F) {
		word val = a - 0x20;
		literal[val] = val;
		return &(literal[val]);
	} else {
		return NULL;
	}
}

BASIC_INSTRUCTION(SET) 
{
	*get_value(a) = *get_value(b);
	cpu.cycles++;
	return 0;
}

BASIC_INSTRUCTION(ADD)
{
	double_word val = *get_value(a) + *get_value(b);

	*get_value(a) = (word) val;

	if (val != (word) val) {
		cpu.O = 0x0001;
	} else {
		cpu.O = 0;
	}

	cpu.cycles++;
	return 0;
}

BASIC_INSTRUCTION(SUB) 
{
	double_word val = *get_value(a) - *get_value(b);

	*get_value(a) = (word) val;

	if (val != (word) val) {
		cpu.O = 0xFFFF;
	} else {
		cpu.O = 0;
	}

	cpu.cycles++;
	cpu.cycles++;
	return 0;
}

BASIC_INSTRUCTION(MUL) 
{
	word a_val = *get_value(a);
	word b_val = *get_value(b);
	*get_value(a) = a_val * b_val;
	cpu.O = ((a_val * b_val) >> 16) & 0xFFFF;
	
	cpu.cycles++;
	cpu.cycles++;
	return 0;
}

BASIC_INSTRUCTION(DIV) 
{
	word a_val = *get_value(a);
	word b_val = *get_value(b);
	*get_value(a) = a_val / b_val;
	cpu.O = ((a_val<< 16) / b_val) & 0xFFFF;

	cpu.cycles++;
	cpu.cycles++;
	cpu.cycles++;
	return 0;
}

BASIC_INSTRUCTION(MOD) 
{
	word b_val = *get_value(b);
	if (b_val == 0) {
		*get_value(a) = 0;
	} else {
		*get_value(a) = *get_value(a) % b_val;
	}

	cpu.cycles++;
	cpu.cycles++;
	cpu.cycles++;
	return 0;
}

BASIC_INSTRUCTION(SHL)
{
	word a_val = *get_value(a);
	word b_val = *get_value(b);
	*get_value(a) = a_val << b_val;
	cpu.O = ((a_val<< b_val) >> 16) & 0xFFFF;

	cpu.cycles++;
	cpu.cycles++;
	return 0;
}

BASIC_INSTRUCTION(SHR) 
{
	word a_val = *get_value(a);
	word b_val = *get_value(b);
	*get_value(a) = a_val >> b_val;
	cpu.O = ((a_val << 16) >> b_val) & 0xFFFF;

	cpu.cycles++;
	cpu.cycles++;
	return 0;
}

BASIC_INSTRUCTION(AND) 
{
	*get_value(a) = *get_value(a) & *get_value(b);

	cpu.cycles++;
	return 0;
}

BASIC_INSTRUCTION(BOR)
{
	*get_value(a) = *get_value(a) | *get_value(b);

	cpu.cycles++;
	return 0;
}

BASIC_INSTRUCTION(XOR)
{
	*get_value(a) = *get_value(a) ^ *get_value(b);

	cpu.cycles++;
	return 0;
}

BASIC_INSTRUCTION(IFE) {
	cpu.cycles++;
	cpu.cycles++;
	if (*get_value(a) == *get_value(b)) {
		return 0;
	} else {
		cpu.cycles++;
		return 1;
	}
}

BASIC_INSTRUCTION(IFN) 
{
	cpu.cycles++;
	cpu.cycles++;
	if (*get_value(a) != *get_value(b)) {
		return 0;
	} else {
		cpu.cycles++;
		return 1;
	}
}

BASIC_INSTRUCTION(IFG) 
{
	cpu.cycles++;
	cpu.cycles++;
	if (*get_value(a) > *get_value(b)) {
		return 0;
	} else {
		cpu.cycles++;
		return 1;
	}
}

BASIC_INSTRUCTION(IFB)
{
	cpu.cycles++;
	cpu.cycles++;
	if ((*get_value(a) & *get_value(b)) != 0) {
		return 0;
	} else {
		cpu.cycles++;
		return 1;
	}
}

basic_op_code basic_op_code_map[] = {
	INSTRUCTION_MAP(0x01, SET),
	INSTRUCTION_MAP(0x02, ADD),
	INSTRUCTION_MAP(0x03, SUB),
	INSTRUCTION_MAP(0x04, MUL),
	INSTRUCTION_MAP(0x05, DIV),
	INSTRUCTION_MAP(0x06, MOD),
	INSTRUCTION_MAP(0x07, SHL),
	INSTRUCTION_MAP(0x08, SHR),
	INSTRUCTION_MAP(0x09, AND),
	INSTRUCTION_MAP(0x0A, BOR),
	INSTRUCTION_MAP(0x0B, XOR),
	INSTRUCTION_MAP(0x0C, IFE),
	INSTRUCTION_MAP(0x0D, IFN),
	INSTRUCTION_MAP(0x0E, IFG),
	INSTRUCTION_MAP(0x0F, IFB)
};

NONBASIC_INSTRUCTION(JSR)
{
	word a_val = *get_value(a);
	cpu.memory[--cpu.SP] = a_val;
	cpu.PC = a_val;

	cpu.cycles++;
	cpu.cycles++;
	return 0;
}

nonbasic_op_code nonbasic_op_code_map[] = {
	INSTRUCTION_MAP(0x01, JSR)
};

void dump_registers() 
{
	printf("DCPU-16 REGISTERS\n");
	printf("PC: 0x%04X SP: 0x%04X  O: 0x%04X\n", 
			cpu.PC, cpu.SP, cpu.O);
	printf(" A: 0x%04X  B: 0x%04X  C: 0x%04X\n", 
			cpu.reg[0], cpu.reg[1], cpu.reg[2]);
	printf(" I: 0x%04X  J: 0x%04X  K: 0x%04X\n", 
			cpu.reg[3], cpu.reg[4], cpu.reg[5]);
	printf(" X: 0x%04X  Y: 0x%04X  Z: 0x%04X\n", 
			cpu.reg[6], cpu.reg[7], cpu.reg[8]);
	printf(" Cycles: %llu\n", cpu.cycles);
	printf("--------------------------------\n");
	return;
}

typedef enum {BASIC, NONBASIC} instruction_t;
typedef struct instruction {
	word op_code;
	word a;
	word b;
	instruction_t type;
} instruction;

void print_instruction(instruction i) {
	switch (i.type) {
	case (BASIC):
		printf("   BASIC opcode: %04X a: %04X b: %04X\n", 
				i.op_code, i.a, i.b);
		break;
	case (NONBASIC):
		printf("NONBASIC opcode: %04X a: %04X\n",
				i.op_code, i.a);
		break;
	}
	return;
}

instruction decode_instruction(word val) {
	instruction i;

	/* Basic op codes:
	 * bbbbbbaaaaaaoooooo */
	i.op_code = val & 0x000F;
	i.a = (val & 0x03F0) >> 4;
	i.b = val & (0xFC00) >> 10;
	i.type = BASIC;

	if (i.op_code == 0x00) {
		/* Non-basic op codes: 
		 * aaaaaaoooooo0000 */
		i.type = NONBASIC;
		i.b = i.a;
		i.op_code = i.a;
	}

	print_instruction(i);

	return i;
}

void handle_instruction(instruction i) {
	int skip = 0;

	switch (i.type) {
	case BASIC:
		skip = basic_op_code_map[i.op_code].instruction(i.a, i.b);
		break;
	case NONBASIC:
		skip = nonbasic_op_code_map[i.op_code].instruction(i.a);
		break;
	}

	while (skip > 0) { 
		/* need to skip the next intruction, including it's values */
		/* this is how I'm doing IFs */
		skip--;
	}
}	

int main(int argc, char **argv)
{
	(void)memset(&cpu, 0, sizeof(cpu));
	cpu.memory[0] = 0x7c01;
	cpu.memory[1] = 0x0030;

	handle_instruction(decode_instruction(cpu.memory[cpu.PC++]));

	dump_registers();
	return 0;
}

	
