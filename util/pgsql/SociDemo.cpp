#include "SociDemo.h"


//int main(){
//    SociDemo::demo();
////    SociDemo::bulkOperationsDemo();
//
//}




#include "SociDB.h" // 假设包含之前的SociDB定义
#include <iostream>
#include <chrono>
#include <iomanip>

// 打印当前时间的辅助函数
void print_current_time(const std::string& msg) {
    auto now = std::chrono::system_clock::now();
    auto now_time = std::chrono::system_clock::to_time_t(now);
    std::cout << "[" << std::put_time(std::localtime(&now_time), "%Y-%m-%d %H:%M:%S")
              << "] " << msg << std::endl;
}

int main() {
    // 初始化数据库连接
    SociDB::init();

    const std::string table_name = "xc_uv_grab_log";
    const std::string partition_key = "created_at";
    const std::string comment_prefix = "抢单日志分区";

    print_current_time("开始分区管理测试");

    // 测试1: 创建下个月分区
    std::cout << "\n=== 测试1: 创建下个月分区 ===" << std::endl;
    bool created = SociDB::create_next_month_partition(table_name, partition_key, comment_prefix);
    std::cout << "创建结果: " << (created ? "成功" : "已存在或失败") << std::endl;

    // 测试2: 确保未来分区
    std::cout << "\n=== 测试2: 确保未来3个月分区 ===" << std::endl;
    SociDB::ensure_future_partitions(table_name, 3, partition_key, comment_prefix);
    print_current_time("未来分区检查完成");

    // 测试3: 检查分区健康状态
    std::cout << "\n=== 测试3: 检查分区健康状态 ===" << std::endl;
    auto missing_months = SociDB::check_partition_health(table_name, partition_key);
    if (missing_months.empty()) {
        std::cout << "所有分区正常，无缺失" << std::endl;
    } else {
        std::cout << "发现缺失分区: ";
        for (const auto& month : missing_months) {
            std::cout << month << " ";
        }
        std::cout << std::endl;
    }

    // 测试4: 修复缺失分区（模拟一个缺失月份）
    std::cout << "\n=== 测试4: 修复缺失分区 ===" << std::endl;
    std::vector<std::string> simulated_missing = missing_months; // 模拟2023年1月分区缺失
    SociDB::repair_missing_partitions(table_name, simulated_missing, partition_key);
    print_current_time("分区修复操作完成");

    // 验证修复结果
    std::cout << "\n=== 验证修复结果 ===" << std::endl;
    missing_months = SociDB::check_partition_health(table_name, partition_key);
    if (missing_months.empty()) {
        std::cout << "所有分区已修复，状态正常" << std::endl;
    } else {
        std::cout << "仍有分区缺失: ";
        for (const auto& month : missing_months) {
            std::cout << month << " ";
        }
        std::cout << std::endl;
    }

    print_current_time("分区管理测试完成");
    return 0;
}
