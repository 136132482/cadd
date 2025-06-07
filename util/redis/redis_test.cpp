#include <iostream>
#include "RedisPool.h"

int main() {
    // 1. 初始化连接池（只需调用一次）
    RedisUtils::Initialize("127.0.0.1", 6379, "123456", 2,10);

    // 2. 字符串操作
    RedisUtils::SetString("config:timeout", "3000");
    std::cout << "Timeout: " << RedisUtils::GetString("config:timeout") << std::endl;

    // 3. 哈希表操作
    RedisUtils::HSet("user:1001", "name", "Alice");
    RedisUtils::HSet("user:1001", "age", "30");
    auto user = RedisUtils::HGetAll("user:1001");
    for (auto& [k, v] : user) {
        std::cout << k << ": " << v << std::endl;
    }

    // 4. 列表操作（消息队列）
    RedisUtils::RPush("msg_queue", "task1");
    RedisUtils::RPush("msg_queue", "task2");
    auto task = RedisUtils::LPop("msg_queue");
    std::cout << "Processing: " << task << std::endl;

    auto tasks = RedisUtils::LRange("msg_queue", 0, -1);
    for (auto& t : tasks) {
        std::cout << "Remaining task: " << t << std::endl;
    }

    // 5. 分布式锁
    {
        RedisUtils::Lock lock("order_lock", 3000);
        if (lock.TryLock()) {
            std::cout << "Do atomic operation..." << std::endl;
            // 锁会在作用域结束时自动释放
        }
    }

    // 6. 原子计数器
    auto new_id = RedisUtils::AtomicIncr("global_id");
    std::cout << "New ID: " << new_id << std::endl;

    // 7. 其他操作示例
    if (RedisUtils::KeyExists("user:1001")) {
        std::cout << "Key user:1001 exists" << std::endl;
    }

    auto ttl = RedisUtils::GetTTL("config:timeout");
    std::cout << "Key TTL: " << ttl << " ms" << std::endl;

    return 0;
}