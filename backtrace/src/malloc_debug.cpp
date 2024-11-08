#include <cstdlib>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/param.h> // powerof2 ---> ((((x) - 1) & (x)) == 0)
#include <unistd.h>

#include <android-base/stringprintf.h>

#include "Config.h"
#include "DebugData.h"
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

bool debug_initialize(void* init_space[]) {
#define RESOLVE(name)                                                   \
    do {                                                                \
        auto addr = dlsym(RTLD_NEXT, #name);                            \
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
        debug_dump_heap(android::base::StringPrintf("%s.txt",
                                                    g_debug->config().backtrace_dump_prefix()).c_str());
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
      g_debug->pointer->Add(result, size);
    }

    return result;
}

static void InternalFree(void* pointer) {
  if (g_debug->TrackPointers()) {
    g_debug->pointer->Remove(pointer);
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
    g_debug->pointer->Remove(pointer);
  }

  void* new_pointer = m_sys_realloc(pointer, bytes);

  if (g_debug->TrackPointers()) {
    g_debug->pointer->Add(new_pointer, bytes);
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
    g_debug->pointer->Add(pointer, size);
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
      g_debug->pointer->Add(pointer, bytes);
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