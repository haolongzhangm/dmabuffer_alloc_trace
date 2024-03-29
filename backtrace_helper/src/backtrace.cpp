#include "backtrace_helper/backtrace.h"

#include <sstream>
#include <fstream>
#include <chrono>

#include <utils/CallStack.h>

namespace debug {
// 对堆栈信息进行hash，标识内存块
unsigned long hash(const char* str) {
    unsigned long hash = 5381;
    auto currentTime = std::chrono::system_clock::now();
    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(currentTime.time_since_epoch()).count();
    std::string time_string = std::to_string(microseconds);
    const char* time_str = time_string.c_str();

    int c;
    while ((c = *str++) || (c = *time_str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

// 函数用于处理括号和"+"后面的内容
void process_call_info(std::string& call_info) {
    size_t left_paren = call_info.find("(");
    size_t right_paren = call_info.rfind(")");
    if (left_paren != std::string::npos && right_paren != std::string::npos && right_paren > left_paren) {
        // 使用 std::string 操作来修改 call_info
        call_info = call_info.substr(left_paren + 1, right_paren - left_paren - 1);
    }
}

BacktraceResult backtrace(unsigned int skip) {
    android::CallStack stack;
    stack.update();
    android::String8 str = stack.toString();
    BacktraceResult result;
    result.m_hash_id = hash(str.c_str());
    std::istringstream iss(str.c_str());
    std::string line;
    while (std::getline(iss, line, '\n')) {
        CallStackInfo info;
        std::istringstream line_stream(line);
        if (skip > 0) {
            skip--;
            continue;
        }
        if (line_stream >> info.m_index >> info.m_pc >> info.m_address >> info.m_directory) {
            std::getline(line_stream, info.m_function);
            process_call_info(info.m_function);
            result.m_info.push_back(info);
        }
    }
    return result;
}

template<typename Container>
void write_mem_info(std::fstream& output_file, const Container& memory_info, const char* id, bool& last_item) {
    for (auto it = memory_info.begin(); it != memory_info.end(); ++it) {
        if (!last_item) {
            output_file << ",\n";  // 不是末尾元素，输出逗号
        }
        last_item = false;

        output_file << "\t\t{\n";
        output_file << "\t\t\t"
                    << "\"" << id << "\":\"" << it->first << "\",\n";
        output_file << "\t\t\t"
                    << "\"Size\":" << it->second.m_size << ",\n";
        output_file << "\t\t\t"
                    << "\"Name\":" << it->second.m_name << ",\n";
        if (it->second.m_end != std::chrono::high_resolution_clock::time_point()) {
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(it->second.m_end - it->second.m_start)
                                    .count();
            output_file << "\t\t\t"
                        << "\"Duration\": " << duration << ",\n";
        } else {
            output_file << "\t\t\t"
                        << "\"Duration\": 0,\n";
        }
        // backtrace info
        {
            bool t_last_item = true;
            output_file << "\t\t\t"
                        << "\"Backtrace\":[\n";
            for (auto j = it->second.m_backtrace.begin(); j != it->second.m_backtrace.end(); ++j) {
                if (!t_last_item) {
                    output_file << ",\n";  // 不是末尾元素，输出逗号
                }
                t_last_item = false;

                output_file << "\t\t\t\t"
                            << "{"
                            << "\"id\":\"" << j->m_index << "\","
                            << "\"address\":\"0x" << j->m_address << "\","
                            << "\"directory\":\"" << j->m_directory << "\","
                            << "\"function\":\"" << j->m_function << "\""
                            << "}";
            }
            output_file << "\n\t\t\t],\n";
        }

        output_file << "\t\t\t"
                    << "\"Color\":\"lightblue\"";
        output_file << "\n\t\t}";
    }
}

bool Record::check_and_create_file(const std::string& filename) {
    std::ifstream file(filename);
    if (file.good()) {
        // 文件已存在
        return true;
    } else {
        // 文件不存在，尝试创建文件
        std::ofstream create_file(filename);
        if (!create_file) {
            std::cerr << "Failed to create " << filename << "\n";
            return false;
        }
        create_file.close();
        return true;
    }
}

void Record::update_host(void* ptr, size_t size, bool flag) {
    LOCK_GUARD(m_mutex);
    if (flag) { // 分配内存
        m_host_used += size;
        m_host_peak = std::max(m_host_peak, m_host_used);
        m_peak = std::max(m_peak, (m_host_used + m_dma_used));
        BacktraceResult result = backtrace(5);
        m_host_info.emplace(ptr, MemBlock(size, result));
        get_json();
    }
    else { // 释放内存
        auto it = m_host_info.find(ptr);
        if (it != m_host_info.end()) {
            m_host_used -= it->second.m_size;
            it->second.set_end();
            get_json();
            m_host_info.erase(it);
        }
    }
}

void Record::update_dma(int fd, size_t len, bool flag) {
    LOCK_GUARD(m_mutex);
    if (flag) { // 分配内存
        m_dma_used += len;
        m_dma_peak = std::max(m_dma_peak, m_dma_used);
        m_peak = std::max(m_peak, (m_host_used + m_dma_used));
        BacktraceResult result = backtrace(5);
        m_dma_info.emplace(fd, MemBlock(len, result));
        get_json();
    }
    else { // 释放内存
        auto it = m_dma_info.find(fd);
        if (it != m_dma_info.end()) {
            m_dma_used -= it->second.m_size;
            it->second.set_end();
            get_json();
            m_dma_info.erase(it);
        }
    }
}

void Record::get_json() {
    const char* file = "trace.json";
    if (!check_and_create_file(file)) {
        FAIL_EXIT("Failed to create %s\n", file);
    }

    std::fstream output_file(file, std::ios::in | std::ios::out);
    if (!output_file) {
        FAIL_EXIT("Failed to open %s\n", file);
    }

    output_file.seekp(0, std::ios::end);
    bool file_is_empty = (output_file.tellp() == 0);
    if (file_is_empty) {
        output_file << "[";
    } else {
        // JSON 数据以数组形式存储
        output_file.seekp(-1, std::ios::cur);  // 定位到文件末尾
        // 如果需要追加逗号，输出逗号和换行
        output_file << ",\n";
    }

    // item 开始
    output_file << "{\n";
    // 打印时间
    auto current_time = std::chrono::system_clock::now();
    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(current_time.time_since_epoch()).count();
    output_file << "\t\"time\":" << microseconds << ",\n";
    // 打印data
    output_file << "\t\"data\": [\n";
    bool last_item = true;  // 用于确定是否是末尾元素
    // 处理 HOST 内存
    write_mem_info(output_file, m_host_info, "Address", last_item);
    // 处理 DMA 内存
    write_mem_info(output_file, m_dma_info, "FD", last_item);
    output_file << "\n\t],\n";
    output_file << "\t\"peak\":" << m_peak << "\n";
    output_file << "}]";
    output_file.close();
}

Record& Record::get_instance() {
    static Record instance;
    return instance;
}

}  // namespace debug