#include "sync_util.h"
#include <string.h>
#include <stdlib.h>

// 初始化文件列表
void init_file_list(file_list_t *list) {
    list->capacity = 100;
    list->count = 0;
    list->current_index = 0;
    list->files = malloc(list->capacity * sizeof(file_info_t));
    if (!list->files) {
        perror("malloc failed");
        exit(1);
    }
    pthread_mutex_init(&list->lock, NULL);
}

// 释放文件列表
void free_file_list(file_list_t *list) {
    if (list->files) {
        free(list->files);
        list->files = NULL;
    }
    list->count = 0;
    list->capacity = 0;
    list->current_index = 0;
    pthread_mutex_destroy(&list->lock);
}

// 添加文件到列表
void add_file_to_list(file_list_t *list, const char *source_path, const char *target_path, off_t size) {
    pthread_mutex_lock(&list->lock);
    
    if (list->count >= list->capacity) {
        list->capacity *= 2;
        list->files = realloc(list->files, list->capacity * sizeof(file_info_t));
        if (!list->files) {
            perror("realloc failed");
            pthread_mutex_unlock(&list->lock);
            exit(1);
        }
    }
    
    file_info_t *file = &list->files[list->count];
    strncpy(file->source_path, source_path, MAX_PATH_LEN - 1);
    file->source_path[MAX_PATH_LEN - 1] = '\0';
    
    strncpy(file->target_path, target_path, MAX_PATH_LEN - 1);
    file->target_path[MAX_PATH_LEN - 1] = '\0';
    
    file->size = size;
    file->needs_sync = 1;
    
    list->count++;
    pthread_mutex_unlock(&list->lock);
}

// 获取下一个需要处理的文件
file_info_t* get_next_file(file_list_t *list) {
    pthread_mutex_lock(&list->lock);
    
    if (list->current_index >= list->count) {
        pthread_mutex_unlock(&list->lock);
        return NULL;
    }
    
    file_info_t *file = &list->files[list->current_index];
    list->current_index++;
    
    pthread_mutex_unlock(&list->lock);
    return file;
}

// 检查是否为目录
int is_directory(const char *path) {
    struct stat stat_buf;
    if (stat(path, &stat_buf) != 0) {
        return 0;
    }
    return S_ISDIR(stat_buf.st_mode);
}

// 检查文件是否存在
int file_exists(const char *path) {
    return access(path, F_OK) == 0;
}

// 获取文件大小
off_t get_file_size(const char *path) {
    struct stat stat_buf;
    if (stat(path, &stat_buf) != 0) {
        return -1;
    }
    return stat_buf.st_size;
}

// 获取文件修改时间
time_t get_file_mtime(const char *path) {
    struct stat stat_buf;
    if (stat(path, &stat_buf) != 0) {
        return 0;
    }
    return stat_buf.st_mtime;
}

// 比较两个文件是否相同（基于大小和修改时间）
int compare_files(const char *file1, const char *file2) {
    if (!file_exists(file2)) {
        return 0;
    }
    
    // 比较文件大小
    off_t size1 = get_file_size(file1);
    off_t size2 = get_file_size(file2);
    
    if (size1 != size2) {
        return 0;
    }
    
    // 比较修改时间（简单优化）
    time_t mtime1 = get_file_mtime(file1);
    time_t mtime2 = get_file_mtime(file2);
    
    if (mtime1 == mtime2 && size1 == size2) {
        return 1;
    }
    
    // 如果需要精确比较，可以比较内容
    if (size1 < 1024 * 1024) { // 只对小文件进行内容比较
        FILE *f1 = fopen(file1, "rb");
        FILE *f2 = fopen(file2, "rb");
        
        if (!f1 || !f2) {
            if (f1) fclose(f1);
            if (f2) fclose(f2);
            return 0;
        }
        
        unsigned char buffer1[BUFFER_SIZE];
        unsigned char buffer2[BUFFER_SIZE];
        size_t bytes_read1, bytes_read2;
        int result = 1;
        
        do {
            bytes_read1 = fread(buffer1, 1, BUFFER_SIZE, f1);
            bytes_read2 = fread(buffer2, 1, BUFFER_SIZE, f2);
            
            if (bytes_read1 != bytes_read2 || 
                memcmp(buffer1, buffer2, bytes_read1) != 0) {
                result = 0;
                break;
            }
        } while (bytes_read1 > 0);
        
        fclose(f1);
        fclose(f2);
        return result;
    }
    
    return 0;
}

