

#include <dlfcn.h>
#include <pthread.h>
#include <cstring>
#include <atomic>

#include "backtrace_helper/backtrace.h"

static std::atomic_bool live(false);
static void* (*m_sys_malloc)(size_t);
static void (*m_sys_free)(void*);
static void* (*m_sys_calloc)(size_t, size_t);
static void* (*m_sys_realloc)(void*, size_t);
static int (*m_sys_posix_memalign)(void**, size_t, size_t);

class NonCopyable {
    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;

public:
    NonCopyable() = default;
};

class AllocHook : public NonCopyable {
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

    pthread_key_t m_tls_key;
    bool m_tls_key_valid = false;

public:
    AllocHook();

    ~AllocHook();

    void *malloc(size_t size);

    void free(void* ptr);

    void* calloc(size_t a, size_t b);

    void* realloc(void* ptr, size_t size);

    int posix_memalign(void** ptr, size_t alignment, size_t size);

    static AllocHook& inst();
};

AllocHook::AllocHook() {
    live.store(true, std::memory_order_seq_cst);
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
    live.store(false, std::memory_order_seq_cst);
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
    if (!tls || !live.load()) {
        return m_sys_malloc(size);
    }
    MarkRecursive mr{tls};
    void* ptr = m_sys_malloc(size);
    debug::Record::get_instance().update_host(ptr, size, true);
    return ptr;
}

void AllocHook::free(void* ptr) {
    auto tls = get_tls_if_enabled();
    if (!tls || !live.load()) {
        // exit func
        return m_sys_free(ptr);
    }
    MarkRecursive mr{tls};
    debug::Record::get_instance().update_host(ptr, 0, false);
    m_sys_free(ptr);
}

void* AllocHook::calloc(size_t a, size_t b) {
    auto tls = get_tls_if_enabled();
    if (!tls || !live.load()) { 
        return m_sys_calloc(a, b);
    }
    MarkRecursive mr{tls};
    void* ptr = m_sys_calloc(a, b);
    debug::Record::get_instance().update_host(ptr, a*b, true);
    return ptr;
}

void* AllocHook::realloc(void* ptr, size_t size) {
    auto tls = get_tls_if_enabled();
    if (!tls || !live.load()) { 
        return m_sys_realloc(ptr, size);
    }
    MarkRecursive mr{tls};
    void* res = m_sys_realloc(ptr, size);
    debug::Record::get_instance().update_host(res, size, true);
    return res;
}

int AllocHook::posix_memalign(void** ptr, size_t alignment, size_t size) {
    auto tls = get_tls_if_enabled();
    if (!tls || !live.load()) {
        return m_sys_posix_memalign(ptr, alignment, size);
    }
    MarkRecursive mr{tls};
    debug::Record::get_instance().update_host(*ptr, size, true);
    return m_sys_posix_memalign(ptr, alignment, size);
}

AllocHook& AllocHook::inst() {
    static AllocHook hook;
    return hook;
}

extern "C" {
void *malloc(size_t size) {
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