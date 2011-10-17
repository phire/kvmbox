#include <QtCore>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <linux/kvm.h>
#include <malloc.h>
#include <assert.h>
#include <stdint.h>
#include <stropts.h>

#include "kvmbox.h"
#include "kvmbox.hh"

/* callback definitions as shown in Listing 2 go here */

void load_file(void *mem, const char *filename)
{
    int  fd;
    int  nr;

    fd = open(filename, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "Cannot open %s", filename);
        perror("open");
        exit(1);
    }
    while ((nr = read(fd, mem, 4096)) != -1  &&  nr != 0)
        mem += nr;

    if (nr == -1) {
        perror("read");
        exit(1);
    }
    close(fd);
}

void printRegs(struct kvm *kvm) {
	struct kvm_regs regs;
	struct kvm_sregs sregs;
	int r = ioctl(kvm->vcpu_fd, KVM_GET_REGS, &regs);
	int s = ioctl(kvm->vcpu_fd, KVM_GET_SREGS, &sregs);
	if (r == -1 || s == -1) {
		fprintf(stderr, "Get Regs failed");
		return;
	}
	debugf("rax: 0x%08llx\n", regs.rax);
	debugf("rbx: 0x%08llx\n", regs.rbx);
	debugf("rcx: 0x%08llx\n", regs.rcx);
	debugf("rdx: 0x%08llx\n", regs.rdx);
	debugf("rsi: 0x%08llx\n", regs.rsi);
	debugf("rdi: 0x%08llx\n", regs.rdi);
	debugf("rsp: 0x%08llx\n", regs.rsp);
	debugf("rbp: 0x%08llx\n", regs.rbp);
	debugf("rip: 0x%08llx\n", regs.rip);
	debugf("=====================\n");
	debugf("cr0: 0x%016llx\n", sregs.cr0);
	debugf("cr2: 0x%016llx\n", sregs.cr2);
	debugf("cr3: 0x%016llx\n", sregs.cr3);
	debugf("cr4: 0x%016llx\n", sregs.cr4);
	debugf("cr8: 0x%016llx\n", sregs.cr8);
	debugf("gdt: 0x%04x:0x%08llx\n", sregs.gdt.limit, sregs.gdt.base);
	debugf("cs: 0x%08llx ds: 0x%08llx es: 0x%08llx\nfs: 0x%08llx gs: 0x%08llx ss: 0x%08llx\n",
			 sregs.cs.base, sregs.ds.base, sregs.es.base, sregs.fs.base, sregs.gs.base, sregs.ss.base);
}

void mmio_handler(struct kvm *kvm) {
	uint32_t addr = kvm->run->mmio.phys_addr;
	if(kvm->run->mmio.is_write) {
		debugf("Write %i to 0x%08x\n", kvm->run->mmio.len, addr);
		debugf("0x%08x\n",*(unsigned int*)(kvm->run->mmio.data));
	} else {
		debugf("Read %i from 0x%08x\n", kvm->run->mmio.len, addr);
	}
}

extern "C" {
void smbusIO(uint16_t, uint8_t, uint8_t, uint8_t*);
void pciConfigIO(uint16_t, uint8_t, uint8_t, uint8_t*);
}

void io_handler(struct kvm *kvm) {
	unsigned char *p = (unsigned char *)(kvm->run) + kvm->run->io.data_offset;
	assert(kvm->run->io.count == 1);
	uint16_t port = kvm->run->io.port;
	if(port >= 0xc000 && port <= 0xc008) 
		smbusIO(port, kvm->run->io.direction, kvm->run->io.size, p); 
	else if(port == 0xcf8 || port == 0xcfc) 
		pciConfigIO(port, kvm->run->io.direction, kvm->run->io.size, p);
	else if(kvm->run->io.direction) {
		debugf("I/O port 0x%04x out ", kvm->run->io.port);
		switch(kvm->run->io.size) {
			case 1:
				debugf("0x%02hhx\n", *(unsigned char*)p);
				break;
			case 2: 
				debugf("0x%04hx\n", *(unsigned short*)p);
				break;
			case 4:
				debugf("0x%08x\n", *(unsigned int*)p);

		}
	} else {
 		debugf("I/O 0x%04x in ", kvm->run->io.port);
		//*p = 0x20;
		switch(kvm->run->io.size) {
			case 1:
				debugf("byte\n");
				break;
			case 2: 
				debugf("short\n");
				break;
			case 4:
				debugf("int\n");

		}
	}
	//sleep(1);
}

Vcpu::Vcpu(struct kvm *kvm) {
	this->kvm = kvm;
}

