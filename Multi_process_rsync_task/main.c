#include "sync_util.h"
#include <getopt.h>

void print_usage(const char *program_name) {
    printf("用法: %s [选项] 源目录 目标目录\n", program_name);
    printf("选项:\n");
    printf("  -p NUM    设置进程数 (默认: 4)\n");
    printf("  -h        显示帮助信息\n");
}

int main(int argc, char *argv[]) {
    sync_config_t config;
    config.process_count = 4;
    
    // 解析命令行参数
    int opt;
    while ((opt = getopt(argc, argv, "p:h")) != -1) {
        switch (opt) {
            case 'p':
                config.process_count = atoi(optarg);
                if (config.process_count <= 0) {
                    fprintf(stderr, "错误: 进程数必须大于0\n");
                    return 1;
                }
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
    
    // 移除末尾的/（在get_relative_path中处理）
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