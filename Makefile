CFLAGS = -g -O2 -Wall

all: kvmbox loader

kvmbox: kvmbox.o
	gcc $(CFLAGS) kvmbox.o  -o kvmbox

kvmbox.o:

loader: loader.asm
	nasm loader.asm

clean:
	rm *.o kvmbox
