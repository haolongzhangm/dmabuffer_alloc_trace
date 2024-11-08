#include <dlfcn.h>
#include <pthread.h>
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

    void* malloc(size_t size);
    void free(void* ptr);

    static AllocHook& inst();
    static std::aligned_storage<sizeof(DebugData), alignof(DebugData)>::type Db_storage;
    static std::aligned_storage<sizeof(PointerData), alignof(PointerData)>::type Pd_storage;
};
std::aligned_storage<sizeof(DebugData), alignof(DebugData)>::type AllocHook::Db_storage;
std::aligned_storage<sizeof(PointerData), alignof(PointerData)>::type AllocHook::Pd_storage;

void* AllocHook::malloc(size_t size) {
    return debug_malloc(size);
}

void AllocHook::free(void* ptr) {
    debug_free(ptr);
}

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

}