add_library(ziparchive STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/zip_archive.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/zip_archive_stream_entry.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/zip_cd_entry_map.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/zip_error.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/zip_writer.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/incfs_support/signal_handling.cpp
)

# libziparchive requires C++20
set_target_properties(ziparchive PROPERTIES CXX_STANDARD 20)

target_include_directories(ziparchive
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/incfs_support/include
        PUBLIC $<TARGET_PROPERTY:base,INTERFACE_INCLUDE_DIRECTORIES>
)

target_compile_definitions(ziparchive PRIVATE ZLIB_CONST=const)

target_compile_options(ziparchive PRIVATE 
        -fPIC
        $<$<COMPILE_LANGUAGE:CXX>:-Wno-missing-field-initializers -Wconversion -Wno-sign-conversion -Wold-style-cast>
        $<$<COMPILE_LANGUAGE:C>:-DZLIB_CONST -Wall -Werror -Wextra -D_FILE_OFFSET_BITS=64 -DINCFS_SUPPORT_DISABLED=1>
)

target_link_libraries(ziparchive PUBLIC base z)
