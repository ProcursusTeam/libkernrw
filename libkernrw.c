#include <unistd.h>
#include <mach/mach.h>
#include <errno.h>
#include "krw_daemonUser.h"

#define KERNRW_EXPORT __attribute__ ((visibility ("default")))

extern kern_return_t mach_vm_read_overwrite(task_t task, mach_vm_address_t addr, mach_vm_size_t size, mach_vm_address_t data, mach_vm_size_t *outsize);
extern kern_return_t mach_vm_write(task_t task, mach_vm_address_t addr, mach_vm_address_t data, mach_msg_type_number_t dataCnt);

mach_port_t krwPort = MACH_PORT_NULL;
mach_port_t kernelPort = MACH_PORT_NULL;

KERNRW_EXPORT int requestKernRw(void) {
	kern_return_t ret = host_get_special_port(mach_host_self(), HOST_LOCAL_NODE, 14, &krwPort);
	if (getuid() == 0 && ret == KERN_SUCCESS){
		if (MACH_PORT_VALID(krwPort)){
			return 0;
		}
	}
	krwPort = MACH_PORT_NULL;

	task_t taskPort = MACH_PORT_NULL;

	ret = host_get_special_port(mach_host_self(), HOST_LOCAL_NODE, 4, &taskPort);
	if (ret == KERN_SUCCESS){
		if (MACH_PORT_VALID(taskPort)){
			kernelPort = taskPort;
			return 0;
		}
	} else if (ret == KERN_INVALID_ARGUMENT){
		return EPERM;
	} else {
		return ENXIO;
	}

	taskPort = MACH_PORT_NULL;
	ret = task_for_pid(mach_task_self(), 0, &taskPort);
	if (ret == KERN_SUCCESS){
		if (MACH_PORT_VALID(taskPort)){
			kernelPort = taskPort;
			return 0;
		}
		return ENXIO;
	}
	return EPERM;
}

KERNRW_EXPORT kern_return_t kernRW_read32(uint64_t addr, uint32_t *val){
	if (MACH_PORT_VALID(krwPort)){
		return krw_read32(krwPort, addr, val);
	}
	if (MACH_PORT_VALID(kernelPort)){
		mach_vm_size_t sz32 = sizeof(uint32_t);
		return mach_vm_read_overwrite(kernelPort, addr, sz32, (mach_vm_address_t)val, &sz32);
	}
	return KERN_INVALID_NAME;
}

KERNRW_EXPORT kern_return_t kernRW_read64(uint64_t addr, uint64_t *val){
	if (MACH_PORT_VALID(krwPort)){
		return krw_read64(krwPort, addr, val);
	}
	if (MACH_PORT_VALID(kernelPort)){
		mach_vm_size_t sz64 = sizeof(uint64_t);
		return mach_vm_read_overwrite(kernelPort, addr, sz64, (mach_vm_address_t)val, &sz64);
	}
	return KERN_INVALID_NAME;
}

KERNRW_EXPORT kern_return_t kernRW_write32(uint64_t addr, uint32_t val){
	if (MACH_PORT_VALID(krwPort)){
		return krw_write32(krwPort, addr, val);
	}
	if (MACH_PORT_VALID(kernelPort)){
		mach_msg_type_number_t sz32 = sizeof(uint32_t);
		return mach_vm_write(kernelPort, addr, (mach_vm_address_t)&val, sz32);
	}
	return KERN_INVALID_NAME;
}

KERNRW_EXPORT kern_return_t kernRW_write64(uint64_t addr, uint64_t val){
	if (MACH_PORT_VALID(krwPort)){
		return krw_write64(krwPort, addr, val);
	}
	if (MACH_PORT_VALID(kernelPort)){
		mach_msg_type_number_t sz64 = sizeof(uint32_t);
		return mach_vm_write(kernelPort, addr, (mach_vm_address_t)&val, sz64);
	}
	return KERN_INVALID_NAME;
}

#define KERNRW_CHUNK_SIZE64 sizeof(uint64_t)
#define KERNRW_CHUNK_SIZE32 sizeof(uint32_t)
#define TFP0_MAX_CHUNK_SIZE 0xFFF

KERNRW_EXPORT kern_return_t kernRW_readbuf(uint64_t addr, void *buf, size_t len){
	if (addr + len < addr || (mach_vm_address_t)buf + len < (mach_vm_address_t)buf){
		return KERN_INVALID_ARGUMENT;
	}
	if (MACH_PORT_VALID(krwPort)){
		kern_return_t ret = KERN_SUCCESS;

		size_t remainder = len % KERNRW_CHUNK_SIZE64;
		if (remainder == 0)
			remainder = KERNRW_CHUNK_SIZE64;
		size_t tmpSz = len + (KERNRW_CHUNK_SIZE64 - remainder);
		if (len == 0)
			tmpSz = 0;

		uint64_t *dstBuf = (uint64_t *)buf;

		size_t alignedSize = (len & ~(KERNRW_CHUNK_SIZE64 - 1));
		for (int64_t i = 0; i < alignedSize; i += KERNRW_CHUNK_SIZE64){
			ret = krw_read64(krwPort, addr + i, &dstBuf[i / KERNRW_CHUNK_SIZE64]);
			if (ret){
				return ret;
			}
		}
		if (len > alignedSize){
			uint64_t r = 0;
			ret = krw_read64(krwPort, addr + alignedSize, &r);
			if (ret){
				return ret;
			}
			memcpy(((uint8_t*)buf)+alignedSize, &r, len-alignedSize);
		}
		return ret;
	}
	if (MACH_PORT_VALID(kernelPort)){
		size_t offset = 0;
		kern_return_t ret = KERN_SUCCESS;
		while (offset < len){
			mach_vm_size_t sz, chunk = TFP0_MAX_CHUNK_SIZE;
			if (chunk > len - offset){
				chunk = len - offset;
			}
			ret = mach_vm_read_overwrite(kernelPort, addr + offset, chunk, (mach_vm_address_t)buf + offset, &sz);
			if (ret || sz == 0){
				break;
			}
			offset += sz;
		}
		return ret;
	}
	return KERN_INVALID_NAME;
}

