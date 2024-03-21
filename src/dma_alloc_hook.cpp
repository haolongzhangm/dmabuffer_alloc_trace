#include <dlfcn.h>
#include <linux/dma-heap.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <utils/CallStack.h>
#include <cstdio>
#include <cstdlib>

#include "backtrace_helper/backtrace.h"

class IoctlHook {
    int (*m_ioctl)(int __fd, int __request, ...);
    int (*m_close)(int __fd);
    
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
        va_list ap;
        va_start(ap, __request);
        void* arg = va_arg(ap, void*);
        va_end(ap);
        int ret = m_ioctl(__fd, __request, arg);
        if (__request == DMA_HEAP_IOCTL_ALLOC) {
            struct dma_heap_allocation_data* heap_data = (struct dma_heap_allocation_data*)arg;
            debug::Record::get_instance().update_dma(heap_data->fd, heap_data->len, true);
        }

        return ret;
    }

    int close(int __fd) {
        debug::Record::get_instance().update_dma(__fd, 0, false);
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
