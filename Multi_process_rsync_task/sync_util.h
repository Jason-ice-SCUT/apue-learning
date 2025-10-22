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
#include <sys/wait.h>

#define MAX_PATH_LEN 1024
#define MAX_FILES 10000
#define BUFFER_SIZE 4096

// 文件信息结构体
typedef struct {
    char path[MAX_PATH_LEN];
    off_t size;
} file_info_t;

// 文件列表结构体
typedef struct {
    file_info_t *files;
    int count;
    int capacity;
} file_list_t;

// 同步配置
typedef struct {
    char source_dir[MAX_PATH_LEN];
    char target_dir[MAX_PATH_LEN];
    int process_count;
} sync_config_t;

// 函数声明
void init_file_list(file_list_t *list);
void free_file_list(file_list_t *list);
void add_file_to_list(file_list_t *list, const char *path);
void traverse_directory(const char *path, file_list_t *list);
int is_directory(const char *path);
int file_exists(const char *path);
off_t get_file_size(const char *path);
int compare_files(const char *file1, const char *file2);
int create_directory(const char *path);
int sync_file(const char *source_file, const char *target_file);
void worker_process(int worker_id, file_list_t *files, const sync_config_t *config);
int perform_sync(const sync_config_t *config);
char* get_relative_path(const char *base, const char *full_path);

#endif