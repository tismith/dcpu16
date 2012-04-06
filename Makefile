all: dcpu16

dcpu16: dcpu16.c
	gcc -o $@ $<

.PHONY: clean
clean: 
	-rm dcpu16

