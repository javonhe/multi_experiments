cmake_minimum_required(VERSION 2.8)
project(mini_libc)

# 设置汇编
enable_language(ASM)
set(CMAKE_ASM_COMPILER aarch64-none-linux-gnu-gcc)

# 设置静态库与可执行文件的输出目录
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/out/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/out/bin)

# 设置头文件以及库的搜寻目录
include_directories(${CMAKE_SOURCE_DIR}/inc)
link_directories(${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})

set(MINI_LIBC_SRC src/mini_libc_entry.S src/write.c src/open.c src/close.c)
set(TEST_MINI_LIBC test/test_mini_lib.c)

# 设置编译选项
add_compile_options(-std=gnu99 -g -fno-builtin)

# 设置链接选项
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -nostdlib -e _mini_libc_entry -no-pie")

# 设置需要生成的目标库和可执行文件
ADD_LIBRARY(mini_libc STATIC ${MINI_LIBC_SRC})
add_executable(test_mini_libc ${TEST_MINI_LIBC})

target_link_libraries(test_mini_libc mini_libc)