#include "TaskScheduler.h"
#include <atomic>
#include <iomanip>
#include "../../nopeoplecar/OrderCreatorScheduler.h"
#include "../Logger/Logger.h"

std::atomic<bool> running(true);
std::atomic<int> total_orders_created{0};  // 添加这行声明

// 信号处理函数（用于Ctrl+C退出）
void signalHandler(int signum) {
    running = false;
}
int main() {
    SociDB::init("host=localhost dbname=pg_db user=root password=root options='-c timezone=UTC'");
    RedisUtils::Initialize();

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    std::cout << "=============================================\n";
    std::cout << "= 订单创建调度系统启动 - " << DateUtil::getCurrentTime() << " =\n";
    std::cout << "=============================================\n";

    AsyncCronScheduler scheduler(4); // 4个工作线程
    std::atomic<int> task_counter{0};



    // 添加多个并发任务（不同频率）

    // 1. 高频订单任务（每分钟创建5个日常百货订单）
    scheduler.add_task("*/1 * * * * *", [&] {
        std::vector<Order> orders;
        for (int i = 0; i < 5; ++i) {
            orders.push_back(createRandomOrder(NoCarOrderType::DAILY_GOODS));
        }
        batchCreateOrders(orders);
        total_orders_created += static_cast<int>(orders.size());
        Logger::Global().DebugFmt("日常百货订单%d", orders.size());
    });

    // 2. 餐饮订单任务（每5分钟创建3个餐饮订单）
    scheduler.add_task("*/5 * * * * *", [&] {
        std::vector<Order> orders;
        for (int i = 0; i < 3; ++i) {
            orders.push_back(createRandomOrder(NoCarOrderType::FOOD));
        }
        batchCreateOrders(orders);
        total_orders_created += static_cast<int>(orders.size());
        Logger::Global().DebugFmt("餐饮订单%d", orders.size());
    });

    // 3. 紧急医药订单（每小时创建10个医药订单）
    scheduler.add_task("*/2 * * * * *", [&] {
        std::vector<Order> orders;
        for (int i = 0; i < 10; ++i) {
            auto order = createRandomOrder(NoCarOrderType::MEDICINE);
            order.reward = order.reward.value() * 1.5; // 医药订单奖励提高50%
            orders.push_back(order);
        }
        batchCreateOrders(orders);
        total_orders_created += static_cast<int>(orders.size());
        Logger::Global().DebugFmt("紧急医药订单%d", orders.size());
    });

    // 4. 快递订单（每30分钟创建一批）
    scheduler.add_task("*/30 * * * *", [&] {
        std::vector<Order> orders;
        for (int i = 0; i < 8; ++i) {
            orders.push_back(createRandomOrder(NoCarOrderType::DELIVERY));
        }
        batchCreateOrders(orders);
        total_orders_created += static_cast<int>(orders.size());
        Logger::Global().DebugFmt("快递订单%d", orders.size());
    });

    // 5. 电子产品订单（每天上午9点和下午3点各一批）
    scheduler.add_task("*/20 * * * * *", [&] {
        std::vector<Order> orders;
        for (int i = 0; i < 6; ++i) {
            auto order = createRandomOrder(NoCarOrderType::ELECTRONICS);
            order.reward = order.reward.value() * 1.2; // 电子产品奖励提高20%
            orders.push_back(order);
        }
        batchCreateOrders(orders);
        total_orders_created += static_cast<int>(orders.size());
        Logger::Global().DebugFmt("电子产品订单%d", orders.size());
    });

    // 6. 冷冻食品订单（每小时15分创建）
    scheduler.add_task("*/15 * * * * *", [&] {
        std::vector<Order> orders;
        for (int i = 0; i < 4; ++i) {
            orders.push_back(createRandomOrder(NoCarOrderType::FROZEN_FOOD));
        }
        batchCreateOrders(orders);
        total_orders_created += static_cast<int>(orders.size());
        Logger::Global().DebugFmt("冷冻食品订单%d", orders.size());
    });

    // 7. 文件快递订单（工作日每小时30分创建）
    scheduler.add_task("*/3 * * * * *", [&] {
        std::vector<Order> orders;
        for (int i = 0; i < 5; ++i) {
            orders.push_back(createRandomOrder(NoCarOrderType::DOCUMENTS));
        }
        batchCreateOrders(orders);
        total_orders_created += static_cast<int>(orders.size());
        Logger::Global().DebugFmt("文件快递订单%d", orders.size());
    });

    // 8. 鲜花订单（每天上午8点和下午5点）
    scheduler.add_task("*/4 * * * * *", [&] {
        std::vector<Order> orders;
        for (int i = 0; i < 7; ++i) {
            orders.push_back(createRandomOrder(NoCarOrderType::FLOWERS));
        }
        batchCreateOrders(orders);
        total_orders_created += static_cast<int>(orders.size());
        Logger::Global().DebugFmt("鲜花订单%d", orders.size());
    });

    // 9. 服装订单（每周一上午10点大批量）
    scheduler.add_task("*/5 * * * * *", [&] {
        std::vector<Order> orders;
        for (int i = 0; i < 15; ++i) {
            orders.push_back(createRandomOrder(NoCarOrderType::CLOTHING));
        }
        batchCreateOrders(orders);
        total_orders_created += static_cast<int>(orders.size());
        Logger::Global().DebugFmt("服装订单%d", orders.size());
    });

    // 10. 图书订单（周末每小时创建）
    scheduler.add_task("*/6 * * * * *", [&] {
        std::vector<Order> orders;
        for (int i = 0; i < 3; ++i) {
            orders.push_back(createRandomOrder(NoCarOrderType::BOOKS));
        }
        batchCreateOrders(orders);
        total_orders_created += static_cast<int>(orders.size());
        Logger::Global().DebugFmt("图书订单%d", orders.size());
    });

    // 启动日志
    std::cout << "======== 订单创建调度系统启动 ========\n";
    std::cout << "已配置10种订单类型定时任务:\n";
    for (auto code : OrderTypeEnum.getAllCodes()) {
        auto params = OrderTypeEnum.getParams(code);
        std::cout << code << ": " << params[2] << "\n";
    }
    std::cout << "===================================\n";
    std::cout << "当前时间: " << time_utils::FormatTimestamp(time(nullptr)) << "\n";
    std::cout << "退出方式: Ctrl+C\n";
    std::cout << "===================================\n";

    // 启动调度器
    scheduler.start();

    // 主循环
    while(running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        // 每分钟打印一次状态
        static time_t last_print = 0;
        time_t now = time(nullptr);
        if (now - last_print >= 60) {
            last_print = now;
            std::cout << "[状态] 当前时间: " << time_utils::FormatTimestamp(now)
                      << " | 已创建订单总数: " << total_orders_created << "\n";
        }
    }

    // 优雅关闭
    std::cout << "\n正在停止调度器..." << std::endl;
    scheduler.stop();
    std::cout << "最终订单创建总数: " << total_orders_created << std::endl;
    std::cout << "系统已安全退出" << std::endl;

    return 0;
}