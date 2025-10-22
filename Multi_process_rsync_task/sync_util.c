#include "sync_util.h"
#include <string.h>
#include <stdlib.h>

// 函数声明
void sync_directory_structure(const char *path, const sync_config_t *config);

// 初始化文件列表
void init_file_list(file_list_t *list) {
    list->capacity = 100;
    list->count = 0;
    list->files = malloc(list->capacity * sizeof(file_info_t));
    if (!list->files) {
        perror("malloc failed");
        exit(1);
    }
}

// 释放文件列表
void free_file_list(file_list_t *list) {
    if (list->files) {
        free(list->files);
        list->files = NULL;
    }
    list->count = 0;
    list->capacity = 0;
}


// 添加文件到列表
void add_file_to_list(file_list_t *list, const char *path) {
    if (list->count >= list->capacity) {
        list->capacity *= 2;
        list->files = realloc(list->files, list->capacity * sizeof(file_info_t));
        if (!list->files) {
            perror("realloc failed");
            exit(1);
        }
    }
    
    strncpy(list->files[list->count].path, path, MAX_PATH_LEN - 1);
    list->files[list->count].path[MAX_PATH_LEN - 1] = '\0';
    list->files[list->count].size = get_file_size(path);
    list->count++;
}

// 遍历目录
void traverse_directory(const char *path, file_list_t *list) {
    DIR *dir = opendir(path);
    if (!dir) {
        fprintf(stderr, "无法打开目录: %s\n", path);
        return;
    }
    
    struct dirent *entry;
    char full_path[MAX_PATH_LEN];
    
    while ((entry = readdir(dir)) != NULL) {
        // 跳过 . 和 ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        snprintf(full_path, sizeof(full_path) - 1, "%s/%s", path, entry->d_name);
        full_path[sizeof(full_path) - 1] = '\0';
        
        if (is_directory(full_path)) {
            // 递归遍历子目录
            traverse_directory(full_path, list);
        } else {
            // 添加到文件列表
            add_file_to_list(list, full_path);
        }
    }
    
    closedir(dir);
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

// 比较两个文件是否相同
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
    
    // 比较文件内容
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

// 同步单个文件
int sync_file(const char *source_file, const char *target_file) {
    // 检查目标文件是否存在且相同
    if (compare_files(source_file, target_file)) {
        return 1; // 文件相同，无需同步
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
    FILE *source_fp = fopen(source_file, "rb");
    if (!source_fp) {
        fprintf(stderr, "无法打开源文件: %s\n", source_file);
        return 0;
    }
    
    FILE *target_fp = fopen(target_file, "wb");
    if (!target_fp) {
        fprintf(stderr, "无法创建目标文件: %s\n", target_file);
        fclose(source_fp);
        return 0;
    }
    
    // 复制文件内容
    unsigned char buffer[BUFFER_SIZE];
    size_t bytes_read;
    int success = 1;
    
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, source_fp)) > 0) {
        if (fwrite(buffer, 1, bytes_read, target_fp) != bytes_read) {
            fprintf(stderr, "写入文件失败: %s\n", target_file);
            success = 0;
            break;
        }
    }
    
    fclose(source_fp);
    fclose(target_fp);
    
    return success;
}

// 获取相对路径
char* get_relative_path(const char *base, const char *full_path) {
    static char relative[MAX_PATH_LEN];
    const char *pos = strstr(full_path, base);
    
    if (pos == full_path) {
        strncpy(relative, full_path + strlen(base), MAX_PATH_LEN - 1);
        relative[MAX_PATH_LEN - 1] = '\0';
        return relative;
    }
    
    strncpy(relative, full_path, MAX_PATH_LEN - 1);
    relative[MAX_PATH_LEN - 1] = '\0';
    return relative;
}

