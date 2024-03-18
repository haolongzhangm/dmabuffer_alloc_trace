#!/usr/bin/env bash
set -ex


SRC_DIR=$(readlink -f "`dirname $0`")
cd ${SRC_DIR}

# check NDK_ROOT env variable
if [ -z "$NDK_ROOT" ]; then
    echo "Please set NDK_ROOT environment variable"
    exit 1
else
    echo "use $NDK_ROOT to build for android"
fi

echo "check NDK is invalid or not..."
if [ ! -f "$NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android21-clang++" ]; then
    echo "NDK_ROOT is invalid"
    exit 1
else
    echo "put $NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin/ to PATH"
    export PATH=$NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin/:$PATH
fi

rm -rf out
mkdir -p out/lib64
mkdir -p out/lib32

# build for android aarch64
echo "build for android aarch64"
bear -- aarch64-linux-android21-clang++ dma_alloc_hook.cpp -std=c++17 -g -O3 -static-libstdc++ -Iaosp_file/aosp_header/include/ -Iopencl-stub/include -lutilscallstack -lutils -Laosp_file/lib64/ -fPIC -shared -o out/lib64/libdma_alloc_hook.cpp.so -Wl,--version-script=version_script.ld

echo "build for android armv7a"
armv7a-linux-androideabi21-clang++ dma_alloc_hook.cpp -std=c++17 -g -O3 -static-libstdc++ -Iaosp_file/aosp_header/include/ -Iopencl-stub/include -lutilscallstack -lutils -Laosp_file/lib/ -fPIC -shared -o out/lib32/libdma_alloc_hook.cpp.so -Wl,--version-script=version_script.ld
