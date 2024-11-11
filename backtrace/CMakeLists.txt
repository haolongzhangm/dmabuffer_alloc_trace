aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src SRC_LIST)
add_library(helper STATIC ${SRC_LIST})
target_include_directories(helper PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(helper unwindstack)
