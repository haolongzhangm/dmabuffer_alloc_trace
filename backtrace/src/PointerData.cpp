#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <mutex>
#include <cxxabi.h>
#include <inttypes.h>

#include "android-base/stringprintf.h"

#include "Config.h"
#include "DebugData.h"
#include "UnwindBacktrace.h"
#include "PointerData.h"

std::atomic_uint8_t PointerData::backtrace_enabled_ = true;
constexpr size_t kBacktraceEmptyIndex = 1;

static inline bool ShouldBacktraceAllocSize(size_t size_bytes) {
  static bool only_backtrace_specific_sizes = g_debug->config().options() & BACKTRACE_SPECIFIC_SIZES;
  if (!only_backtrace_specific_sizes) {
    return true;
  }

  static size_t min_size_bytes = g_debug->config().backtrace_min_size_bytes();
  static size_t max_size_bytes = g_debug->config().backtrace_max_size_bytes();
  return size_bytes >= min_size_bytes && size_bytes <= max_size_bytes;
}

bool PointerData::Initialize(const Config& config) {
  pointers_.clear();
  key_to_index_.clear();
  frames_.clear();
  // A hash index of kBacktraceEmptyIndex indicates that we tried to get
  // a backtrace, but there was nothing recorded.
  cur_hash_index_ = kBacktraceEmptyIndex + 1;
  current_used = current_host = current_dma = 0;
  peak_tot = peak_host = peak_dma = 0;
  peak_needs_update = false;

  if (config.backtrace_sampling()) {
    // 设置信号处理函数
    signal(SIGALRM, ToggleBacktraceEnabled);

    // 配置定时器
    struct itimerval timer;
    // 采样间隔
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = config.backtrace_interval();
    // 延迟时间
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = config.backtrace_interval();

    // 启动定时器
    setitimer(ITIMER_REAL, &timer, nullptr);
  }

  return true;
}

void PointerData::Add(uintptr_t ptr, size_t pointer_size) {
  size_t hash_index = 0;
  if (backtrace_enabled_) {
    hash_index = AddBacktrace(g_debug->config().backtrace_frames(), pointer_size);
  }

  std::lock_guard<std::mutex> pointer_guard(pointer_mutex_);
  uintptr_t mangled_ptr = ManglePointer(ptr);
  pointers_[mangled_ptr] =
      PointerInfoType{PointerInfoType::GetEncodedSize(pointer_size), hash_index};
  current_used += pointer_size;

  if (peak_tot < current_used) {
    peak_tot = current_used;
    if (peak_tot > g_debug->config().backtrace_dump_peak_val()) {
      peak_needs_update = true;
    }
  }
}

void PointerData::AddHost(const void* ptr, size_t pointer_size) {
  Add(reinterpret_cast<uintptr_t>(ptr), pointer_size);
  std::lock_guard<std::mutex> pointer_guard(pointer_mutex_);
  current_host += pointer_size;
  if (current_host > peak_host) {
    peak_host = current_host;
  }
}

void PointerData::AddDMA(const uint32_t ptr, size_t pointer_size) {
  Add(static_cast<uintptr_t>(ptr), pointer_size);
  std::lock_guard<std::mutex> pointer_guard(pointer_mutex_);
  current_dma += pointer_size;
  if (current_dma > peak_dma) {
    peak_dma = current_dma;
  }
}

void PointerData::Remove(uintptr_t ptr, bool is_dma) {
  size_t hash_index;
  {
    std::lock_guard<std::mutex> pointer_guard(pointer_mutex_);
    uintptr_t mangled_ptr = ManglePointer((ptr));
    auto entry = pointers_.find(mangled_ptr);
    if (entry == pointers_.end()) {
      // No tracked pointer.
      return;
    }

    current_used -= entry->second.size;
    if (peak_needs_update) {
      size_t dump_peak_increment = g_debug->config().backtrace_dump_peak_increment();
      if ((g_debug->config().options() & RECORD_MEMORY_PEAK) && entry->second.size > dump_peak_increment) {
        std::lock_guard<std::mutex> frame_guard(frame_mutex_);
        list.clear();
        GetUniqueList(&list, false);
      }
      peak_needs_update = false;
    }

    size_t* target = is_dma ? &current_dma : &current_host;
    *target -= entry->second.size;
    hash_index = entry->second.hash_index;
    pointers_.erase(mangled_ptr);
  }

  RemoveBacktrace(hash_index);
}

