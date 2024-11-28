#include <bionic/reserved_signals.h>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Config.h"

static constexpr size_t DEFAULT_BACKTRACE_FRAMES = 128;
static constexpr const char DEFAULT_BACKTRACE_DUMP_PREFIX[] =
        "/data/local/tmp/trace/backtrace_heap";

static bool ParseValue(const char* value, size_t* parsed_value) {
    *parsed_value = 0;
    if (value == nullptr) {
        printf("Not Set DUMP_PEAK_VALUE_MB\n");
        return false;
    }
    // Parse the value into a size_t value.
    errno = 0;
    char* end;
    long long_value = strtol(value, &end, 10);
    if (errno != 0) {
        printf("Error DUMP_PEAK_VALUE_MB:%s:%s\n", value, strerror(errno));
        return false;
    }
    if (end == value) {
        printf("Error DUMP_PEAK_VALUE_MB:%s\n", value);
        return false;
    }
    // 指针值相减
    if (static_cast<size_t>(end - value) != strlen(value)) {
        printf("Error DUMP_PEAK_VALUE_MB:%s\n", value);
        return false;
    }
    if (long_value < 0) {
        printf("Error DUMP_PEAK_VALUE_MB:%s\n", value);
        return false;
    }

    printf("DUMP_PEAK_VALUE_MB:%zuMB\n", long_value);
    *parsed_value = static_cast<size_t>(long_value) * 1024 * 1024;
    return true;
}

bool Config::Init() {
    // 退出时输出 trace
    backtrace_dump_on_exit_ = true;
    backtrace_frames_ = DEFAULT_BACKTRACE_FRAMES;
    backtrace_dump_prefix_ = DEFAULT_BACKTRACE_DUMP_PREFIX;

    // 如果开启 BACKTRACE_SPECIFIC_SIZES, 请指定内存申请的最大和最小 size
    options_ |= BACKTRACE_SPECIFIC_SIZES;
    backtrace_min_size_bytes_ = 0;
    backtrace_max_size_bytes_ = SIZE_MAX;

    // 开启 unwind
    options_ |= BACKTRACE;
    // 记录 trace
    options_ |= TRACK_ALLOCS;

    // 峰值大于 backtrace_dump_peak_val_ 才记录峰值时刻的 trace
    if (ParseValue(getenv("DUMP_PEAK_VALUE_MB"), &backtrace_dump_peak_val_)) {
        // 记录峰值
        options_ |= RECORD_MEMORY_PEAK;
        backtrace_min_size_bytes_ = 1024;
    }

    // 通过信号插入 check point
    options_ |= DUMP_ON_SINGAL;
    backtrace_dump_signal_ = BIONIC_SIGNAL_BACKTRACE;  // BIONIC_SIGNAL_BACKTRACE: 33

    return true;
}