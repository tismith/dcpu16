all: dcpu16 disassemble

dcpu16: dcpu16.c
	gcc -o $@ $<

disassemble: disassemble.c
	gcc -o $@ $<

.PHONY: clean
clean: 
	-rm dcpu16 disassemble

