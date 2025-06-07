#include "OrderSubscriber.h"
#include <atomic>
#include <thread>
#include <iostream>

std::atomic<bool> running{true};

void input_listener() {
    std::cout << "按回车键停止程序...";
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::string line;
    std::getline(std::cin, line); // 直接等待回车
    running = false;
}

int main() {
    std::thread input_thread(input_listener);

    try {
        // 初始化组件
        SociDB::init();
        RedisUtils::Initialize("127.0.0.1", 6379, "123456", 0, 10);

        // 直接实例化客户端（不使用vector）
        VehicleClient client30(30);
        VehicleClient client32(32);
        VehicleClient client34(34);
        VehicleClient client44(44);

        // 直接启动
        client30.start();
        client32.start();
        client34.start();
        client44.start();

        // 主循环
        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // 直接停止
        client44.stop();
        client34.stop();
        client32.stop();
        client30.stop();

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        RedisUtils::Del(OrderCache::get_vehicle_key(30));
        RedisUtils::Del(OrderCache::get_vehicle_key(32));
        RedisUtils::Del(OrderCache::get_vehicle_key(34));
        RedisUtils::Del(OrderCache::get_vehicle_key(44));


    } catch (const std::exception& e) {
        running = false;
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }

    if (input_thread.joinable()) {
        input_thread.join();
    }
    return 0;
}