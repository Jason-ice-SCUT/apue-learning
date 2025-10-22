#include "sync_util.h"
#include <string.h>
#include <stdlib.h>

// ��ʼ���ļ��б�
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

// �ͷ��ļ��б�
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

// ����ļ����б�
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

// ��ȡ��һ����Ҫ������ļ�
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

// ����Ƿ�ΪĿ¼
int is_directory(const char *path) {
    struct stat stat_buf;
    if (stat(path, &stat_buf) != 0) {
        return 0;
    }
    return S_ISDIR(stat_buf.st_mode);
}

// ����ļ��Ƿ����
int file_exists(const char *path) {
    return access(path, F_OK) == 0;
}

// ��ȡ�ļ���С
off_t get_file_size(const char *path) {
    struct stat stat_buf;
    if (stat(path, &stat_buf) != 0) {
        return -1;
    }
    return stat_buf.st_size;
}

// ��ȡ�ļ��޸�ʱ��
time_t get_file_mtime(const char *path) {
    struct stat stat_buf;
    if (stat(path, &stat_buf) != 0) {
        return 0;
    }
    return stat_buf.st_mtime;
}

// �Ƚ������ļ��Ƿ���ͬ�����ڴ�С���޸�ʱ�䣩
int compare_files(const char *file1, const char *file2) {
    if (!file_exists(file2)) {
        return 0;
    }
    
    // �Ƚ��ļ���С
    off_t size1 = get_file_size(file1);
    off_t size2 = get_file_size(file2);
    
    if (size1 != size2) {
        return 0;
    }
    
    // �Ƚ��޸�ʱ�䣨���Ż���
    time_t mtime1 = get_file_mtime(file1);
    time_t mtime2 = get_file_mtime(file2);
    
    if (mtime1 == mtime2 && size1 == size2) {
        return 1;
    }
    
    // �����Ҫ��ȷ�Ƚϣ����ԱȽ�����
    if (size1 < 1024 * 1024) { // ֻ��С�ļ��������ݱȽ�
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

// ����Ŀ¼
int create_directory(const char *path) {
    if (file_exists(path)) {
        return 1;
    }
    
    char tmp[MAX_PATH_LEN];
    char *p = NULL;
    size_t len;
    
    // ����·���ַ���
    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    
    // �Ƴ�ĩβ��/
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }
    
    // �ݹ鴴��Ŀ¼
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                return 0;
            }
            *p = '/';
        }
    }
    
    // �������һ��Ŀ¼
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        return 0;
    }
    
    return 1;
}

