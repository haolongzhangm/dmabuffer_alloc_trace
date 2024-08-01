add_library(base STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/chrono_utils.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/file.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/hex.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/mapped_file.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/parsebool.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/parsenetaddress.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/logging.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/posix_strerror_r.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/process.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/stringprintf.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/strings.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/threads.cpp
)

target_include_directories(base PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(base PUBLIC custom_log)
