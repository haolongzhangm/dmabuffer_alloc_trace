#include <fcntl.h>
#include <dlfcn.h>
#include <cstdlib>

#include "memory_hook.h"

void* (*m_sys_malloc)(size_t) = nullptr;
void (*m_sys_free)(void*) = nullptr;
void* (*m_sys_calloc)(size_t, size_t) = nullptr;
void* (*m_sys_realloc)(void*, size_t) = nullptr;
void* (*m_sys_memalign)(size_t, size_t) = nullptr;
int (*m_sys_posix_memalign)(void**, size_t, size_t) = nullptr;
int (*m_sys_ioctl)(int fd, unsigned int request, ...) = nullptr;
int (*m_sys_close)(int fd) = nullptr;
void* (*m_sys_mmap)(
        void* addr, size_t size, int prot, int flags, int fd, off_t offset) = nullptr;
int (*m_sys_munmap)(void* addr, size_t size) = nullptr;


void init_hook() {
#ifdef __OHOS__
        void* handle = dlopen("libc.so", RTLD_LAZY);
        if (!handle) {
            abort();
        }
#else
        void* handle = RTLD_NEXT;
#endif

#define RESOLVE(name)                                                      \
        do {                                                               \
            auto addr = dlsym(handle, #name);                              \
            if (!addr) {                                                   \
                abort();                                                   \
            }                                                              \
            m_sys_##name = reinterpret_cast<decltype(m_sys_##name)>(addr); \
        } while (0)
        RESOLVE(malloc);
        RESOLVE(free);
        RESOLVE(calloc);
        RESOLVE(realloc);
        RESOLVE(memalign);
        RESOLVE(posix_memalign);
        RESOLVE(ioctl);
        RESOLVE(close);
        RESOLVE(mmap);
        RESOLVE(munmap);
#undef RESOLVE
}