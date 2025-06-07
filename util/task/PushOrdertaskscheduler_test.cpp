#include "TaskScheduler.h"
#include "../../nopeoplecar/snatchorders/OrderDispatcher.h"
#include <atomic>
#include <iomanip>
#include <csignal>

std::atomic<bool> running(true);
std::atomic<int> total_orders_created{0};



static std::atomic<int> current_page(1);
constexpr int PAGE_SIZE = 100;

// 信号处理函数
void signalHandler(int signum) {
    running = false;
}


int main() {
    // 初始化组件
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    SociDB::init("host=localhost dbname=pg_db user=root password=root");
    RedisUtils::Initialize();

    std::cout << "=============================================\n"
              << "= 订单推送系统启动 - " << DateUtil::getCurrentTime() << " =\n"
              << "=============================================\n";
    // 定时任务调度器（4个工作线程）
    AsyncCronScheduler scheduler(8);
    // 任务1：高频订单生成（每分钟）
    scheduler.add_task("*/10 * * * * *", [&] {
        // 1. 同步获取数据（主线程）
        auto pageResult = fetchPendingOrders<Order>(current_page.load(), PAGE_SIZE);
        if (pageResult.items.empty()) return;
        // 2. 提前计算下一页（避免线程竞争）
        const int next_page = (current_page >= pageResult.total_pages) ? 1 : current_page + 1;
        // 3. 启动线程（完全值捕获，不依赖任何外部变量）
        std::thread([items = std::move(pageResult.items)]() {
            try {
                publish_orders_with_stats(items);
            } catch (const std::exception& e) {
                std::cerr << "发布线程异常: " << e.what() << std::endl;
            }
        }).detach();

        // 4. 主线程安全更新状态
        total_orders_created += pageResult.items.size();
        current_page.store(next_page);
    });

    // 任务2：数据库同步（每5分钟）
//    scheduler.add_task("*/5 * * * * *", [&] {
//        std::cout << "[同步] 执行数据库同步检查...\n";
//        // 添加实际的同步逻辑
//    });

    // 任务3：状态报告（每分钟）
//    scheduler.add_task("0 * * * * *", [&] {
//        std::cout << "[状态] 已创建:" << total_orders_created.load()
//                  << " 已推送:" << total_orders_published.load() << "\n";
//    });




    // 启动日志
    std::cout << "已配置定时任务:\n"
              << "1. 订单抓取推送 (每分钟)\n"
              << "2. 数据库同步 (每5分钟)\n"
              << "3. 状态报告 (每分钟)\n"
              << "===================================\n";

    // 启动调度器
    scheduler.start();

    // 主循环
    while(running) {
        std::this_thread::sleep_for(1s);
    }

    // 优雅关闭
    std::cout << "\n正在停止系统...\n";
    scheduler.stop();

    std::cout << "最终统计:\n"
              << "总创建订单: " << total_orders_created << "\n"
              << "总推送订单: " << total_orders_published << "\n"
              << "系统已安全退出\n";
    return 0;
}