#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <linux/kvm.h>
#include <malloc.h>

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
	printf("rax: 0x%08x\n", regs.rax);
	printf("rbx: 0x%08x\n", regs.rbx);
	printf("rcx: 0x%08x\n", regs.rcx);
	printf("rdx: 0x%08x\n", regs.rdx);
	printf("rsi: 0x%08x\n", regs.rsi);
	printf("rdi: 0x%08x\n", regs.rdi);
	printf("rsp: 0x%08x\n", regs.rsp);
	printf("rbp: 0x%08x\n", regs.rbp);
	printf("rip: 0x%08x\n", regs.rip);
	printf("=====================\n");
	printf("cr0: 0x%016x\n", sregs.cr0);
	printf("cr2: 0x%016x\n", sregs.cr2);
	printf("cr3: 0x%016x\n", sregs.cr3);
	printf("cr4: 0x%016x\n", sregs.cr4);
	printf("cr8: 0x%016x\n", sregs.cr8);
	printf("gdt: 0x%016x\n", sregs.gdt);
	printf("cs: 0x%08x ds: 0x%08x es: 0x%08x\nfs: 0x%08x gs: 0x%08x ss: 0x%08x\n",
			 sregs.cs, sregs.ds, sregs.es, sregs.fs, sregs.gs, sregs.ss);
}

void initRegs(struct kvm *kvm) {
	struct kvm_regs regs;
	struct kvm_sregs sregs;
	int r = ioctl(kvm->vcpu_fd, KVM_GET_REGS, &regs);
	int s = ioctl(kvm->vcpu_fd, KVM_GET_SREGS, &sregs);
	if (r == -1 || s == -1) {
		printf("Get Regs failed");
		return;
	}
	sregs.cr0 |= 0x80000021; // protected mode with paging
	sregs.cr4 = 0x00;
	regs.rip = 0x004001bc; // Entery point
	r = ioctl(kvm->vcpu_fd, KVM_SET_REGS, &regs);
	s = ioctl(kvm->vcpu_fd, KVM_SET_SREGS, &sregs);
	if (r == -1 || s == -1) {
		printf("Set Regs failed");
		return;
	}

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

	kvm->ram = memalign(0x0040000, 0x04000000); // 64mb of 4mb aligned memory
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

	load_file(kvm->ram + 0x00f0000, "loader");
    load_file(kvm->ram + 0x0040000, argv[1]);
	//initRegs(kvm);
	printRegs(kvm);
	r = ioctl(kvm->vcpu_fd, KVM_RUN, 0);
	printf("exit reason: %i\n", kvm->run->exit_reason);
	printf("exit reason: %x\n", (int) kvm->run->internal.suberror);
	printRegs(kvm);

	printf("Done\n");
    return 0;
}
