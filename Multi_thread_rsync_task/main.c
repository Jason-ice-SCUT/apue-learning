#include "sync_util.h"
#include <getopt.h>

void print_usage(const char *program_name) {
    printf("�÷�: %s [ѡ��] ԴĿ¼ Ŀ��Ŀ¼\n", program_name);
    printf("ѡ��:\n");
    printf("  -t NUM    �����߳��� (Ĭ��: 4)\n");
    printf("  -v        ��ϸ���\n");
    printf("  -n        ������ģʽ����ʵ�ʸ����ļ���\n");
    printf("  -h        ��ʾ������Ϣ\n");
    printf("\nʾ��:\n");
    printf("  %s -t 8 /path/to/source /path/to/target\n", program_name);
    printf("  %s -v -n /backup/src /backup/dst\n", program_name);
}

int main(int argc, char *argv[]) {
    sync_config_t config;
    config.thread_count = 4;
    config.verbose = 0;
    config.dry_run = 0;
    
    // ���������в���
    int opt;
    while ((opt = getopt(argc, argv, "t:vnh")) != -1) {
        switch (opt) {
            case 't':
                config.thread_count = atoi(optarg);
                if (config.thread_count <= 0 || config.thread_count > MAX_THREADS) {
                    fprintf(stderr, "����: �߳��������� 1-%d ֮��\n", MAX_THREADS);
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
        fprintf(stderr, "����: ��Ҫָ��ԴĿ¼��Ŀ��Ŀ¼\n");
        print_usage(argv[0]);
        return 1;
    }
    
    // ����Ŀ¼·��
    const char *source_dir = argv[optind];
    const char *target_dir = argv[optind + 1];
    
    // ȷ��Ŀ¼��ʽ��ȷ
    snprintf(config.source_dir, sizeof(config.source_dir), "%s", source_dir);
    snprintf(config.target_dir, sizeof(config.target_dir), "%s", target_dir);
    
    // �Ƴ�ĩβ��/
    size_t source_len = strlen(config.source_dir);
    if (source_len > 0 && config.source_dir[source_len - 1] == '/') {
        config.source_dir[source_len - 1] = '\0';
    }
    
    size_t target_len = strlen(config.target_dir);
    if (target_len > 0 && config.target_dir[target_len - 1] == '/') {
        config.target_dir[target_len - 1] = '\0';
    }
    
    // ִ��ͬ��
    if (!perform_sync(&config)) {
        fprintf(stderr, "ͬ��ʧ��\n");
        return 1;
    }
    
    return 0;
}