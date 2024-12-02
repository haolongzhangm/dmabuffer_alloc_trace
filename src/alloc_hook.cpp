#include <sys/syscall.h>

#include "memory_hook.h"
#include "DebugData.h"
#include "PointerData.h"
#include "malloc_debug.h"

static bool before_main = true;
void __attribute__((constructor(111))) check(void) {
    before_main = false;
}

class AllocHook {
public:
    struct InitState {
        InitState() { 
            init_hook();
            before_init = true; 
        }
        ~InitState() { before_init = false; }
        static bool before_init;
    };

    AllocHook() {
        InitState state;
        void* ptr[2] = {&Db_storage, &Pd_storage};
        debug_initialize(ptr);
    }
    ~AllocHook() { debug_finalize(); }

    void* malloc(size_t size) { return debug_malloc(size); }
    void free(void* ptr) { debug_free(ptr); }
    void* calloc(size_t a, size_t b) { return debug_calloc(a, b); }
    void* realloc(void* ptr, size_t size) { return debug_realloc(ptr, size); }
    int posix_memalign(void** ptr, size_t alignment, size_t size) {
        return debug_posix_memalign(ptr, alignment, size);
    }
    int ioctl(int fd, int request, void* arg) { return debug_ioctl(fd, request, arg); }
    int close(int fd) { return debug_close(fd); }
    void* mmap(void* addr, size_t size, int prot, int flags, int fd, off_t offset) {
        return debug_mmap(addr, size, prot, flags, fd, offset);
    }
    int munmap(void* addr, size_t size) { return debug_munmap(addr, size); }

    void checkpoint(const char* file_name) { return debug_dump_heap(file_name); }

    static AllocHook& inst();

private:
    static std::aligned_storage<sizeof(DebugData), alignof(DebugData)>::type Db_storage;
    static std::aligned_storage<sizeof(PointerData), alignof(PointerData)>::type
            Pd_storage;
};
std::aligned_storage<sizeof(DebugData), alignof(DebugData)>::type AllocHook::Db_storage;
std::aligned_storage<sizeof(PointerData), alignof(PointerData)>::type
        AllocHook::Pd_storage;
bool AllocHook::InitState::before_init = false;

AllocHook& AllocHook::inst() {
    static AllocHook hook;
    return hook;
}

extern "C" {

void* malloc(size_t size) {
    if (AllocHook::InitState::before_init) {
        return m_sys_malloc(size);
    }
    return AllocHook::inst().malloc(size);
}

void free(void* ptr) {
    if (AllocHook::InitState::before_init) {
       return m_sys_free(ptr);
    }
    AllocHook::inst().free(ptr);
}

void* calloc(size_t a, size_t b) {
    if (AllocHook::InitState::before_init) {
        return m_sys_calloc(a, b);
    }
    return AllocHook::inst().calloc(a, b);
}

void* realloc(void* ptr, size_t size) {
    if (AllocHook::InitState::before_init) {
        return m_sys_realloc(ptr, size);
    }
    return AllocHook::inst().realloc(ptr, size);
}

int posix_memalign(void** ptr, size_t alignment, size_t size) {
    if (AllocHook::InitState::before_init) {
        return m_sys_posix_memalign(ptr, alignment, size);
    }
    return AllocHook::inst().posix_memalign(ptr, alignment, size);
}

int ioctl(int fd, int request, ...) {
    va_list ap;
    va_start(ap, request);
    void* arg = va_arg(ap, void*);
    va_end(ap);
    if (AllocHook::InitState::before_init) {
        return m_sys_ioctl(fd, request, arg);
    }
    return AllocHook::inst().ioctl(fd, request, arg);
}

int close(int fd) {
    if (AllocHook::InitState::before_init) {
        return m_sys_close(fd);
    }
    return AllocHook::inst().close(fd);
}

void* mmap(void* addr, size_t size, int prot, int flags, int fd, off_t offset) {
    // 程序初始化、类实例化时，会调用 mmap
    if (before_main) {
        return (void*)syscall(SYS_mmap, addr, size, prot, flags, fd, offset);
    }
    return AllocHook::inst().mmap(addr, size, prot, flags, fd, offset);
}

int munmap(void* addr, size_t size) {
    if (before_main) {
        return (int)syscall(SYS_munmap, addr, size);
    }
    return AllocHook::inst().munmap(addr, size);
}

void checkpoint(const char* file_name) {
    AllocHook::inst().checkpoint(file_name);
}
}