# dmabuffer_alloc_trace



use to get dma alloc and free backtrace

* how to build

  * ./build_android.sh

* how to use

  * LD_PRELOAD=libdma_alloc_hook.cpp.so LD_LIBRARY_PATH=. ls
  * then replace `ls` to you real command

  