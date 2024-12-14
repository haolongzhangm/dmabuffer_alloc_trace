#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <cstddef>

#include "DebugData.h"
#include "PointerData.h"
#include "malloc_debug.h"
#include "memory_hook.h"

struct InitState {
    InitState() { allocHook_setup = true; }
    ~InitState() { allocHook_setup = false; }
    static bool allocHook_setup;
};
bool InitState::allocHook_setup = false;

class AllocHook {
public:
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

AllocHook& AllocHook::inst() {
    static AllocHook hook;
    return hook;
}

static bool befor_main = true;
void __attribute__((constructor(201))) check(void) {
    init_hook();
    befor_main = false;
}

extern "C" {
// 程序初始化会间接调用 malloc 和 free
void* malloc(size_t size) {
    if (befor_main || InitState::allocHook_setup) {
        return (void*)syscall(
                SYS_mmap, 0, PAGE_SIZE, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
    }
    return AllocHook::inst().malloc(size);
}

void free(void* ptr) {
    if (befor_main || InitState::allocHook_setup) {
        syscall(SYS_munmap, ptr, PAGE_SIZE);
        return;
    }
    return AllocHook::inst().free(ptr);
}

// calloc 和 realloc 属于用户级函数
void* calloc(size_t a, size_t b) {
    if (InitState::allocHook_setup) {
        return (void*)syscall(
                SYS_mmap, 0, PAGE_SIZE, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
    }
    return AllocHook::inst().calloc(a, b);
}

void* realloc(void* ptr, size_t size) {
    if (InitState::allocHook_setup) {
        return (void*)syscall(
                SYS_mmap, 0, PAGE_SIZE, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
    }
    return AllocHook::inst().realloc(ptr, size);
}

// 进程初始化 和 debug init 的过程不应该调用 posix_memalign
int posix_memalign(void** ptr, size_t alignment, size_t size) {
    return AllocHook::inst().posix_memalign(ptr, alignment, size);
}

// 程序初始化时会调用 mmap, 类的构造函数如果包含内存申请，可能也会调用 mmap
void* mmap(void* addr, size_t size, int prot, int flags, int fd, off_t offset) {
    if (befor_main) {
        return (void*)syscall(SYS_mmap, addr, size, prot, flags, fd, offset);
    }
    return AllocHook::inst().mmap(addr, size, prot, flags, fd, offset);
}

int munmap(void* addr, size_t size) {
    if (befor_main) {
        return (int)syscall(SYS_munmap, addr, size);
    }
    return AllocHook::inst().munmap(addr, size);
}

void checkpoint(const char* file_name) {
    AllocHook::inst().checkpoint(file_name);
}
}