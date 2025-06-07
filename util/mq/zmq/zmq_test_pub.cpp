// publisher_main.cpp
#include "ZmqInstance.h"
#include <iostream>
#include <thread>
#include <chrono>
#include "ZmqManager.h"

void message_handler(const std::string& topic,
                     const std::string& body,
                     const std::unordered_map<std::string, std::string>& headers) {
    std::cout << "\n[PUB] 收到订阅者的响应:\n"
              << "主题: " << topic << "\n"
              << "内容: " << body << "\n"
              << "头信息: ";
    for (const auto& [k, v] : headers) {
        std::cout << k << "=" << v << "; ";
    }
    std::cout << "\n";
}

int publish_test() {
    // 发布者绑定5555端口（发布数据）
    ZmqIn publisher(true, "tcp://*:5555");

    // 订阅者连接5556端口（接收响应）
//    ZmqIn subscriber(false, "tcp://localhost:5556");

    publisher.start();
//    subscriber.start();

    // 订阅来自订阅者的响应
//    subscriber.subscribe({"response"}, message_handler);

    int count = 0;
    while (true) {
        std::string msg = "数据包#" + std::to_string(++count);

        // 发布数据到订阅者
        publisher.publish("data", msg);

        publisher.publish("alerts", "CPU过载!", ZmqIn::ExchangeType::HEADERS,
                       {{"priority", "high"}, {"type", "alert"}, {"source", "server1"}});
        publisher.publish("alerts", "内存警告", ZmqIn::ExchangeType::HEADERS,
                       {{"priority", "medium"}, {"type", "alert"}});

//        publisher.publish("sensor.temperature", "25.6℃", ZmqIn::ExchangeType::DIRECT);
//        std::cout << "已发布DIRECT消息\n";

        // 2. TOPIC消息（通配符匹配）
//        publisher.publish("room.101", "22.5℃", ZmqIn::ExchangeType::TOPIC, {}, "room");
//        publisher.publish("room.102", "23.0℃", ZmqIn::ExchangeType::TOPIC, {}, "room");
//        std::cout << "已发布2条TOPIC消息\n";

        // 3. FANOUT消息（广播到所有订阅者）
//        publisher.publish("system", "维护通知", ZmqIn::ExchangeType::FANOUT);
        std::cout << "[PUB] 已发布: " << msg << std::endl;

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

//    subscriber.stop();
    publisher.stop();
    return 0;
}


#include "ZmqInstance.h"
#include <iostream>
#include <chrono>
#include <thread>

// 彩色输出宏
#define COLOR_RED    "\033[31m"
#define COLOR_GREEN  "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_BLUE   "\033[34m"
#define COLOR_RESET  "\033[0m"

void run_publisher() {
    std::cout << COLOR_BLUE << "[Publisher] 启动发布者..." << COLOR_RESET << std::endl;

    // 初始化所有发布者
    auto& pub1 = ZmqGlobal::InstanceManager::getPublisher("tcp://*:5555"); // DIRECT
    auto& pub2 = ZmqGlobal::InstanceManager::getPublisher("tcp://*:5556"); // TOPIC
    auto& pub3 = ZmqGlobal::InstanceManager::getPublisher("tcp://*:5557"); // FANOUT
    auto& pub4 = ZmqGlobal::InstanceManager::getPublisher("tcp://*:5558"); // HEADERS

    int count = 0;
    while (true) {
        std::cout << "\n" << COLOR_YELLOW << "=== 发布循环 " << ++count << " ===" << COLOR_RESET << std::endl;

        // 1. DIRECT消息
        pub1.publish("sensor.temp", "温度: " + std::to_string(20 + (count % 10)) + "℃",
                     ZmqIn::ExchangeType::DIRECT);
        std::cout << COLOR_GREEN << "[DIRECT] 已发布温度数据" << COLOR_RESET << std::endl;

        // 2. TOPIC消息
        pub2.publish("room.101", "湿度: " + std::to_string(40 + (count % 20)) + "%",
                     ZmqIn::ExchangeType::TOPIC, {}, "room");
        std::cout << COLOR_GREEN << "[TOPIC] 已发布房间数据" << COLOR_RESET << std::endl;

        // 3. FANOUT消息
        pub3.publish("system", "系统状态: " + std::to_string(count),
                     ZmqIn::ExchangeType::FANOUT);
        std::cout << COLOR_GREEN << "[FANOUT] 已发布系统广播" << COLOR_RESET << std::endl;

        // 4. HEADERS消息
        pub4.publish("alerts", (count % 2 == 0) ? "CPU告警" : "内存警告",
                     ZmqIn::ExchangeType::HEADERS,
                     {{"priority", (count % 2 == 0) ? "high" : "medium"},
                      {"type", "alert"}});
        std::cout << COLOR_GREEN << "[HEADERS] 已发布告警" << COLOR_RESET << std::endl;

        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
}

int main() {
    try {
//        run_publisher();
        publish_test();
    } catch (const std::exception& e) {
        std::cerr << COLOR_RED << "发布者崩溃: " << e.what() << COLOR_RESET << std::endl;
        return 1;
    }
    return 0;
}