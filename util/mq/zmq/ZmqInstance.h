// ZmqInstance.h
#pragma once
#include <utility>
#include <zmq.hpp>
#include <zmq_addon.hpp>
#include <functional>
#include <thread>
#include <mutex>
#include <string>
#include <memory>
#include <atomic>
#include <iostream>
#include <map>
#include <unordered_map>
#include <sstream>
#include <queue>
#include <condition_variable>
#include <chrono>

class ZmqIn {
public:
    enum class ExchangeType { DIRECT, TOPIC, FANOUT, HEADERS };

    struct Config {
        size_t max_queue_size = 10000;      // 内存队列最大长度
        int send_timeout_ms = 200;          // 发送超时(ms)
        int send_hwm = 1000;                // 发送高水位
        int linger_ms = 100;                // 关闭等待时间
        size_t batch_size = 50;            // 批量发送大小

        Config() : max_queue_size(10000), send_timeout_ms(200),
                   send_hwm(1000), linger_ms(100), batch_size(50) {}
    };

    using MsgCallback = std::function<void(
            const std::string& topic,
            const std::string& body,
            const std::unordered_map<std::string, std::string>& headers)>;

    ZmqIn(bool is_binder, std::string endpoint, const Config& config = Config());
    ~ZmqIn();

    void start();

    void stop();

    size_t queue_size() const;
    bool is_connected() const;

    void publish(const std::string& topic,
                 const std::string& message,
                 ExchangeType type = ExchangeType::DIRECT,
                 const std::map<std::string, std::string>& headers = {},
                 const std::string& routing_key = "");

    void subscribe(const std::string& topic_filter,
                   MsgCallback callback,
                   ExchangeType type = ExchangeType::DIRECT);

    void subscribe_headers(const std::map<std::string, std::string>& headers,
                           MsgCallback callback,
                           const std::string& filter_topic = "");

private:
    struct Message {
        std::string topic;
        std::string content;
        std::string routing_key;
        std::unordered_map<std::string, std::string> headers;
        ExchangeType type;
    };

    struct HeaderSubscription {
        std::map<std::string, std::string> headers;
        std::string filter_topic;
        MsgCallback callback;
    };

    zmq::context_t ctx_;
    bool is_binder_;
    std::string endpoint_;
    Config config_;
    std::atomic<bool> running_{false};

    std::unique_ptr<zmq::socket_t> pub_sock_;
    std::unique_ptr<zmq::socket_t> sub_sock_;
    std::thread recv_thread_;
    std::thread send_thread_;

    std::mutex pub_mutex_;
    std::mutex sub_mutex_;
    mutable  std::mutex queue_mutex_;
    std::condition_variable cv_;

    std::map<ExchangeType, std::unordered_map<std::string, MsgCallback>> subscriptions_;
    std::vector<HeaderSubscription> header_subscriptions_;
    std::queue<Message> msg_queue_;

    void send_loop();
    void recv_loop();
    bool try_reconnect();
    static std::unordered_map<std::string, std::string> parse_headers(const std::string& str);
    static bool headers_match(const std::map<std::string, std::string>& filter,
                              const std::string& filter_topic,
                              const std::string& msg_topic,
                              const std::unordered_map<std::string, std::string>& msg_headers);
};