#!/bin/bash

# 多线程文件同步程序测试脚本

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 程序名称
PROGRAM="./file_sync"
TEST_DIR="./test_dir"
SOURCE_DIR="$TEST_DIR/source"
TARGET_DIR="$TEST_DIR/target"

# 创建测试目录结构
create_test_structure() {
    echo -e "${YELLOW}创建测试目录结构...${NC}"
    
    # 清理旧测试目录
    rm -rf "$TEST_DIR"
    
    # 创建目录结构
    mkdir -p "$SOURCE_DIR/dir1"
    mkdir -p "$SOURCE_DIR/dir2/subdir"
    mkdir -p "$SOURCE_DIR/empty_dir"
    mkdir -p "$SOURCE_DIR/large_files"
    
    # 创建测试文件
    echo "这是文件1的内容" > "$SOURCE_DIR/file1.txt"
    echo "这是文件2的内容，稍长一些" > "$SOURCE_DIR/file2.txt"
    echo "dir1中的文件" > "$SOURCE_DIR/dir1/file3.txt"
    echo "子目录中的文件" > "$SOURCE_DIR/dir2/subdir/file4.txt"
    
    # 创建多个小文件用于测试多线程
    for i in {1..20}; do
        echo "文件 $i 的内容" > "$SOURCE_DIR/dir1/small_file_$i.txt"
    done
    
    # 创建较大的文件
    dd if=/dev/zero of="$SOURCE_DIR/large_files/large1.bin" bs=1M count=2 2>/dev/null
    dd if=/dev/zero of="$SOURCE_DIR/large_files/large2.bin" bs=1M count=1 2>/dev/null
    
    echo -e "${GREEN}测试目录结构创建完成${NC}"
}

# 编译程序
compile_program() {
    echo -e "${YELLOW}编译程序...${NC}"
    gcc -std=c99 -Wall -Wextra -O2 -pthread -o file_sync main.c sync_util.c
    if [ $? -ne 0 ]; then
        echo -e "${RED}编译失败${NC}"
        exit 1
    fi
    echo -e "${GREEN}编译成功${NC}"
}

# 基本功能测试
test_basic_function() {
    echo -e "${YELLOW}测试基本功能...${NC}"
    
    # 执行同步
    $PROGRAM -t 4 "$SOURCE_DIR" "$TARGET_DIR"
    
    # 检查同步结果
    if [ ! -d "$TARGET_DIR" ]; then
        echo -e "${RED}错误: 目标目录未创建${NC}"
        return 1
    fi
    
    # 检查文件是否同步
    check_files_synced
    
    echo -e "${GREEN}基本功能测试通过${NC}"
}

# 多线程测试
test_multithread() {
    echo -e "${YELLOW}测试多线程功能...${NC}"
    
    # 使用不同线程数测试
    for threads in 1 2 4 8; do
        echo -e "使用 $threads 个线程进行同步..."
        rm -rf "$TARGET_DIR"
        
        # 测量执行时间
        start_time=$(date +%s.%N)
        $PROGRAM -t $threads "$SOURCE_DIR" "$TARGET_DIR"
        end_time=$(date +%s.%N)
        
        execution_time=$(echo "$end_time - $start_time" | bc)
        
        if [ $? -ne 0 ]; then
            echo -e "${RED}错误: $threads 个线程同步失败${NC}"
            return 1
        fi
        
        check_files_synced
        echo -e "${GREEN}$threads 个线程测试通过 (耗时: ${execution_time}s)${NC}"
    done
}

# 增量同步测试
test_incremental_sync() {
    echo -e "${YELLOW}测试增量同步...${NC}"
    
    # 修改源文件
    echo "修改后的内容" >> "$SOURCE_DIR/file1.txt"
    touch "$SOURCE_DIR/dir1/file3.txt"
    
    # 再次同步
    $PROGRAM -t 2 "$SOURCE_DIR" "$TARGET_DIR"
    
    # 检查文件是否更新
    if ! diff "$SOURCE_DIR/file1.txt" "$TARGET_DIR/file1.txt" > /dev/null; then
        echo -e "${RED}错误: 增量同步失败${NC}"
        return 1
    fi
    
    echo -e "${GREEN}增量同步测试通过${NC}"
}

# 空目录测试
test_empty_directory() {
    echo -e "${YELLOW}测试空目录同步...${NC}"
    
    # 检查空目录是否同步
    if [ ! -d "$TARGET_DIR/empty_dir" ]; then
        echo -e "${RED}错误: 空目录未同步${NC}"
        return 1
    fi
    
    echo -e "${GREEN}空目录测试通过${NC}"
}

