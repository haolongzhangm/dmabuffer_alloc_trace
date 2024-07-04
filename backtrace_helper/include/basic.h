#pragma once
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>

#include "memory_hook.h"

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

    pointer allocate(size_type n, const void * hint = 0) {
        if (n > max_size()) {
            FAIL_EXIT("Requested memory size exceeds maximum allowed size");
        }
        if (m_sys_malloc == nullptr) {
            RESOLVE(malloc);
        }
        void* p = m_sys_malloc(n * sizeof(T));
        if (!p) {
            FAIL_EXIT("bad alloc");
        }
        return static_cast<pointer>(p);
    }

    void deallocate(pointer p, size_type) {
        if (m_sys_free == nullptr) {
            RESOLVE(free);
        }
        m_sys_free(p);
    }

    void construct(pointer p, const value_type& x) { new(p) value_type(x); }
    void destroy(pointer p) { p->~value_type(); }

private:
    size_type max_size() const throw() { return size_t(-1) / sizeof(T); }
};
