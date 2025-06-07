// subscriber_main.cpp
#include "ZmqInstance.h"
#include <iostream>
#include <thread>
#include <chrono>
#include "ZmqManager.h"

void message_handler(const std::string& topic,
                     const std::string& body,
                     const std::unordered_map<std::string, std::string>& headers) {
    std::cout << "\n[SUB] 收到发布者的数据:\n"
              << "主题: " << topic << "\n"
              << "内容: " << body << "\n"
              << "头信息: ";
    for (const auto& [k, v] : headers) {
        std::cout << k << "=" << v << "; ";
    }
    std::cout << "\n";
}

void message_handler1(const std::string& topic,
                     const std::string& body,
                     const std::unordered_map<std::string, std::string>& headers) {
    std::cout << "\n[SUB] xxxxxxxxxxxxxxxxxxxxx自发自用 :\n"
              << "主题: " << topic << "\n"
              << "内容: " << body << "\n"
              << "头信息: ";
    for (const auto& [k, v] : headers) {
        std::cout << k << "=" << v << "; ";
    }
    std::cout << "\n";
}


int sub_test() {
    // 订阅者连接发布者的5555端口
    ZmqIn subscriber(false, "tcp://localhost:5555");

    // 发布者绑定5556端口（用于发送响应）
//    ZmqIn publisher(true, "tcp://*:5556");

//    ZmqIn subscriber1(false, "tcp://localhost:5556");

    subscriber.start();
//    publisher.start();
//    subscriber1.start();

    // 订阅发布者的数据
    subscriber.subscribe({"data"}, message_handler);
    subscriber.subscribe_headers({{"priority", "high,medium"}, {"type", "alert"}}, message_handler,"alerts");
//    subscriber.subscribe({"sensor.temperature"}, message_handler, ZmqIn::ExchangeType::DIRECT);

    // 2. TOPIC模式订阅（通配符匹配）
//    subscriber.subscribe({"room"}, message_handler, ZmqIn::ExchangeType::TOPIC);

    // 3. FANOUT模式订阅（接收所有消息）
//    subscriber.subscribe({"system"}, message_handler, ZmqIn::ExchangeType::FANOUT);
//    subscriber1.subscribe({"response"}, message_handler1);

    int response_count = 0;
    while (true) {
        // 模拟处理数据后发送响应
        std::string response = "响应#" + std::to_string(++response_count);
//        publisher.publish("response", response);
        std::cout << "[SUB] 已发送响应: " << response << std::endl;

        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

//    publisher.stop();
    subscriber.stop();
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

void message_callback(const std::string& topic,
                     const std::string& body,
                     const std::unordered_map<std::string, std::string>& headers) {
    std::cout << COLOR_GREEN << "\n=== 收到消息 ===" << COLOR_RESET
              << "\n主题: " << COLOR_YELLOW << topic << COLOR_RESET
              << "\n内容: " << COLOR_BLUE << body << COLOR_RESET
              << "\n头信息: ";

    for (const auto& [k, v] : headers) {
        std::cout << k << "=" << v << "; ";
    }
    std::cout << "\n================\n";
}

void run_subscriber() {
    std::cout << COLOR_BLUE << "[Subscriber] 启动订阅者..." << COLOR_RESET << std::endl;

    // 1. 初始化所有订阅者
    auto& sub1 = ZmqGlobal::InstanceManager::getSubscriber("tcp://localhost:5555"); // DIRECT
    auto& sub2 = ZmqGlobal::InstanceManager::getSubscriber("tcp://localhost:5556"); // TOPIC
    auto& sub3 = ZmqGlobal::InstanceManager::getSubscriber("tcp://localhost:5557"); // FANOUT
    auto& sub4 = ZmqGlobal::InstanceManager::getSubscriber("tcp://localhost:5558"); // HEADERS

    // 2. 设置订阅
    sub1.subscribe({"sensor.temp"}, message_callback, ZmqIn::ExchangeType::DIRECT);
    sub2.subscribe({"room"}, message_callback, ZmqIn::ExchangeType::TOPIC);
    sub3.subscribe({"system"}, message_callback, ZmqIn::ExchangeType::FANOUT);
    sub4.subscribe_headers({{"priority", "high,medium"}, {"type", "alert"}},
                           message_callback, "alerts");

    std::cout << COLOR_YELLOW << "所有订阅已设置，等待消息..." << COLOR_RESET << std::endl;

    // 3. 保持运行
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int main() {
    try {
//        run_subscriber();
        sub_test();
    } catch (const std::exception& e) {
        std::cerr << COLOR_RED << "订阅者崩溃: " << e.what() << COLOR_RESET << std::endl;
        return 1;
    }
    return 0;
}