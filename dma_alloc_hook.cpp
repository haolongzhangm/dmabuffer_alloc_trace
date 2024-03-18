#include <dlfcn.h>
#include <linux/dma-heap.h>
#include <sys/ioctl.h>
#include <utils/CallStack.h>
#include <chrono>
#include <cstdio>
#include <map>
#include <mutex>

class Timer {
    std::chrono::high_resolution_clock::time_point m_start;

public:
    Timer() { reset(); }

    void reset() { m_start = std::chrono::high_resolution_clock::now(); }

    double get_secs() const {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(now - m_start)
                       .count() *
               1e-9;
    }

    double get_msecs() const { return get_secs() * 1e3; }

    double get_secs_reset() {
        auto ret = get_secs();
        reset();
        return ret;
    }
    double get_msecs_reset() {
        auto ret = get_msecs();
        reset();
        return ret;
    }
};

class IoctlHook {
    int (*m_ioctl)(int __fd, int __request, ...);
    int (*m_close)(int __fd);
    std::mutex m_mutex;
    //! fd with time info
    std::map<int, double> m_alloc_info;
    Timer m_timer;

    void bt() {
        android::CallStack stack;
        stack.update();
        android::String8 str = stack.toString();
        printf("%s\n", str.string());
    }

public:
    IoctlHook() {
#ifdef __OHOS__
        auto handle = dlopen("libc.so", RTLD_LAZY);
        if (!handle) {
            printf("dlopen failed: %s\n", dlerror());
            abort();
        }
        m_ioctl = reinterpret_cast<decltype(m_ioctl)>(dlsym(handle, "ioctl"));
        m_close = reinterpret_cast<decltype(m_close)>(dlsym(handle, "close"));
#else
        m_ioctl = reinterpret_cast<decltype(m_ioctl)>(dlsym(RTLD_NEXT, "ioctl"));
        m_close = reinterpret_cast<decltype(m_close)>(dlsym(RTLD_NEXT, "close"));
#endif
        if (!m_ioctl) {
            printf("dlsym failed: %s\n", dlerror());
            abort();
        }
        if (!m_close) {
            printf("dlsym failed: %s\n", dlerror());
            abort();
        }
    }

    int ioctl(int __fd, unsigned int __request, ...) {
        std::lock_guard<std::mutex> lock(m_mutex);
        va_list ap;
        va_start(ap, __request);
        void* arg = va_arg(ap, void*);
        va_end(ap);
        int ret = m_ioctl(__fd, __request, arg);
        //! show bactrace when __request is DMA_HEAP_IOCTL_ALLOC
        if (__request == DMA_HEAP_IOCTL_ALLOC) {
            struct dma_heap_allocation_data* heap_data =
                    (struct dma_heap_allocation_data*)arg;
            printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++"
                   "++++\n");
            printf("dma alloc happened: fd: %d, request: %u size: %f KB\n", __fd,
                   __request, heap_data->len / 1024.0);
            bt();
            printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++"
                   "++++\n");

            //! save the alloc info with time info
            m_alloc_info[heap_data->fd] = m_timer.get_msecs();
            //   printf("alloc info: fd: %d, time: %ld\n", __fd,
            //   m_alloc_info[heap_data->fd]);
        }

        return ret;
    }

    int close(int __fd) {
        // printf("close fd: %d\n", __fd);
        //! check __fd in m_alloc_info
        if (m_alloc_info.find(__fd) != m_alloc_info.end()) {
            //! TODO: recursive_mutex?
            std::lock_guard<std::mutex> lock(m_mutex);
            printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++"
                   "++++\n");
            auto now = m_timer.get_msecs();
            auto diff = now - m_alloc_info[__fd];
            printf("dma free happened: fd: %d, duration: %f ms\n", __fd, diff);
            bt();
            printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++"
                   "++++\n");
            m_alloc_info.erase(__fd);
        }
        return m_close(__fd);
    }

    static IoctlHook& GetInstance() {
        static IoctlHook instance;
        return instance;
    }
};

extern "C" {
int ioctl(int fd, int request, ...) {
    va_list ap;
    va_start(ap, request);
    void* arg = va_arg(ap, void*);
    va_end(ap);

    return IoctlHook::GetInstance().ioctl(fd, request, arg);
}

int close(int fd) {
    return IoctlHook::GetInstance().close(fd);
}
}
