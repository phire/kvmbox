CFLAGS = -g -O2 -Wall

all: kvmbox loader

kvmbox: kvmbox.o smbus.o pci.o
	gcc $(CFLAGS) kvmbox.o smbus.o pci.o -o kvmbox

loader: loader.asm
	nasm loader.asm

clean:
	rm *.o kvmbox

run: kvmbox
	./kvmbox BIOSs/mine/2bl.img BIOSs/mine/bios.bin