# 检查文件是否同步
check_files_synced() {
    local files=(
        "file1.txt"
        "file2.txt"
        "dir1/file3.txt"
        "dir2/subdir/file4.txt"
        "large_files/large1.bin"
        "large_files/large2.bin"
    )
    
    # 检查小文件
    for i in {1..20}; do
        files+=("dir1/small_file_$i.txt")
    done
    
    for file in "${files[@]}"; do
        if ! diff "$SOURCE_DIR/$file" "$TARGET_DIR/$file" > /dev/null; then
            echo -e "${RED}错误: 文件 $file 同步失败${NC}"
            return 1
        fi
    done
}

# 错误处理测试
test_error_handling() {
    echo -e "${YELLOW}测试错误处理...${NC}"
    
    # 测试不存在的源目录
    $PROGRAM "/nonexistent/source" "$TARGET_DIR/error_test" >/dev/null 2>&1
    local exit_code=$?
    if [ $exit_code -eq 0 ]; then
        echo -e "${RED}错误: 应该检测到不存在的源目录${NC}"
        return 1
    fi
    
    # 测试文件作为源目录
    $PROGRAM "$SOURCE_DIR/file1.txt" "$TARGET_DIR/error_test2" >/dev/null 2>&1
    exit_code=$?
    if [ $exit_code -eq 0 ]; then
        echo -e "${RED}错误: 应该检测到文件作为源目录${NC}"
        return 1
    fi
    
    echo -e "${GREEN}错误处理测试通过${NC}"
}

# 干运行模式测试
test_dry_run() {
    echo -e "${YELLOW}测试干运行模式...${NC}"
    
    rm -rf "$TARGET_DIR"
    
    # 干运行模式不应该创建目标文件
    $PROGRAM -n -t 2 "$SOURCE_DIR" "$TARGET_DIR" >/dev/null 2>&1
    
    if [ -d "$TARGET_DIR" ]; then
        echo -e "${RED}错误: 干运行模式不应该创建文件${NC}"
        return 1
    fi
    
    echo -e "${GREEN}干运行模式测试通过${NC}"
}

# 性能测试
test_performance() {
    echo -e "${YELLOW}性能测试...${NC}"
    
    local many_files_dir="$TEST_DIR/many_files_source"
    local many_files_target="$TEST_DIR/many_files_target"
    
    mkdir -p "$many_files_dir"
    
    # 创建大量小文件
    echo "创建大量测试文件..."
    for i in {1..100}; do
        echo "文件 $i 的内容" > "$many_files_dir/file$i.txt"
    done
    
    # 测试不同线程数的性能
    echo -e "性能对比测试:"
    for threads in 1 2 4 8; do
        rm -rf "$many_files_target"
        
        start_time=$(date +%s.%N)
        $PROGRAM -t $threads "$many_files_dir" "$many_files_target" >/dev/null 2>&1
        end_time=$(date +%s.%N)
        
        execution_time=$(echo "$end_time - $start_time" | bc)
        echo -e "  线程数 $threads: ${execution_time}s"
    done
    
    echo -e "${GREEN}性能测试完成${NC}"
}

# 清理函数
cleanup() {
    echo -e "${YELLOW}清理测试文件...${NC}"
    rm -rf "$TEST_DIR"
    rm -f file_sync
    echo -e "${GREEN}清理完成${NC}"
}

# 主测试函数
main() {
    echo -e "${YELLOW}开始多线程文件同步程序测试${NC}"
    
    # 编译程序
    compile_program
    
    # 创建测试结构
    create_test_structure
    
    # 运行测试
    local tests=(
        test_basic_function
        test_multithread
        test_incremental_sync
        test_empty_directory
        test_error_handling
        test_dry_run
        test_performance
    )
    
    local failed_tests=0
    
    for test_func in "${tests[@]}"; do
        if $test_func; then
            echo -e "${GREEN}✓ $test_func 通过${NC}"
        else
            echo -e "${RED}✗ $test_func 失败${NC}"
            ((failed_tests++))
        fi
        echo
    done
    
    # 显示测试结果
    if [ $failed_tests -eq 0 ]; then
        echo -e "${GREEN}所有测试通过! 🎉${NC}"
    else
        echo -e "${RED}$failed_tests 个测试失败${NC}"
    fi
    
    # 清理
    cleanup
    
    return $failed_tests
}

# 运行测试
main