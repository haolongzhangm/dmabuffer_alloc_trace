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
