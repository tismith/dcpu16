/* Disassembler for the DCPU-16 from 0x10c.com
 * This source, copyright Toby Smith toby@tismith.id.au */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

typedef uint16_t word;
uint8_t buffer[2*0x10000]; /* 8 bit */

char reg[] = {'A', 'B', 'C', 'X', 'Y', 'Z', 'I', 'J'};

typedef struct op_code {
	word op_code;
	char *name;
} op_code;
#define INSTRUCTION_MAP(opcode, symbol) \
	[opcode] = { \
		.op_code = opcode, \
		.name = #symbol \
	}

/* offset is the index into the buffer of the opcode */
int print_value(int offset, word a) {
	int size = 0;
	int val = 0;
	if (a <= 0x07) {
		/* register */
		printf("%c", reg[a]);
	} else if (a <= 0x0F) {
		/* [register] */
		printf("[%c]", reg[a-0x08]);
	} else if (a <= 0x17) {
		/* [next word + register] */
		val = (buffer[offset+2] << 8) + buffer[offset+3];
		printf("[0x%x + %c]", val, reg[a-0x10]);
		size = 2;
	} else if (a <= 0x1f) {
		switch (a) {
		case 0x18:
			printf("POP");
			break;
		case 0x19:
			printf("PEEK");
			break;
		case 0x1a:
			printf("PUSH");
			break;
		case 0x1b:
			printf("SP");
			break;
		case 0x1c:
			printf("PC");
			break;
		case 0x1d:
			printf("O");
			break;
		case 0x1e:
			/* [next word] */
			val = (buffer[offset+2] << 8) + buffer[offset+3];
			printf("[0x%x]", val);
			size = 2;
			break;
		case 0x1f:
			/* next word (literal) */
			val = (buffer[offset+2] << 8) + buffer[offset+3];
			printf("0x%x", val);
			size = 2;
			break;
		}
	} else if (a <= 0x3F) {
		printf("0x%x", a - 0x20);
	} 
	return size;
}

op_code basic_op_code_map[] = {
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

op_code nonbasic_op_code_map[] = {
	INSTRUCTION_MAP(0x01, JSR)
};

int print_instruction(int offset, word val) {
	word op_code, a, b;
	int size = 2;

	if (val == 0x00) {
		printf("NULL");
		return size;
	}

	/* Basic op codes:
	 * bbbbbbaaaaaaoooooo */
	op_code = val & 0x000F;

	a = (val & 0x03F0) >> 4;
	b = (val & 0xFC00) >> 10;

	if (op_code == 0x00) {
		/* Non-basic op codes: 
		 * aaaaaaoooooo0000 */
		printf("%s ", nonbasic_op_code_map[a].name);
		size += print_value(offset, b);

	} else {
		int s1, s2 = 0;
		printf("%s ", basic_op_code_map[op_code].name);

		s1 = print_value(offset, a);
		printf(", ");
		s2 = print_value(offset+s1, b);

		size += s1 + s2;
	}

	return size;
}


void usage(char * name) {
	fprintf(stderr, "%s: -f <filename.bin>\n", name);
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
	int ch;
	int fd;
	char * filename = NULL;
	int i = 0;
	ssize_t filelen = 0;

	while ((ch = getopt(argc, argv, "f:")) != -1) {
		switch (ch) {
		case 'f':
			filename = optarg;
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

	if ((filelen = read(fd, buffer, sizeof(buffer))) < 0) {
		perror("error reading file ");
		exit(EXIT_FAILURE);
	}

	/* filelen - 1, since opcodes are at least 2 bytes */
	for (i = 0; i < (filelen - 1); ) {
		printf("%04x: ", i/2);
		word value = (buffer[i] << 8) + buffer[i+1];
		i += print_instruction(i, value);
		printf("\n");
	}

	exit(EXIT_SUCCESS);
}
	
