CFLAGS = -g -O2 -Wall
LIBS = -lpthread

all: kvmbox loader

kvmbox: main.c kvmbox.o smbus.o pci.o
	gcc $(CFLAGS) $(LIBS) main.c kvmbox.o smbus.o pci.o -o kvmbox

loader: loader.asm
	nasm loader.asm

clean:
	rm *.o kvmbox

run: kvmbox
	./kvmbox BIOSs/mine/2bl.img BIOSs/mine/bios.bin
