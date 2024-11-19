#pragma once

#include <stdint.h>
#include <cstddef>

constexpr uint64_t ZYGOTE_CHILD = 0x0; // zygote 创建的子进程：true-> 0x1, false -> 0x0
constexpr uint64_t BACKTRACE = 0x2; // 记录堆栈
constexpr uint64_t TRACK_ALLOCS = 0x4; // 记录内存申请动作
constexpr uint64_t BACKTRACE_SPECIFIC_SIZES = 0x8; // 记录特定大小的内存申请
constexpr uint64_t RECORD_MEMORY_PEAK = 0x80; // 记录内存峰值
constexpr uint64_t DUMP_ON_SINGAL = 0x800; // 记录内存峰值

class Config {
public:
    bool Init();

    uint64_t options() const { return options_; }

    int backtrace_dump_signal() const { return backtrace_dump_signal_; }

    size_t backtrace_frames() const { return backtrace_frames_; }
    size_t backtrace_sampling() const { return backtrace_sampling_; }
    long backtrace_interval() const { return backtrace_interval_; }
    bool backtrace_dump_on_exit() const { return backtrace_dump_on_exit_; }
    const char* backtrace_dump_prefix() const { return backtrace_dump_prefix_; }

    size_t backtrace_min_size_bytes() const { return backtrace_min_size_bytes_; }
    size_t backtrace_max_size_bytes() const { return backtrace_max_size_bytes_; }

    size_t backtrace_dump_peak_val() const { return backtrace_dump_peak_val_; }
    size_t backtrace_dump_peak_increment() const { return backtrace_dump_peak_increment_; }

private:
    bool backtrace_sampling_ = false;
    long backtrace_interval_ = 0;

    int backtrace_dump_signal_ = 0;

    size_t backtrace_frames_ = 0;
    bool backtrace_dump_on_exit_ = false;
    const char* backtrace_dump_prefix_;

    size_t backtrace_min_size_bytes_ = 0;
    size_t backtrace_max_size_bytes_ = 0;

    size_t backtrace_dump_peak_val_ = 0;
    size_t backtrace_dump_peak_increment_ = 0;

    uint64_t options_ = 0;
};