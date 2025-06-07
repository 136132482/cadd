#pragma once
#include <zmq.hpp>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <vector>
#include <string>
#include <iostream>
#include <map>
#include <memory>
#include <chrono>
#include <queue>
#include <condition_variable>

using namespace std::chrono_literals;

class ZmqRouter {
public:
    enum class ExchangeType {
        DIRECT, TOPIC, FANOUT, HEADERS, AUTO
    };



    using MsgCallback = std::function<void(
            const std::string& topic,
            const std::string& body,
            const std::unordered_map<std::string, std::string>& headers)>;

    static ZmqRouter& instance() {
        static ZmqRouter inst;
        return inst;
    }

    struct Message {
        std::string topic;       // 消息主题
        std::string content;     // 消息内容
        std::string routing_key; // 路由键（仅TOPIC模式需要）
        std::unordered_map<std::string, std::string> headers; // 头信息
        int priority = 0;        // 消息优先级 (0-10, 越大优先级越高)
        ExchangeType type = ExchangeType::DIRECT; // 消息类型
        std::string target_endpoint; // 新增：目标端口
    };

    /****************** 发布方法 ******************/
    void publish(const std::string& topic, const std::string& message,
                 int priority = 0) {
        enqueue_message({topic, message, "", {}, priority, ExchangeType::DIRECT});
    }

    void publish(const std::string& topic, const std::string& message,
                 const std::string& routing_key, int priority = 0) {
        enqueue_message({topic, message, routing_key, {}, priority, ExchangeType::TOPIC});
    }

    void publish(const std::string& topic, const std::string& message,
                 const std::map<std::string, std::string>& headers,
                 int priority = 0) {
        std::unordered_map<std::string, std::string> h(headers.begin(), headers.end());
        enqueue_message({topic, message, "", h, priority, ExchangeType::HEADERS});
    }

    void publish(const Message& msg) {
        enqueue_message(msg);
    }

    // 批量发布接口
    void publish_batch(const std::vector<Message>& messages) {
        for (const auto& msg : messages) {
            enqueue_message(msg);
        }
    }

    // 在ZmqRouter类中添加：
    void publish_async(const Message& msg) {
        std::thread([this, msg]() {
            try {
                this->publish(msg);
            } catch (const std::exception& e) {
                std::cerr << "Async publish failed: " << e.what() << std::endl;
            }
        }).detach(); // 分离线程自动回收
    }

    /****************** 订阅方法 ******************/
    void subscribe(const std::string& topic,
                   const MsgCallback& callback,
                   ExchangeType type = ExchangeType::DIRECT) {
        create_subscriber({topic}, {}, "", callback, type);
    }

    void subscribe(const std::vector<std::string>& topics,
                   const MsgCallback& callback,
                   ExchangeType type = ExchangeType::DIRECT) {
        create_subscriber(topics, {}, "", callback, type);
    }

    void subscribe(const std::map<std::string, std::string>& headers,
                   const MsgCallback& callback,
                   const std::string& topic = "") {
        create_subscriber({}, headers, topic, callback, ExchangeType::HEADERS);
    }

    /****************** 初始化方法 ******************/
    void init_publisher(const std::string& endpoint, int worker_threads = 4) {
        std::lock_guard<std::mutex> lock(pub_mutex_);
        if (running_) return;

        // 主路由绑定物理端口
        router_.bind(endpoint);

        // 启动代理线程
        proxy_thread_ = std::thread([this](){
            zmq::socket_t workers_sock(ctx_, ZMQ_XSUB);
            workers_sock.bind("inproc://workers");
            zmq::proxy(workers_sock, router_);  // 消息路由
        });

        // 工作线程连接内部端点
        endpoint_ = "inproc://workers";
        running_ = true;
        for (int i = 0; i < worker_threads; ++i) {
            workers_.emplace_back([this]() { worker_thread(); });
        }
    }

    void init_subscriber(const std::string& endpoint) {
        std::lock_guard<std::mutex> lock(sub_mutex_);
        sub_endpoint_ = endpoint;
    }

    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(sub_mutex_);
            for (auto& sub : subscribers_) {
                sub->running = false;
                if (sub->thread.joinable()) sub->thread.join();
                sub->socket.close();
            }
            subscribers_.clear();
        }

        {
            std::lock_guard<std::mutex> lock(pub_mutex_);
            running_ = false;
            cv_.notify_all();
            for (auto& worker : workers_) {
                if (worker.joinable()) worker.join();
            }
//            if (pub_sock_.connected()) {
//                pub_sock_.close();
//            }
        }

        ctx_.close();
    }

