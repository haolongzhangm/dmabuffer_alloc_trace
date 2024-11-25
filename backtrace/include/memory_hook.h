#pragma once
#include <sys/types.h>
#include <cstddef>

extern void* (*m_sys_malloc)(size_t);
extern void (*m_sys_free)(void*);
extern void* (*m_sys_calloc)(size_t, size_t);
extern void* (*m_sys_realloc)(void*, size_t);
extern void* (*m_sys_memalign)(size_t, size_t);
extern int (*m_sys_posix_memalign)(void**, size_t, size_t);
extern int (*m_sys_ioctl)(int fd, unsigned int request, ...);
extern int (*m_sys_close)(int fd);
extern void* (*m_sys_mmap)(void* addr, size_t size, int prot, int flags, int fd, off_t offset);
extern int (*m_sys_munmap)(void* addr, size_t size);
