#include "OrderSubscriber.h"
#include <atomic>
#include <thread>
#include <iostream>
#include <unordered_map>
#include <mutex>
#include <vector>

// 全局控制
std::atomic<bool> running{true};
std::mutex clients_mutex;
int64_t last_loaded_id = 0;
constexpr int MAX_VEHICLES = 5000;  // 新增：最大车辆数限制

// 简化后的车辆管理器
class VehicleManager {
public:
    void add_vehicle(int64_t id) {
        std::lock_guard<std::mutex> lock(clients_mutex);
        if (vehicles_.count(id) == 0) {
//            if (vehicles_.size() >= MAX_VEHICLES) {
//                std::cout << "[警告] 车辆数量已达上限 " << MAX_VEHICLES << "，跳过添加车辆 " << id << std::endl;
//                return;
//            }
            std::cout << "[启动] 车辆ID: " << id << std::endl;
            vehicles_[id] = std::make_unique<VehicleClient>(id);
            vehicles_[id]->start();
            print_vehicle_ids();
        }
    }

    void stop_vehicle(int64_t id) {
        std::lock_guard<std::mutex> lock(clients_mutex);
        if (vehicles_.count(id)) {
            std::cout << "[停止] 车辆ID: " << id << std::endl;
            vehicles_[id]->stop();
            RedisUtils::Del(OrderCache::get_vehicle_key(id));
            vehicles_.erase(id);
            print_vehicle_ids();
        }
    }

    void stop_all() {
        // 第一阶段：停止所有客户端
        std::vector<int64_t> ids_to_clean;
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            // 1. 先收集所有需要清理的车辆ID
            for (auto& [id, client] : vehicles_) {
                ids_to_clean.push_back(id);
            }
            // 2. 停止所有客户端（不立即清理Redis）
            for (auto& [id, client] : vehicles_) {
                client->stop();
            }
            // 3. 清空容器但保留锁
            vehicles_.clear();
        }
        // 第二阶段：等待所有操作完成
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        // 第三阶段：安全清理Redis
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            for (auto id : ids_to_clean) {
                RedisUtils::Del(OrderCache::get_vehicle_key(id));
                std::cout << "[清理] 已清理车辆 " << id << " 的Redis缓存" << std::endl;
            }
        }
        std::cout << "[系统] 所有车辆已停止" << std::endl;
    }

private:
    std::unordered_map<int64_t, std::unique_ptr<VehicleClient>> vehicles_;

    void print_vehicle_ids() {
        std::cout << "当前运行车辆 (" << vehicles_.size() << "辆): [";
        bool first = true;
        for (const auto& [id, _] : vehicles_) {
            if (!first) std::cout << ", ";
            std::cout << id;
            first = false;
        }
        std::cout << "]" << std::endl;
    }
};

// 获取可用车辆ID
std::vector<int64_t> fetch_vehicle_ids(int limit) {
    try {
        auto result = SociDB::query<UVehicle>(
                "SELECT uv_id FROM xc_uv_vehicle "
                "WHERE is_delete = 0 AND uv_id > ? "
                "ORDER BY uv_id ASC LIMIT ?",
                last_loaded_id, limit
        );

        std::vector<int64_t> ids;
        for (const auto& v : result) {
            if (v.id) {
                ids.push_back(*v.id);
                last_loaded_id = *v.id;
            }
        }
        return ids;
    } catch (const std::exception& e) {
        std::cerr << "[错误] 查询失败: " << e.what() << std::endl;
        return {};
    }
}

// 持续添加车辆
void continuous_adding(VehicleManager& manager) {
    while (running) {
        auto ids = fetch_vehicle_ids(1);
        if (!ids.empty()) {
            manager.add_vehicle(ids[0]);
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int main() {
    try {
        // 初始化
        SociDB::init();
        RedisUtils::Initialize("127.0.0.1", 6379, "123456", 0, 100);

        VehicleManager manager;

        // 启动初始车辆
        auto initial_ids = fetch_vehicle_ids(1000);
        for (auto id : initial_ids) {
            manager.add_vehicle(id);
        }

        // 启动持续添加线程
        global_pool.detach_task([&manager]{
            std::this_thread::sleep_for(std::chrono::minutes(1));
            continuous_adding(manager);
        },LOW_PRIORITY);

        // 控制台命令处理
        global_pool.detach_task([]{
            std::cout << "\n输入车辆ID停止，按回车退出\n";
            std::string input;
            while (std::getline(std::cin, input)) {
                if (input.empty()) {
                    running = false;
                    break;
                }
                try {
                    int64_t id = std::stoll(input);
                    // 这里需要访问manager，实际使用时需要处理线程安全
                } catch (...) {
                    std::cerr << "无效输入" << std::endl;
                }
            }
        });

        // 主循环
        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // 清理
        manager.stop_all();

    } catch (const std::exception& e) {
        std::cerr << "[严重错误] " << e.what() << std::endl;
        running = false;
        return 1;
    }

    return 0;
}