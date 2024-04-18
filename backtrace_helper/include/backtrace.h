#pragma once
#include <map>
#include <atomic>
#include <cstdlib>
#include <dlfcn.h>

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

#define RESOLVE(name)                                                  \
    do {                                                               \
        auto addr = dlsym(RTLD_NEXT, #name);                           \
        if (!addr) {                                                   \
            FAIL_EXIT("can not resolve %s: %s", #name, dlerror());     \
        }                                                              \
        m_sys_##name = reinterpret_cast<decltype(m_sys_##name)>(addr); \
    } while (0)

#define TOKENPASTE(x, y) x##y
#define TOKENPASTE2(x, y) TOKENPASTE(x, y)
#define LOCK_GUARD_CTOR(mtx) TOKENPASTE2(__lock_guard_, __LINE__)(mtx)
#define LOCK_GUARD(mtx) std::lock_guard<decltype(mtx)> LOCK_GUARD_CTOR(mtx)

namespace debug {
class NonCopyable {
    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;

public:
    NonCopyable() = default;
};

class Spinlock : public NonCopyable {
    std::atomic_flag m_state = ATOMIC_FLAG_INIT;

public:
    void lock() { while (m_state.test_and_set(std::memory_order_acquire)); }
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
    struct rebind { typedef SysAlloc<X> other; };

    SysAlloc() throw() {}

    template<typename X>
    SysAlloc(const SysAlloc<X>&) throw() {}

    SysAlloc(const SysAlloc&) = delete;
    void operator=(const SysAlloc&) = delete;

    ~SysAlloc() throw() {};

    pointer address(reference x) const { return &x; }
    const_pointer address(const_reference x) const { return &x; }

    pointer allocate(size_type n, const void * hint = 0);
    void deallocate(pointer p, size_type);

    void construct(pointer p, const value_type& x) { new(p) value_type(x); }
    void destroy(pointer p) { p->~value_type(); }

private:
    size_type max_size() const throw() { return size_t(-1) / sizeof(T); }
};

class Record {
    // memory info
    struct Block {
        size_t size;
        int64_t free_time;
        char* backtrace;

        Block() = default;
        Block(size_t len);
        ~Block();
    };

public:
    void host_alloc(void* ptr, size_t size);
    void host_free(void* ptr);
    void dma_alloc(int fd, size_t len);
    void dma_free(int fd);
    static Record& get_instance();
    Record() : m_file("memory_trace.txt") {}
    ~Record();

private:
    bool check_and_create_file();
    char* backtrace(int skip);
    void print_peak_memory();
    void update_free_time(int64_t malloc_time, int64_t free_time);
    void delete_peak_info();

private:
    std::map<void*, std::pair<size_t, int64_t>, std::less<void*>, SysAlloc<std::pair<void* const, std::pair<size_t, int64_t>>>> m_host_info;
    std::map<int, std::pair<size_t, int64_t>, std::less<int>, SysAlloc<std::pair<const int, std::pair<size_t, int64_t>>>> m_dma_info;
    std::map<int64_t, Block, std::less<int64_t>, SysAlloc<std::pair<const int64_t, Block>>> m_peak_info;
    Spinlock m_mutex;
    size_t m_dma_used = 0;
    size_t m_dma_peak = 0;
    size_t m_host_used = 0;
    size_t m_host_peak = 0;
    size_t m_peak = 0;
    int64_t m_peak_time = 0;
    bool m_is_exit = 0;
    const char* m_file;
};

}
