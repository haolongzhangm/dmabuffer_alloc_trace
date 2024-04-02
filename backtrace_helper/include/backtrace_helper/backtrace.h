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

class Record {
public:
    void host_alloc(void* ptr, size_t size);
    void host_free(void* ptr);
    void dma_alloc(int fd, size_t len);
    void dma_free(int fd);
    static Record& get_instance();
    Record() : m_skip(5), m_file("memory_trace.txt") {}
    ~Record() { 
        printf("\n+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
        printf("Host peak: %.2f MB, DMA peak: %.2f MB, Total peak: %.2f MB\n", 
            m_host_peak / 1024.0 / 1024.0, m_dma_peak / 1024.0 / 1024.0, m_peak / 1024.0 / 1024.0);
    }

private:
    bool check_and_create_file();
    size_t backtrace(int32_t skip, size_t size);
    size_t modify(size_t bias);

private:
    std::map<void*, size_t> m_host_bias;
    std::map<int, size_t> m_dma_bias;
    Spinlock m_mutex;
    size_t m_dma_used = 0;
    size_t m_dma_peak = 0;
    size_t m_host_used = 0;
    size_t m_host_peak = 0;
    size_t m_peak = 0;
    const char* m_file;
    const int32_t m_skip;
};

}