// 创建目录
int create_directory(const char *path) {
    if (file_exists(path)) {
        return 1;
    }
    
    char tmp[MAX_PATH_LEN];
    char *p = NULL;
    size_t len;
    
    // 复制路径字符串
    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    
    // 移除末尾的/
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }
    
    // 递归创建目录
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                return 0;
            }
            *p = '/';
        }
    }
    
    // 创建最后一级目录
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        return 0;
    }
    
    return 1;
}

// 修改 sync_file 函数，正确处理干运行模式
int sync_file(const char *source_file, const char *target_file, int dry_run) {
    // 检查目标文件是否存在且相同
    if (compare_files(source_file, target_file)) {
        if (dry_run) {
            printf("干运行: 跳过相同文件 %s\n", source_file);
        }
        return 1; // 文件相同，无需同步
    }
    
    if (dry_run) {
        printf("干运行: 需要同步 %s -> %s\n", source_file, target_file);
        return 1; // 干运行模式，总是返回成功
    }
    
    // 创建目标目录
    char target_dir[MAX_PATH_LEN];
    char *last_slash = strrchr(target_file, '/');
    if (last_slash) {
        size_t dir_len = last_slash - target_file;
        strncpy(target_dir, target_file, dir_len);
        target_dir[dir_len] = '\0';
        
        if (!create_directory(target_dir)) {
            fprintf(stderr, "无法创建目录: %s\n", target_dir);
            return 0;
        }
    }
    
  // 复制文件
    int source_fd = open(source_file, O_RDONLY);
    if (source_fd < 0) {
        fprintf(stderr, "无法打开源文件: %s\n", source_file);
        return 0;
    }
    
    int target_fd = open(target_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (target_fd < 0) {
        fprintf(stderr, "无法创建目标文件: %s\n", target_file);
        close(source_fd);
        return 0;
    }
    
    // 复制文件内容
    unsigned char buffer[BUFFER_SIZE];
    ssize_t bytes_read, bytes_written;
    int success = 1;
    
    while ((bytes_read = read(source_fd, buffer, BUFFER_SIZE)) > 0) {
        bytes_written = write(target_fd, buffer, bytes_read);
        if (bytes_written != bytes_read) {
            fprintf(stderr, "写入文件失败: %s\n", target_file);
            success = 0;
            break;
        }
    }
    
       // 同步文件时间（使用 utimes 替代 futimes）
    if (success) {
        struct stat stat_buf;
        if (fstat(source_fd, &stat_buf) == 0) {
            struct timeval times[2];
            times[0].tv_sec = stat_buf.st_atime;
            times[0].tv_usec = 0;
            times[1].tv_sec = stat_buf.st_mtime;
            times[1].tv_usec = 0;
            
            // 使用 utimes 而不是 futimes
            close(target_fd); // 先关闭文件
            if (utimes(target_file, times) != 0) {
                // 时间同步失败不影响文件内容同步
                fprintf(stderr, "警告: 无法设置文件时间: %s\n", target_file);
            }
        }
    } else {
        close(target_fd);
    }
    
    if (source_fd >= 0) close(source_fd);
    
    return success;
}

// 获取相对路径
char* get_relative_path(const char *base, const char *full_path) {
    static char relative[MAX_PATH_LEN];
    size_t base_len = strlen(base);
    
    // 如果完整路径以基础路径开头
    if (strncmp(full_path, base, base_len) == 0) {
        const char *relative_start = full_path + base_len;
        
        // 跳过开头的/
        if (*relative_start == '/') {
            relative_start++;
        }
        
        // 如果相对路径为空，返回空字符串
        if (*relative_start == '\0') {
            relative[0] = '\0';
        } else {
            strncpy(relative, relative_start, MAX_PATH_LEN - 1);
            relative[MAX_PATH_LEN - 1] = '\0';
        }
    } else {
        // 如果不匹配，返回完整路径
        strncpy(relative, full_path, MAX_PATH_LEN - 1);
        relative[MAX_PATH_LEN - 1] = '\0';
    }
    
    return relative;
}

// 遍历目录
void traverse_directory(const char *source_path, const char *target_path, file_list_t *list) {
    DIR *dir = opendir(source_path);
    if (!dir) {
        fprintf(stderr, "无法打开目录: %s\n", source_path);
        return;
    }
    
    struct dirent *entry;
    char source_full_path[MAX_PATH_LEN];
    char target_full_path[MAX_PATH_LEN];
    
    while ((entry = readdir(dir)) != NULL) {
        // 跳过 . 和 ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        snprintf(source_full_path, sizeof(source_full_path) - 1, "%s/%s", source_path, entry->d_name);
        source_full_path[sizeof(source_full_path) - 1] = '\0';
        
        snprintf(target_full_path, sizeof(target_full_path) - 1, "%s/%s", target_path, entry->d_name);
        target_full_path[sizeof(target_full_path) - 1] = '\0';
        
        if (is_directory(source_full_path)) {
            // 递归遍历子目录
            traverse_directory(source_full_path, target_full_path, list);
        } else {
            // 添加到文件列表
            off_t size = get_file_size(source_full_path);
            add_file_to_list(list, source_full_path, target_full_path, size);
        }
    }
    
    closedir(dir);
}

// 同步目录结构
void sync_directory_structure(const char *path, const sync_config_t *config) {
    DIR *dir = opendir(path);
    if (!dir) return;
    
    struct dirent *entry;
    char source_full_path[MAX_PATH_LEN];
    char target_full_path[MAX_PATH_LEN];
    
    // 创建当前目录在目标位置
    char *relative = get_relative_path(config->source_dir, path);
    if (strlen(relative) > 0) {
        snprintf(target_full_path, sizeof(target_full_path) - 1, "%s/%s", config->target_dir, relative);
        target_full_path[sizeof(target_full_path) - 1] = '\0';
        create_directory(target_full_path);
    }
    
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        snprintf(source_full_path, sizeof(source_full_path) - 1, "%s/%s", path, entry->d_name);
        source_full_path[sizeof(source_full_path) - 1] = '\0';
        
        if (is_directory(source_full_path)) {
            // 递归同步子目录
            sync_directory_structure(source_full_path, config);
        }
    }
    
    closedir(dir);
}

