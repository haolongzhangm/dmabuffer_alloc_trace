# dmabuffer_alloc_trace



use to get dma alloc and free backtrace

* how to build

  * ./build_android.sh [armeabi-v7a]

* how to use

  * LD_PRELOAD=liballoc_hook.so LD_LIBRARY_PATH=. ls
  * then replace `ls` to you real command

  