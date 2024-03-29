#pragma once
#include <vector>
#include <string>
#include <chrono>
#include <map>
#include <atomic>
#include <cstdlib>

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
namespace debug {
// 定义一个结构体来存储堆栈信息
struct CallStackInfo {
    std::string m_index;
    std::string m_pc;
    std::string m_address;
    std::string m_directory;
    std::string m_function;
};

struct BacktraceResult {
    unsigned long m_hash_id;
    std::vector<CallStackInfo> m_info;
};

BacktraceResult backtrace(unsigned int skip = 1);

struct MemBlock {
    size_t m_size;
    unsigned long m_name;
    std::vector<CallStackInfo> m_backtrace;
    std::chrono::high_resolution_clock::time_point m_start;
    std::chrono::high_resolution_clock::time_point m_end;

    MemBlock() : m_size(0), m_name(0) {
        m_start = std::chrono::high_resolution_clock::time_point();
        m_end = std::chrono::high_resolution_clock::time_point();
    }

    MemBlock(size_t size) : m_size(size), m_name(0) {
        m_start = std::chrono::high_resolution_clock::now();
        m_end = std::chrono::high_resolution_clock::time_point();
    }

    MemBlock(size_t size, BacktraceResult backtrace) : m_size(size), m_name(backtrace.m_hash_id), m_backtrace(backtrace.m_info) {
        m_start = std::chrono::high_resolution_clock::now();
        m_end = std::chrono::high_resolution_clock::time_point();
    }

    void set_end() { m_end = std::chrono::high_resolution_clock::now(); }
};

class Record {
public:
    void update_host(void* ptr, size_t size, bool flag);
    void update_dma(int fd, size_t len, bool flag);
    static Record& get_instance();
    Record() {}
    ~Record() { 
        printf("\n+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
        printf("Host peak: %.2f MB, DMA peak: %.2f MB, Total peak: %.2f MB\n", 
            m_host_peak / 1024.0 / 1024.0, m_dma_peak / 1024.0 / 1024.0, m_peak / 1024.0 / 1024.0);
    }

private:
    bool check_and_create_file(const std::string& filename);
    void get_json();

private:
    std::map<void*, MemBlock> m_host_info;
    std::map<int, MemBlock> m_dma_info;
    Spinlock m_mutex;
    size_t m_dma_used = 0;
    size_t m_dma_peak = 0;
    size_t m_host_used = 0;
    size_t m_host_peak = 0;
    size_t m_peak = 0;
};

}
