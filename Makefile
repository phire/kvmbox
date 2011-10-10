CFLAGS = -g -O2 -Wall

all: kvmbox loader

kvmbox: kvmbox.o smbus.o
	gcc $(CFLAGS) kvmbox.o smbus.o -o kvmbox

loader: loader.asm
	nasm loader.asm

clean:
	rm *.o kvmbox
