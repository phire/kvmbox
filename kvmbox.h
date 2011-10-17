#ifndef KVMBOX_H
#define KVMBOX_H
#ifdef __cplusplus
extern "C" {
#endif

struct kvm {
	int fd;
	int vm_fd;
	int vcpu_fd;
	struct kvm_run *run;
	void *ram;
	void *rom;
};

int vcpu_run(struct kvm *kvm);
struct kvm *vm_init(int argc, char *argv[]);

void debugf(const char *format, ...);

#ifdef __cplusplus
}
#endif
#endif
