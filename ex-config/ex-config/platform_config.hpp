#ifndef ISU_PLATFORM_CONFIG_HPP
#define ISU_PLATFORM_CONFIG_HPP

#ifdef _MSC_VER
#define _WINDOWS
#endif

typedef void* sysptr_t;

#ifdef _WINDOWS

typedef sysptr_t kernel_object_handle;
typedef sysptr_t file_descriptor;

#define SYSTEM_THREAD_CALLBACK __stdcall
#define SYSTEM_THREAD_RET DWORD

#define SYSTEM_THREAD_RET_CODE(val) \
	return static_cast<SYSTEM_THREAD_RET>(val);

#define SYS_CALLBACK CALLBACK

#endif

#ifdef _LINUX

typedef FD kernel_object_handle;
typedef FD file_descriptor;

#endif
#include <memory>
inline void mymemcpy(void* dest, const void* src, unsigned int count)
{
	memcpy(dest, src, count);
}
#endif