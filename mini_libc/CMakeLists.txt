cmake_minimum_required(VERSION 2.8)
project(mini_libc)

# 设置汇编
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
add_compile_options(-std=gnu99 -g -fno-builtin-strlen -fno-builtin-malloc -fno-builtin-printf)

# 设置通用链接选项（不包含-static）
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -nostdlib -e _mini_libc_entry -no-pie")

# 设置需要生成的静态库和静态链接的可执行文件
ADD_LIBRARY(mini_libc STATIC ${MINI_LIBC_SRC})
add_executable(test_mini_libc ${TEST_MINI_LIBC})
target_link_libraries(test_mini_libc mini_libc)
set_target_properties(test_mini_libc PROPERTIES
    LINK_FLAGS "-static")

# 添加动态链接器可执行文件
add_executable(dynamic_linker src/dynamic_linker.c)
set_target_properties(dynamic_linker PROPERTIES
    LINK_FLAGS "-static -nostdlib"
)
target_link_libraries(dynamic_linker mini_libc)

# 添加动态库版本的mini_libc
add_library(mini_libc_shared SHARED ${MINI_LIBC_SRC})
set_target_properties(mini_libc_shared PROPERTIES 
    OUTPUT_NAME "mini_libc"
    POSITION_INDEPENDENT_CODE ON
    LINK_FLAGS "-nostdlib -shared -Wl,-export-dynamic")

# 添加动态链接版本的test_mini_libc
add_executable(test_mini_libc_dynamic ${TEST_MINI_LIBC} src/dynamic_entry.S)
set_target_properties(test_mini_libc_dynamic PROPERTIES
    LINK_FLAGS "-nostdlib -Wl,--entry=_dynamic_entry -Wl,-rpath,'$ORIGIN'")
target_link_libraries(test_mini_libc_dynamic mini_libc_shared)

# 添加自定义命令，复制动态库到可执行文件目录，确保动态库可以被找到
add_custom_command(TARGET test_mini_libc_dynamic POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libmini_libc.so
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libmini_libc.so)
