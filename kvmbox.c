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

struct kvm {
	int fd;
	int vm_fd;
	int vcpu_fd;
	struct kvm_run *run;
	void *ram;
	void *rom;
};

void printRegs(struct kvm *kvm) {
	struct kvm_regs regs;
	struct kvm_sregs sregs;
	int r = ioctl(kvm->vcpu_fd, KVM_GET_REGS, &regs);
	int s = ioctl(kvm->vcpu_fd, KVM_GET_SREGS, &sregs);
	if (r == -1 || s == -1) {
		printf("Get Regs failed");
		return;
	}
	printf("rax: 0x%08llx\n", regs.rax);
	printf("rbx: 0x%08llx\n", regs.rbx);
	printf("rcx: 0x%08llx\n", regs.rcx);
	printf("rdx: 0x%08llx\n", regs.rdx);
	printf("rsi: 0x%08llx\n", regs.rsi);
	printf("rdi: 0x%08llx\n", regs.rdi);
	printf("rsp: 0x%08llx\n", regs.rsp);
	printf("rbp: 0x%08llx\n", regs.rbp);
	printf("rip: 0x%08llx\n", regs.rip);
	printf("=====================\n");
	printf("cr0: 0x%016llx\n", sregs.cr0);
	printf("cr2: 0x%016llx\n", sregs.cr2);
	printf("cr3: 0x%016llx\n", sregs.cr3);
	printf("cr4: 0x%016llx\n", sregs.cr4);
	printf("cr8: 0x%016llx\n", sregs.cr8);
	printf("gdt: 0x%04x:0x%08llx\n", sregs.gdt.limit, sregs.gdt.base);
	printf("cs: 0x%08llx ds: 0x%08llx es: 0x%08llx\nfs: 0x%08llx gs: 0x%08llx ss: 0x%08llx\n",
			 sregs.cs.base, sregs.ds.base, sregs.es.base, sregs.fs.base, sregs.gs.base, sregs.ss.base);
}

void mmio_handler(struct kvm *kvm) {
	uint32_t addr = kvm->run->mmio.phys_addr;
	if(addr > 0xfffc0000 && kvm->run->mmio.len == 4) {
		*(uint32_t *)(kvm->run->mmio.data) = *(uint32_t *)(kvm->rom + (addr & 0x3ffff));
	}
	if(kvm->run->mmio.is_write) {
		printf("Write %i to 0x%08x\n", kvm->run->mmio.len, kvm->run->mmio.phys_addr);
		printf("0x%08x\n",*(unsigned int*)kvm->run->mmio.data);
	} else {
		printf("Read %i from 0x%08x\n", kvm->run->mmio.len, kvm->run->mmio.phys_addr);
	//	kvm->run->mmio.data[0] = 0xc0;
	//	kvm->run->mmio.data[1] = 0x0d;
	//	kvm->run->mmio.data[2] = 0;
	//	kvm->run->mmio.data[3] = 0;
		
	}
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
		printf("I/O port 0x%04x out ", kvm->run->io.port);
		switch(kvm->run->io.size) {
			case 1:
				printf("0x%02hhx\n", *(unsigned char*)p);
				break;
			case 2: 
				printf("0x%04hx\n", *(unsigned short*)p);
				break;
			case 4:
				printf("0x%08x\n", *(unsigned int*)p);

		}
	} else {
 		printf("I/O 0x%04x in ", kvm->run->io.port);
		//*p = 0x20;
		switch(kvm->run->io.size) {
			case 1:
				printf("byte\n");
				break;
			case 2: 
				printf("short\n");
				break;
			case 4:
				printf("int\n");

		}
	}
	//sleep(1);
}

