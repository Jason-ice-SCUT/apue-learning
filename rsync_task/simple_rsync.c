#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <utime.h>
#include <libgen.h>

#define BUFFER_SIZE 4096
#define MAX_PATH_LEN 1024

typedef struct {
    int verbose;
    int dry_run;
} sync_options_t;

void print_usage(const char *program_name) {
    fprintf(stderr, "Usage: %s [options] <source> <destination>\n", program_name);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -v    Verbose output\n");
    fprintf(stderr, "  -n    Dry run (simulate without copying)\n");
    fprintf(stderr, "  -h    Show this help\n");
}

int copy_file_data(const char *src_path, const char *dst_path) {
    int src_fd, dst_fd;
    ssize_t bytes_read, bytes_written;
    char buffer[BUFFER_SIZE];
    
    src_fd = open(src_path, O_RDONLY);
    if (src_fd == -1) {
        perror("open source file");
        return -1;
    }
    
    dst_fd = open(dst_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dst_fd == -1) {
        perror("open destination file");
        close(src_fd);
        return -1;
    }
    
    while ((bytes_read = read(src_fd, buffer, BUFFER_SIZE)) > 0) {
        bytes_written = write(dst_fd, buffer, bytes_read);
        if (bytes_written != bytes_read) {
            perror("write error");
            close(src_fd);
            close(dst_fd);
            return -1;
        }
    }
    
    if (bytes_read == -1) {
        perror("read error");
        close(src_fd);
        close(dst_fd);
        return -1;
    }
    
    close(src_fd);
    close(dst_fd);
    return 0;
}

int copy_file_attributes(const char *src_path, const char *dst_path) {
    struct stat src_stat;
    struct utimbuf time_buf;
    
    if (stat(src_path, &src_stat) == -1) {
        perror("stat source file");
        return -1;
    }
    
    // 复制文件权限
    if (chmod(dst_path, src_stat.st_mode) == -1) {
        perror("chmod");
        return -1;
    }
    
    // 复制文件时间戳
    time_buf.actime = src_stat.st_atime;
    time_buf.modtime = src_stat.st_mtime;
    if (utime(dst_path, &time_buf) == -1) {
        perror("utime");
        return -1;
    }
    
    // 复制文件所有者和组（需要root权限）
    if (chown(dst_path, src_stat.st_uid, src_stat.st_gid) == -1) {
        if (errno != EPERM) {
            perror("chown");
            return -1;
        }
        // 非root用户无法更改所有者，这是正常的
    }
    
    return 0;
}

int files_are_identical(const char *file1, const char *file2) {
    struct stat stat1, stat2;
    int fd1, fd2;
    ssize_t bytes1, bytes2;
    char buf1[BUFFER_SIZE], buf2[BUFFER_SIZE];
    
    if (stat(file1, &stat1) == -1 || stat(file2, &stat2) == -1) {
        return 0;
    }
    
    // 比较文件大小
    if (stat1.st_size != stat2.st_size) {
        return 0;
    }
    
    // 比较修改时间
    if (stat1.st_mtime == stat2.st_mtime) {
        return 1;
    }
    
    // 如果修改时间不同，比较文件内容
    fd1 = open(file1, O_RDONLY);
    fd2 = open(file2, O_RDONLY);
    
    if (fd1 == -1 || fd2 == -1) {
        if (fd1 != -1) close(fd1);
        if (fd2 != -1) close(fd2);
        return 0;
    }
    
    while (1) {
        bytes1 = read(fd1, buf1, BUFFER_SIZE);
        bytes2 = read(fd2, buf2, BUFFER_SIZE);
        
        if (bytes1 != bytes2) {
            close(fd1);
            close(fd2);
            return 0;
        }
        
        if (bytes1 == 0) {
            break;  // 文件结束
        }
        
        if (memcmp(buf1, buf2, bytes1) != 0) {
            close(fd1);
            close(fd2);
            return 0;
        }
    }
    
    close(fd1);
    close(fd2);
    return 1;
}

int create_directory_recursive(const char *path) {
    char tmp[MAX_PATH_LEN];
    char *p = NULL;
    size_t len;
    
    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }
    
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                return -1;
            }
            *p = '/';
        }
    }
    
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        return -1;
    }
    
    return 0;
}

