#pragma once
#include <vector>
#include <map>
#include <atomic>
#include <cstdlib>
#include <dlfcn.h>
#include <thread>

#ifndef LOG
#define LOG(fmt...) \
    do {                             \
        printf(fmt);                 \
    } while (0)
#endif

#ifndef ABORT
#define ABORT() abort()
#endif

#define FAIL_EXIT(fmt...)           \
    do {                            \
        LOG(fmt);                   \
        ABORT();                    \
        __builtin_trap();           \
    } while (0)

#define TOKENPASTE(x, y) x##y
#define TOKENPASTE2(x, y) TOKENPASTE(x, y)
#define LOCK_GUARD_CTOR(mtx) TOKENPASTE2(__lock_guard_, __LINE__)(mtx)
#define LOCK_GUARD(mtx) std::lock_guard<decltype(mtx)> LOCK_GUARD_CTOR(mtx)
namespace debug {
static thread_local bool is_dma_hook = false;
class NonCopyable {
    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;

public:
    NonCopyable() = default;
};

class Spinlock : public NonCopyable {
    std::atomic_flag m_state = ATOMIC_FLAG_INIT;

public:
    void lock() {
        while (m_state.test_and_set(std::memory_order_acquire));
    }
    void unlock() { m_state.clear(std::memory_order_release); }
};

template<typename T>
class SysAlloc : public NonCopyable {
public:
    typedef size_t     size_type;
    typedef ptrdiff_t  difference_type;
    typedef T*         pointer;
    typedef const T*   const_pointer;
    typedef T&         reference;
    typedef const T&   const_reference;
    typedef T          value_type;

    template<typename X>
    struct rebind
    { typedef SysAlloc<X> other; };

    SysAlloc() throw() {
        #define RESOLVE(name)                                                  \
            do {                                                               \
                auto addr = dlsym(RTLD_NEXT, #name);                           \
                if (!addr) {                                                   \
                    FAIL_EXIT("can not resolve %s: %s", #name, dlerror());     \
                }                                                              \
                sys_##name = reinterpret_cast<decltype(sys_##name)>(addr); \
            } while (0)
            RESOLVE(malloc);
            RESOLVE(free);
        #undef RESOLVE
    }

    template<typename X>
    SysAlloc(const SysAlloc<X>&) throw() {}

    SysAlloc(const SysAlloc&) = delete;
    void operator=(const SysAlloc&) = delete;

    ~SysAlloc() throw() {};

    pointer address(reference x) const { return &x; }
    const_pointer address(const_reference x) const { return &x; }

    pointer allocate(size_type n, const void * hint = 0) {
        if (n > max_size()) {
            FAIL_EXIT("Requested memory size exceeds maximum allowed size");
        }

        void* p = sys_malloc(n * sizeof(T));
        if (!p) {
            FAIL_EXIT("bad alloc");
        }
        return static_cast<pointer>(p);
    }

    void deallocate(pointer p, size_type) { sys_free(p); }
    void construct(pointer p, const value_type& x) { new(p) value_type(x); }
    void destroy(pointer p) { p->~value_type(); }

private:
    size_type max_size() const throw() { return size_t(-1) / sizeof(T); }

    void* (*sys_malloc)(size_t);
    void (*sys_free)(void*);
};

template<typename T>
inline bool operator==(const SysAlloc<T>&, const SysAlloc<T>&) {
  return true;
}

template<typename T>
inline bool operator!=(const SysAlloc<T>&, const SysAlloc<T>&) {
  return false;
}

class Record {
public:
    void host_alloc(void* ptr, size_t size);
    void host_free(void* ptr);
    void dma_alloc(int fd, size_t len);
    void dma_free(int fd);
    static Record& get_instance();
    Record() : m_skip(4), m_file("memory_trace.txt") {}
    ~Record() { 
        printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
        printf("Host peak: %.2f MB, DMA peak: %.2f MB, Total peak: %.2f MB\n", 
            m_host_peak / 1024.0 / 1024.0, m_dma_peak / 1024.0 / 1024.0, m_peak / 1024.0 / 1024.0);
        print_peak_time();
    }

private:
    bool check_and_create_file();
    size_t backtrace(int32_t skip, size_t size);
    size_t modify(size_t bias);
    void print_peak_time();

private:
    std::map<void*, size_t, std::less<void*>, SysAlloc<std::pair<void* const, size_t>>> m_host_bias;
    std::map<int, size_t, std::less<int>, SysAlloc<std::pair<const int, size_t>>> m_dma_bias;
    Spinlock m_mutex;
    size_t m_dma_used = 0;
    size_t m_dma_peak = 0;
    size_t m_host_used = 0;
    size_t m_host_peak = 0;
    size_t m_peak = 0;
    int64_t m_peak_time = 0;
    bool m_is_exit = 0;
    const char* m_file;
    const int32_t m_skip;
};

}
