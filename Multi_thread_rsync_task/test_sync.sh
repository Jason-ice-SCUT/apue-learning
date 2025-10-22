#!/bin/bash

# ���߳��ļ�ͬ��������Խű�

set -e

# ��ɫ����
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# ��������
PROGRAM="./file_sync"
TEST_DIR="./test_dir"
SOURCE_DIR="$TEST_DIR/source"
TARGET_DIR="$TEST_DIR/target"

# ��������Ŀ¼�ṹ
create_test_structure() {
    echo -e "${YELLOW}��������Ŀ¼�ṹ...${NC}"
    
    # ����ɲ���Ŀ¼
    rm -rf "$TEST_DIR"
    
    # ����Ŀ¼�ṹ
    mkdir -p "$SOURCE_DIR/dir1"
    mkdir -p "$SOURCE_DIR/dir2/subdir"
    mkdir -p "$SOURCE_DIR/empty_dir"
    mkdir -p "$SOURCE_DIR/large_files"
    
    # ���������ļ�
    echo "�����ļ�1������" > "$SOURCE_DIR/file1.txt"
    echo "�����ļ�2�����ݣ��Գ�һЩ" > "$SOURCE_DIR/file2.txt"
    echo "dir1�е��ļ�" > "$SOURCE_DIR/dir1/file3.txt"
    echo "��Ŀ¼�е��ļ�" > "$SOURCE_DIR/dir2/subdir/file4.txt"
    
    # �������С�ļ����ڲ��Զ��߳�
    for i in {1..20}; do
        echo "�ļ� $i ������" > "$SOURCE_DIR/dir1/small_file_$i.txt"
    done
    
    # �����ϴ���ļ�
    dd if=/dev/zero of="$SOURCE_DIR/large_files/large1.bin" bs=1M count=2 2>/dev/null
    dd if=/dev/zero of="$SOURCE_DIR/large_files/large2.bin" bs=1M count=1 2>/dev/null
    
    echo -e "${GREEN}����Ŀ¼�ṹ�������${NC}"
}

# �������
compile_program() {
    echo -e "${YELLOW}�������...${NC}"
    gcc -std=c99 -Wall -Wextra -O2 -pthread -o file_sync main.c sync_util.c
    if [ $? -ne 0 ]; then
        echo -e "${RED}����ʧ��${NC}"
        exit 1
    fi
    echo -e "${GREEN}����ɹ�${NC}"
}

# �������ܲ���
test_basic_function() {
    echo -e "${YELLOW}���Ի�������...${NC}"
    
    # ִ��ͬ��
    $PROGRAM -t 4 "$SOURCE_DIR" "$TARGET_DIR"
    
    # ���ͬ�����
    if [ ! -d "$TARGET_DIR" ]; then
        echo -e "${RED}����: Ŀ��Ŀ¼δ����${NC}"
        return 1
    fi
    
    # ����ļ��Ƿ�ͬ��
    check_files_synced
    
    echo -e "${GREEN}�������ܲ���ͨ��${NC}"
}

# ���̲߳���
test_multithread() {
    echo -e "${YELLOW}���Զ��̹߳���...${NC}"
    
    # ʹ�ò�ͬ�߳�������
    for threads in 1 2 4 8; do
        echo -e "ʹ�� $threads ���߳̽���ͬ��..."
        rm -rf "$TARGET_DIR"
        
        # ����ִ��ʱ��
        start_time=$(date +%s.%N)
        $PROGRAM -t $threads "$SOURCE_DIR" "$TARGET_DIR"
        end_time=$(date +%s.%N)
        
        execution_time=$(echo "$end_time - $start_time" | bc)
        
        if [ $? -ne 0 ]; then
            echo -e "${RED}����: $threads ���߳�ͬ��ʧ��${NC}"
            return 1
        fi
        
        check_files_synced
        echo -e "${GREEN}$threads ���̲߳���ͨ�� (��ʱ: ${execution_time}s)${NC}"
    done
}

# ����ͬ������
test_incremental_sync() {
    echo -e "${YELLOW}��������ͬ��...${NC}"
    
    # �޸�Դ�ļ�
    echo "�޸ĺ������" >> "$SOURCE_DIR/file1.txt"
    touch "$SOURCE_DIR/dir1/file3.txt"
    
    # �ٴ�ͬ��
    $PROGRAM -t 2 "$SOURCE_DIR" "$TARGET_DIR"
    
    # ����ļ��Ƿ����
    if ! diff "$SOURCE_DIR/file1.txt" "$TARGET_DIR/file1.txt" > /dev/null; then
        echo -e "${RED}����: ����ͬ��ʧ��${NC}"
        return 1
    fi
    
    echo -e "${GREEN}����ͬ������ͨ��${NC}"
}

