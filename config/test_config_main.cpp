#include "ConfigManager.h"
#include <iostream>
#include <vector>

void test_config_manager() {
    try {
        // 1. 加载配置文件
        ConfigManager::instance().load(R"(D:\CLionProjects\cadd\config\upload.json)");

        // 2. 获取各种类型的参数
        std::cout << "=== 基础参数测试 ===" << std::endl;
        std::string upload_dir = ConfigManager::instance().get("upload.base_dir");
        int max_size = ConfigManager::instance().get("upload.max_size_mb", 10);
        auto allow_types = ConfigManager::instance().get<std::vector<std::string>>("upload.allowed_types");

        std::cout << "上传目录: " << upload_dir << std::endl;
        std::cout << "最大尺寸(MB): " << max_size << std::endl;
        std::cout << "允许类型: ";
        for (const auto& type : allow_types) {
            std::cout << type << " ";
        }
        std::cout << "\n\n";

        // 3. 测试不存在的参数（带默认值）
        std::cout << "=== 默认值测试 ===" << std::endl;
        bool debug_mode = ConfigManager::instance().get("debug.enabled", false);
        int thread_count = ConfigManager::instance().get("performance.threads", 4);

        std::cout << "调试模式: " << std::boolalpha << debug_mode << std::endl;
        std::cout << "线程数: " << thread_count << "\n\n";

        // 4. 动态修改参数
        std::cout << "=== 动态修改测试 ===" << std::endl;
        ConfigManager::instance().set("upload.max_size_mb", 50);
        ConfigManager::instance().set("debug.enabled", true);

        std::cout << "修改后的最大尺寸: "
                  << ConfigManager::instance().get("upload.max_size_mb") << std::endl;
        std::cout << "修改后的调试模式: "
                  << ConfigManager::instance().get("debug.enabled") << "\n\n";

        // 5. 测试嵌套参数
        std::cout << "=== 嵌套参数测试 ===" << std::endl;
        int timeout = ConfigManager::instance().get("network.timeout");
        int retry = ConfigManager::instance().get("network.retry_times");

        std::cout << "网络超时: " << timeout << "秒" << std::endl;
        std::cout << "重试次数: " << retry << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "测试失败: " << e.what() << std::endl;
    }
}

int main() {
    test_config_manager();
    return 0;
}