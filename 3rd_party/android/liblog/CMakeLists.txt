aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src SRC_LIST)
add_library(custom_log STATIC ${SRC_LIST})
target_include_directories(custom_log PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(custom_log PUBLIC platform)
