#include <cstdio>
#include <fstream>
#include <chrono>
#include <string>
#include <vector>

#include "helper.h"
#include "memory_hook.h"
#include "backtrace.h"

namespace debug {
static thread_local bool is_dma_hook = false;

int64_t get_current_time() {
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now()
                .time_since_epoch()).count();
}

bool Record::check_and_create_file() {
    std::ofstream create_file(m_file);
    if (!create_file.is_open()) {
        return false;
    }
    create_file.close();
    return true;
}

void Record::update_free_time(int64_t malloc_time, int64_t free_time) {
    auto it = m_peak_info.find(malloc_time);
    if (it != m_peak_info.end()) {
        if (malloc_time > m_peak_time) {
            m_peak_info.erase(it);
        } else {
            it->second.free_time = free_time;
        }
    }
}

void Record::delete_peak_info() {
    auto it = m_peak_info.begin();
    while (it != m_peak_info.end()) {
        if (it->second.free_time < m_peak_time) {
            it = m_peak_info.erase(it);
        } else {
            ++it;
        }
    }
}

char* Record::backtrace(int skip) {
    is_host_hook = true;
    std::vector<uintptr_t> frames(128);
    size_t num_frames = backtrace_get(frames.data(), frames.size());
    std::string str = backtrace_string(frames.data(), num_frames);
    if (m_sys_malloc == nullptr) {
        RESOLVE(malloc);
    }
    int length = str.size() + 1; // '\0'
    char* trace = (char*)m_sys_malloc(length);
    memcpy(trace, str.c_str(), length);
    is_host_hook = false;
    return trace;
}

void Record::print_peak_memory() {
    if (!check_and_create_file()) {
        FAIL_EXIT("Failed to create %s\n", m_file);
    }
    std::ofstream file(m_file, std::ios::app);
    if (!file.is_open()) {
        FAIL_EXIT("Failed to open %s\n", m_file);
    }
    for (auto& it : m_peak_info) {
        file << "S" << it.second.size << "\t";
        file << "A" << it.first << "\t";
        file << "F" << it.second.free_time << "\n";
        file << it.second.backtrace << "\n";
    }
    file << "P" << m_peak_time;
    file.close();
}

void Record::host_alloc(void* ptr, size_t size) {
    if (is_dma_hook || is_host_hook) {
        return;
    }
    LOCK_GUARD(m_mutex);
    m_host_used += size;
    m_host_peak = std::max(m_host_peak, m_host_used);
    auto current_time = get_current_time();
    m_host_info[ptr] = {size, current_time};
    m_peak_info[current_time] = {size};
    m_peak_info[current_time].backtrace = backtrace(4);
    if (m_peak < (m_host_used + m_dma_used)) {
        m_peak = m_host_used + m_dma_used;
        m_peak_time = current_time;
        delete_peak_info();
    }
}

void Record::host_free(void* ptr) {
    if (is_dma_hook || is_host_hook) {
        return;
    }
    LOCK_GUARD(m_mutex);
    auto it = m_host_info.find(ptr);
    if (it != m_host_info.end()) {
        auto free_time = get_current_time();
        m_host_used -= it->second.first;
        int64_t malloc_time = it->second.second;
        update_free_time(malloc_time, free_time);
        m_host_info.erase(it);
    }
}

void Record::dma_alloc(int fd, size_t len) {
    LOCK_GUARD(m_mutex);
    is_dma_hook = true; // 设置 DMA 钩子状态
    m_dma_used += len;
    m_dma_peak = std::max(m_dma_peak, m_dma_used);
    auto current_time = get_current_time();
    m_dma_info[fd] = {len, current_time};
    m_peak_info[current_time] = {len};
    m_peak_info[current_time].backtrace = backtrace(5);
    if (m_peak < (m_host_used + m_dma_used)) {
        m_peak = m_host_used + m_dma_used;
        m_peak_time = current_time;
        delete_peak_info();
    }
    is_dma_hook = false; // 恢复 DMA 钩子状态
}

void Record::dma_free(int fd) {
    LOCK_GUARD(m_mutex);
    auto it = m_dma_info.find(fd);
    if (it != m_dma_info.end()) {
        is_dma_hook = true; // 设置 DMA 钩子状态
        auto free_time = get_current_time();
        m_dma_used -= it->second.first;
        int64_t malloc_time = it->second.second;
        update_free_time(malloc_time, free_time);
        m_dma_info.erase(it);
        is_dma_hook = false; // 恢复 DMA 钩子状态
    }
}

Record::~Record() {
    is_host_hook = true;
    printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
    printf("Host peak: %.2f MB, DMA peak: %.2f MB, Total peak: %.2f MB\n",
        m_host_peak / 1024.0 / 1024.0, m_dma_peak / 1024.0 / 1024.0, m_peak / 1024.0 / 1024.0);
    print_peak_memory();
    is_host_hook = false;
}

Record& Record::get_instance() {
    static Record instance;
    return instance;
}

Record::Block::Block(size_t len) : size(len), free_time(__LONG_MAX__), backtrace(nullptr) {}

Record::Block::~Block() {
    if (m_sys_free == nullptr) {
        RESOLVE(free);
    }
    m_sys_free(backtrace);
}
}  // namespace debug