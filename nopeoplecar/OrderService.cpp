#include "../util/redis/RedisPool.h"
#include "../util/task/TaskScheduler.h"
// 抢单服务核心逻辑
class OrderService {
public:
    void handle_grab_order(const GrabOrder& order) {
        // 1. 获取Redis分布式锁
        RedisUtils::Lock lock("order:" + std::to_string(order.id()));
        if (!lock.try_lock()) return;

        // 2. 使用无锁队列缓冲
        order_queue_.push(order);

        // 3. 异步处理
        TaskScheduler::instance().schedule_after(
                "process_" + std::to_string(order.id()),
                10ms,
                [this] { process_orders(); }
        );
    }

private:
    void process_orders() {
        GrabOrder order;
        while (order_queue_.pop(order)) {
            // 4. 发布到ZeroMQ
            ZmqPubSub::instance().publish("orders", ProtoCodec::encode(order));
            Metrics::inc("orders.processed");
        }
    }
    LockFreeQueue<GrabOrder> order_queue_;
};