#include <cassert>
#include <cstdio>

#include "Config.h"

static constexpr size_t DEFAULT_BACKTRACE_FRAMES = 16;
static constexpr const char DEFAULT_BACKTRACE_DUMP_PREFIX[] = "/data/local/tmp/trace/backtrace_heap";

bool Config::Init() {
    backtrace_sampling_ = true;
    // 采用间隔，单位 us
    backtrace_interval_ = 1000;

    backtrace_dump_on_exit_ = true;
    backtrace_frames_ = DEFAULT_BACKTRACE_FRAMES;
    backtrace_dump_prefix_ = DEFAULT_BACKTRACE_DUMP_PREFIX;

    // 如果开启 BACKTRACE_SPECIFIC_SIZES, 请指定内存申请的最大和最小 size
    // options_ |= BACKTRACE_SPECIFIC_SIZES;
    // backtrace_min_size_bytes_ = 1024;
    // backtrace_max_size_bytes_ = SIZE_MAX;

    options_ |= BACKTRACE;
    options_ |= TRACK_ALLOCS;
    options_ |= RECORD_MEMORY_PEAK;

    return true;
}