int main(int argc, char *argv[])
{
	struct kvm *kvm = malloc(sizeof(struct kvm));

	int fd, r;
	fd = open("/dev/kvm", O_RDWR);
	if (fd == -1) {
		perror("open /dev/kvm");
		return -1;
	}
	kvm->fd = fd;

	r = ioctl(kvm->fd, KVM_GET_API_VERSION, 0);
	printf("api version is %i\n", r);

	fd = ioctl(kvm->fd, KVM_CREATE_VM, 0);
	if (fd == -1) {
		fprintf(stderr, "kvm_create_vm: %m\n");
		return -1;
	}
	kvm->vm_fd = fd;
	
	r = ioctl(kvm->vm_fd, KVM_CREATE_VCPU, 0);
	if (r == -1) {
		fprintf(stderr, "kvm_create_vcpu: %m\n");
		return -1;
	}
	kvm->vcpu_fd = r;
	
	long mmap_size = ioctl(kvm->fd, KVM_GET_VCPU_MMAP_SIZE, 0);
	if (mmap_size == -1) {
		fprintf(stderr, "get vcpu mmap size: %m\n");
		return -1;
	}
	void *map = mmap(NULL, mmap_size, PROT_READ|PROT_WRITE, MAP_SHARED,
			      kvm->vcpu_fd, 0);
	if (map == MAP_FAILED) {
		fprintf(stderr, "mmap vcpu area: %m\n");
		return -1;
	}
	kvm->run = (struct kvm_run*) map;

	// Give intel it's TSS space, I think this address is unused.
    r = ioctl(kvm->vm_fd, KVM_SET_TSS_ADDR, 0x0f000000);
    if (r == -1) {
        fprintf(stderr, "Error assigning TSS space: %m\n");
        return -1;
    }


	kvm->ram = memalign(0x00400000, 0x04000000); // 64mb of 4mb aligned memory
	struct kvm_userspace_memory_region memory = {
		.memory_size = 0x04000000,
		.guest_phys_addr = 0x0,
		.userspace_addr = (unsigned long) kvm->ram,
		.flags = 0,
		.slot = 0,
	};
	
	r = ioctl(kvm->vm_fd, KVM_SET_USER_MEMORY_REGION, &memory);
	if (r == -1) {
		fprintf(stderr, "create_userspace_phys_mem: %i\n", r);
		return -1;
	}

	load_file(kvm->ram + 0x000f0000, "loader");
    load_file(kvm->ram + 0x00000000, argv[1]);

	kvm->rom = memalign(0x00100000, 0x00100000); //1mb of 1mb aligned

	struct kvm_userspace_memory_region rom = {
		.memory_size = 0x00100000,
		.guest_phys_addr = 0xfff00000,
		.userspace_addr = (unsigned long) kvm->rom,
		.flags = 0,
		.slot = 1,
	};
	
	r = ioctl(kvm->vm_fd, KVM_SET_USER_MEMORY_REGION, &rom);
	if (r == -1) {
		fprintf(stderr, "create_userspace_phys_mem: %i\n", r);
		return -1;
	}

	load_file(kvm->rom, argv[2]);

	((unsigned char*)kvm->ram)[0x6b7] = 0x90;
	((unsigned char*)kvm->ram)[0x6b8] = 0x90;
	((unsigned char*)kvm->ram)[0x6b9] = 0x90;
	((unsigned char*)kvm->ram)[0x6ba] = 0x90;
	((unsigned char*)kvm->ram)[0x6bb] = 0x90;

	while (1) {
		ioctl(kvm->vcpu_fd, KVM_RUN, 0);
		switch(kvm->run->exit_reason){
			case KVM_EXIT_IO:
				io_handler(kvm);
				break;
			case KVM_EXIT_HLT:
				printf("halted\n");
				printRegs(kvm);
				return -1;
			case KVM_EXIT_MMIO:
				mmio_handler(kvm);
				break;
			case KVM_EXIT_SHUTDOWN:
				printRegs(kvm);
				printf("Triple fault\n");
				return -1;
			case KVM_EXIT_FAIL_ENTRY:
				printf("Failed to enter emulation: %x\n", kvm->run->fail_entry.hardware_entry_failure_reason);
				return -1;
			default:
				printf("unhandled exit reason: %i\n", kvm->run->exit_reason);
				printRegs(kvm);
				return -1;
		}
	}

	printf("Done\n");
    return 0;
}