// �޸� sync_file ��������ȷ���������ģʽ
int sync_file(const char *source_file, const char *target_file, int dry_run) {
    // ���Ŀ���ļ��Ƿ��������ͬ
    if (compare_files(source_file, target_file)) {
        if (dry_run) {
            printf("������: ������ͬ�ļ� %s\n", source_file);
        }
        return 1; // �ļ���ͬ������ͬ��
    }
    
    if (dry_run) {
        printf("������: ��Ҫͬ�� %s -> %s\n", source_file, target_file);
        return 1; // ������ģʽ�����Ƿ��سɹ�
    }
    
    // ����Ŀ��Ŀ¼
    char target_dir[MAX_PATH_LEN];
    char *last_slash = strrchr(target_file, '/');
    if (last_slash) {
        size_t dir_len = last_slash - target_file;
        strncpy(target_dir, target_file, dir_len);
        target_dir[dir_len] = '\0';
        
        if (!create_directory(target_dir)) {
            fprintf(stderr, "�޷�����Ŀ¼: %s\n", target_dir);
            return 0;
        }
    }
    
  // �����ļ�
    int source_fd = open(source_file, O_RDONLY);
    if (source_fd < 0) {
        fprintf(stderr, "�޷���Դ�ļ�: %s\n", source_file);
        return 0;
    }
    
    int target_fd = open(target_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (target_fd < 0) {
        fprintf(stderr, "�޷�����Ŀ���ļ�: %s\n", target_file);
        close(source_fd);
        return 0;
    }
    
    // �����ļ�����
    unsigned char buffer[BUFFER_SIZE];
    ssize_t bytes_read, bytes_written;
    int success = 1;
    
    while ((bytes_read = read(source_fd, buffer, BUFFER_SIZE)) > 0) {
        bytes_written = write(target_fd, buffer, bytes_read);
        if (bytes_written != bytes_read) {
            fprintf(stderr, "д���ļ�ʧ��: %s\n", target_file);
            success = 0;
            break;
        }
    }
    
       // ͬ���ļ�ʱ�䣨ʹ�� utimes ��� futimes��
    if (success) {
        struct stat stat_buf;
        if (fstat(source_fd, &stat_buf) == 0) {
            struct timeval times[2];
            times[0].tv_sec = stat_buf.st_atime;
            times[0].tv_usec = 0;
            times[1].tv_sec = stat_buf.st_mtime;
            times[1].tv_usec = 0;
            
            // ʹ�� utimes ������ futimes
            close(target_fd); // �ȹر��ļ�
            if (utimes(target_file, times) != 0) {
                // ʱ��ͬ��ʧ�ܲ�Ӱ���ļ�����ͬ��
                fprintf(stderr, "����: �޷������ļ�ʱ��: %s\n", target_file);
            }
        }
    } else {
        close(target_fd);
    }
    
    if (source_fd >= 0) close(source_fd);
    
    return success;
}

// ��ȡ���·��
char* get_relative_path(const char *base, const char *full_path) {
    static char relative[MAX_PATH_LEN];
    size_t base_len = strlen(base);
    
    // �������·���Ի���·����ͷ
    if (strncmp(full_path, base, base_len) == 0) {
        const char *relative_start = full_path + base_len;
        
        // ������ͷ��/
        if (*relative_start == '/') {
            relative_start++;
        }
        
        // ������·��Ϊ�գ����ؿ��ַ���
        if (*relative_start == '\0') {
            relative[0] = '\0';
        } else {
            strncpy(relative, relative_start, MAX_PATH_LEN - 1);
            relative[MAX_PATH_LEN - 1] = '\0';
        }
    } else {
        // �����ƥ�䣬��������·��
        strncpy(relative, full_path, MAX_PATH_LEN - 1);
        relative[MAX_PATH_LEN - 1] = '\0';
    }
    
    return relative;
}

// ����Ŀ¼
void traverse_directory(const char *source_path, const char *target_path, file_list_t *list) {
    DIR *dir = opendir(source_path);
    if (!dir) {
        fprintf(stderr, "�޷���Ŀ¼: %s\n", source_path);
        return;
    }
    
    struct dirent *entry;
    char source_full_path[MAX_PATH_LEN];
    char target_full_path[MAX_PATH_LEN];
    
    while ((entry = readdir(dir)) != NULL) {
        // ���� . �� ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        snprintf(source_full_path, sizeof(source_full_path) - 1, "%s/%s", source_path, entry->d_name);
        source_full_path[sizeof(source_full_path) - 1] = '\0';
        
        snprintf(target_full_path, sizeof(target_full_path) - 1, "%s/%s", target_path, entry->d_name);
        target_full_path[sizeof(target_full_path) - 1] = '\0';
        
        if (is_directory(source_full_path)) {
            // �ݹ������Ŀ¼
            traverse_directory(source_full_path, target_full_path, list);
        } else {
            // ��ӵ��ļ��б�
            off_t size = get_file_size(source_full_path);
            add_file_to_list(list, source_full_path, target_full_path, size);
        }
    }
    
    closedir(dir);
}

// ͬ��Ŀ¼�ṹ
void sync_directory_structure(const char *path, const sync_config_t *config) {
    DIR *dir = opendir(path);
    if (!dir) return;
    
    struct dirent *entry;
    char source_full_path[MAX_PATH_LEN];
    char target_full_path[MAX_PATH_LEN];
    
    // ������ǰĿ¼��Ŀ��λ��
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
            // �ݹ�ͬ����Ŀ¼
            sync_directory_structure(source_full_path, config);
        }
    }
    
    closedir(dir);
}

// �޸� worker_thread ���������� dry_run ����
void* worker_thread(void *arg) {
    thread_args_t *args = (thread_args_t *)arg;
    file_list_t *file_list = args->file_list;
    int dry_run = args->dry_run;  // �������ֶε� thread_args_t
    
    if (args->thread_id == 0 && !dry_run) {
        printf("�߳� %d ����\n", args->thread_id);
    }
    
    while (1) {
        file_info_t *file = get_next_file(file_list);
        if (!file) {
            break;
        }
        
        // ����ͳ����Ϣ
        pthread_mutex_lock(args->stats_lock);
        (*args->files_processed)++;
        pthread_mutex_unlock(args->stats_lock);
        
        if (file->needs_sync) {
            int result = sync_file(file->source_path, file->target_path, dry_run);
            
            pthread_mutex_lock(args->stats_lock);
            if (result) {
                (*args->files_synced)++;
                if (args->thread_id == 0 && !dry_run) {
                    printf("�߳� %d ͬ���ɹ�: %s\n", args->thread_id, 
                           get_relative_path("", file->source_path));
                }
            } else {
                (*args->errors)++;
                if (!dry_run) {
                    fprintf(stderr, "�߳� %d ͬ��ʧ��: %s\n", args->thread_id, 
                            get_relative_path("", file->source_path));
                }
            }
            pthread_mutex_unlock(args->stats_lock);
        }
    }
    
    if (args->thread_id == 0 && !dry_run) {
        printf("�߳� %d ���\n", args->thread_id);
    }
    
    return NULL;
}

