/* Emulator for the DCPU-16 from 0x10c.com
 * This source, copyright Toby Smith toby@tismith.id.au */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

/* halts the main loop */
volatile int halt = 0;
int verbose = 1;

typedef uint16_t word;
typedef uint32_t double_word;
typedef uint64_t quad_word;
enum registers {A = 0, B, C, X, Y, Z, I, J};
typedef struct dcpu16 {
	/* Special */
	word PC;
	word SP;
	word O;
	
	/* Registers */
	word reg[8];

	/* Memory */
	word memory[0x10000];

	quad_word cycles;
} dcpu16;
dcpu16 cpu;

/* temps for addressing and values */
word tempA = 0;
word tempB = 0;

/* returns number of following instructions to skip */
typedef int (* basic_instruction_ptr) (word * a, word * b);
#define BASIC_INSTRUCTION(name) int name (word * a, word * b)
typedef struct basic_op_code {
	word op_code;
	basic_instruction_ptr instruction;
} basic_op_code;

/* returns number of following instructions to skip */
typedef int (* nonbasic_instruction_ptr) (word * a);
#define NONBASIC_INSTRUCTION(name) int name (word * a)
typedef struct nonbasic_op_code {
	word op_code;
	nonbasic_instruction_ptr instruction;
} nonbasic_op_code;

#define INSTRUCTION_MAP(opcode, name) \
	[opcode] = { \
		.op_code = opcode, \
		.instruction = name \
	}

/* returns how many extra words this addressing scheme needs */
int addressing_length(word a) {
	if (a <= 0x07) {
		/* register */
		return 0;
	} else if (a <= 0x0F) {
		/* [register] */
		return 0;
	} else if (a <= 0x17) {
		/* [next word + register] */
		return 1;
	} else if (a <= 0x1f) {
		switch(a) {
		case 0x1e:
			/* [next word] */
			return 1;
		case 0x1f:
			/* next word (literal) */
			return 1;
		default:
			return 0;
		}
	} else if (a <= 0x3F) {
		return 0;
	} 
	return 0;
}

/* returns a pointer to either a temporary value (aka a literal) or
 * to a memory or register */
word * get_value(word a, word * target) {
	/* register accesses */
	word * val = NULL;
	if (a <= 0x07) {
		/* register */
		val = &(cpu.reg[a-0x00]);
	} else if (a <= 0x0F) {
		/* [register] */
		val = &(cpu.memory[cpu.reg[a-0x08]]);
	} else if (a <= 0x17) {
		/* [next word + register] */
		val = &(cpu.memory[cpu.memory[cpu.PC++] + cpu.reg[a-0x10]]);
		cpu.cycles++;
	} else if (a <= 0x1f) {
		switch (a) {
		case 0x18:
			/* POP / [SP++] */
			val = &(cpu.memory[cpu.SP++]);
			break;
		case 0x19:
			/* PEEK / [SP] */
			val = &(cpu.memory[cpu.SP]);
			break;
		case 0x1a:
			/* PUSH / [--SP] */
			val = &(cpu.memory[--cpu.SP]);
			break;
		case 0x1b:
			/* SP */
			val = &(cpu.SP);
			break;
		case 0x1c:
			/* PC */
			val = &(cpu.PC);
			break;
		case 0x1d:
			/* O */
			val = &(cpu.O);
			break;
		case 0x1e:
			/* [next word] */
			val = &(cpu.memory[cpu.memory[cpu.PC++]]);
			cpu.cycles++;
			break;
		case 0x1f:
			/* next word (literal) */
			*target = cpu.memory[cpu.PC++];
			val = target;
			cpu.cycles++;
			break;
		}
	} else if (a <= 0x3F) {
		*target = a - 0x20;
		val = target;
	} 
	return val;
}

BASIC_INSTRUCTION(SET) 
{
	*a = *b;
	cpu.cycles++;
	return 0;
}

BASIC_INSTRUCTION(ADD)
{
	double_word val = *a + *b;

	*a = (word) val;

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
	double_word val = *a - *b;

	*a = (word) val;

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
	word a_val = *a;
	word b_val = *b;
	*a = a_val * b_val;
	cpu.O = ((a_val * b_val) >> 16) & 0xFFFF;
	
	cpu.cycles++;
	cpu.cycles++;
	return 0;
}

BASIC_INSTRUCTION(DIV) 
{
	word a_val = *a;
	word b_val = *b;
	*a = a_val / b_val;
	cpu.O = ((a_val<< 16) / b_val) & 0xFFFF;

	cpu.cycles++;
	cpu.cycles++;
	cpu.cycles++;
	return 0;
}

BASIC_INSTRUCTION(MOD) 
{
	word b_val = *b;
	if (b_val == 0) {
		*a = 0;
	} else {
		*a = *a % b_val;
	}

	cpu.cycles++;
	cpu.cycles++;
	cpu.cycles++;
	return 0;
}

BASIC_INSTRUCTION(SHL)
{
	word a_val = *a;
	word b_val = *b;
	*a = a_val << b_val;
	cpu.O = ((a_val<< b_val) >> 16) & 0xFFFF;

	cpu.cycles++;
	cpu.cycles++;
	return 0;
}