void PointerData::RemoveHost(const void* ptr) {
  Remove(reinterpret_cast<uintptr_t>(ptr), false);
}

void PointerData::RemoveDMA(const uint32_t ptr) {
  Remove(static_cast<uintptr_t>(ptr), true);
}

void PointerData::RemoveBacktrace(size_t hash_index) {
  if (hash_index <= kBacktraceEmptyIndex) {
    return;
  }

  std::lock_guard<std::mutex> frame_guard(frame_mutex_);
  auto frame_entry = frames_.find(hash_index);
  if (frame_entry == frames_.end()) {
    // does not have matching frame data.
    return;
  }
  FrameInfoType* frame_info = &frame_entry->second;
  if (--frame_info->references == 0) {
    FrameKeyType key{.num_frames = frame_info->frames.size(), .frames = frame_info->frames.data()};
    key_to_index_.erase(key);
    frames_.erase(hash_index);
    if (g_debug->config().options() & BACKTRACE) {
      backtraces_info_.erase(hash_index);
    }
  }
}

size_t PointerData::AddBacktrace(size_t num_frames, size_t size_bytes) {
  if (!ShouldBacktraceAllocSize(size_bytes)) {
    return kBacktraceEmptyIndex;
  }

  std::vector<uintptr_t> frames;
  std::vector<unwindstack::FrameData> frames_info;
  if (g_debug->config().options() & BACKTRACE) {
    if (!Unwind(&frames, &frames_info, num_frames)) {
      return kBacktraceEmptyIndex;
    }
  } else {
    return kBacktraceEmptyIndex;
  }
  FrameKeyType key{.num_frames = frames.size(), .frames = frames.data()};
  size_t hash_index;
  std::lock_guard<std::mutex> frame_guard(frame_mutex_);
  auto entry = key_to_index_.find(key);
  if (entry == key_to_index_.end()) {
    hash_index = cur_hash_index_++;
    key.frames = frames.data();
    key_to_index_.emplace(key, hash_index);

    frames_.emplace(hash_index, FrameInfoType{.references = 1, .frames = std::move(frames)});
    if (g_debug->config().options() & BACKTRACE) {
      backtraces_info_.emplace(hash_index, std::move(frames_info));
    }
  } else {
    hash_index = entry->second;
    FrameInfoType* frame_info = &frames_[hash_index];
    frame_info->references++;
  }
  return hash_index;
}