// ��ӡͳ����Ϣ
void print_stats(const sync_config_t *config, int total_files, int files_synced, int errors) {
    printf("\n=== ͬ��ͳ�� ===\n");
    printf("ԴĿ¼: %s\n", config->source_dir);
    printf("Ŀ��Ŀ¼: %s\n", config->target_dir);
    printf("�߳���: %d\n", config->thread_count);
    printf("���ļ���: %d\n", total_files);
    printf("ͬ���ļ���: %d\n", files_synced);
    printf("�����ļ���: %d\n", total_files - files_synced - errors);
    printf("������: %d\n", errors);
    printf("===============\n");
}

// ִ��ͬ��
// �޸� perform_sync ����
int perform_sync(const sync_config_t *config) {
    printf("��ʼͬ��: %s -> %s\n", config->source_dir, config->target_dir);
    
    if (config->dry_run) {
        printf("������ģʽ - ����ʵ�ʸ����ļ�\n");
    }
    
    printf("ʹ�� %d ���߳�\n", config->thread_count);
    
    // �ϸ���ԴĿ¼
    struct stat stat_buf;
    if (stat(config->source_dir, &stat_buf) != 0) {
        fprintf(stderr, "����: ԴĿ¼������: %s\n", config->source_dir);
        return 0;
    }
    
    if (!S_ISDIR(stat_buf.st_mode)) {
        fprintf(stderr, "����: Դ·������Ŀ¼: %s\n", config->source_dir);
        return 0;
    }
    
    // �ڸ�����ģʽ��Ҳ����Ŀ���Ŀ¼���ڲ���Ŀ¼�ṹ
    if (!config->dry_run) {
        if (!create_directory(config->target_dir)) {
            fprintf(stderr, "����: �޷�����Ŀ��Ŀ¼: %s\n", config->target_dir);
            return 0;
        }
    }
    
    // ͬ��Ŀ¼�ṹ���ڸ�����ģʽ��ֻ��ӡ��Ϣ��
    if (config->dry_run) {
        printf("������: ������Ŀ¼�ṹ\n");
    } else {
        printf("ͬ��Ŀ¼�ṹ...\n");
        sync_directory_structure(config->source_dir, config);
    }
    
    // ��ȡ�ļ��б�
    printf("ɨ���ļ�...\n");
    file_list_t file_list;
    init_file_list(&file_list);
    traverse_directory(config->source_dir, config->target_dir, &file_list);
    
    printf("�ҵ� %d ���ļ���Ҫ����\n", file_list.count);
    
    if (file_list.count == 0) {
        printf("û���ļ���Ҫͬ��\n");
        free_file_list(&file_list);
        return 1;
    }
    
    // �ڸ�����ģʽ�£�ֻ��ӡ��Ϣ��ʵ�ʴ����߳�
    if (config->dry_run) {
        printf("\n������ģʽ����:\n");
        printf("��ͬ�������ļ�:\n");
        
        for (int i = 0; i < file_list.count; i++) {
            file_info_t *file = &file_list.files[i];
            if (!compare_files(file->source_path, file->target_path)) {
                printf("  %s -> %s\n", file->source_path, file->target_path);
            }
        }
        
        free_file_list(&file_list);
        printf("\n������ģʽ��� - δʵ�ʸ����κ��ļ�\n");
        return 1;
    }
    
    // ������ʵ�ʵ�ͬ�����루�Ǹ�����ģʽ��
    // ��ʼ��ͳ����Ϣ
    int files_processed = 0;
    int files_synced = 0;
    int errors = 0;
    pthread_mutex_t stats_lock;
    pthread_mutex_init(&stats_lock, NULL);
    
    // �����߳�
    pthread_t threads[MAX_THREADS];
    thread_args_t thread_args[MAX_THREADS];
    
    printf("���� %d �������߳�...\n", config->thread_count);
    
    for (int i = 0; i < config->thread_count; i++) {
        thread_args[i].thread_id = i;
        thread_args[i].file_list = &file_list;
        thread_args[i].files_processed = &files_processed;
        thread_args[i].files_synced = &files_synced;
        thread_args[i].errors = &errors;
        thread_args[i].stats_lock = &stats_lock;
        thread_args[i].dry_run = config->dry_run;
        
        if (pthread_create(&threads[i], NULL, worker_thread, &thread_args[i]) != 0) {
            fprintf(stderr, "�����߳� %d ʧ��\n", i);
            free_file_list(&file_list);
            pthread_mutex_destroy(&stats_lock);
            return 0;
        }
    }
    
    // �ȴ������߳����
    for (int i = 0; i < config->thread_count; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // ��ӡͳ����Ϣ
    print_stats(config, file_list.count, files_synced, errors);
    
    // ������Դ
    free_file_list(&file_list);
    pthread_mutex_destroy(&stats_lock);
    
    if (errors > 0) {
        fprintf(stderr, "ͬ����ɣ����� %d ������\n", errors);
        return 0;
    }
    
    printf("ͬ���ɹ����\n");
    return 1;
}