int sync_file(const char *src_path, const char *dst_path, const sync_options_t *options) {
    char dst_dir[MAX_PATH_LEN];
    struct stat src_stat, dst_stat;
    
    if (options->verbose) {
        printf("Checking: %s -> %s\n", src_path, dst_path);
    }
    
    // 检查源文件是否存在
    if (stat(src_path, &src_stat) == -1) {
        perror("stat source file");
        return -1;
    }
    
    // 检查目标文件是否存在并比较
    if (stat(dst_path, &dst_stat) != -1) {
        if (files_are_identical(src_path, dst_path)) {
            if (options->verbose) {
                printf("File already synchronized: %s\n", src_path);
            }
            return 0;
        }
    }
    
    if (options->dry_run) {
        printf("DRY-RUN: Would sync %s -> %s\n", src_path, dst_path);
        return 0;
    }
    
    // 创建目标目录
    strncpy(dst_dir, dst_path, sizeof(dst_dir));
    dirname(dst_dir);
    if (create_directory_recursive(dst_dir) == -1) {
        perror("create directory");
        return -1;
    }
    
    // 复制文件内容
    if (copy_file_data(src_path, dst_path) == -1) {
        fprintf(stderr, "Failed to copy file data: %s\n", src_path);
        return -1;
    }
    
    // 复制文件属性
    if (copy_file_attributes(src_path, dst_path) == -1) {
        fprintf(stderr, "Failed to copy file attributes: %s\n", src_path);
        return -1;
    }
    
    if (options->verbose) {
        printf("Synced: %s -> %s\n", src_path, dst_path);
    }
    
    return 0;
}

int sync_directory(const char *src_dir, const char *dst_dir, const sync_options_t *options) {
    DIR *dir;
    struct dirent *entry;
    char src_path[MAX_PATH_LEN], dst_path[MAX_PATH_LEN];
    struct stat statbuf;
    
    if ((dir = opendir(src_dir)) == NULL) {
        perror("opendir");
        return -1;
    }
    
    while ((entry = readdir(dir)) != NULL) {
        // 跳过 . 和 ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        snprintf(src_path, sizeof(src_path), "%s/%s", src_dir, entry->d_name);
        snprintf(dst_path, sizeof(dst_path), "%s/%s", dst_dir, entry->d_name);
        
        if (stat(src_path, &statbuf) == -1) {
            perror("stat");
            continue;
        }
        
        if (S_ISDIR(statbuf.st_mode)) {
            // 递归同步子目录
            if (sync_directory(src_path, dst_path, options) == -1) {
                closedir(dir);
                return -1;
            }
        } else if (S_ISREG(statbuf.st_mode)) {
            // 同步普通文件
            if (sync_file(src_path, dst_path, options) == -1) {
                closedir(dir);
                return -1;
            }
        }
    }
    
    closedir(dir);
    return 0;
}

int main(int argc, char *argv[]) {
    sync_options_t options = {0, 0};
    int opt;
    char *source = NULL, *destination = NULL;
    struct stat src_stat;
    
    // 解析命令行参数
    while ((opt = getopt(argc, argv, "vnh")) != -1) {
        switch (opt) {
            case 'v':
                options.verbose = 1;
                break;
            case 'n':
                options.dry_run = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    // 检查参数数量
    if (optind + 2 != argc) {
        fprintf(stderr, "Error: Source and destination paths are required\n");
        print_usage(argv[0]);
        return 1;
    }
    
    source = argv[optind];
    destination = argv[optind + 1];
    
    if (options.dry_run) {
        printf("=== DRY RUN MODE ===\n");
    }
    
    // 检查源路径是否存在
    if (stat(source, &src_stat) == -1) {
        perror("stat source");
        return 1;
    }
    
    // 执行同步
    if (S_ISDIR(src_stat.st_mode)) {
        if (sync_directory(source, destination, &options) == -1) {
            fprintf(stderr, "Directory synchronization failed\n");
            return 1;
        }
    } else if (S_ISREG(src_stat.st_mode)) {
        if (sync_file(source, destination, &options) == -1) {
            fprintf(stderr, "File synchronization failed\n");
            return 1;
        }
    } else {
        fprintf(stderr, "Error: Source is not a regular file or directory\n");
        return 1;
    }
    
    printf("Synchronization completed successfully\n");
    return 0;
}