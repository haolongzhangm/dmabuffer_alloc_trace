#include <dlfcn.h>
#include <pthread.h>
#include <cstddef>
#include <type_traits>

#include "DebugData.h"
#include "PointerData.h"
#include "malloc_debug.h"

class AllocHook {
public:
    AllocHook() { 
        void* ptr [2] = { &Db_storage, &Pd_storage }; 
        debug_initialize(ptr); 
    }
    ~AllocHook() { debug_finalize(); }

    void* malloc(size_t size) { return debug_malloc(size); }
    void free(void* ptr) { debug_free(ptr); }
    void* calloc(size_t a, size_t b) { return debug_calloc(a, b); }
    void* realloc(void* ptr, size_t size) { return debug_realloc(ptr, size); }
    int posix_memalign(void** ptr, size_t alignment, size_t size) { return debug_posix_memalign(ptr, alignment, size); }
    int ioctl(int fd, int request, void* arg) { return debug_ioctl(fd, request, arg); }
    int close(int fd) { return debug_close(fd); }

    static AllocHook& inst();

private:
    static std::aligned_storage<sizeof(DebugData), alignof(DebugData)>::type Db_storage;
    static std::aligned_storage<sizeof(PointerData), alignof(PointerData)>::type Pd_storage;
};
std::aligned_storage<sizeof(DebugData), alignof(DebugData)>::type AllocHook::Db_storage;
std::aligned_storage<sizeof(PointerData), alignof(PointerData)>::type AllocHook::Pd_storage;

AllocHook& AllocHook::inst() {
    static AllocHook hook;
    return hook;
}

extern "C" {
void* malloc(size_t size) {
    return AllocHook::inst().malloc(size);
}

void free(void* ptr) {
    AllocHook::inst().free(ptr);
}

void* calloc(size_t a, size_t b) {
    return AllocHook::inst().calloc(a, b);
}

void* realloc(void* ptr, size_t size) {
    return AllocHook::inst().realloc(ptr, size);
}

int posix_memalign(void** ptr, size_t alignment, size_t size) {
    return AllocHook::inst().posix_memalign(ptr, alignment, size);
}

int ioctl(int fd, int request, ...) {
    va_list ap;
    va_start(ap, request);
    void* arg = va_arg(ap, void*);
    va_end(ap);

    return AllocHook::inst().ioctl(fd, request, arg);
}

int close(int fd) {
    return AllocHook::inst().close(fd);
}

}