void PointerData::GetList(std::vector<ListInfoType>* list, bool only_with_backtrace) {
  for (const auto& entry : pointers_) {
    FrameInfoType* frame_info = nullptr;
    std::vector<unwindstack::FrameData> backtrace_info;
    uintptr_t pointer = DemanglePointer(entry.first);
    size_t hash_index = entry.second.hash_index;
    if (hash_index > kBacktraceEmptyIndex) {
      auto frame_entry = frames_.find(hash_index);
      if (frame_entry == frames_.end()) {
        // Somehow wound up with a pointer with a valid hash_index, but
        // no frame data. This should not be possible since adding a pointer
        // occurs after the hash_index and frame data have been added.
        // When removing a pointer, the pointer is deleted before the frame
        // data.

        // Pointer --> hash_index does not exist.
      } else {
        frame_info = &frame_entry->second;
      }

      if (g_debug->config().options() & BACKTRACE) {
        auto backtrace_entry = backtraces_info_.find(hash_index);
        if (backtrace_entry == backtraces_info_.end()) {
          // Pointer --> hash_index does not exist.
        } else {
          backtrace_info.resize(backtrace_entry->second.size());
          std::copy(backtrace_entry->second.begin(), backtrace_entry->second.end(), backtrace_info.begin());
        }
      }

      list->emplace_back(ListInfoType{pointer, 1, entry.second.RealSize(),
                                entry.second.ZygoteChildAlloc(), frame_info, std::move(backtrace_info)});
    }

    if (hash_index == 0 && only_with_backtrace) {
      continue;
    }
  }

  // Sort by the size of the allocation.
  std::sort(list->begin(), list->end(), [](const ListInfoType& a, const ListInfoType& b) {
    // Put zygote child allocations first.
    bool a_zygote_child_alloc = a.zygote_child_alloc;
    bool b_zygote_child_alloc = b.zygote_child_alloc;
    if (a_zygote_child_alloc && !b_zygote_child_alloc) {
      return false;
    }
    if (!a_zygote_child_alloc && b_zygote_child_alloc) {
      return true;
    }

    // Sort by size, descending order.
    if (a.size != b.size) return a.size > b.size;

    // Put pointers with no backtrace last.
    FrameInfoType* a_frame = a.frame_info;
    FrameInfoType* b_frame = b.frame_info;
    if (a_frame == nullptr && b_frame != nullptr) {
      return false;
    } else if (a_frame != nullptr && b_frame == nullptr) {
      return true;
    } else if (a_frame == nullptr && b_frame == nullptr) {
      return a.pointer < b.pointer;
    }

    // Put the pointers with longest backtrace first.
    if (a_frame->frames.size() != b_frame->frames.size()) {
      return a_frame->frames.size() > b_frame->frames.size();
    }

    // Last sort by pointer.
    return a.pointer < b.pointer;
  });
}

void PointerData::GetUniqueList(std::vector<ListInfoType>* list, bool only_with_backtrace) {
  GetList(list, only_with_backtrace);

  // Remove duplicates of size/backtraces.
  for (auto iter = list->begin(); iter != list->end();) {
    auto dup_iter = iter + 1;
    bool zygote_child_alloc = iter->zygote_child_alloc;
    size_t size = iter->size;
    FrameInfoType* frame_info = iter->frame_info;
    for (; dup_iter != list->end(); ++dup_iter) {
      if (zygote_child_alloc != dup_iter->zygote_child_alloc || size != dup_iter->size || frame_info != dup_iter->frame_info) {
        break;
      }
      iter->num_allocations++;
    }
    iter = list->erase(iter + 1, dup_iter);
  }
}

void PointerData::DumpLiveToFile(int fd) {
  std::lock_guard<std::mutex> pointer_guard(pointer_mutex_);
  std::lock_guard<std::mutex> frame_guard(frame_mutex_);
  printf("\n+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
  printf("host peak used: %zuMB, dma peak used %zuMB, peak used: %zuMB\n\n", peak_host / 1024 / 1024, peak_dma / 1024 / 1024, peak_tot / 1024 / 1024);
  // 如果不打印峰值，进程结束时仍未释放的内存
  if (!(g_debug->config().options() & RECORD_MEMORY_PEAK)) {
    list.clear();
    GetUniqueList(&list, false);
  }

  for (const auto& info : list) {
    dprintf(fd, "malloc size: %zu\n", info.size);
    for (size_t i = 0; i < info.backtrace_info.size(); ++i) {
      const unwindstack::FrameData* frame = &info.backtrace_info[i];
      auto map_info = frame->map_info;
      
      std::string line = android::base::StringPrintf("#%0zd %" PRIx64 " ", i, frame->rel_pc);
      // so path
      if (map_info == nullptr) {
        line += "<unknown>";
      } else if (map_info->name().empty()) {
        line += android::base::StringPrintf("<anonymous:%" PRIx64 ">", map_info->start());
      } else {
        line += map_info->name();
      }

      if (!frame->function_name.empty()) {
        line += " (";
        char* demangled_name =
            abi::__cxa_demangle(frame->function_name.c_str(), nullptr, nullptr, nullptr);
        if (demangled_name != nullptr) {
          line += demangled_name;
          free(demangled_name);
        } else {
          line += frame->function_name;
        }
        if (frame->function_offset != 0) {
          line += "+" + std::to_string(frame->function_offset);
        }
        line += ")";
      }
      dprintf(fd, "%s\n", line.c_str());
    }
    dprintf(fd, "\n");
  }
}
