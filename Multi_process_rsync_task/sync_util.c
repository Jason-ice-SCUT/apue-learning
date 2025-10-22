#include "sync_util.h"
#include <string.h>
#include <stdlib.h>

// ��������
void sync_directory_structure(const char *path, const sync_config_t *config);

// ��ʼ���ļ��б�
void init_file_list(file_list_t *list) {
    list->capacity = 100;
    list->count = 0;
    list->files = malloc(list->capacity * sizeof(file_info_t));
    if (!list->files) {
        perror("malloc failed");
        exit(1);
    }
}

// �ͷ��ļ��б�
void free_file_list(file_list_t *list) {
    if (list->files) {
        free(list->files);
        list->files = NULL;
    }
    list->count = 0;
    list->capacity = 0;
}


// ����ļ����б�
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

// ����Ŀ¼
void traverse_directory(const char *path, file_list_t *list) {
    DIR *dir = opendir(path);
    if (!dir) {
        fprintf(stderr, "�޷���Ŀ¼: %s\n", path);
        return;
    }
    
    struct dirent *entry;
    char full_path[MAX_PATH_LEN];
    
    while ((entry = readdir(dir)) != NULL) {
        // ���� . �� ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        snprintf(full_path, sizeof(full_path) - 1, "%s/%s", path, entry->d_name);
        full_path[sizeof(full_path) - 1] = '\0';
        
        if (is_directory(full_path)) {
            // �ݹ������Ŀ¼
            traverse_directory(full_path, list);
        } else {
            // ��ӵ��ļ��б�
            add_file_to_list(list, full_path);
        }
    }
    
    closedir(dir);
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

// �Ƚ������ļ��Ƿ���ͬ
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
    
    // �Ƚ��ļ�����
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

// ͬ�������ļ�
int sync_file(const char *source_file, const char *target_file) {
    // ���Ŀ���ļ��Ƿ��������ͬ
    if (compare_files(source_file, target_file)) {
        return 1; // �ļ���ͬ������ͬ��
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
    FILE *source_fp = fopen(source_file, "rb");
    if (!source_fp) {
        fprintf(stderr, "�޷���Դ�ļ�: %s\n", source_file);
        return 0;
    }
    
    FILE *target_fp = fopen(target_file, "wb");
    if (!target_fp) {
        fprintf(stderr, "�޷�����Ŀ���ļ�: %s\n", target_file);
        fclose(source_fp);
        return 0;
    }
    
    // �����ļ�����
    unsigned char buffer[BUFFER_SIZE];
    size_t bytes_read;
    int success = 1;
    
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, source_fp)) > 0) {
        if (fwrite(buffer, 1, bytes_read, target_fp) != bytes_read) {
            fprintf(stderr, "д���ļ�ʧ��: %s\n", target_file);
            success = 0;
            break;
        }
    }
    
    fclose(source_fp);
    fclose(target_fp);
    
    return success;
}

// ��ȡ���·��
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

// �������̺���
void worker_process(int worker_id, file_list_t *files, const sync_config_t *config) {
    printf("�������� %d ��ʼ���� %d ���ļ�\n", worker_id, files->count);
    
    for (int i = 0; i < files->count; i++) {
        const char *source_file = files->files[i].path;
        char *relative_path = get_relative_path(config->source_dir, source_file);
        
        char target_file[MAX_PATH_LEN];
        snprintf(target_file, sizeof(target_file), "%s%s", config->target_dir, relative_path);
        
        if (sync_file(source_file, target_file)) {
            printf("���� %d ͬ���ɹ�: %s\n", worker_id, relative_path);
        } else {
            fprintf(stderr, "���� %d ͬ��ʧ��: %s\n", worker_id, relative_path);
        }
    }
    
    printf("�������� %d ���\n", worker_id);
}

// ִ��ͬ��
int perform_sync(const sync_config_t *config) {
    printf("��ʼͬ��: %s -> %s\n", config->source_dir, config->target_dir);
    
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

      // ����Ŀ���Ŀ¼
    if (!create_directory(config->target_dir)) {
        fprintf(stderr, "����: �޷�����Ŀ��Ŀ¼: %s\n", config->target_dir);
        return 0;
    }
    
 // ͬ��Ŀ¼�ṹ
    sync_directory_structure(config->source_dir, config);

 // ��ȡ�ļ��б�����ͬ��
    file_list_t files;
    init_file_list(&files);
    traverse_directory(config->source_dir, &files);
    
    printf("�ҵ� %d ���ļ���Ҫͬ��\n", files.count);
    
    if (files.count == 0) {
        printf("û���ļ���Ҫͬ��\n");
        free_file_list(&files);
        return 1;
    }
    
    // ����������̽���ͬ��
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
        
        if (pid == 0) { // �ӽ���
            file_list_t worker_files;
            init_file_list(&worker_files);
            
            int end_index = start_index + files_per_process;
            if (i == config->process_count - 1) {
                end_index += remaining_files;
            }
            
            // �����ļ��б�Ƭ��
            for (int j = start_index; j < end_index; j++) {
                add_file_to_list(&worker_files, files.files[j].path);
            }
            
            worker_process(i, &worker_files, config);
            free_file_list(&worker_files);
            exit(0);
        } else if (pid > 0) { // ������
            pids[i] = pid;
            start_index += files_per_process;
        } else {
            perror("��������ʧ��");
            free(pids);
            free_file_list(&files);
            return 0;
        }
    }
    
    // �ȴ������ӽ������
    int all_success = 1;
    for (int i = 0; i < config->process_count; i++) {
        int status;
        waitpid(pids[i], &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            fprintf(stderr, "�ӽ��� %d ͬ��ʧ��\n", i);
            all_success = 0;
        }
    }
    
    free(pids);
    free_file_list(&files);
    
    if (all_success) {
        printf("ͬ�����\n");
    } else {
        fprintf(stderr, "ͬ�������г��ִ���\n");
    }
    
    return all_success;
}



// ͬ��Ŀ¼�ṹ
void sync_directory_structure(const char *path, const sync_config_t *config) {
    DIR *dir = opendir(path);
    if (!dir) return;
    
    struct dirent *entry;
    char full_path[MAX_PATH_LEN];
    char target_path[MAX_PATH_LEN];
    
    // ������ǰĿ¼��Ŀ��λ��
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
            // �ݹ�ͬ����Ŀ¼
            sync_directory_structure(full_path, config);
        }
    }
    
    closedir(dir);
}