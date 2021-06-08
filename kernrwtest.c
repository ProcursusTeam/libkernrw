#include <mach/mach.h>
#include <stdio.h>
#include "libkernrw.h"

int main(){
	printf("Request RW? %d\n", requestKernRw());
	uint64_t addr = 0;
	kern_return_t ret = kernRW_getKernelBase(&addr);
	printf("kbase: 0x%llx [%s]\n", addr, mach_error_string(ret));
	ret = kernRW_getKernelProc(&addr);
	printf("kproc: 0x%llx [%s]\n", addr, mach_error_string(ret));
	ret = kernRW_getAllProc(&addr);
	printf("allproc: 0x%llx [%s]\n", addr, mach_error_string(ret));
	return 0;
}