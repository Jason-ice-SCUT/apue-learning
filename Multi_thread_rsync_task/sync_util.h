#ifndef SYNC_UTIL_H
#define SYNC_UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>  // ��� timeval �ṹ��֧��

#define MAX_PATH_LEN 1024
#define MAX_FILES 10000
#define BUFFER_SIZE 8192
#define MAX_THREADS 64

// �ļ���Ϣ�ṹ��
typedef struct {
    char source_path[MAX_PATH_LEN];
    char target_path[MAX_PATH_LEN];
    off_t size;
    int needs_sync;
} file_info_t;

// �ļ��б�ṹ��
typedef struct {
    file_info_t *files;
    int count;
    int capacity;
    int current_index;
    pthread_mutex_t lock;
} file_list_t;

// �� sync_util.h ���޸� thread_args_t �ṹ��
typedef struct {
    int thread_id;
    file_list_t *file_list;
    int *files_processed;
    int *files_synced;
    int *errors;
    pthread_mutex_t *stats_lock;
    int dry_run;  // �������ֶ�
} thread_args_t;

// ͬ������
typedef struct {
    char source_dir[MAX_PATH_LEN];
    char target_dir[MAX_PATH_LEN];
    int thread_count;
    int verbose;
    int dry_run;
} sync_config_t;

// ��������
// �ļ��б����
void init_file_list(file_list_t *list);
void free_file_list(file_list_t *list);
void add_file_to_list(file_list_t *list, const char *source_path, const char *target_path, off_t size);
file_info_t* get_next_file(file_list_t *list);

// Ŀ¼����
void traverse_directory(const char *source_path, const char *target_path, file_list_t *list);

// �ļ�����
int is_directory(const char *path);
int file_exists(const char *path);
off_t get_file_size(const char *path);
time_t get_file_mtime(const char *path);
int compare_files(const char *file1, const char *file2);
int create_directory(const char *path);
int sync_file(const char *source_file, const char *target_file, int dry_run);
char* get_relative_path(const char *base, const char *full_path);

// Ŀ¼ͬ��
void sync_directory_structure(const char *path, const sync_config_t *config);

// �̺߳���
void* worker_thread(void *arg);

// ��ͬ������
int perform_sync(const sync_config_t *config);
void print_stats(const sync_config_t *config, int total_files, int files_synced, int errors);

#endif