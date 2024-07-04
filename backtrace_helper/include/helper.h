#pragma once
#include <map>

#include "basic.h"

namespace debug {
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
    std::atomic_bool is_host_hook{false};
    const char* m_file;
};

}
