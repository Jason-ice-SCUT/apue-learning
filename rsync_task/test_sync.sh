#!/bin/bash

# C语言rsync-like工具测试脚本

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# 日志函数
log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 编译程序
compile_program() {
    log_info "编译同步程序..."
    if ! make; then
        log_error "编译失败"
        exit 1
    fi
}

# 清理函数
cleanup() {
    log_info "清理测试环境..."
    rm -rf test_source test_dest test_backup sync*.log 2>/dev/null || true
    make clean
}

# 设置陷阱
trap cleanup EXIT

# 创建测试目录和文件
setup_test_environment() {
    log_info "设置测试环境..."
    
    # 创建测试目录
    mkdir -p test_source test_dest test_backup
    mkdir -p test_source/subdir1 test_source/subdir2
    
    # 创建测试文件
    echo "这是测试文件1的内容" > test_source/file1.txt
    echo "这是测试文件2的内容" > test_source/file2.txt
    echo "配置文件内容" > test_source/config.conf
    echo "子目录文件1" > test_source/subdir1/file3.txt
    echo "子目录文件2" > test_source/subdir2/file4.txt
    
    # 设置不同的文件权限
    chmod 755 test_source/file1.txt
    chmod 600 test_source/file2.txt
    chmod 644 test_source/config.conf
    chmod 750 test_source/subdir1/file3.txt
    
    # 设置文件时间戳
    touch -t 202301010000 test_source/file1.txt
    touch -t 202301020000 test_source/file2.txt
    
    log_info "源目录结构:"
    find test_source -type f -exec ls -la {} \;
}

# 测试1: 基本文件同步
test_basic_sync() {
    log_info "测试1: 基本文件同步"
    
    if ! ./simple_rsync -v test_source/ test_dest/ > sync1.log 2>&1; then
        log_error "同步命令执行失败"
        exit 1
    fi
    
    # 检查文件是否同步
    local missing_files=0
    for file in file1.txt file2.txt config.conf subdir1/file3.txt subdir2/file4.txt; do
        if [[ ! -f "test_dest/$file" ]]; then
            log_error "文件未同步: $file"
            missing_files=1
        fi
    done
    
    if [[ $missing_files -eq 0 ]]; then
        log_info "✓ 基本文件同步测试通过"
    else
        log_error "✗ 基本文件同步测试失败"
        exit 1
    fi
}

# 测试2: 文件属性同步
test_attribute_sync() {
    log_info "测试2: 文件属性同步"
    
    # 检查文件权限
    local src_perm dest_perm
    src_perm=$(stat -c "%a" test_source/file1.txt)
    dest_perm=$(stat -c "%a" test_dest/file1.txt)
    
    if [[ "$src_perm" == "$dest_perm" ]]; then
        log_info "✓ 文件权限同步测试通过"
    else
        log_error "✗ 文件权限同步测试失败: 源=$src_perm, 目标=$dest_perm"
        exit 1
    fi
    
    # 检查文件时间戳
    local src_mtime dest_mtime
    src_mtime=$(stat -c "%Y" test_source/file1.txt)
    dest_mtime=$(stat -c "%Y" test_dest/file1.txt)
    
    if [[ "$src_mtime" == "$dest_mtime" ]]; then
        log_info "✓ 文件时间戳同步测试通过"
    else
        log_error "✗ 文件时间戳同步测试失败"
        exit 1
    fi
}

# 测试3: 增量同步
test_incremental_sync() {
    log_info "测试3: 增量同步测试"
    
    # 第一次同步
    ./simple_rsync -v test_source/ test_dest/ > sync2.log 2>&1
    local sync_count=$(grep -c "already synchronized" sync2.log || true)
    
    if [[ $sync_count -gt 0 ]]; then
        log_info "✓ 增量同步测试通过 (跳过了 $sync_count 个文件)"
    else
        log_error "✗ 增量同步测试失败"
        exit 1
    fi
}

# 测试4: 文件更新检测
test_file_update() {
    log_info "测试4: 文件更新检测"
    
    # 修改源文件
    echo "更新内容" >> test_source/file1.txt
    sleep 1
    
    if ! ./simple_rsync -v test_source/ test_dest/ > update.log 2>&1; then
        log_error "更新同步失败"
        exit 1
    fi
    
    if grep -q "Synced:" update.log; then
        log_info "✓ 文件更新检测测试通过"
    else
        log_error "✗ 文件更新检测测试失败"
        exit 1
    fi
}

# 测试5: 模拟运行模式
test_dry_run() {
    log_info "测试5: 模拟运行模式测试"
    
    # 备份当前状态
    cp -r test_dest test_backup
    
    # 创建新文件
    echo "新文件" > test_source/new_file.txt
    
    # 使用模拟运行
    ./simple_rsync -n -v test_source/ test_dest/ > dry_run.log 2>&1
    
    # 检查文件是否真的没有创建
    if [[ ! -f "test_dest/new_file.txt" ]]; then
        log_info "✓ 模拟运行模式测试通过"
    else
        log_error "✗ 模拟运行模式测试失败"
        exit 1
    fi
    
    # 恢复状态
    rm -rf test_dest
    mv test_backup test_dest
}

# 测试6: 单个文件同步
test_single_file_sync() {
    log_info "测试6: 单个文件同步"
    
    rm -f test_dest/single_test.txt 2>/dev/null || true
    echo "单个文件测试" > test_source/single_test.txt
    
    if ./simple_rsync test_source/single_test.txt test_dest/single_test.txt > /dev/null 2>&1; then
        if [[ -f "test_dest/single_test.txt" ]]; then
            log_info "✓ 单个文件同步测试通过"
        else
            log_error "✗ 单个文件同步测试失败 - 文件未创建"
            exit 1
        fi
    else
        log_error "✗ 单个文件同步测试失败 - 命令执行错误"
        exit 1
    fi
}

# 主测试函数
main() {
    log_info "开始C语言文件同步程序测试..."
    
    compile_program
    setup_test_environment
    
    # 运行所有测试
    test_basic_sync
    test_attribute_sync
    test_incremental_sync
    test_file_update
    test_dry_run
    test_single_file_sync
    
    log_info "所有测试通过！"
    log_info "测试总结:"
    log_info "  - 基本文件同步: ✓"
    log_info "  - 文件属性同步: ✓" 
    log_info "  - 增量同步: ✓"
    log_info "  - 文件更新检测: ✓"
    log_info "  - 模拟运行模式: ✓"
    log_info "  - 单个文件同步: ✓"
}

# 运行主函数
main "$@"