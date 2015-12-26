// Copyright (c) 2011 Scott Mansell <phiren@gmail.com>
// Licensed under the MIT license
// Refer to the included LICENCE file.

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

#ifdef __cplusplus
}
#endif
#endif