BASIC_INSTRUCTION(SHR) 
{
	word a_val = *a;
	word b_val = *b;
	*a = a_val >> b_val;
	cpu.O = ((a_val << 16) >> b_val) & 0xFFFF;

	cpu.cycles++;
	cpu.cycles++;
	return 0;
}

BASIC_INSTRUCTION(AND) 
{
	*a = *a & *b;

	cpu.cycles++;
	return 0;
}

BASIC_INSTRUCTION(BOR)
{
	*a = *a | *b;

	cpu.cycles++;
	return 0;
}

BASIC_INSTRUCTION(XOR)
{
	*a = *a ^ *b;

	cpu.cycles++;
	return 0;
}

BASIC_INSTRUCTION(IFE) {
	cpu.cycles++;
	cpu.cycles++;
	if (*a == *b) {
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
	if (*a != *b) {
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
	if (*a > *b) {
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
	if ((*a & *b) != 0) {
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
	word a_val = *a;
	cpu.memory[--cpu.SP] = cpu.PC;
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
	printf(" X: 0x%04X  Y: 0x%04X  Z: 0x%04X\n", 
			cpu.reg[3], cpu.reg[4], cpu.reg[5]);
	printf(" I: 0x%04X  J: 0x%04X\n", 
			cpu.reg[6], cpu.reg[7]);
	printf(" Cycles: %" PRIu64 "\n", cpu.cycles);
	printf("--------------------------------\n");
	return;
}

#define COLUMNS 8
void dump_memory() 
{
	int i = 0;
	printf("DCPU-16 MEMORY\n");
	for (i = 0x00; i < 0x10000; i++) {
		if ((i % COLUMNS) == 0) {
			printf("0x%04X:", i);
		}
		printf(" %04X", cpu.memory[i]);
		if ((i % COLUMNS) == (COLUMNS - 1)) {
			printf("\n");
		}
	}
	printf("--------------------------------\n");
	return;
}

typedef enum {BASIC, NONBASIC} instruction_t;
typedef struct instruction {
	word op_code;
	word * a;
	word * b;
	instruction_t type;
} instruction;

void print_instruction(instruction i) {
	switch (i.type) {
	case (BASIC):
		printf("   BASIC opcode: %04X a: %04X b: %04X\n", 
				i.op_code, *(i.a), *(i.b));
		break;
	case (NONBASIC):
		printf("NONBASIC opcode: %04X a: %04X\n",
				i.op_code, *(i.a));
		break;
	}
	return;
}

instruction decode_instruction(word val) {
	instruction i;
	word a, b;

	/* Basic op codes:
	 * bbbbbbaaaaaaoooooo */
	i.op_code = val & 0x000F;
	i.type = BASIC;

	a = (val & 0x03F0) >> 4;
	b = (val & 0xFC00) >> 10;

	if (i.op_code == 0x00) {
		/* Non-basic op codes: 
		 * aaaaaaoooooo0000 */
		i.op_code = a;
		i.type = NONBASIC;
		i.a = get_value(b, &tempA);
	} else {
		i.a = get_value(a, &tempA);
		i.b = get_value(b, &tempB);
	}

	if (verbose) print_instruction(i);

	return i;
}

/* returns the number of words that make up the instruction */
int instruction_length(word val) {
	int length = 1;
	word op_code = val & 0x000F;
	word a = (val & 0x03F0) >> 4;
	word b = (val & 0xFC00) >> 10;

	if (op_code == 0x00) {
		length += addressing_length(b);
	} else {
		length += addressing_length(a);
		length += addressing_length(b);
	}
	return length;
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
		word value = cpu.memory[cpu.PC];
		cpu.PC += instruction_length(value);
		skip--;
	}
}	

void handle_usr1(int sig) {
	dump_registers();
	if (halt) { 
		halt = 0; 
	} else {
		halt = 1;
	}
}

void handle_usr2(int sig) {
	dump_memory();
}

void usage(char * name) {
	fprintf(stderr, "%s: -f <filename.bin> [-q] [-v] " \
			"[-i <interval>]\n", name);
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
	int ch;
	int fd;
	char * filename = NULL;
	int interval = 1;
	int i = 0;
	signal(SIGUSR1, handle_usr1);
	signal(SIGUSR2, handle_usr2);
	(void)memset(&cpu, 0, sizeof(cpu));
	cpu.SP=0xFFFF;

	while ((ch = getopt(argc, argv, "qvi:f:")) != -1) {
		switch (ch) {
		case 'f':
			filename = optarg;
			break;
		case 'i':
			interval = atoi(optarg);
			break;
		case 'v':
			verbose++;
			break;
		case 'q':
			verbose--;
			break;
		default:
			usage(argv[0]);
		}
	}

	if (filename == NULL) usage(argv[0]);

	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		perror("error opening file");
		exit(EXIT_FAILURE);
	}

	/* big endian data files */
	for (i = 0; i < 0x10000; i++) {
		uint8_t byte;

		if (read(fd, &byte, sizeof(byte)) < 0) {
			break;
		}

		cpu.memory[i] = byte << 8;

		if (read(fd, &byte, sizeof(byte)) < 0) {
			break;
		}
		cpu.memory[i] += byte;
	}

	while(1) {
		word value = cpu.memory[cpu.PC++];
		instruction i = decode_instruction(value);
		handle_instruction(i);

		if (verbose) dump_registers();

		sleep(interval);

		while(halt) {};
	}

	exit(EXIT_SUCCESS);
}

	
