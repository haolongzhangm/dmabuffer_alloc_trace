add_library(procinfo STATIC ${CMAKE_CURRENT_SOURCE_DIR}/process.cpp)
target_include_directories(procinfo PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(procinfo PRIVATE -fPIC)
target_link_libraries(procinfo PUBLIC base)