// 修改 worker_thread 函数，传递 dry_run 参数
void* worker_thread(void *arg) {
    thread_args_t *args = (thread_args_t *)arg;
    file_list_t *file_list = args->file_list;
    int dry_run = args->dry_run;  // 添加这个字段到 thread_args_t
    
    if (args->thread_id == 0 && !dry_run) {
        printf("线程 %d 启动\n", args->thread_id);
    }
    
    while (1) {
        file_info_t *file = get_next_file(file_list);
        if (!file) {
            break;
        }
        
        // 更新统计信息
        pthread_mutex_lock(args->stats_lock);
        (*args->files_processed)++;
        pthread_mutex_unlock(args->stats_lock);
        
        if (file->needs_sync) {
            int result = sync_file(file->source_path, file->target_path, dry_run);
            
            pthread_mutex_lock(args->stats_lock);
            if (result) {
                (*args->files_synced)++;
                if (args->thread_id == 0 && !dry_run) {
                    printf("线程 %d 同步成功: %s\n", args->thread_id, 
                           get_relative_path("", file->source_path));
                }
            } else {
                (*args->errors)++;
                if (!dry_run) {
                    fprintf(stderr, "线程 %d 同步失败: %s\n", args->thread_id, 
                            get_relative_path("", file->source_path));
                }
            }
            pthread_mutex_unlock(args->stats_lock);
        }
    }
    
    if (args->thread_id == 0 && !dry_run) {
        printf("线程 %d 完成\n", args->thread_id);
    }
    
    return NULL;
}

// 打印统计信息
void print_stats(const sync_config_t *config, int total_files, int files_synced, int errors) {
    printf("\n=== 同步统计 ===\n");
    printf("源目录: %s\n", config->source_dir);
    printf("目标目录: %s\n", config->target_dir);
    printf("线程数: %d\n", config->thread_count);
    printf("总文件数: %d\n", total_files);
    printf("同步文件数: %d\n", files_synced);
    printf("跳过文件数: %d\n", total_files - files_synced - errors);
    printf("错误数: %d\n", errors);
    printf("===============\n");
}

