#include <dlfcn.h>
#include <pthread.h>

#include "backtrace_helper/backtrace.h"

class AllocHook {
    //! thread-local storage, 防止递归调用
    struct Tls {
        size_t recur_depth = 0;

        static void dtor(void* tls) {
            if (tls) {
                static_cast<Tls*>(tls)->~Tls();
                m_sys_free(tls);
            }
        }
    };

    class MarkRecursive {
        Tls* m_tls;

    public:
        explicit MarkRecursive(Tls* tls) : m_tls{tls} { ++tls->recur_depth; }
        ~MarkRecursive() { --m_tls->recur_depth; }
    };

    Tls* get_tls();
    Tls* get_tls_if_enabled();
    static bool is_recursive(const Tls* tls) { return tls->recur_depth; }

    void* (*m_sys_malloc)(size_t);
    static void (*m_sys_free)(void*);
    void* (*m_sys_calloc)(size_t, size_t);
    void* (*m_sys_realloc)(void*, size_t);
    int (*m_sys_posix_memalign)(void**, size_t, size_t);

    pthread_key_t m_tls_key;
    bool m_tls_key_valid = false;

public:
    AllocHook();
    ~AllocHook();

    void* malloc(size_t size);
    void free(void* ptr);
    void* calloc(size_t a, size_t b);
    void* realloc(void* ptr, size_t size);
    int posix_memalign(void** ptr, size_t alignment, size_t size);
    static AllocHook& inst();
};

// 定义静态成员变量
void (*AllocHook::m_sys_free)(void*) = nullptr;

AllocHook::AllocHook() {
#define RESOLVE(name)                                                  \
    do {                                                               \
        auto addr = dlsym(RTLD_NEXT, #name);                           \
        if (!addr) {                                                   \
            FAIL_EXIT("can not resolve %s: %s", #name, dlerror());     \
        }                                                              \
        m_sys_##name = reinterpret_cast<decltype(m_sys_##name)>(addr); \
    } while (0)
    RESOLVE(malloc);
    RESOLVE(free);
    RESOLVE(calloc);
    RESOLVE(realloc);
    RESOLVE(posix_memalign);
#undef RESOLVE
    if (auto err = pthread_key_create(&m_tls_key, Tls::dtor)) {
        FAIL_EXIT("failed to create pthread key: %s", strerror(err));
    }
    m_tls_key_valid = true;
}

AllocHook::~AllocHook() {
    if (m_tls_key_valid) {
        pthread_key_delete(m_tls_key);
    }
}

AllocHook::Tls* AllocHook::get_tls() {
    void* ptr;
    if (!(ptr = pthread_getspecific(m_tls_key))) {
        ptr = m_sys_malloc(sizeof(Tls));
        new (ptr) Tls{};
        pthread_setspecific(m_tls_key, ptr);
    }
    return static_cast<Tls*>(ptr);
}

AllocHook::Tls* AllocHook::get_tls_if_enabled() {
    auto tls = get_tls();
    if (is_recursive(tls)) {
        return nullptr;
    }
    return tls;
}

void* AllocHook::malloc(size_t size) {
    auto tls = get_tls_if_enabled();
    if (!tls) {
        return m_sys_malloc(size);
    }
    MarkRecursive mr{tls};
    void* ptr = m_sys_malloc(size);
    debug::Record::get_instance().host_alloc(ptr, size);
    return ptr;
}

void AllocHook::free(void* ptr) {
    auto tls = get_tls_if_enabled();
    if (!tls) {
        // exit func
        return m_sys_free(ptr);
    }
    MarkRecursive mr{tls};
    debug::Record::get_instance().host_free(ptr);
    m_sys_free(ptr);
}

void* AllocHook::calloc(size_t a, size_t b) {
    auto tls = get_tls_if_enabled();
    if (!tls) {
        return m_sys_calloc(a, b);
    }
    MarkRecursive mr{tls};
    void* ptr = m_sys_calloc(a, b);
    debug::Record::get_instance().host_alloc(ptr, a * b);
    return ptr;
}

void* AllocHook::realloc(void* ptr, size_t size) {
    auto tls = get_tls_if_enabled();
    if (!tls) {
        return m_sys_realloc(ptr, size);
    }
    MarkRecursive mr{tls};
    void* res = m_sys_realloc(ptr, size);
    debug::Record::get_instance().host_alloc(res, size);
    return res;
}

int AllocHook::posix_memalign(void** ptr, size_t alignment, size_t size) {
    auto tls = get_tls_if_enabled();
    if (!tls) {
        return m_sys_posix_memalign(ptr, alignment, size);
    }
    MarkRecursive mr{tls};
    debug::Record::get_instance().host_alloc(*ptr, size);
    return m_sys_posix_memalign(ptr, alignment, size);
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

void* calloc(size_t a, size_t b) {
    return AllocHook::inst().calloc(a, b);
}

void* realloc(void* ptr, size_t size) {
    return AllocHook::inst().realloc(ptr, size);
}

int posix_memalign(void** ptr, size_t alignment, size_t size) {
    return AllocHook::inst().posix_memalign(ptr, alignment, size);
}
}