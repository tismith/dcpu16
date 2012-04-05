#include <stdio.h>
#include <stdint.h>

typedef uint16_t word;
typedef uint64_t double_word;

enum registers {A = 0x00, B, C, X, Y, Z, I, J, K};
typedef struct dcpu16 {
	/* Special */
	word PC;
	word SP;
	word O;
	
	/* Registers */
	word reg[9];

	/* Memory */
	word memory[0x10000];
} dcpu16;
dcpu16 cpu;

/* This feels a bit kludgey, but my get_value needs to return a ptr, at least
 * assigments to these should be ignored ... */
word literal[0x3F-0x20+1];
word PC_literal;

typedef void (* instruction_ptr) (word a, word b);
#define INSTRUCTION(name) void name (word a, word b)

typedef struct op_code {
	word op_code;
	instruction_ptr instruction;
} op_code;

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
		return &(cpu.memory[cpu.memory[cpu.PC++] 
				+ cpu.reg[a-0x10]]);
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
			return &(cpu.memory[cpu.PC++]);
		case 0x1f:
			/* next word (literal) */
			/* TOBY: is this correct? */
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

INSTRUCTION(SET) 
{
	*get_value(a) = *get_value(b);
}

INSTRUCTION(ADD)
{
	double_word val = *get_value(a) + *get_value(b);

	*get_value(a) = (word) val;

	if (val != (word) val) {
		cpu.O = 0x0001;
	} else {
		cpu.O = 0;
	}
}

INSTRUCTION(SUB) 
{
	double_word val = *get_value(a) - *get_value(b);

	*get_value(a) = (word) val;

	if (val != (word) val) {
		cpu.O = 0xFFFF;
	} else {
		cpu.O = 0;
	}
}

INSTRUCTION(MUL) 
{
	word a_val = *get_value(a);
	word b_val = *get_value(b);
	*get_value(a) = a_val * b_val;
	cpu.O = ((a_val * b_val) >> 16) & 0xFFFF;
}

INSTRUCTION(DIV) 
{
	word a_val = *get_value(a);
	word b_val = *get_value(b);
	*get_value(a) = a_val / b_val;
	cpu.O = ((a_val<< 16) / b_val) & 0xFFFF;
}

INSTRUCTION(MOD) 
{
	word b_val = *get_value(b);
	if (b_val == 0) {
		*get_value(a) = 0;
	} else {
		*get_value(a) = *get_value(a) % b_val;
	}
}

INSTRUCTION(SHL)
{
	word a_val = *get_value(a);
	word b_val = *get_value(b);
	*get_value(a) = a_val << b_val;
	cpu.O = ((a_val<< b_val) >> 16) & 0xFFFF;
}

INSTRUCTION(SHR) 
{
	word a_val = *get_value(a);
	word b_val = *get_value(b);
	*get_value(a) = a_val >> b_val;
	cpu.O = ((a_val << 16) >> b_val) & 0xFFFF;
}

INSTRUCTION(AND) 
{
	*get_value(a) = *get_value(a) & *get_value(b);
}

INSTRUCTION(BOR)
{
	*get_value(a) = *get_value(a) | *get_value(b);
}

INSTRUCTION(XOR)
{
	*get_value(a) = *get_value(a) ^ *get_value(b);
}

INSTRUCTION(IFE);
INSTRUCTION(IFN);
INSTRUCTION(IFG);
INSTRUCTION(IFB);

#define INSTRUCTION_MAP(opcode, name) \
	{ \
		.op_code = opcode, \
		.instruction = name \
	}
op_code op_code_map[] = {
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

int main(int argc, char **argv)
{
	return 0;
}

	
