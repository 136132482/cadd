// UVehicleScheduler.cpp
#include "../../nopeoplecar/createRandomUVehicle.h"
#include "TaskScheduler.h"
#include <atomic>
#include <iomanip>
#include <iostream>

std::atomic<bool> running(true);
std::atomic<int> total_vehicles_created{0};

// 信号处理函数
void signalHandler(int signum) {
    running = false;
}

int main() {
    SociDB::init("host=localhost dbname=pg_db user=root password=root ");
    RedisUtils::Initialize();

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    std::cout << "=============================================\n";
    std::cout << "= 无人车调度系统启动 - " << DateUtil::getCurrentTime() << " =\n";
    std::cout << "=============================================\n";

    AsyncCronScheduler scheduler(4); // 4个工作线程

    // 1. 四轮车创建任务（每10分钟创建3辆）
    scheduler.add_task("*/10 * * * * *", [&] {
        std::vector<UVehicle> vehicles;
        for (int i = 0; i < 3; ++i) {
            auto vehicle = createRandomUVehicle();
            vehicles.push_back(vehicle);
        }
        batchCreateUVehicles(vehicles);
        total_vehicles_created += static_cast<int>(vehicles.size());
        Logger::Global().DebugFmt("创建运行车辆%d辆", vehicles.size());
    });

    // 2. 无人机创建任务（每小时创建2架）
    scheduler.add_task("*/5 * * * * *", [&] {
        std::vector<UVehicle> vehicles;
        for (int i = 0; i < 2; ++i) {
            auto vehicle = createRandomUVehicle();
            vehicles.push_back(vehicle);
        }
        batchCreateUVehicles(vehicles);
        total_vehicles_created += static_cast<int>(vehicles.size());
        Logger::Global().DebugFmt("创建无人机%d架", vehicles.size());
    });

    // 3. 机器人创建任务（每天创建5台）
    scheduler.add_task("*/3 * * * * *", [&] {
        std::vector<UVehicle> vehicles;
        for (int i = 0; i < 5; ++i) {
            auto vehicle = createRandomUVehicle();
            vehicles.push_back(vehicle);
        }
        batchCreateUVehicles(vehicles);
        total_vehicles_created += static_cast<int>(vehicles.size());
        Logger::Global().DebugFmt("创建机器人%d台", vehicles.size());
    });

    // 4. 维护任务（每分钟检查心跳）
//    scheduler.add_task("*/1 * * * * *", [&] {
//        try {
//            soci::session sql;
//            auto threshold = std::chrono::system_clock::now() - std::chrono::minutes(5);
//
//            // 使用soci直接绑定参数
//            std::vector<UVehicle> inactive_vehicles;
//            inactive_vehicles= SociDB::query<UVehicle>("SELECT * FROM xc_uv_vehicle WHERE heartbeat_time < ? AND status != ?",threshold,2);
//
//            for (auto& vehicle : inactive_vehicles) {
//                vehicle.status = 2; // 设置为维护状态
//
//                // 使用命名参数绑定
//                sql << "UPDATE xc_uv_vehicle SET status = 2 WHERE uv_id = :id",
//                        soci::use(vehicle.id.value(), "id");
//
//                // 安全处理可能为空的uv_code
//                std::string vehicle_code = vehicle.uv_code.value_or("未知");
//                Logger::Global().WarningFmt("无人车%s进入维护状态", vehicle_code.c_str());
//            }
//        } catch (const std::exception& e) {
//            Logger::Global().ErrorFmt("检查无人车状态失败: %s", e.what());
//        }
//    });

    // 启动日志
    std::cout << "======== 无人车调度系统配置 ========\n";
    std::cout << "1. 四轮车: 每10分钟3辆\n";
    std::cout << "2. 无人机: 每小时2架\n";
    std::cout << "3. 机器人: 每天5台\n";
    std::cout << "4. 维护任务: 每分钟检查心跳\n";
    std::cout << "===================================\n";

    // 启动调度器
    scheduler.start();

    // 主循环
    while(running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // 每分钟打印状态
        static time_t last_print = 0;
        time_t now = time(nullptr);
        if (now - last_print >= 60) {
            last_print = now;
            std::cout << "[状态] " << DateUtil::getCurrentTime()
                      << " | 已创建总数: " << total_vehicles_created << "\n";
        }
    }

    // 优雅关闭
    std::cout << "\n正在停止调度器..." << std::endl;
    scheduler.stop();
    std::cout << "最终创建无人车总数: " << total_vehicles_created << std::endl;
    std::cout << "系统已安全退出" << std::endl;

    return 0;
}