void Vcpu::init() {
		int r;

		r = ioctl(this->kvm->vm_fd, KVM_CREATE_VCPU, 0);
		if (r == -1) {
			fprintf(stderr, "kvm_create_vcpu: %m\n");
			return;
		}
		this->kvm->vcpu_fd = r;
	
		long mmap_size = ioctl(kvm->fd, KVM_GET_VCPU_MMAP_SIZE, 0);
		if (mmap_size == -1) {
			fprintf(stderr, "get vcpu mmap size: %m\n");
			return;
		}
		void *map = mmap(NULL, mmap_size, PROT_READ|PROT_WRITE, MAP_SHARED,
				      kvm->vcpu_fd, 0);
		if (map == MAP_FAILED) {
			fprintf(stderr, "mmap vcpu area: %m\n");
			return;
		}
		this->kvm->run = (struct kvm_run*) map;

		this->threadId = QThread::currentThreadId();

		emit initialised();
}

void Vcpu::run() {
		ioctl(this->kvm->vcpu_fd, KVM_RUN, 0);
		switch(this->kvm->run->exit_reason){
			case KVM_EXIT_IO:
				io_handler(this->kvm);
				break;
			case KVM_EXIT_HLT:
				debugf("halted\n");
				printRegs(this->kvm);
				emit failure();
				return;
			case KVM_EXIT_MMIO:
				mmio_handler(this->kvm);
				break;
			case KVM_EXIT_INTR:
				debugf("Interrupt\n");
				emit failure();
				return;
			case KVM_EXIT_SHUTDOWN:
				printRegs(this->kvm);
				debugf("Triple fault\n");
				emit failure();
				return;
			case KVM_EXIT_FAIL_ENTRY:
				debugf("Failed to enter emulation: %llx\n", this->kvm->run->fail_entry.hardware_entry_failure_reason);
				emit failure();
				return;
			default:
				debugf("unhandled exit reason: %i\n", this->kvm->run->exit_reason);
				printRegs(this->kvm);
				emit failure();
				return;
		}
		emit exit();
}

struct kvm *vm_init(int argc, char *argv[]) {
	struct kvm *kvm = (struct kvm *) malloc(sizeof(struct kvm));

	int fd, r;
	fd = open("/dev/kvm", O_RDWR);
	if (fd == -1) {
		perror("open /dev/kvm");
		return NULL;
	}
	kvm->fd = fd;

	r = ioctl(kvm->fd, KVM_GET_API_VERSION, 0);
	assert(r == 12);

	fd = ioctl(kvm->fd, KVM_CREATE_VM, 0);
	if (fd == -1) {
		fprintf(stderr, "kvm_create_vm: %m\n");
		return NULL;
	}
	kvm->vm_fd = fd;
	
	// Give intel it's TSS space, I think this address is unused.
    r = ioctl(kvm->vm_fd, KVM_SET_TSS_ADDR, 0x0f000000);
    if (r == -1) {
        fprintf(stderr, "Error assigning TSS space: %m\n");
        return NULL;
    }


	kvm->ram = memalign(0x00400000, 0x04000000); // 64mb of 4mb aligned memory
	struct kvm_userspace_memory_region memory = {0, 0, 0x0, 0x04000000, (unsigned long) kvm->ram};

	r = ioctl(kvm->vm_fd, KVM_SET_USER_MEMORY_REGION, &memory);
	if (r == -1) {
		fprintf(stderr, "create_userspace_phys_mem: %i\n", r);
		return NULL;
	}

	load_file(kvm->ram + 0x000f0000, "loader");
    load_file(kvm->ram + 0x00000000, argv[1]);

	kvm->rom = memalign(0x00100000, 0x00100000); //1mb of 1mb aligned

	struct kvm_userspace_memory_region rom = {1, 0, 0xfff00000, 0x00100000, (unsigned long) kvm->rom};

	r = ioctl(kvm->vm_fd, KVM_SET_USER_MEMORY_REGION, &rom);
	if (r == -1) {
		fprintf(stderr, "create_userspace_phys_mem: %i\n", r);
		return NULL;
	}

	load_file(kvm->rom, argv[2]);

	// Patch out an annoying function call
	((unsigned char*)kvm->ram)[0x6b7] = 0x90;
	((unsigned char*)kvm->ram)[0x6b8] = 0x90;
	((unsigned char*)kvm->ram)[0x6b9] = 0x90;
	((unsigned char*)kvm->ram)[0x6ba] = 0x90;
	((unsigned char*)kvm->ram)[0x6bb] = 0x90;

	return kvm;
}
