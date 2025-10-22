#include "sync_util.h"
#include <getopt.h>

void print_usage(const char *program_name) {
    printf("用法: %s [选项] 源目录 目标目录\n", program_name);
    printf("选项:\n");
    printf("  -t NUM    设置线程数 (默认: 4)\n");
    printf("  -v        详细输出\n");
    printf("  -n        干运行模式（不实际复制文件）\n");
    printf("  -h        显示帮助信息\n");
    printf("\n示例:\n");
    printf("  %s -t 8 /path/to/source /path/to/target\n", program_name);
    printf("  %s -v -n /backup/src /backup/dst\n", program_name);
}

int main(int argc, char *argv[]) {
    sync_config_t config;
    config.thread_count = 4;
    config.verbose = 0;
    config.dry_run = 0;
    
    // 解析命令行参数
    int opt;
    while ((opt = getopt(argc, argv, "t:vnh")) != -1) {
        switch (opt) {
            case 't':
                config.thread_count = atoi(optarg);
                if (config.thread_count <= 0 || config.thread_count > MAX_THREADS) {
                    fprintf(stderr, "错误: 线程数必须在 1-%d 之间\n", MAX_THREADS);
                    return 1;
                }
                break;
            case 'v':
                config.verbose = 1;
                break;
            case 'n':
                config.dry_run = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    if (optind + 2 != argc) {
        fprintf(stderr, "错误: 需要指定源目录和目标目录\n");
        print_usage(argv[0]);
        return 1;
    }
    
    // 处理目录路径
    const char *source_dir = argv[optind];
    const char *target_dir = argv[optind + 1];
    
    // 确保目录格式正确
    snprintf(config.source_dir, sizeof(config.source_dir), "%s", source_dir);
    snprintf(config.target_dir, sizeof(config.target_dir), "%s", target_dir);
    
    // 移除末尾的/
    size_t source_len = strlen(config.source_dir);
    if (source_len > 0 && config.source_dir[source_len - 1] == '/') {
        config.source_dir[source_len - 1] = '\0';
    }
    
    size_t target_len = strlen(config.target_dir);
    if (target_len > 0 && config.target_dir[target_len - 1] == '/') {
        config.target_dir[target_len - 1] = '\0';
    }
    
    // 执行同步
    if (!perform_sync(&config)) {
        fprintf(stderr, "同步失败\n");
        return 1;
    }
    
    return 0;
}