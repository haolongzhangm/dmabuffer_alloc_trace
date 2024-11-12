# liballoc_hook.so

use to get malloc and free backtrace, include dmabuffer by hook `ioctl` and `close`

* how to build

  * ./build_android.sh [armeabi-v7a]

* how to use
  * adb shell mkdir `path`, where `path` is the output path for the unwindstack, default: `/data/local/tmp/trace`
  * Config Options at `/dmabuffer_alloc_trace/backtrace/src/Config.cpp`
  * LD_PRELOAD=liballoc_hook.so LD_LIBRARY_PATH=. ls
  * then replace `ls` to you real command

* how to visualize
  * adb pull `path` `dest`
  * python3 /dmabuffer_alloc_trace/tools/merge_trace.py `dest/xxxx.txt` `xxx.html`
  
# libcheckleak.so

* TODO：泄漏相关的逻辑会迁移到 
* 基于 [malloc_debug](https://android.googlesource.com/platform/bionic/+/master/libc/malloc_debug/README.md) API 进行封装
* 添加自动保存 trace，一些辅助脚本进行内存泄漏查找

## known issue

* 因为 dlopen/dlclose thread_local 产生的泄漏，堆栈信息中看不见符号路径

## 关于编译

* 根目录执行 ./build_android.sh

  ````
  会在工程相对路径生成 out/lib/libcheckleak.so
  ````



## 关于如何使用

* 被测试程序是需要带符号表的，目的是让生成的trace 堆栈是明文的，而不是地址。

* 通过 LD_PRELOAD 将 libcheckleak.so 动态加入库依赖

* 通过 LD_LIBRARY_PATH 绕开 android  VNDK namespace 限制

* 具体的命令

  ```
  adb shell mkdir -p /data/local/tmp/checkleak/
  adb push out/lib/libcheckleak.so /data/local/tmp/checkleak/
  adb shell
  export LD_LIBRARY_PATH=/data/local/tmp/checkleak/:/system/lib64:/vendor/lib64:/apex/com.android.vndk.v30/lib64:/apex/com.android.runtime/lib64/
  export LD_PRELOAD=/apex/com.android.vndk.v30/lib64/libunwindstack.so:/apex/com.android.runtime/lib64/libunwindstack.so:/apex/com.android.runtime/lib64/bionic/libc.so:/apex/com.android.runtime/lib64/libc_malloc_debug.so:/data/local/tmp/checkleak/libcheckleak.so
  
  start your test bin:
  LIBC_DEBUG_MALLOC_OPTIONS=backtrace you_test_bin
  
  内存快照文件生成在: /data/local/tmp/checkleak/
  
  如果发生加 malloc debug 后， 发生 you_test_bin crash 的现象， 则logcat -s DEBUG  把log发出来，提一个issue，同时贴上 getprop 的 log
  ```

* 如何触发采样

  ```
  1：首先开一个窗口执行，logcat -s checkmem 来观察此工具的状态
  当执行按照上面的步骤执行 `LIBC_DEBUG_MALLOC_OPTIONS=backtrace you_test_bin` 后， 首先会观察到
  会自动打印(每5s): +++++++ into check
  
  2：通过 setprop triger_libcheck 1 来触发一次采样， 当执行后，可观察到 +++++++ into check 这个log，等待一定的时间，当观察到 +++++++ end check 后表示一次采样完成， 请务必在观察到 +++++++ end check 再进行下次采样
  ```

* 如何改造自己的被测试程序以便此工具能`有效`采样

  另外在采样过程中，也请务必保证程序处于`停止`状态，常见的做法是在被测试的代码适当位置加上 getchar()
  下面解释一下什么叫做`适当`位置

  ```
  比如典型的 pipline:
  create_ctx()
  ....
  use_ctx()
  ....
  free_ctx()
  ```

  一般期望是 free_ctx 后应该资源都释放了（除开常驻的外), 我们改造上述 pipline 来加入 getchar(), 以便让此工具有采样点

  ```
  for(;;)
  {
  	create_ctx()
  	....
  	use_ctx()
  	....
  	free_ctx()
  	getchar()
  }
  ```

  典型的，我们需要让整个 pipline 运行几次分别得到不同运行次数的采样 trace, 当然除了 getchar 让程序`暂停`外，我们也可以通过 `gdb` 等调试工具来达到相同的目的。关键在于加的位置，一定是你认为这个点应该释放了资源，比如典型的上面的 free_ctx，一定不要在其他点去进行采样，比如 `use_ctx` 阶段，这个时候本身属于内存用量高峰，即使没有释放也不能说明`泄漏`对吧。

  

## 如何过滤 trace

  

  通过上面的多次采样，比如可能会的到类型如下命名的文件

```
checkleak_23227_1678176607.csv
checkleak_23227_1678176843.csv

其中 23227 表示被测试程序的 PID，后缀是时间，所以可以通过后缀的大小来区分采样时间的前后关系
```

这个时候我们需要通过 `word_statistics.py ` 文件进行词频统计，其用法如下

```
usage: word_statistics.py [-h] --csv_file  CSV_FILE [--print_count  PRINT_COUNT]

optional arguments:
  -h, --help            show this help message and exit
  --csv_file  CSV_FILE  model file
  --print_count  PRINT_COUNT
                        print the first number few words, default 50
```

比如可执行`python3 word_statistics.py --csv_file checkleak_23227_1678176607.csv --print_count  20`   来观察词语出现频率最高前20名的统计