private:
    struct PrioritizedMessage {
        Message msg;
        time_t timestamp;

        bool operator<(const PrioritizedMessage& other) const {
            if (msg.priority == other.msg.priority) {
                return timestamp > other.timestamp; // 相同优先级按时间排序
            }
            return msg.priority < other.msg.priority;
        }
    };

    struct Subscriber {
        zmq::socket_t socket;
        std::thread thread;
        std::atomic<bool> running{true};
        std::vector<std::string> topics;
        std::map<std::string, std::string> headers;
        std::string filter_topic;
        ExchangeType type;
    };

    zmq::context_t ctx_{10};
    zmq::socket_t pub_sock_{ctx_, ZMQ_PUB};
    std::string endpoint_;
    std::string sub_endpoint_;
    std::vector<std::unique_ptr<Subscriber>> subscribers_;
    std::vector<std::thread> workers_;
    std::priority_queue<PrioritizedMessage> msg_queue_;
    std::mutex queue_mutex_;
    std::mutex pub_mutex_;
    std::mutex sub_mutex_;
    std::condition_variable cv_;
    std::atomic<bool> running_{false};

    zmq::socket_t router_{ctx_, ZMQ_XPUB};  // 添加路由socket
    std::thread proxy_thread_;               // 代理线程

    ZmqRouter() = default;
    ~ZmqRouter() { shutdown(); }

    void enqueue_message(const Message& msg) {
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            msg_queue_.push({msg, time(nullptr)});
        }
        cv_.notify_all();
    }



    void worker_thread() {
        // 每个线程独立socket
        zmq::socket_t sock(ctx_, ZMQ_PUB);
        sock.set(zmq::sockopt::linger, 0); // 必须设置！
        try {
            std::lock_guard<std::mutex> lock(pub_mutex_);
//            sock.bind(endpoint_);
            sock.connect(endpoint_);  // 改为连接而非绑定
        } catch (const zmq::error_t& e) {
            std::cerr << "Worker bind failed: " << e.what() << std::endl;
            return;
        }
        while (true) {
            Message msg;
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                cv_.wait(lock, [this]{
                    return !msg_queue_.empty() || !running_;
                });
                if (!running_ && msg_queue_.empty()) break;
                if (msg_queue_.empty()) continue;

                msg = msg_queue_.top().msg;
                msg_queue_.pop();
            }
            try {
                send_message(sock, msg);
            } catch (const zmq::error_t& e) {
                if (e.num() != ETERM) {
                    std::cerr << "Send error: " << e.what() << std::endl;
                }
            }
        }
        sock.close(); // 显式关闭
    }

    void send_message(zmq::socket_t& sock, const Message& msg) {
        switch(msg.type) {
            case ExchangeType::DIRECT:
                sock.send(zmq::buffer(msg.topic), zmq::send_flags::sndmore);
                sock.send(zmq::buffer(msg.content));
                break;
            case ExchangeType::TOPIC: {
                std::string full_topic = msg.routing_key.empty() ?
                                         msg.topic : msg.routing_key + ":" + msg.topic;
                sock.send(zmq::buffer(full_topic), zmq::send_flags::sndmore);
                sock.send(zmq::buffer(msg.content));
                break;
            }
            case ExchangeType::FANOUT:
                sock.send(zmq::buffer(""), zmq::send_flags::sndmore);
                sock.send(zmq::buffer(msg.topic), zmq::send_flags::sndmore);
                sock.send(zmq::buffer(msg.content));
                break;
            case ExchangeType::HEADERS: {
                std::string header_str;
                for (const auto& [k, v] : msg.headers) {
                    header_str += k + "=" + v + ";";
                }
                sock.send(zmq::buffer(header_str), zmq::send_flags::sndmore);
                sock.send(zmq::buffer(msg.topic), zmq::send_flags::sndmore);
                sock.send(zmq::buffer(msg.content));
                break;
            }
            default: break;
        }
    }

    void create_subscriber(const std::vector<std::string>& topics,
                           const std::map<std::string, std::string>& headers,
                           const std::string& filter_topic,
                           const MsgCallback& callback,
                           ExchangeType type) {
        std::lock_guard<std::mutex> lock(sub_mutex_);
        auto sub = std::make_unique<Subscriber>();
        sub->socket = zmq::socket_t(ctx_, ZMQ_SUB);
        sub->socket.connect(sub_endpoint_);
        sub->topics = topics;
        sub->headers = headers;
        sub->filter_topic = filter_topic;
        sub->type = type;

        // 设置订阅过滤器
        if (type == ExchangeType::HEADERS) {
            sub->socket.set(zmq::sockopt::subscribe, "");
        } else {
            for (const auto& topic : topics) {
                sub->socket.set(zmq::sockopt::subscribe, topic);
            }
        }
        // 启动消息循环线程
        sub->thread = std::thread([this, sub_ptr = sub.get(), callback]() {
            message_loop(sub_ptr, callback);
        });
        subscribers_.push_back(std::move(sub));
    }


    static void message_loop(Subscriber* sub, const MsgCallback& callback) {
        while (sub->running) {
            try {
                zmq::pollitem_t items[] = {{sub->socket, 0, ZMQ_POLLIN, 0}};
                if (zmq::poll(items, 1, std::chrono::milliseconds(100)) <= 0) continue;

                switch (sub->type) {
                    case ExchangeType::DIRECT:
                    case ExchangeType::TOPIC:
                        handle_standard_message(sub->socket, callback);
                        break;
                    case ExchangeType::FANOUT:
                        handle_fanout_message(sub->socket, callback);
                        break;
                    case ExchangeType::HEADERS:
                        handle_headers_message(sub, callback);
                        break;
                    default: break;
                }
            } catch (...) {
                if (sub->running) std::cerr << "ZMQ error occurred" << std::endl;
            }
        }
    }

    static void handle_standard_message(zmq::socket_t& sock, const MsgCallback& callback) {
        zmq::message_t topic, body;
        if (sock.recv(topic) && sock.recv(body)) {
            callback(topic.to_string(), body.to_string(), {});
        }
    }

    static void handle_fanout_message(zmq::socket_t& sock, const MsgCallback& callback) {
        zmq::message_t empty, topic, body;
        if (sock.recv(empty) && sock.recv(topic) && sock.recv(body)) {
            callback(topic.to_string(), body.to_string(), {});
        }
    }

    static void handle_headers_message(Subscriber* sub, const MsgCallback& callback) {
        zmq::message_t headers, topic, body;
        if (sub->socket.recv(headers) &&
            sub->socket.recv(topic) &&
            sub->socket.recv(body)) {
            auto msg_headers = parse_headers(headers.to_string());
            if (header_matches(sub->headers, sub->filter_topic,
                               topic.to_string(), msg_headers)) {
                callback(topic.to_string(), body.to_string(), msg_headers);
            }
        }
    }

    static std::unordered_map<std::string, std::string> parse_headers(const std::string& header_str) {
        std::unordered_map<std::string, std::string> headers;
        size_t pos = 0;
        while (pos < header_str.size()) {
            size_t eq = header_str.find('=', pos);
            if (eq == std::string::npos) break;

            size_t sc = header_str.find(';', eq);
            if (sc == std::string::npos) sc = header_str.size();

            headers[header_str.substr(pos, eq - pos)] =
                    header_str.substr(eq + 1, sc - eq - 1);
            pos = sc + 1;
        }
        return headers;
    }

    static bool header_matches(
            const std::map<std::string, std::string>& filter_headers,
            const std::string& filter_topic,
            const std::string& topic,
            const std::unordered_map<std::string, std::string>& msg_headers) {
        // 1. 检查topic是否匹配
        if (!filter_topic.empty() && topic != filter_topic) {
            return false;
        }

        // 2. 检查每个过滤条件
        for (const auto& [key, filter_value] : filter_headers) {
            auto msg_it = msg_headers.find(key);
            if (msg_it == msg_headers.end()) {
                return false; // 键不存在
            }

            // 3. 判断匹配模式（新增多值匹配支持）
            if (filter_value.find(',') != std::string::npos) {
                // 多值匹配模式（filter_value格式如 "701,101"）
                std::istringstream iss(filter_value);
                std::string expected_value;
                bool any_match = false;

                while (std::getline(iss, expected_value, ',')) {
                    // 去除前后空格
                    expected_value.erase(0, expected_value.find_first_not_of(' '));
                    expected_value.erase(expected_value.find_last_not_of(' ') + 1);

                    if (msg_it->second == expected_value) {
                        any_match = true;
                        break;
                    }
                }

                if (!any_match) {
                    return false;
                }
            }
            else {
                // 精确匹配模式（原逻辑）
                if (msg_it->second != filter_value) {
                    return false;
                }
            }
        }

        return true;
    }
};