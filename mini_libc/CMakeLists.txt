cmake_minimum_required(VERSION 2.8)
project(mini_libc)

# 生成 compile_commands.json，供 IDE/Clangd 等使用
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 让编译器支持汇编语言
enable_language(ASM)
set(CMAKE_ASM_COMPILER aarch64-none-linux-gnu-gcc)

# 设置静态库与可执行文件的输出目录
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/out/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/out/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/out/bin)

# 设置头文件以及库的搜寻目录
include_directories(${CMAKE_SOURCE_DIR}/inc)
link_directories(${CMAKE_ARCHIVE_OUTPUT_DIRECTORY} ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})

# 设置源文件列表
set(MINI_LIBC_SRC 
    src/mini_libc_entry.S 
    src/string.c 
    src/printf.c 
    src/logger.c
    src/write.c 
    src/read.c
    src/open.c 
    src/close.c 
    src/lseek.c 
    src/brk.c 
    src/mmap.c 
    src/malloc.c
    src/clone.c
    src/fork.c 
    src/socket.c
    src/lock.c
    src/pthread.c
)

set(TEST_MINI_LIBC test/test_mini_lib.c)

# 设置编译选项
add_compile_options(-std=gnu99 -g -O0 -fno-builtin-strlen -fno-builtin-malloc -fno-builtin-printf)

# 设置通用链接选项（不包含-static）
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -nostdlib -e _mini_libc_entry -no-pie")

# 设置需要生成的静态库和静态链接的可执行文件
add_library(mini_libc STATIC ${MINI_LIBC_SRC})
add_executable(test_mini_libc ${TEST_MINI_LIBC})
target_link_libraries(test_mini_libc mini_libc)
set_target_properties(test_mini_libc PROPERTIES
    LINK_FLAGS "-static")

# 添加动态库版本的mini_libc
add_library(mini_libc_shared SHARED ${MINI_LIBC_SRC})
set_target_properties(mini_libc_shared PROPERTIES 
    OUTPUT_NAME "mini_libc"
    POSITION_INDEPENDENT_CODE ON
    LINK_FLAGS "-shared -fPIC")


# 添加动态链接版本的test_mini_libc
add_executable(test_mini_libc_dynamic ${TEST_MINI_LIBC} src/mini_libc_entry.S)
# 设置动态链接器路径，使用绝对路径
get_filename_component(DYNAMIC_LINKER_PATH "${CMAKE_SOURCE_DIR}/../dynamic_linker/out/lib/libdynamic_linker.so" ABSOLUTE)
set_target_properties(test_mini_libc_dynamic PROPERTIES
    LINK_FLAGS "-Wl,-rpath,${CMAKE_RUNTIME_OUTPUT_DIRECTORY} -Wl,--dynamic-linker,${DYNAMIC_LINKER_PATH}")
target_link_libraries(test_mini_libc_dynamic mini_libc_shared)

# 添加PIE版本的动态链接测试程序
# PIE (Position Independent Executable) 支持地址空间布局随机化 (ASLR)
add_executable(test_mini_libc_dynamic_PIE ${TEST_MINI_LIBC} src/mini_libc_entry.S)
set_target_properties(test_mini_libc_dynamic_PIE PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    COMPILE_FLAGS "-fPIE"
    LINK_FLAGS "-pie -Wl,-rpath,${CMAKE_RUNTIME_OUTPUT_DIRECTORY} -Wl,--dynamic-linker,${DYNAMIC_LINKER_PATH}")
target_link_libraries(test_mini_libc_dynamic_PIE mini_libc_shared)
