#include "TaskScheduler.h"
#include "../../nopeoplecar/snatchorders/Orderdeadletter.h"
#include <atomic>
#include <iomanip>
#include <csignal>

std::atomic<bool> running(true);

// 信号处理函数
void signalHandler(int signum) {
    running = false;
}

int main() {
    // 初始化组件
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    RedisUtils::Initialize();

    // 配置监听端口（修复：声明并初始化zmq_endpoints）
    std::vector<std::string> zmq_endpoints = {
            "tcp://192.168.1.100:5555",
            "tcp://192.168.1.101:5556",
            "tcp://192.168.1.102:5557"
    };

    zmq::context_t ctx(1);
    ZmqCleaner cleaner(ctx, zmq_endpoints);

    // 定时任务配置
    AsyncCronScheduler scheduler(2); // 2个线程足够

    // 每5分钟清理过期消息
    scheduler.add_task("* */5 * * * *", [&] {
        auto cleaned = cleaner.cleanup_expired();
        std::cout << "[" << DateUtil::getCurrentTime()
                  << "] 清理完成 数量:" << cleaned << "\n";
    });

    // 死信处理（改为更频繁的执行，例如每小时）
    scheduler.add_task("* 0 * * * *", [&] {
        cleaner.process_deadletters();
    });

    // 每日凌晨维护
    scheduler.add_task("0 0 0 * * *", [&] {
        cleaner.maintain_deadletters();
    });

    // 状态报告（每分钟）
    scheduler.add_task("* 0 * * * *", [&] {
        std::cout << "[" << DateUtil::getCurrentTime()
                  << "] 系统状态 | 死信数量:" << stats.deadletter_size.load()
                  << " 已处理:" << stats.processed_count.load() << "\n";
    });

    // 每月25号23:50创建下个月分区（提前几天创建）
    scheduler.add_task("0 50 23 25 * *", [&] {
        SociDB::create_next_month_partition(
                "xc_uv_grab_log",
                "created_at",
                "抢单日志分区"
        );
    });

    // 每天凌晨检查未来3个月的分区（冗余保障）
    scheduler.add_task("0 30 0 * * *", [&] {
        SociDB::ensure_future_partitions(
                "xc_uv_grab_log",
                3,  // 预创建3个月
                "created_at",
                "抢单日志分区"
        );
    });

    // 每周日凌晨检查分区健康状态
    scheduler.add_task("0 0 3 * * 0", [&] {
        auto missing = SociDB::check_partition_health("xc_uv_grab_log");
        if (!missing.empty()) {
            Logger::Global().Error("发现缺失分区: " + boost::algorithm::join(missing, ", "));
            SociDB::repair_missing_partitions("xc_uv_grab_log", missing);

            // 发送警报
//            RedisUtils::Publish("alerts",
//                                "检测到缺失分区: " + boost::algorithm::join(missing, ", "));
        }
    });

    // 启动日志（修复：使用正确的统计字段）
    std::cout << "=============================================\n"
              << "= 消息处理系统启动 - " << DateUtil::getCurrentTime() << " =\n"
              << "=============================================\n"
              << "已配置定时任务:\n"
              << "1. 过期消息清理 (每5分钟)\n"
              << "2. 死信处理 (每小时)\n"
              << "3. 死信维护 (每日0点)\n"
              << "4. 状态报告 (每分钟)\n"
              << "监听端口:" << zmq_endpoints.size() << "个\n"
              << "===================================\n";

    // 启动调度器
    scheduler.start();

    // 主循环
    while(running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // 优雅关闭
    std::cout << "\n[" << DateUtil::getCurrentTime() << "] 正在停止系统...\n";
    scheduler.stop();

    // 最终统计（修复：使用正确的统计字段）
    std::cout << "[" << DateUtil::getCurrentTime() << "] 最终统计:\n"
              << "处理消息: " << stats.processed_count.load() << "\n"
              << "死信数量: " << stats.deadletter_size.load() << "\n"
              << "系统已安全退出\n";
    return 0;
}