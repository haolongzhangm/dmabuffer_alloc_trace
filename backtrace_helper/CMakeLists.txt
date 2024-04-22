aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src SRC_LIST)
add_library(backtrace_helper STATIC ${SRC_LIST})
target_include_directories(backtrace_helper PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(backtrace_helper PUBLIC $<TARGET_NAME_IF_EXISTS:backtrace_header>)
