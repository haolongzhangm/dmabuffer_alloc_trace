cmake_minimum_required(VERSION "3.23.0")

# 设置库的导入路径
if (ANDROID_ABI MATCHES "arm64-v8a")
    set(UTILSCALLSTACK_IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/com.android.vndk.v30/lib64/libutilscallstack.so")
    set(UTILS_IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/com.android.vndk.v30/lib64/libutils.so")
else ()
    set(UTILSCALLSTACK_IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/com.android.vndk.v30/lib/libutilscallstack.so")
    set(UTILS_IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/com.android.vndk.v30/lib/libutils.so")
endif ()

# 链接所需的库
add_library(utilscallstack SHARED IMPORTED)
set_target_properties(utilscallstack PROPERTIES IMPORTED_LOCATION "${UTILSCALLSTACK_IMPORTED_LOCATION}")
add_library(utils SHARED IMPORTED)
set_target_properties(utils PROPERTIES IMPORTED_LOCATION "${UTILS_IMPORTED_LOCATION}")

add_library(backtrace_header INTERFACE)
target_include_directories(backtrace_header INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/aosp_header/include)
target_link_libraries(backtrace_header INTERFACE utilscallstack utils)
