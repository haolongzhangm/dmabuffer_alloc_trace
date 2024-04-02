#include <fstream>
#include <chrono>

#include <utils/CallStack.h>

#include "backtrace_helper/backtrace.h"

namespace debug {
bool Record::check_and_create_file() {
    std::ifstream file(m_file);
    if (file.is_open()) {
        file.close();
        return true;
    }

    std::ofstream create_file(m_file);
    if (!create_file.is_open()) {
        return false;
    }
    create_file.close();
    return true;
}

size_t Record::backtrace(int32_t skip, size_t size) {
    android::CallStack stack;
    stack.update(skip);
    android::String8 str = stack.toString();

    if (!check_and_create_file()) {
        FAIL_EXIT("Failed to create %s\n", m_file);
    }

    std::ofstream output_file(m_file, std::ios::app);
    if (!output_file.is_open()) {
        FAIL_EXIT("Failed to open %s\n", m_file);
    }
    output_file.seekp(0, std::ios::end);
    size_t len = output_file.tellp();
    // 获取当前时间
    auto current_time = std::chrono::system_clock::now();
    auto time = std::chrono::duration_cast<std::chrono::microseconds>(current_time.time_since_epoch()).count();
    output_file << "S" << size << "\t";
    output_file << "A" << time << "\t";
    output_file << "F" << "                       " << std::endl;
    output_file << str.string() << std::endl;
    output_file.close();
    return len;
}

size_t Record::modify(size_t bias) {
    std::fstream file(m_file, std::ios::in | std::ios::out);
    if (!file.is_open()) {
        FAIL_EXIT("Failed to open %s\n", m_file);
    }
    file.seekg(bias);
    char ch;
    size_t size = 0;
    while(file.get(ch)) {
        if (ch == 'S') { 
            while (file.get(ch)) {
                if (ch == '\t') {
                    break;
                }
                size = size * 10 + (ch - '0');
            }
        }

        if (ch == 'F') {
            break;
        }
    }
    size_t pos = file.tellg();
    file.seekp(pos);
    auto current_time = std::chrono::system_clock::now();
    auto time = std::chrono::duration_cast<std::chrono::microseconds>(current_time.time_since_epoch()).count();
    file << time;
    file.close();
    return size;
}

void Record::host_alloc(void* ptr, size_t size) {
    if (is_dma_hook) {
        return; // 在 DMA 钩子状态下，直接返回
    }
    LOCK_GUARD(m_mutex);
    m_host_used += size;
    m_host_peak = std::max(m_host_peak, m_host_used);
    m_peak = std::max(m_peak, (m_host_used + m_dma_used));
    size_t bias = backtrace(m_skip, size);
    m_host_bias[ptr] = bias;
}

void Record::host_free(void* ptr) {
    if (is_dma_hook) {
        return; // 在 DMA 钩子状态下，直接返回
    }
    LOCK_GUARD(m_mutex);
    auto it = m_host_bias.find(ptr);
    if (it != m_host_bias.end()) {
        size_t size = modify(it->second);
        m_host_used -= size;
        m_host_bias.erase(it);
    }
}

void Record::dma_alloc(int fd, size_t len) {
    LOCK_GUARD(m_mutex);
    is_dma_hook = true; // 设置 DMA 钩子状态
    m_dma_used += len;
    m_dma_peak = std::max(m_dma_peak, m_dma_used);
    m_peak = std::max(m_peak, (m_host_used + m_dma_used));
    size_t bias = backtrace(m_skip, len);
    m_dma_bias[fd] = bias;
    is_dma_hook = false; // 恢复 DMA 钩子状态
}

void Record::dma_free (int fd) {
    LOCK_GUARD(m_mutex);
    auto it = m_dma_bias.find(fd);
    if (it != m_dma_bias.end()) {
        is_dma_hook = true; // 设置 DMA 钩子状态
        size_t size = modify(it->second);
        m_dma_used -= size;
        m_dma_bias.erase(it);
        is_dma_hook = false; // 恢复 DMA 钩子状态
    }
}

Record& Record::get_instance() {
    static Record instance;
    return instance;
}

}  // namespace debug