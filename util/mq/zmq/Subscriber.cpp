#include "ZmqAdvanced.h"
#include <iostream>
#include "../../date/DateUtil.h"

// 直连模式订阅回调
void direct_callback(const std::string& topic,
                     const std::string& msg,
                     const std::unordered_map<std::string, std::string>& headers) {
    std::cout << "[" << DateUtil::getCurrentTime() << "] "
              << "Direct received [" << topic << "]: " << msg << std::endl;}

// 主题模式订阅回调
void topic_callback(const std::string& topic,
                    const std::string& msg,
                    const std::unordered_map<std::string, std::string>& headers) {
    std::cout << "[" << DateUtil::getCurrentTime() << "] "
              << "Topic received [" << topic << "]: " << msg;
    if (!headers.empty() && headers.count("routing_key")) {
        std::cout << " (Routing key: " << headers.at("routing_key") << ")";
    }
    std::cout << std::endl;
}

// 扇出模式订阅回调
void fanout_callback(const std::string& topic,
                     const std::string& msg,
                     const std::unordered_map<std::string, std::string>& headers) {
    std::cout << "[" << DateUtil::getCurrentTime() << "] "
              << "Fanout received: " << msg << std::endl;}

// 头模式订阅回调
void headers_callback(const std::string& topic,
                      const std::string& msg,
                      const std::unordered_map<std::string, std::string>& headers) {
    std::cout << "[" << DateUtil::getCurrentTime() << "] "
              << "Headers received [" << topic << "]: " << msg << "\n  ";
    for (const auto& [k, v] : headers) {
        std::cout << k << "=" << v << " ";
    }
    std::cout << std::endl;
}

// 批量订阅直连消息
void batch_subscribe_direct(ZmqRouter& router) {
    std::vector<std::string> topics = {"queue1", "queue2", "queue3"};
    auto callback = [](const std::string& topic,
                       const std::string& body,
                       const std::unordered_map<std::string, std::string>& headers) {
        std::cout << "[" << DateUtil::getCurrentTime() << "] "
                  << "Initializing DIRECT batch subscribe..." << std::endl;    };
    router.subscribe(topics, callback, ZmqRouter::ExchangeType::DIRECT);
//    std::cout << "Subscribed to 3 DIRECT topics" << std::endl;
}

// 批量订阅主题消息
void batch_subscribe_topic(ZmqRouter& router) {
    std::vector<std::string> topics = {"sensor", "device.#", "log.info"};
    auto callback = [](const std::string& topic,
                       const std::string& body,
                       const std::unordered_map<std::string, std::string>& headers) {
        std::cout << "[" << DateUtil::getCurrentTime() << "] "
                  << "[DIRECT] Received on " << topic << ": " << body << std::endl;    };
    router.subscribe(topics, callback, ZmqRouter::ExchangeType::TOPIC);
//    std::cout << "Subscribed to 3 TOPIC patterns" << std::endl;
}

// 批量订阅头消息（通过不同header条件）
void batch_subscribe_headers(ZmqRouter& router) {
    // 虽然HEADERS模式通常不批量订阅，但可以模拟多个订阅条件
    auto callback = [](const std::string& topic,
                       const std::string& body,
                       const std::unordered_map<std::string, std::string>& headers) {
        std::cout << "[" << DateUtil::getCurrentTime() << "] "
                  << "[HEADERS] Received alert: " << body << " (";
        for (const auto& [k, v] : headers) {
            std::cout << k << "=" << v << " ";
        }
        std::cout << ")" << std::endl;
    };
    router.subscribe({
                             {"priority", "high,low"},{"type", "alert"},
                     }, callback,"alerts");
//    std::cout << "Subscribed to HEADERS messages" << std::endl;
}

void batch_subscribe_headers1(ZmqRouter& router) {
    // 虽然HEADERS模式通常不批量订阅，但可以模拟多个订阅条件
    auto callback = [](const std::string& topic,
                       const std::string& body,
                       const std::unordered_map<std::string, std::string>& headers) {
        std::cout << "[" << DateUtil::getCurrentTime() << "] "
                  << "[HEADERS] Received update: " << body << " (";
        for (const auto& [k, v] : headers) {
            std::cout << k << "=" << v << " ";
        }
        std::cout << ")" << std::endl;
    };
    router.subscribe({
                             {"priority", "high"},{"type", "alert"},
                     }, callback,"update");
//    std::cout << "Subscribed to HEADERS messages" << std::endl;
}

void batch_subscribe_headers2(ZmqRouter& router) {
    // 虽然HEADERS模式通常不批量订阅，但可以模拟多个订阅条件
    auto callback = [](const std::string& topic,
                       const std::string& body,
                       const std::unordered_map<std::string, std::string>& headers) {
        std::cout << "[" << DateUtil::getCurrentTime() << "] "
                  << "[HEADERS] Received update: " << body << " (";
        for (const auto& [k, v] : headers) {
            std::cout << k << "=" << v << " ";
        }
        std::cout << ")" << std::endl;
    };
    router.subscribe({
                             {"priority", "high"},{"type", "alert"},
                     }, callback,"myself");
//    std::cout << "Subscribed to HEADERS messages" << std::endl;
}





int subscriber_test(ZmqRouter &router) {
    router.subscribe("queue", direct_callback);
    // 2. 主题模式（带通配符）
//    router.init_subscriber("tcp://localhost:5556", ZmqRouter::ExchangeType::TOPIC);
    router.subscribe("sensor", topic_callback, ZmqRouter::ExchangeType::TOPIC);
    // 3. 扇出模式
//    router.init_subscriber("tcp://localhost:5557", ZmqRouter::ExchangeType::FANOUT);
    router.subscribe("", fanout_callback, ZmqRouter::ExchangeType::FANOUT);
    // 4. 头模式
//    router.init_subscriber("tcp://localhost:5558", ZmqRouter::ExchangeType::HEADERS);
    router.subscribe(
            {
                    {"priority", "high"},
            },  // headers
            headers_callback,        // 回调
            "alerts"                 // topic
    );
    // 等待订阅完全建立
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "Subscriber ready. Press Enter to exit..." << std::endl;
//    std::cin.get();
//    router.shutdown();
    return 0;
}

void batch_publish_headers1(ZmqRouter& router) {
    std::vector<ZmqRouter::Message> messages;
    for (int i = 1; i <= 10; ++i) {
        messages.push_back({
                                   "myself",                       // topic
                                   "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx message " + std::to_string(i), // content
                                   "",                             // routing_key (不需要)
                                   {
                                           {"priority", i % 2 ? "high" : "low"},
                                           {"type", "alert"},
                                           {"id", std::to_string(i)}
                                   },
                                   0,
                                   ZmqRouter::ExchangeType::HEADERS
                           });
    }
    router.publish_batch(messages);
    std::cout << "Published xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx HEADERS messages" << std::endl;
}

// 主函数
int batch_publish(ZmqRouter &router) {
//    auto& router = ZmqRouter::instance();
//    // 1. 直连模式
//    router.init_subscriber("tcp://localhost:5556");
//    batch_subscribe_direct(router);
//    batch_subscribe_topic(router);
    batch_subscribe_headers(router);
    batch_subscribe_headers1(router);
    batch_subscribe_headers2(router);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "Subscriber ready. Press Enter to exit..." << std::endl;
    std::cin.get();
    router.shutdown();
    return 0;
}
std::atomic<int> received_count{0};




int main(){
    auto& router = ZmqRouter::instance();
    // 1. 直连模式
    router.init_subscriber("tcp://localhost:5556");

//    subscriber_test(router);
    batch_publish(router);
//    sub_test();
}