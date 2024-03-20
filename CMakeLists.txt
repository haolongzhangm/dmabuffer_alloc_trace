cmake_minimum_required(VERSION 3.23)

set(VERSION_SCRIPT ${CMAKE_SOURCE_DIR}/version_script.ld)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O3 -static-libstdc++ -fPIC")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Check if using Clang
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--version-script=${VERSION_SCRIPT}")
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

add_subdirectory(aosp_file)

# 添加 .so
add_library(dma_alloc_hook SHARED ${CMAKE_SOURCE_DIR}/src/dma_alloc_hook.cpp)
# 链接需要的库和头文件
target_link_libraries(dma_alloc_hook PRIVATE backtrace_header)
# 安装到 out/lib 目录
install(TARGETS dma_alloc_hook DESTINATION ${CMAKE_INSTALL_PREFIX}/out/lib)