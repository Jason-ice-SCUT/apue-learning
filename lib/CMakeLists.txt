# 使用 file(GLOB) 来明确查找所有 .c 文件
file(GLOB SRC_LIST *.c)

# 打印调试信息
message(STATUS "Found source files: ${SRC_LIST}")

include_directories(../include)

add_library(apue STATIC ${SRC_LIST})

