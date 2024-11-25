# liballoc_hook.so

use to get malloc and free backtrace, include dmabuffer by hook `ioctl` and `close`

* how to build

  * ./build_android.sh [armeabi-v7a]

* how to use
  * adb shell mkdir `path`, where `path` is the output path for the unwindstack, default: `/data/local/tmp/trace`
  * Config Options at `/dmabuffer_alloc_trace/backtrace/src/Config.cpp`
  * LD_PRELOAD=liballoc_hook.so LD_LIBRARY_PATH=. ls
  * then replace `ls` to you real command
  * 如果要抓 trace 请先创建 trace 的输出目录

* how to visualize
  * 只是一个 demo，可视化功能固定显示 30ms，并不是内存生命周期就是 30ms
  * adb pull `path` `dest`
  * python3 tools/merge_trace.py `dest/xxxx.txt` `xxx.html`

* checkpoint
  * 支持在程序指定位置插入检查点，输出当前时刻的堆栈信息
  * 第一种方式：使用信号的方式触发堆栈输出，默认信号值为 33，可以在上述配置文件 Config.cpp 中修改，trace 文件以当前时间命名
    ``` C++
      #include <unistd.h>
      #include <signal.h>
      #include <errno.h>
      #include <stdlib.h>
      #include <string.h>

      if (kill(getpid(), 33) == -1) {
        fprintf(stderr, "Error in file %s at line %d: %s\n", __FILE__, __LINE__, strerror(errno));
        exit(1);
      }
    ```
  * 第二种方式：在代码中插入调用，支持用户自定义文件名，前提需要用户的可执行程序依赖该项目编译生成的 so, 例如在 cmake 文件中做出下面的修改
    ```
      add_library(hook SHARED IMPORTED)
      set(HOOK_IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/liballoc_hook.so")
      set_target_properties(hook PROPERTIES IMPORTED_LOCATION "${HOOK_IMPORTED_LOCATION}")

      add_executable(xxxx ./code/xxxxx.cpp)
      target_link_libraries(xxxx hook)
    ```
    用户代码需做以下修改
    ``` C++
      extern "C" void checkpoint(const char* file_name);
      int main() {
          size_t size = 200 * 1024 * 1024;
          void* addr_mmap = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
          checkpoint("/data/local/tmp/trace/check_point.1.txt");
          void* addr_malloc = malloc(size * 2);
          
          return 0;
      }
    ```
    如果以 SHARED 方式生成了 liballoc_hook.so 共享库，则需要将 liballoc_hook.so 文件放在可执行程序可读取的目录中，例如
    ``` bash
      adb push /path/liballoc_hook.so ${test_dir}

      adb exec-out "cd ${test_dir}; \
                export LD_LIBRARY_PATH=.; \
                ./xxxx;"
    ```

  ```

* 如何改造自己的被测试程序以便此工具能`有效`采样

  另外在采样过程中，也请务必保证程序处于`停止`状态，常见的做法是在被测试的代码适当位置加上 checkpoint() 或者 kill(getpid(), 33) 以便触发采样，
  下面解释一下什么叫做`适当`位置

  ```
  比如典型的 pipline:
  create_ctx()
  ....
  use_ctx()
  ....
  free_ctx()
  ```

  一般期望是 free_ctx 后应该资源都释放了（除开常驻的外), 我们改造上述 pipline 来加入 checkpoint()/kill, 以便让此工具有采样点

  ```
  for(;;)
  {
  	create_ctx()
  	....
  	use_ctx()
  	....
  	free_ctx()
  	checkpoint() or kill(getpid(), 33)
  }
  ```

  典型的，我们需要让整个 pipline 运行几次分别得到不同运行次数的采样 trace, 当然除了 checkpoint/kill 让程序`暂停`外，我们也可以通过 `gdb` 等调试工具来达到相同的目的。关键在于加的位置，一定是你认为这个点应该释放了资源，比如典型的上面的 free_ctx，一定不要在其他点去进行采样，比如 `use_ctx` 阶段，这个时候本身属于内存用量高峰，即使没有释放也不能说明`泄漏`对吧。
