get_filename_component(ANDROID_DIR ${CMAKE_CURRENT_SOURCE_DIR} PATH)

add_library(unwindstack STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/AndroidUnwinder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ArmExidx.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Demangle.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DexFiles.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DwarfCfa.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DwarfEhFrameWithHdr.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DwarfMemory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DwarfOp.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DwarfSection.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Elf.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ElfInterface.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ElfInterfaceArm.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Global.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/JitDebug.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/MapInfo.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Maps.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Memory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/MemoryMte.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/MemoryXz.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Regs.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/RegsArm.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/RegsArm64.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/RegsX86.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/RegsX86_64.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/RegsRiscv64.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/RegsMips.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/RegsMips64.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Symbols.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ThreadEntry.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ThreadUnwinder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Unwinder.cpp

        ${CMAKE_CURRENT_SOURCE_DIR}/DexFile.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/LogAndroid.cpp

        ${UNWINDSTACK_ARCH_SRC}
)

target_include_directories(unwindstack BEFORE
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE ${ANDROID_DIR}/platform
)
target_compile_options(unwindstack PRIVATE -Wno-c99-designator -Wno-deprecated-declarations)
# for clang
if (CMAKE_CXX_COMPILER_ID STREQUAL Clang)
    target_compile_options(unwindstack PRIVATE -Wexit-time-destructors -fno-c++-static-destructors)
endif ()

target_compile_definitions(unwindstack PRIVATE DEXFILE_SUPPORT=1)

target_link_libraries(unwindstack PUBLIC lzma base dexfile_stub procinfo)
