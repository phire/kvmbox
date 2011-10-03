all: kvmbox loader

kvmbox: kvmbox.o
	gcc kvmbox.o  -o kvmbox

kvmbox.o:

loader: loader.asm
	nasm loader.asm

clean:
	rm *.o kvmbox
