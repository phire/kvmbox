// Copyright (c) 2011 Scott Mansell <phiren@gmail.com>
// Licensed under the MIT license
// Refer to the included LICENCE file.

#include <signal.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include "kvmbox.h"

void debugf(char *format, ...) {
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
} 

void nopSignalHandler() {
	// We don't actually need to do anything here, but we need to interrupt
	// the execution of the guest.
}

int main(int argc, char *argv[])
{
	int r;
	signal(SIGUSR1,nopSignalHandler); // Prevent termination on USER1 signals

	struct kvm *kvm = vm_init(argc, argv);
	if(kvm == NULL) {
		return -1;
	}
	pthread_t vcpuThread;
	r = pthread_create(&vcpuThread, NULL, vcpu_run, kvm);
    assert(r == 0);
	pthread_join(vcpuThread, &r);
	return r;
}