KERNRW_EXPORT kern_return_t kernRW_writebuf(uint64_t addr, const void *buf, size_t len){
	if (addr + len < addr || (mach_vm_address_t)buf + len < (mach_vm_address_t)buf){
		return KERN_INVALID_ARGUMENT;
	}
	if (MACH_PORT_VALID(krwPort)){
		kern_return_t ret = KERN_SUCCESS;

		size_t remainder = len & KERNRW_CHUNK_SIZE64;
		if (remainder == 0)
			remainder = KERNRW_CHUNK_SIZE64;
		size_t tmpSz = len + (KERNRW_CHUNK_SIZE64 - remainder);
		if (len == 0)
			tmpSz = 0;

		uint64_t *dstBuf = (uint64_t *)buf;
		size_t alignedSize = (len & ~(KERNRW_CHUNK_SIZE64 - 1));
		for (int64_t i = 0; i < alignedSize; i+= KERNRW_CHUNK_SIZE64){
			ret = krw_write64(krwPort, addr + i, dstBuf[i / KERNRW_CHUNK_SIZE64]);
			if (ret){
				return ret;
			}
		}
		if (len > alignedSize){
			size_t remainingSize = len - alignedSize;
			if (remainingSize > KERNRW_CHUNK_SIZE32){
				uint32_t val = 0;
				ret = krw_read32(krwPort, addr + alignedSize, &val);
				if (ret){
					return ret;
				}
				memcpy(&val, ((uint8_t*)buf) + alignedSize, remainingSize);
				ret = krw_write32(krwPort, addr + alignedSize, val);
				if (ret){
					return ret;
				}
			} else {
				uint64_t val = 0;
				ret = krw_read64(krwPort, addr + alignedSize, &val);
				if (ret){
					return ret;
				}
				memcpy(&val, ((uint8_t*)buf) + alignedSize, remainingSize);
				ret = krw_write64(krwPort, addr + alignedSize, val);
				if (ret){
					return ret;
				}
			}
		}
		return ret;

	}
	if (MACH_PORT_VALID(kernelPort)){
		size_t offset = 0;
		kern_return_t ret = KERN_SUCCESS;
		while (offset < len){
			mach_vm_size_t chunk = TFP0_MAX_CHUNK_SIZE;
			if (chunk > len - offset){
				chunk = len - offset;
			}
			ret = mach_vm_write(kernelPort, addr + offset, (mach_vm_address_t)buf + offset, chunk);
			if (ret){
				break;
			}
			offset += chunk;
		}
		return ret;
	}
	return KERN_INVALID_NAME;
}

KERNRW_EXPORT kern_return_t kernRW_getKernelBase(uint64_t *val){
	if (MACH_PORT_VALID(krwPort)){
		return krw_kernelBase(krwPort, val);
	}
	if (MACH_PORT_VALID(kernelPort)){
		task_dyld_info_data_t info;
		uint32_t count = TASK_DYLD_INFO_COUNT;
		kern_return_t ret = task_info(kernelPort, TASK_DYLD_INFO, (task_info_t)&info, &count);
		if (ret != KERN_SUCCESS){
			return ret;
		}
		if (info.all_image_info_addr == 0 && info.all_image_info_size == 0){
			return KERN_INVALID_ARGUMENT;
		}
		if (val){
			*val = info.all_image_info_addr;
		}
		return KERN_SUCCESS;
	}
	return KERN_INVALID_NAME;
}

KERNRW_EXPORT kern_return_t kernRW_getKernelProc(uint64_t *val){
	if (MACH_PORT_VALID(krwPort)){
		return krw_kernelProc(krwPort, val);
	}
	if (MACH_PORT_VALID(kernelPort)){
		return KERN_INVALID_ARGUMENT;
	}
	return KERN_INVALID_NAME;
}

KERNRW_EXPORT kern_return_t kernRW_getAllProc(uint64_t *val){
	if (MACH_PORT_VALID(krwPort)){
		return krw_allProc(krwPort, val);
	}
	if (MACH_PORT_VALID(kernelPort)){
		return KERN_INVALID_ARGUMENT;
	}
	return KERN_INVALID_NAME;
}