# ��Ŀ¼����
test_empty_directory() {
    echo -e "${YELLOW}���Կ�Ŀ¼ͬ��...${NC}"
    
    # ����Ŀ¼�Ƿ�ͬ��
    if [ ! -d "$TARGET_DIR/empty_dir" ]; then
        echo -e "${RED}����: ��Ŀ¼δͬ��${NC}"
        return 1
    fi
    
    echo -e "${GREEN}��Ŀ¼����ͨ��${NC}"
}

# ����ļ��Ƿ�ͬ��
check_files_synced() {
    local files=(
        "file1.txt"
        "file2.txt"
        "dir1/file3.txt"
        "dir2/subdir/file4.txt"
        "large_files/large1.bin"
        "large_files/large2.bin"
    )
    
    # ���С�ļ�
    for i in {1..20}; do
        files+=("dir1/small_file_$i.txt")
    done
    
    for file in "${files[@]}"; do
        if ! diff "$SOURCE_DIR/$file" "$TARGET_DIR/$file" > /dev/null; then
            echo -e "${RED}����: �ļ� $file ͬ��ʧ��${NC}"
            return 1
        fi
    done
}

# ���������
test_error_handling() {
    echo -e "${YELLOW}���Դ�����...${NC}"
    
    # ���Բ����ڵ�ԴĿ¼
    $PROGRAM "/nonexistent/source" "$TARGET_DIR/error_test" >/dev/null 2>&1
    local exit_code=$?
    if [ $exit_code -eq 0 ]; then
        echo -e "${RED}����: Ӧ�ü�⵽�����ڵ�ԴĿ¼${NC}"
        return 1
    fi
    
    # �����ļ���ΪԴĿ¼
    $PROGRAM "$SOURCE_DIR/file1.txt" "$TARGET_DIR/error_test2" >/dev/null 2>&1
    exit_code=$?
    if [ $exit_code -eq 0 ]; then
        echo -e "${RED}����: Ӧ�ü�⵽�ļ���ΪԴĿ¼${NC}"
        return 1
    fi
    
    echo -e "${GREEN}���������ͨ��${NC}"
}

# ������ģʽ����
test_dry_run() {
    echo -e "${YELLOW}���Ը�����ģʽ...${NC}"
    
    rm -rf "$TARGET_DIR"
    
    # ������ģʽ��Ӧ�ô���Ŀ���ļ�
    $PROGRAM -n -t 2 "$SOURCE_DIR" "$TARGET_DIR" >/dev/null 2>&1
    
    if [ -d "$TARGET_DIR" ]; then
        echo -e "${RED}����: ������ģʽ��Ӧ�ô����ļ�${NC}"
        return 1
    fi
    
    echo -e "${GREEN}������ģʽ����ͨ��${NC}"
}

# ���ܲ���
test_performance() {
    echo -e "${YELLOW}���ܲ���...${NC}"
    
    local many_files_dir="$TEST_DIR/many_files_source"
    local many_files_target="$TEST_DIR/many_files_target"
    
    mkdir -p "$many_files_dir"
    
    # ��������С�ļ�
    echo "�������������ļ�..."
    for i in {1..100}; do
        echo "�ļ� $i ������" > "$many_files_dir/file$i.txt"
    done
    
    # ���Բ�ͬ�߳���������
    echo -e "���ܶԱȲ���:"
    for threads in 1 2 4 8; do
        rm -rf "$many_files_target"
        
        start_time=$(date +%s.%N)
        $PROGRAM -t $threads "$many_files_dir" "$many_files_target" >/dev/null 2>&1
        end_time=$(date +%s.%N)
        
        execution_time=$(echo "$end_time - $start_time" | bc)
        echo -e "  �߳��� $threads: ${execution_time}s"
    done
    
    echo -e "${GREEN}���ܲ������${NC}"
}

# ������
cleanup() {
    echo -e "${YELLOW}��������ļ�...${NC}"
    rm -rf "$TEST_DIR"
    rm -f file_sync
    echo -e "${GREEN}�������${NC}"
}

# �����Ժ���
main() {
    echo -e "${YELLOW}��ʼ���߳��ļ�ͬ���������${NC}"
    
    # �������
    compile_program
    
    # �������Խṹ
    create_test_structure
    
    # ���в���
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
            echo -e "${GREEN}�7�7 $test_func ͨ��${NC}"
        else
            echo -e "${RED}�7�1 $test_func ʧ��${NC}"
            ((failed_tests++))
        fi
        echo
    done
    
    # ��ʾ���Խ��
    if [ $failed_tests -eq 0 ]; then
        echo -e "${GREEN}���в���ͨ��! �9�5${NC}"
    else
        echo -e "${RED}$failed_tests ������ʧ��${NC}"
    fi
    
    # ����
    cleanup
    
    return $failed_tests
}

# ���в���
main