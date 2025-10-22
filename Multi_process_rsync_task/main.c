#include "sync_util.h"
#include <getopt.h>

void print_usage(const char *program_name) {
    printf("�÷�: %s [ѡ��] ԴĿ¼ Ŀ��Ŀ¼\n", program_name);
    printf("ѡ��:\n");
    printf("  -p NUM    ���ý����� (Ĭ��: 4)\n");
    printf("  -h        ��ʾ������Ϣ\n");
}

int main(int argc, char *argv[]) {
    sync_config_t config;
    config.process_count = 4;
    
    // ���������в���
    int opt;
    while ((opt = getopt(argc, argv, "p:h")) != -1) {
        switch (opt) {
            case 'p':
                config.process_count = atoi(optarg);
                if (config.process_count <= 0) {
                    fprintf(stderr, "����: �������������0\n");
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
    
    // �Ƴ�ĩβ��/����get_relative_path�д���
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