#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <fcntl.h>
#include <linux/dma-heap.h>
#include <sys/param.h> // powerof2 ---> ((((x) - 1) & (x)) == 0)
#include <sys/mman.h>
#include <unistd.h>
#include <signal.h>

#include <android-base/stringprintf.h>

#include "Config.h"
#include "DebugData.h"
#include "PointerData.h"
#include "debug_disable.h"
#include "malloc_debug.h"

#include "memory_hook.h"

class ScopedConcurrentLock {
 public:
  ScopedConcurrentLock() {
    pthread_rwlock_rdlock(&lock_);
  }
  ~ScopedConcurrentLock() {
    pthread_rwlock_unlock(&lock_);
  }

  static void Init() {
    pthread_rwlockattr_t attr;
    // Set the attribute so that when a write lock is pending, read locks are no
    // longer granted.
    pthread_rwlockattr_setkind_np(&attr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
    pthread_rwlock_init(&lock_, &attr);
  }

  static void BlockAllOperations() {
    pthread_rwlock_wrlock(&lock_);
  }

 private:
  static pthread_rwlock_t lock_;
};
pthread_rwlock_t ScopedConcurrentLock::lock_;

DebugData* g_debug;

static void singal_dump_heap(int) {
  if ((g_debug->config().options() & BACKTRACE)) {
    debug_dump_heap(android::base::StringPrintf("%s.time.%ld.txt",
                                        g_debug->config().backtrace_dump_prefix(), 
                                        time(NULL)).c_str());
  }
}

bool debug_initialize(void* init_space[]) {
#ifdef __OHOS__
    void* handle = dlopen("libc.so", RTLD_LAZY);
    if (!handle) {
        abort();
    }
#else
    void* handle = RTLD_NEXT;
#endif

#define RESOLVE(name)                                                   \
    do {                                                                \
        auto addr = dlsym(handle, #name);                               \
        if (!addr) {                                                    \
            abort();                                                    \
        }                                                               \
        m_sys_##name = reinterpret_cast<decltype(m_sys_##name)>(addr);  \
    } while (0)
    RESOLVE(malloc);
    RESOLVE(free);
    RESOLVE(calloc);
    RESOLVE(realloc);
    RESOLVE(memalign);
    RESOLVE(posix_memalign);
    RESOLVE(ioctl);
    RESOLVE(close);
    RESOLVE(mmap);
    RESOLVE(munmap);
#undef RESOLVE

    if (!DebugDisableInitialize()) {
        return false;
    }

    DebugData* debug = new (init_space[0]) DebugData();
    if (!debug->Initialize(init_space[1])) {
        DebugDisableFinalize();
        return false;
    }
    g_debug = debug;
    

    ScopedConcurrentLock::Init();

    if (g_debug->config().options() & DUMP_ON_SINGAL) {
      struct sigaction enable_act = {};
      enable_act.sa_handler = singal_dump_heap;
      enable_act.sa_flags = SA_RESTART | SA_ONSTACK;
      if (sigaction(g_debug->config().backtrace_dump_signal(), &enable_act, nullptr) != 0) {
        return false;
      }
    }

    return true;
}

void debug_finalize() {
    if (g_debug == nullptr) {
        return;
    }

    // Make sure that there are no other threads doing debug allocations
    // before we kill everything.
    ScopedConcurrentLock::BlockAllOperations();

    // Turn off capturing allocations calls.
    DebugDisableSet(true);

    if ((g_debug->config().options() & BACKTRACE) && g_debug->config().backtrace_dump_on_exit()) {
        debug_dump_heap(android::base::StringPrintf("%s.exit.%ld.txt",
                                                    g_debug->config().backtrace_dump_prefix(), 
                                                    time(NULL)).c_str());
    }

    if (g_debug->TrackPointers()) {
      g_debug->pointer->DumpPeakInfo();
    }

    // 对于调试工具或在调试模式下运行的代码, 资源管理可能不是首要关注点.
    // 为了避免在清理过程中出现多线程访问冲突, 决定故意不释放这些资源. 包括 g_debug、pthread 键等.
}

void debug_dump_heap(const char* file_name) {
    ScopedConcurrentLock lock;
    ScopedDisableDebugCalls disable;

    int fd = open(file_name, O_RDWR | O_CREAT | O_NOFOLLOW | O_TRUNC | O_CLOEXEC, 0644);
    if (fd == -1) {
        return;
    }

    g_debug->pointer->DumpLiveToFile(fd);
    close(fd);
}

static void* InternalMalloc(size_t size) {
    void* result = m_sys_malloc(size);
    if (g_debug->TrackPointers()) {
      g_debug->pointer->AddHost(result, size);
    }

    return result;
}

static void InternalFree(void* pointer) {
  if (g_debug->TrackPointers()) {
    g_debug->pointer->RemoveHost(pointer);
  }
  m_sys_free(pointer);
}


void* debug_malloc(size_t size) {
    if (DebugCallsDisabled()) {
        return m_sys_malloc(size);
    }

    ScopedConcurrentLock lock;
    ScopedDisableDebugCalls disable;

    if (size > PointerInfoType::MaxSize()) {
        errno = ENOMEM;
        return nullptr;
    }

    return InternalMalloc(size);
}

void debug_free(void* pointer) {
  if (DebugCallsDisabled() || pointer == nullptr) {
    return m_sys_free(pointer);
  }

  ScopedConcurrentLock lock;
  ScopedDisableDebugCalls disable;

  InternalFree(pointer);
}

void* debug_realloc(void* pointer, size_t bytes) {
  if (DebugCallsDisabled()) {
    return m_sys_realloc(pointer, bytes);
  }

  ScopedConcurrentLock lock;
  ScopedDisableDebugCalls disable;

  if (pointer == nullptr) {
    return InternalMalloc(bytes);
  }

  if (bytes == 0) {
    InternalFree(pointer);
    return nullptr;
  }

  if (bytes > PointerInfoType::MaxSize()) {
    errno = ENOMEM;
    return nullptr;
  }

  if (g_debug->TrackPointers()) {
    g_debug->pointer->RemoveHost(pointer);
  }

  void* new_pointer = m_sys_realloc(pointer, bytes);

  if (g_debug->TrackPointers()) {
    g_debug->pointer->AddHost(new_pointer, bytes);
  }

  return new_pointer;
}

void* debug_calloc(size_t nmemb, size_t bytes) {
  if (DebugCallsDisabled()) {
    return m_sys_calloc(nmemb, bytes);
  }

  ScopedConcurrentLock lock;
  ScopedDisableDebugCalls disable;

  size_t size;
  if (__builtin_mul_overflow(nmemb, bytes, &size)) {
    // Overflow
    errno = ENOMEM;
    return nullptr;
  }

  void* pointer = m_sys_calloc(1, size);
  if (pointer != nullptr && g_debug->TrackPointers()) {
    g_debug->pointer->AddHost(pointer, size);
  }

  return pointer;
}

void* debug_memalign(size_t alignment, size_t bytes) {
  if (DebugCallsDisabled()) {
    return m_sys_memalign(alignment, bytes);
  }

  ScopedConcurrentLock lock;
  ScopedDisableDebugCalls disable;

  if (bytes > PointerInfoType::MaxSize()) {
    errno = ENOMEM;
    return nullptr;
  }

  void* pointer = m_sys_memalign(alignment, bytes);

  if (pointer != nullptr && g_debug->TrackPointers()) {
      g_debug->pointer->AddHost(pointer, bytes);
  }

  return pointer;
}

int debug_posix_memalign(void** memptr, size_t alignment, size_t size) {
  if (DebugCallsDisabled()) {
    return m_sys_posix_memalign(memptr, alignment, size);
  }

  if (alignment < sizeof(void*) || !powerof2(alignment)) {
    return EINVAL;
  }
  int saved_errno = errno;
  *memptr = debug_memalign(alignment, size);
  errno = saved_errno;
  return (*memptr != nullptr) ? 0 : ENOMEM;
}

int debug_ioctl(int fd, unsigned int request, void* arg) {
  if (DebugCallsDisabled() || request != DMA_HEAP_IOCTL_ALLOC) {
    return m_sys_ioctl(fd, request, arg);
  }

  ScopedConcurrentLock lock;
  ScopedDisableDebugCalls disable;

  struct dma_heap_allocation_data* heap_data = (struct dma_heap_allocation_data*)arg;
  if (heap_data->len > PointerInfoType::MaxSize()) {
    errno = ENOMEM;
    return -1;
  }

  int ret = m_sys_ioctl(fd, request, arg);
  if (g_debug->TrackPointers()) {
    g_debug->pointer->AddDMA(heap_data->fd, heap_data->len);
  }

  return ret;
}

int debug_close(int fd) {
  if (DebugCallsDisabled() || fd <= 0) {
    return m_sys_close(fd);
  }

  ScopedConcurrentLock lock;
  ScopedDisableDebugCalls disable;

  if (g_debug->TrackPointers()) {
    g_debug->pointer->RemoveDMA(fd);
  }

  return m_sys_close(fd);
}

void* debug_mmap(void* addr, size_t size, int prot, int flags, int fd, off_t offset) {
  if (DebugCallsDisabled() || addr != nullptr || fd >= 0) {
    return m_sys_mmap(addr, size, prot, flags, fd, offset);
  }

  ScopedConcurrentLock lock;
  ScopedDisableDebugCalls disable;

  if (size > PointerInfoType::MaxSize()) {
    errno = ENOMEM;
    return nullptr;
  }

  void* result = m_sys_mmap(addr, size, prot, flags, fd, offset);
  if (g_debug->TrackPointers()) {
    g_debug->pointer->AddHost(result, size, MMAP);
  }

  return result;
}

int debug_munmap(void* addr, size_t size) {
  if (DebugCallsDisabled()) {
    return m_sys_munmap(addr, size);
  }

  ScopedConcurrentLock lock;
  ScopedDisableDebugCalls disable;

  if (g_debug->TrackPointers()) {
    g_debug->pointer->RemoveHost(addr);
  }

  return m_sys_munmap(addr, size);
}