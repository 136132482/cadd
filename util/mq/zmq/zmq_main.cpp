#include "ZmqInstance.h"
#include <iostream>
#include <thread>
#include <chrono>
#include "ZmqManager.h"

// 消息回调函数
void message_handler(const std::string& topic,
                     const std::string& body,
                     const std::unordered_map<std::string, std::string>& headers)
{
    std::cout << "\n=== 收到消息 ==="
              << "\n主题: " << topic
              << "\n内容: " << body
              << "\n头信息: ";

    for (const auto& [k, v] : headers) {
        std::cout << k << "=" << v << "; ";
    }
    std::cout << "\n================\n";
}

int many_zmq() {
    // 1. 创建服务端实例（绑定端口）
    ZmqIn server(true, "tcp://*:5556");

    // 2. 创建客户端实例（连接服务端）
    ZmqIn client(false, "tcp://localhost:5556");

    // 3. 启动实例
    server.start();
    client.start();

    // 4. 设置订阅（等待1秒确保连接建立）
    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::cout << "设置订阅...\n";

    // 1. DIRECT模式订阅（精确匹配）
    client.subscribe({"sensor.temperature"}, message_handler, ZmqIn::ExchangeType::DIRECT);

    // 2. TOPIC模式订阅（通配符匹配）
    client.subscribe({"room"}, message_handler, ZmqIn::ExchangeType::TOPIC);

    // 3. FANOUT模式订阅（接收所有消息）
    client.subscribe({"system"}, message_handler, ZmqIn::ExchangeType::FANOUT);

    // 4. HEADERS模式订阅（匹配特定头信息）
    client.subscribe_headers({{"priority", "high,medium"}, {"type", "alert"}}, message_handler,"alerts");

    // 5. 发布各种类型的测试消息
    std::cout << "\n发布测试消息...\n";

    // 1. DIRECT消息（精确匹配）
    server.publish("sensor.temperature", "25.6℃", ZmqIn::ExchangeType::DIRECT);
    std::cout << "已发布DIRECT消息\n";

    // 2. TOPIC消息（通配符匹配）
    server.publish("room.101", "22.5℃", ZmqIn::ExchangeType::TOPIC, {}, "room");
    server.publish("room.102", "23.0℃", ZmqIn::ExchangeType::TOPIC, {}, "room");
    std::cout << "已发布2条TOPIC消息\n";

    // 3. FANOUT消息（广播到所有订阅者）
    server.publish("system", "维护通知", ZmqIn::ExchangeType::FANOUT);
    std::cout << "已发布FANOUT消息\n";

    // 4. HEADERS消息（匹配头信息）
    server.publish("alerts", "CPU过载!", ZmqIn::ExchangeType::HEADERS,
                   {{"priority", "high"}, {"type", "alert"}, {"source", "server1"}});
    server.publish("alerts", "内存警告", ZmqIn::ExchangeType::HEADERS,
                   {{"priority", "medium"}, {"type", "alert"}});
    std::cout << "已发布2条HEADERS消息\n";

    // 6. 等待消息处理
    std::cout << "\n等待5秒接收消息...\n";
    std::this_thread::sleep_for(std::chrono::seconds(10));

    // 7. 清理
    client.stop();
    server.stop();

    std::cout << "测试完成\n";
    return 0;
}



// 彩色输出宏
#define COLOR_RED    "\033[31m"
#define COLOR_GREEN  "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_BLUE   "\033[34m"
#define COLOR_RESET  "\033[0m"

// 全局消息处理器
void message_callback(const std::string& topic,
                     const std::string& body,
                     const std::unordered_map<std::string, std::string>& headers)
{
    std::cout << COLOR_GREEN << "\n=== 收到消息 ===" << COLOR_RESET
              << "\n主题: " << COLOR_YELLOW << topic << COLOR_RESET
              << "\n内容: " << COLOR_BLUE << body << COLOR_RESET
              << "\n头信息: ";

    for (const auto& [k, v] : headers) {
        std::cout << k << "=" << v << "; ";
    }
    std::cout << "\n================\n";
}

// 测试DIRECT模式
void test_direct_mode() {
    auto& pub = ZmqGlobal::InstanceManager::getPublisher("tcp://*:5555");
    auto& sub = ZmqGlobal::InstanceManager::getSubscriber("tcp://localhost:5555");
    sub.subscribe({"sensor.temp"}, message_callback, ZmqIn::ExchangeType::DIRECT);
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 等待连接建立
    pub.publish("sensor.temp", "25.6℃", ZmqIn::ExchangeType::DIRECT);
    std::cout << COLOR_RED << "[DIRECT] 已发送温度数据\n" << COLOR_RESET;
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

// 测试TOPIC模式
void test_topic_mode() {
    auto& pub = ZmqGlobal::InstanceManager::getPublisher("tcp://*:5556");
    auto& sub = ZmqGlobal::InstanceManager::getSubscriber("tcp://localhost:5556");

    sub.subscribe({"room"}, message_callback, ZmqIn::ExchangeType::TOPIC);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    pub.publish("room.101", "22.5℃", ZmqIn::ExchangeType::TOPIC, {}, "room");
    pub.publish("room.102", "23.0℃", ZmqIn::ExchangeType::TOPIC, {}, "room");

    std::cout << COLOR_RED << "[TOPIC] 已发送2个房间温度\n" << COLOR_RESET;
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

// 测试FANOUT模式
void test_fanout_mode() {
    auto& pub = ZmqGlobal::InstanceManager::getPublisher("tcp://*:5557");
    auto& sub1 = ZmqGlobal::InstanceManager::getSubscriber("tcp://localhost:5557");
    auto& sub2 = ZmqGlobal::InstanceManager::getSubscriber("tcp://localhost:5557");

    sub1.subscribe({"system"}, message_callback, ZmqIn::ExchangeType::FANOUT);
    sub2.subscribe({"system"}, message_callback, ZmqIn::ExchangeType::FANOUT);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    pub.publish("system", "系统即将维护重启", ZmqIn::ExchangeType::FANOUT);

    std::cout << COLOR_RED << "[FANOUT] 已广播系统消息\n" << COLOR_RESET;
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

// 测试HEADERS模式
void test_headers_mode() {
    auto& pub = ZmqGlobal::InstanceManager::getPublisher("tcp://*:5558");
    auto& sub = ZmqGlobal::InstanceManager::getSubscriber("tcp://localhost:5558");

    // 订阅高优先级告警
    sub.subscribe_headers(
            {{"priority", "high"}, {"type", "alert"}},
            message_handler,
            "alerts"
    );

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    pub.publish("alerts", "CPU温度超过阈值!",
                ZmqIn::ExchangeType::HEADERS,
                {{"priority", "high"}, {"type", "alert"}, {"source", "server1"}}
    );

    std::cout << COLOR_RED << "[HEADERS] 已发送高优先级告警\n" << COLOR_RESET;
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

int main() {
    std::cout << COLOR_BLUE << "=== ZeroMQ 全局实例测试开始 ===\n" << COLOR_RESET;

    // 测试四种模式
    test_direct_mode();
    test_topic_mode();
    test_fanout_mode();
    test_headers_mode();

    std::cout << COLOR_BLUE << "=== 测试完成 ===\n" << COLOR_RESET;

//    many_zmq();
    return 0;
}