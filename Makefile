all: dcpu16

dcpu16: dcpu16.c
	gcc -o $@ $<

