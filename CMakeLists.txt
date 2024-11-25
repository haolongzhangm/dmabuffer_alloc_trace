cmake_minimum_required(VERSION 3.23)

set(VERSION_SCRIPT ${CMAKE_SOURCE_DIR}/version_script.ld)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O3 -fPIC")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(ANDROID OR OHOS)
  # now old ANDROID have libc++_shared.so, new ANDROID have libc++.so so we use
  # libc++_static by default, by set ANDROID_STL=c++_static only take effect on old NDK,
  # so we force config by use xxx_LINKER_FLAGS
  message(STATUS "config static-libstdc++ for ANDROID or OHOS")
  set(CMAKE_EXE_LINKER_FLAGS "-fuse-ld=lld")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libstdc++")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++")
  set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -static-libstdc++")
endif()
# Check if using Clang
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--version-script=${VERSION_SCRIPT}")
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

add_subdirectory(aosp_file)
add_subdirectory(unwindstack/cmake)
add_subdirectory(backtrace)

if (ANDROID_ABI MATCHES "arm64-v8a")
    set(VNDK_LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/aosp_file/com.android.vndk.v30/lib64")
    set(APEX_LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/aosp_file/com.android.runtime/lib64")
else ()
    set(VNDK_LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/aosp_file/com.android.vndk.v30/lib")
    set(APEX_LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/aosp_file/com.android.runtime/lib")
endif ()

# 添加 .so
add_library(alloc_hook SHARED ${CMAKE_SOURCE_DIR}/src/alloc_hook.cpp)
# 链接需要的库和头文件
target_link_libraries(alloc_hook PRIVATE helper)
# 安装到 out/lib 目录
install(TARGETS alloc_hook DESTINATION ${CMAKE_INSTALL_PREFIX}/out/lib)

add_library(checkleak SHARED ${CMAKE_SOURCE_DIR}/src/checkleak.cpp)
target_link_directories(checkleak PRIVATE ${VNDK_LIB_PATH} ${APEX_LIB_PATH})
target_link_libraries(checkleak PRIVATE log backtrace backtrace_header c_malloc_debug)
install(TARGETS checkleak DESTINATION ${CMAKE_INSTALL_PREFIX}/out/lib)

# copy database compile_commands.json to PROJECT_SOURCE_DIR
add_custom_target(
  copy_database_compile_commands_mem_trace ALL
  COMMAND
    ${CMAKE_COMMAND} -E copy_if_different ${PROJECT_BINARY_DIR}/compile_commands.json
    ${PROJECT_SOURCE_DIR}/compile_commands.json)