// 工作进程函数
void worker_process(int worker_id, file_list_t *files, const sync_config_t *config) {
    printf("工作进程 %d 开始处理 %d 个文件\n", worker_id, files->count);
    
    for (int i = 0; i < files->count; i++) {
        const char *source_file = files->files[i].path;
        char *relative_path = get_relative_path(config->source_dir, source_file);
        
        char target_file[MAX_PATH_LEN];
        snprintf(target_file, sizeof(target_file), "%s%s", config->target_dir, relative_path);
        
        if (sync_file(source_file, target_file)) {
            printf("进程 %d 同步成功: %s\n", worker_id, relative_path);
        } else {
            fprintf(stderr, "进程 %d 同步失败: %s\n", worker_id, relative_path);
        }
    }
    
    printf("工作进程 %d 完成\n", worker_id);
}

// 执行同步
int perform_sync(const sync_config_t *config) {
    printf("开始同步: %s -> %s\n", config->source_dir, config->target_dir);
    
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

      // 创建目标根目录
    if (!create_directory(config->target_dir)) {
        fprintf(stderr, "错误: 无法创建目标目录: %s\n", config->target_dir);
        return 0;
    }
    
 // 同步目录结构
    sync_directory_structure(config->source_dir, config);

 // 获取文件列表并进行同步
    file_list_t files;
    init_file_list(&files);
    traverse_directory(config->source_dir, &files);
    
    printf("找到 %d 个文件需要同步\n", files.count);
    
    if (files.count == 0) {
        printf("没有文件需要同步\n");
        free_file_list(&files);
        return 1;
    }
    
    // 创建多个进程进行同步
    pid_t *pids = malloc(config->process_count * sizeof(pid_t));
    if (!pids) {
        perror("malloc failed");
        free_file_list(&files);
        return 0;
    }
    
    int files_per_process = files.count / config->process_count;
    int remaining_files = files.count % config->process_count;
    int start_index = 0;
    
    for (int i = 0; i < config->process_count; i++) {
        pid_t pid = fork();
        
        if (pid == 0) { // 子进程
            file_list_t worker_files;
            init_file_list(&worker_files);
            
            int end_index = start_index + files_per_process;
            if (i == config->process_count - 1) {
                end_index += remaining_files;
            }
            
            // 复制文件列表片段
            for (int j = start_index; j < end_index; j++) {
                add_file_to_list(&worker_files, files.files[j].path);
            }
            
            worker_process(i, &worker_files, config);
            free_file_list(&worker_files);
            exit(0);
        } else if (pid > 0) { // 父进程
            pids[i] = pid;
            start_index += files_per_process;
        } else {
            perror("创建进程失败");
            free(pids);
            free_file_list(&files);
            return 0;
        }
    }
    
    // 等待所有子进程完成
    int all_success = 1;
    for (int i = 0; i < config->process_count; i++) {
        int status;
        waitpid(pids[i], &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            fprintf(stderr, "子进程 %d 同步失败\n", i);
            all_success = 0;
        }
    }
    
    free(pids);
    free_file_list(&files);
    
    if (all_success) {
        printf("同步完成\n");
    } else {
        fprintf(stderr, "同步过程中出现错误\n");
    }
    
    return all_success;
}



// 同步目录结构
void sync_directory_structure(const char *path, const sync_config_t *config) {
    DIR *dir = opendir(path);
    if (!dir) return;
    
    struct dirent *entry;
    char full_path[MAX_PATH_LEN];
    char target_path[MAX_PATH_LEN];
    
    // 创建当前目录在目标位置
    char *relative = get_relative_path(config->source_dir, path);
    if (strlen(relative) > 0) {
        snprintf(target_path, sizeof(target_path) - 1, "%s/%s", config->target_dir, relative);
        target_path[sizeof(target_path) - 1] = '\0';
        create_directory(target_path);
    }
    
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        snprintf(full_path, sizeof(full_path) - 1, "%s/%s", path, entry->d_name);
        full_path[sizeof(full_path) - 1] = '\0';
        
        if (is_directory(full_path)) {
            // 递归同步子目录
            sync_directory_structure(full_path, config);
        }
    }
    
    closedir(dir);
}