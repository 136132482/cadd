#include "ZmqAdvanced.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <map>

// 直连模式发布
void direct_publish(ZmqRouter& router) {
    static int count = 0;
    router.publish("queue1", "Direct message " + std::to_string(++count));
    std::cout << "Published direct message" << std::endl;
}

// 主题模式发布
void topic_publish(ZmqRouter& router) {
    static int count = 0;
    const std::string routing_key = "sensor" + std::to_string(count % 3);
    router.publish("sensor_data", "Reading " + std::to_string(++count), routing_key);
    std::cout << "Published topic message to " << routing_key << std::endl;
}

// 扇出模式发布
void fanout_publish(ZmqRouter& router) {
    static int count = 0;
    router.publish("announcement", "Broadcast #" + std::to_string(++count));
    std::cout << "Published fanout message" << std::endl;
}

// 头模式发布
void headers_publish(ZmqRouter& router) {
    static int count = 0;
    std::map<std::string, std::string> headers = {
            {"priority", count % 2 ? "high" : "low"},
            {"type", "update"}
    };
    router.publish("alerts", "Alert " + std::to_string(++count), headers);
    std::cout << "Published message with headers" << std::endl;
}

// 批量发布直连消息
void batch_publish_direct(ZmqRouter& router) {
    std::vector<ZmqRouter::Message> messages;

    for (int i = 1; i <= 10; ++i) {
        messages.push_back({
                                 .topic=  "queue1",                       // topic
                                  .content= "Direct message " + std::to_string(i), // content
                                  .routing_key= "",                             // routing_key (不需要)
                                  .headers= {},                              // headers (不需要)
                                  .type= ZmqRouter::ExchangeType::DIRECT
                           });
    }

    router.publish_batch(messages);
    std::cout << "Published 10 DIRECT messages" << std::endl;
}

// 批量发布主题消息
void batch_publish_topic(ZmqRouter& router) {
    std::vector<ZmqRouter::Message> messages;
    for (int i = 1; i <= 10; ++i) {
        messages.push_back({
                                   .topic="sensor.data",                  // topic
                                   .content="Sensor reading " + std::to_string(i), // content
                                   .routing_key="sensor" + std::to_string(i % 3), // routing_key
                                   .headers={},                              // headers (不需要)
                                   .type=ZmqRouter::ExchangeType::TOPIC
                           });
    }
    router.publish_batch(messages);
    std::cout << "Published 10 TOPIC messages" << std::endl;
}

// 批量发布头消息
void batch_publish_headers(ZmqRouter& router) {
    std::vector<ZmqRouter::Message> messages;
    for (int i = 1; i <= 10; ++i) {
        messages.push_back({
                                   "alerts",                       // topic
                                   "Alert message " + std::to_string(i), // content
                                   "",                             // routing_key (不需要)
                                   {
                                           {"priority", i % 2 ? "high" : "low"},
                                           {"type", "alert"},
                                           {"id", std::to_string(i)}
                                   } ,
                                   0,
                                   ZmqRouter::ExchangeType::HEADERS
                           });
    }
    router.publish_batch(messages);
    std::cout << "Published 10 HEADERS messages" << std::endl;
}

void batch_publish_headers1(ZmqRouter& router) {
    std::vector<ZmqRouter::Message> messages;
    for (int i = 1; i <= 10; ++i) {
        messages.push_back({
                                   "update",                       // topic
                                   "update message " + std::to_string(i), // content
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
    std::cout << "Published 10 HEADERS messages" << std::endl;
}






int publish_test() {
    auto& router = ZmqRouter::instance();

    // 初始化所有发布者
//    router.init_publisher("tcp://*:5555"); // 直连
//    std::this_thread::sleep_for(100ms);  // 关键点1：增加100ms间隔

    router.init_publisher("tcp://*:5556"); // 主题
    std::this_thread::sleep_for(1s);  // 关键点1：增加100ms间隔
//
//    router.init_publisher("tcp://*:5557", ZmqRouter::ExchangeType::FANOUT); // 扇出
//    std::this_thread::sleep_for(100ms);  // 关键点1：增加100ms间隔
//
//    router.init_publisher("tcp://*:5558", ZmqRouter::ExchangeType::HEADERS); // 头模式
//    std::this_thread::sleep_for(100ms);  // 关键点1：增加100ms间隔


    int mode = 0;
    while (true) {
        switch (mode % 8) {
//            case 0: direct_publish(router); break;
//            case 1: topic_publish(router); break;
//            case 2: fanout_publish(router); break;
//            case 3: headers_publish(router); break;
//            case 4: batch_publish_direct(router); break;
//            case 5: batch_publish_topic(router); break;
            case 6: batch_publish_headers(router); break;
            case 7:
                batch_publish_headers1(router);
            break;
        }
        mode++;
        std::this_thread::sleep_for(1s);
    }
    router.shutdown();
    return 0;
}


int main() {
//    test_multi_endpoint();
    publish_test();
    return 0;
}