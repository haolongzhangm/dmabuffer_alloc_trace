# Set the library directory and Android directory
set(LIBDEXFILE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
get_filename_component(ANDROID_DIR ${LIBDEXFILE_DIR} PATH)

# Add the dexfile_stub static library
add_library(dexfile_stub STATIC
    ${LIBDEXFILE_DIR}/dex/compact_offset_table.cc
    ${LIBDEXFILE_DIR}/dex/descriptors_names.cc
    ${LIBDEXFILE_DIR}/dex/dex_file.cc
    ${LIBDEXFILE_DIR}/dex/standard_dex_file.cc
    ${LIBDEXFILE_DIR}/dex/compact_dex_file.cc
    ${LIBDEXFILE_DIR}/dex/dex_file_loader.cc
    ${LIBDEXFILE_DIR}/dex/dex_file_verifier.cc
    ${LIBDEXFILE_DIR}/dex/modifiers.cc
    ${LIBDEXFILE_DIR}/dex/primitive.cc
    ${LIBDEXFILE_DIR}/dex/signature.cc
    ${LIBDEXFILE_DIR}/dex/utf.cc
    ${LIBDEXFILE_DIR}/external/dex_file_ext.cc
    ${LIBDEXFILE_DIR}/external/dex_file_supp.cc
    ${LIBDEXFILE_DIR}/generated/libdexfile.operator_out.cc
    ${ANDROID_DIR}/libartbase/base/unix_file/fd_file.cc
    ${ANDROID_DIR}/libartbase/base/logging.cc
    ${ANDROID_DIR}/libartbase/base/utils.cc
    ${ANDROID_DIR}/libartbase/base/file_magic.cc
    ${ANDROID_DIR}/libartbase/base/mem_map.cc
    ${ANDROID_DIR}/libartbase/base/mem_map_unix.cc
    ${ANDROID_DIR}/libartbase/base/zip_archive.cc
    ${ANDROID_DIR}/libartbase/gen/libartbase.operator_out.cc
    ${ANDROID_DIR}/libartbase/base/runtime_common.cc
    ${ANDROID_DIR}/libartbase/base/native_stack_dump.cc
)

# Set include directories
target_include_directories(dexfile_stub BEFORE
    PUBLIC ${LIBDEXFILE_DIR}
    PUBLIC ${LIBDEXFILE_DIR}/external/include
    PUBLIC ${ANDROID_DIR}/libartpalette/include
    PRIVATE ${ANDROID_DIR}/libartbase
)

# Check if we are targeting Android or Linux
if (ANDROID)
    target_compile_definitions(dexfile_stub PUBLIC -DART_TARGET -DSTATIC_LIB=1 -DART_TARGET_ANDROID)
else ()
    target_compile_definitions(dexfile_stub PUBLIC -DART_TARGET -DSTATIC_LIB=1 -DART_TARGET_LINUX)
endif ()

# Set compile options
target_compile_options(dexfile_stub PRIVATE ${ART_COMPILE_C_FLAGS} -Wno-deprecated-declarations)

# Link libraries
target_link_libraries(dexfile_stub PUBLIC base unwindstack ziparchive)
