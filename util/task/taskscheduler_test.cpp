#include "TaskScheduler.h"
#include <atomic>
#include <iomanip>
std::atomic<bool> running(true);

// 信号处理函数（用于Ctrl+C退出）
void signalHandler(int signum) {
    running = false;
}
int main() {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    AsyncCronScheduler scheduler(4); // 4个工作线程
    std::atomic<int> task_counter{0};

    // 添加多个并发任务（不同频率）

    // 1. 高频任务（每秒执行）
    scheduler.add_task("* * * * * *", [&] {
        auto count = ++task_counter;
        auto now = std::chrono::system_clock::now();
        auto now_c = std::chrono::system_clock::to_time_t(now);
        std::cout << "任务1(1秒) - 计数: " << count
                  << " - 线程: " << std::this_thread::get_id()
                  << " - 时间: " << time_utils::FormatTimestamp(now_c) << std::endl;
    });

    // 2. 中频任务（每2秒执行，模拟耗时操作）
    scheduler.add_task("*/2 * * * * *", [&] {
        auto count = ++task_counter;
        auto now = std::chrono::system_clock::now();
        auto now_c = std::chrono::system_clock::to_time_t(now);
        std::cout << "任务2(2秒) - 计数: " << count
                  << " - 线程: " << std::this_thread::get_id()
                << " - 时间: " << time_utils::FormatTimestamp(now_c) << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(800)); // 模拟耗时操作
        now = std::chrono::system_clock::now();
        now_c = std::chrono::system_clock::to_time_t(now);
        std::cout << " - 结束: " <<    time_utils::FormatTimestamp(now_c) <<" - 时间: "<< std::endl;
    });

    // 3. 低频任务（每5秒执行）
    scheduler.add_task("*/5 * * * * *", [&] {
        auto count = ++task_counter;
        auto now = std::chrono::system_clock::now();
        auto now_c = std::chrono::system_clock::to_time_t(now);
        std::cout << "任务3(5秒) - 计数: " << count
                  << " - 线程: " << std::this_thread::get_id()
                << " - 时间: " << time_utils::FormatTimestamp(now_c) << std::endl;
    });


    // 2.1 固定星期任务（每周三和周五的10:30:00执行）
    scheduler.add_task("0 30 10 * * 3,5", [&] {
        auto count = ++task_counter;
        auto now = std::chrono::system_clock::now();
        auto now_c = std::chrono::system_clock::to_time_t(now);
        std::cout << "[星期] 每周三/五10:30执行 - 计数: " << count
                  << " - 线程: " << std::this_thread::get_id()
                  << " - 时间: " << time_utils::FormatTimestamp(now_c) << std::endl;
    });

    // 2.2 时间范围任务（每天9点到18点每小时的第15分钟执行）
    scheduler.add_task("0 15 9-18 * * *", [&] {
        auto count = ++task_counter;
        auto now = std::chrono::system_clock::now();
        auto now_c = std::chrono::system_clock::to_time_t(now);
        std::cout << "[范围] 工作日每小时15分执行 - 计数: " << count
                  << " - 线程: " << std::this_thread::get_id()
                  << " - 时间: " << time_utils::FormatTimestamp(now_c) << std::endl;
    });

    // 2.3 复合规则任务（工作日上午每半小时执行）
    scheduler.add_task("0 0,30 9-12 * * 1-5", [&] {
        auto count = ++task_counter;
        auto now = std::chrono::system_clock::now();
        auto now_c = std::chrono::system_clock::to_time_t(now);
        std::cout << "[复合] 工作日上午半小时任务 - 计数: " << count
                  << " - 线程: " << std::this_thread::get_id()
                  << " - 时间: " << time_utils::FormatTimestamp(now_c) << std::endl;
    });

    // 2.4 精确时间点任务（每天14:25:30执行）
    scheduler.add_task("30 25 14 * * *", [&] {
        auto count = ++task_counter;
        auto now = std::chrono::system_clock::now();
        auto now_c = std::chrono::system_clock::to_time_t(now);
        std::cout << "[精确] 每天14:25:30执行 - 计数: " << count
                  << " - 线程: " << std::this_thread::get_id()
                  << " - 时间: " << time_utils::FormatTimestamp(now_c) << std::endl;
    });

    // 2.5 月末任务（每月最后一天23:59:00执行）
    scheduler.add_task("0 59 23 28-31 * *", [&] {
        auto count = ++task_counter;
        auto now =  std::chrono::system_clock::now();
        auto now_c = std::chrono::system_clock::to_time_t(now);
        std::cout << "[月末] 每月最后一天23:59执行 - 计数: " << count
                  << " - 线程: " << std::this_thread::get_id()
                  << " - 时间: " << time_utils::FormatTimestamp(now_c) << std::endl;
        // 检查是否是当月最后一天
        if(DateUtil::compaCurr(now)) {
            std::cout << "[月末] 触发月末任务 - 时间: "
                      << time_utils::FormatTimestamp(now_c) << std::endl;
        }
    });

    // 2.6 季度任务（每季度第一天9:00:00执行）
    scheduler.add_task("0 0 9 1 3,6,9,12 *", [&] {
        auto count = ++task_counter;
        auto now = std::chrono::system_clock::now();
        auto now_c = std::chrono::system_clock::to_time_t(now);
        std::cout << "[季度] 每季度第一天9:00执行 - 计数: " << count
                  << " - 线程: " << std::this_thread::get_id()
                  << " - 时间: " << time_utils::FormatTimestamp(now_c) << std::endl;
    });


    std::cout << "======== 测试开始 ========\n";
    auto start_time = std::chrono::system_clock::now();
    auto start_c = std::chrono::system_clock::to_time_t(start_time);
    std::cout << "开始时间: " << time_utils::FormatTimestamp(start_c) << std::endl;
    std::cout << "======== 定时任务系统 ========\n"
              << "已加载任务:\n"
              << "1. * * * * * *       每秒任务\n"
              << "2. 0 30 10 * * 3,5   每周三/五10:30\n"
              << "3. 0 15 9-18 * * *   每天9-18点每小时15分\n"
              << "4. 0 0,30 9-12 * * 1-5 工作日上午每半小时\n"
              << "5. 0 59 23 28-31 * * 月末检查任务\n"
              << "===========================\n"
              << "当前时间: " << time_utils::FormatTimestamp(time(nullptr)) << "\n"
              << "退出方式: Ctrl+C\n"
              << "===========================\n";


    scheduler.start();

    // 主循环（保持长连接）
    while(running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // 每10秒打印一次心跳（可选）
        static int heartbeat = 0;
        if(++heartbeat % 10 == 0) {
            std::cout << "[心跳] 系统运行中... 当前时间: "
                      << time_utils::FormatTimestamp(time(nullptr)) << std::endl;
        }
    }

    // 优雅关闭
    std::cout << "\n正在停止调度器..." << std::endl;
    scheduler.stop();
    std::cout << "任务总执行次数: " << task_counter << std::endl;
    std::cout << "系统已安全退出" << std::endl;

    return 0;
}