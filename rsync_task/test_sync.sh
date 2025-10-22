#!/bin/bash

# C����rsync-like���߲��Խű�

set -e

# ��ɫ����
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# ��־����
log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# �������
compile_program() {
    log_info "����ͬ������..."
    if ! make; then
        log_error "����ʧ��"
        exit 1
    fi
}

# ������
cleanup() {
    log_info "������Ի���..."
    rm -rf test_source test_dest test_backup sync*.log 2>/dev/null || true
    make clean
}

# ��������
trap cleanup EXIT

# ��������Ŀ¼���ļ�
setup_test_environment() {
    log_info "���ò��Ի���..."
    
    # ��������Ŀ¼
    mkdir -p test_source test_dest test_backup
    mkdir -p test_source/subdir1 test_source/subdir2
    
    # ���������ļ�
    echo "���ǲ����ļ�1������" > test_source/file1.txt
    echo "���ǲ����ļ�2������" > test_source/file2.txt
    echo "�����ļ�����" > test_source/config.conf
    echo "��Ŀ¼�ļ�1" > test_source/subdir1/file3.txt
    echo "��Ŀ¼�ļ�2" > test_source/subdir2/file4.txt
    
    # ���ò�ͬ���ļ�Ȩ��
    chmod 755 test_source/file1.txt
    chmod 600 test_source/file2.txt
    chmod 644 test_source/config.conf
    chmod 750 test_source/subdir1/file3.txt
    
    # �����ļ�ʱ���
    touch -t 202301010000 test_source/file1.txt
    touch -t 202301020000 test_source/file2.txt
    
    log_info "ԴĿ¼�ṹ:"
    find test_source -type f -exec ls -la {} \;
}

# ����1: �����ļ�ͬ��
test_basic_sync() {
    log_info "����1: �����ļ�ͬ��"
    
    if ! ./simple_rsync -v test_source/ test_dest/ > sync1.log 2>&1; then
        log_error "ͬ������ִ��ʧ��"
        exit 1
    fi
    
    # ����ļ��Ƿ�ͬ��
    local missing_files=0
    for file in file1.txt file2.txt config.conf subdir1/file3.txt subdir2/file4.txt; do
        if [[ ! -f "test_dest/$file" ]]; then
            log_error "�ļ�δͬ��: $file"
            missing_files=1
        fi
    done
    
    if [[ $missing_files -eq 0 ]]; then
        log_info "�7�7 �����ļ�ͬ������ͨ��"
    else
        log_error "�7�1 �����ļ�ͬ������ʧ��"
        exit 1
    fi
}

# ����2: �ļ�����ͬ��
test_attribute_sync() {
    log_info "����2: �ļ�����ͬ��"
    
    # ����ļ�Ȩ��
    local src_perm dest_perm
    src_perm=$(stat -c "%a" test_source/file1.txt)
    dest_perm=$(stat -c "%a" test_dest/file1.txt)
    
    if [[ "$src_perm" == "$dest_perm" ]]; then
        log_info "�7�7 �ļ�Ȩ��ͬ������ͨ��"
    else
        log_error "�7�1 �ļ�Ȩ��ͬ������ʧ��: Դ=$src_perm, Ŀ��=$dest_perm"
        exit 1
    fi
    
    # ����ļ�ʱ���
    local src_mtime dest_mtime
    src_mtime=$(stat -c "%Y" test_source/file1.txt)
    dest_mtime=$(stat -c "%Y" test_dest/file1.txt)
    
    if [[ "$src_mtime" == "$dest_mtime" ]]; then
        log_info "�7�7 �ļ�ʱ���ͬ������ͨ��"
    else
        log_error "�7�1 �ļ�ʱ���ͬ������ʧ��"
        exit 1
    fi
}

# ����3: ����ͬ��
test_incremental_sync() {
    log_info "����3: ����ͬ������"
    
    # ��һ��ͬ��
    ./simple_rsync -v test_source/ test_dest/ > sync2.log 2>&1
    local sync_count=$(grep -c "already synchronized" sync2.log || true)
    
    if [[ $sync_count -gt 0 ]]; then
        log_info "�7�7 ����ͬ������ͨ�� (������ $sync_count ���ļ�)"
    else
        log_error "�7�1 ����ͬ������ʧ��"
        exit 1
    fi
}

# ����4: �ļ����¼��
test_file_update() {
    log_info "����4: �ļ����¼��"
    
    # �޸�Դ�ļ�
    echo "��������" >> test_source/file1.txt
    sleep 1
    
    if ! ./simple_rsync -v test_source/ test_dest/ > update.log 2>&1; then
        log_error "����ͬ��ʧ��"
        exit 1
    fi
    
    if grep -q "Synced:" update.log; then
        log_info "�7�7 �ļ����¼�����ͨ��"
    else
        log_error "�7�1 �ļ����¼�����ʧ��"
        exit 1
    fi
}

# ����5: ģ������ģʽ
test_dry_run() {
    log_info "����5: ģ������ģʽ����"
    
    # ���ݵ�ǰ״̬
    cp -r test_dest test_backup
    
    # �������ļ�
    echo "���ļ�" > test_source/new_file.txt
    
    # ʹ��ģ������
    ./simple_rsync -n -v test_source/ test_dest/ > dry_run.log 2>&1
    
    # ����ļ��Ƿ����û�д���
    if [[ ! -f "test_dest/new_file.txt" ]]; then
        log_info "�7�7 ģ������ģʽ����ͨ��"
    else
        log_error "�7�1 ģ������ģʽ����ʧ��"
        exit 1
    fi
    
    # �ָ�״̬
    rm -rf test_dest
    mv test_backup test_dest
}

# ����6: �����ļ�ͬ��
test_single_file_sync() {
    log_info "����6: �����ļ�ͬ��"
    
    rm -f test_dest/single_test.txt 2>/dev/null || true
    echo "�����ļ�����" > test_source/single_test.txt
    
    if ./simple_rsync test_source/single_test.txt test_dest/single_test.txt > /dev/null 2>&1; then
        if [[ -f "test_dest/single_test.txt" ]]; then
            log_info "�7�7 �����ļ�ͬ������ͨ��"
        else
            log_error "�7�1 �����ļ�ͬ������ʧ�� - �ļ�δ����"
            exit 1
        fi
    else
        log_error "�7�1 �����ļ�ͬ������ʧ�� - ����ִ�д���"
        exit 1
    fi
}

# �����Ժ���
main() {
    log_info "��ʼC�����ļ�ͬ���������..."
    
    compile_program
    setup_test_environment
    
    # �������в���
    test_basic_sync
    test_attribute_sync
    test_incremental_sync
    test_file_update
    test_dry_run
    test_single_file_sync
    
    log_info "���в���ͨ����"
    log_info "�����ܽ�:"
    log_info "  - �����ļ�ͬ��: �7�7"
    log_info "  - �ļ�����ͬ��: �7�7" 
    log_info "  - ����ͬ��: �7�7"
    log_info "  - �ļ����¼��: �7�7"
    log_info "  - ģ������ģʽ: �7�7"
    log_info "  - �����ļ�ͬ��: �7�7"
}

# ����������
main "$@"