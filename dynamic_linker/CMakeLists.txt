cmake_minimum_required(VERSION 2.8)
project(dynamic_linker)

# 生成 compile_commands.json，供 IDE/Clangd 等使用
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 让编译器支持汇编语言
enable_language(ASM)
set(CMAKE_ASM_COMPILER aarch64-none-linux-gnu-gcc)
set(CMAKE_C_COMPILER aarch64-none-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-none-linux-gnu-g++)

# 设置输出目录
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/out/lib)

# 设置头文件以及库的搜寻目录
include_directories(${CMAKE_SOURCE_DIR}/inc)

# 设置源文件列表
set(DYNAMIC_LINKER_SRC 
    src/dynamic_linker.c
    src/dl_runtime.c
    src/dynamic_entry.S
)

# 设置编译选项
add_compile_options(-std=gnu99 -g -O0 -fno-builtin-strlen -fno-builtin-malloc -fno-builtin-printf -fPIC -fno-plt)

# 设置通用链接选项（不包含-static）
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -nostdlib -Wl,-e,_dynamic_entry -Wl,-Bsymbolic -Wl,-z,now")

# 添加动态链接器共享库（作为解释器，由内核调用）
add_library(dynamic_linker SHARED ${DYNAMIC_LINKER_SRC})