// 执行同步
// 修改 perform_sync 函数
int perform_sync(const sync_config_t *config) {
    printf("开始同步: %s -> %s\n", config->source_dir, config->target_dir);
    
    if (config->dry_run) {
        printf("干运行模式 - 不会实际复制文件\n");
    }
    
    printf("使用 %d 个线程\n", config->thread_count);
    
    // 严格检查源目录
    struct stat stat_buf;
    if (stat(config->source_dir, &stat_buf) != 0) {
        fprintf(stderr, "错误: 源目录不存在: %s\n", config->source_dir);
        return 0;
    }
    
    if (!S_ISDIR(stat_buf.st_mode)) {
        fprintf(stderr, "错误: 源路径不是目录: %s\n", config->source_dir);
        return 0;
    }
    
    // 在干运行模式下也创建目标根目录用于测试目录结构
    if (!config->dry_run) {
        if (!create_directory(config->target_dir)) {
            fprintf(stderr, "错误: 无法创建目标目录: %s\n", config->target_dir);
            return 0;
        }
    }
    
    // 同步目录结构（在干运行模式下只打印信息）
    if (config->dry_run) {
        printf("干运行: 将创建目录结构\n");
    } else {
        printf("同步目录结构...\n");
        sync_directory_structure(config->source_dir, config);
    }
    
    // 获取文件列表
    printf("扫描文件...\n");
    file_list_t file_list;
    init_file_list(&file_list);
    traverse_directory(config->source_dir, config->target_dir, &file_list);
    
    printf("找到 %d 个文件需要处理\n", file_list.count);
    
    if (file_list.count == 0) {
        printf("没有文件需要同步\n");
        free_file_list(&file_list);
        return 1;
    }
    
    // 在干运行模式下，只打印信息不实际创建线程
    if (config->dry_run) {
        printf("\n干运行模式报告:\n");
        printf("将同步以下文件:\n");
        
        for (int i = 0; i < file_list.count; i++) {
            file_info_t *file = &file_list.files[i];
            if (!compare_files(file->source_path, file->target_path)) {
                printf("  %s -> %s\n", file->source_path, file->target_path);
            }
        }
        
        free_file_list(&file_list);
        printf("\n干运行模式完成 - 未实际复制任何文件\n");
        return 1;
    }
    
    // 以下是实际的同步代码（非干运行模式）
    // 初始化统计信息
    int files_processed = 0;
    int files_synced = 0;
    int errors = 0;
    pthread_mutex_t stats_lock;
    pthread_mutex_init(&stats_lock, NULL);
    
    // 创建线程
    pthread_t threads[MAX_THREADS];
    thread_args_t thread_args[MAX_THREADS];
    
    printf("启动 %d 个工作线程...\n", config->thread_count);
    
    for (int i = 0; i < config->thread_count; i++) {
        thread_args[i].thread_id = i;
        thread_args[i].file_list = &file_list;
        thread_args[i].files_processed = &files_processed;
        thread_args[i].files_synced = &files_synced;
        thread_args[i].errors = &errors;
        thread_args[i].stats_lock = &stats_lock;
        thread_args[i].dry_run = config->dry_run;
        
        if (pthread_create(&threads[i], NULL, worker_thread, &thread_args[i]) != 0) {
            fprintf(stderr, "创建线程 %d 失败\n", i);
            free_file_list(&file_list);
            pthread_mutex_destroy(&stats_lock);
            return 0;
        }
    }
    
    // 等待所有线程完成
    for (int i = 0; i < config->thread_count; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // 打印统计信息
    print_stats(config, file_list.count, files_synced, errors);
    
    // 清理资源
    free_file_list(&file_list);
    pthread_mutex_destroy(&stats_lock);
    
    if (errors > 0) {
        fprintf(stderr, "同步完成，但有 %d 个错误\n", errors);
        return 0;
    }
    
    printf("同步成功完成\n");
    return 1;
}
