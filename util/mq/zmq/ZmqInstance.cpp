// ZmqInstance.cpp
#include "ZmqInstance.h"

#include <utility>

// 构造函数实现
ZmqIn::ZmqIn(bool is_binder, std::string endpoint, const Config& config)
        : is_binder_(is_binder),
          endpoint_(std::move(endpoint)),
          config_(config),  // 注意：这里不再需要std::move，因为参数已经是const引用
          running_(false)
{
    std::cout << (is_binder_ ? "BINDER" : "CONNECTOR")
              << " Endpoint: " << endpoint_ << std::endl;
}

ZmqIn::~ZmqIn() { stop(); }

void ZmqIn::start() {
    if (running_) return;

    try {
        if (is_binder_) {
            pub_sock_ = std::make_unique<zmq::socket_t>(ctx_, ZMQ_PUB);
            pub_sock_->set(zmq::sockopt::sndhwm, config_.send_hwm);
            pub_sock_->set(zmq::sockopt::sndtimeo, config_.send_timeout_ms);
            pub_sock_->set(zmq::sockopt::linger, config_.linger_ms);
            pub_sock_->bind(endpoint_);
            send_thread_ = std::thread(&ZmqIn::send_loop, this);
        } else {
            sub_sock_ = std::make_unique<zmq::socket_t>(ctx_, ZMQ_SUB);
            sub_sock_->set(zmq::sockopt::rcvhwm, config_.send_hwm);
            sub_sock_->set(zmq::sockopt::linger, config_.linger_ms);
            sub_sock_->connect(endpoint_);
        }

        running_ = true;

        if (!is_binder_) {
            recv_thread_ = std::thread(&ZmqIn::recv_loop, this);
        }
    } catch (const zmq::error_t& e) {
        std::cerr << "启动失败: " << e.what() << std::endl;
        throw;
    }
}

void ZmqIn::stop() {
    if (!running_) return;

    running_ = false;
    cv_.notify_all();

    if (recv_thread_.joinable()) recv_thread_.join();
    if (send_thread_.joinable()) send_thread_.join();
    if (pub_sock_) pub_sock_->close();
    if (sub_sock_) sub_sock_->close();
    ctx_.close();
}

size_t ZmqIn::queue_size() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return msg_queue_.size();
}

bool ZmqIn::is_connected() const {
    return running_ && (is_binder_ ? (pub_sock_ != nullptr) : (sub_sock_ != nullptr));
}

bool ZmqIn::try_reconnect() {
    if (!is_binder_) return false;
    try {
        pub_sock_->close();
        pub_sock_ = std::make_unique<zmq::socket_t>(ctx_, ZMQ_PUB);
        pub_sock_->set(zmq::sockopt::sndhwm, config_.send_hwm);
        pub_sock_->set(zmq::sockopt::sndtimeo, config_.send_timeout_ms);
        pub_sock_->set(zmq::sockopt::linger, config_.linger_ms);
        pub_sock_->bind(endpoint_);
        return true;
    } catch (const zmq::error_t& e) {
        std::cerr << "重连失败: " << e.what() << std::endl;
        return false;
    }
}

void ZmqIn::publish(const std::string& topic,
                    const std::string& message,
                    ExchangeType type,
                    const std::map<std::string, std::string>& headers,
                    const std::string& routing_key) {
    if (!is_binder_) throw std::runtime_error("Only binder can publish");

    // 流控检查
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (msg_queue_.size() >= config_.max_queue_size) {
            throw std::runtime_error("Message queue overflow (" +
                                     std::to_string(config_.max_queue_size) + ")");
        }
    }

    Message msg{topic, message, routing_key,
                {headers.begin(), headers.end()}, type};

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        msg_queue_.push(std::move(msg));
    }
    cv_.notify_one();
}

void ZmqIn::subscribe(const std::string& topic_filter,
                      MsgCallback callback,
                      ExchangeType type) {
    if (is_binder_) throw std::runtime_error("Only connector can subscribe");
    std::lock_guard<std::mutex> lock(sub_mutex_);
    if (type == ExchangeType::FANOUT) {
        sub_sock_->set(zmq::sockopt::subscribe, "");
    } else {
        sub_sock_->set(zmq::sockopt::subscribe, topic_filter);
    }
    subscriptions_[type][topic_filter] = std::move(callback);
}

void ZmqIn::subscribe_headers(const std::map<std::string, std::string>& headers,
                              MsgCallback callback,
                              const std::string& filter_topic) {
    if (is_binder_) throw std::runtime_error("Only connector can subscribe");
    std::lock_guard<std::mutex> lock(sub_mutex_);
    sub_sock_->set(zmq::sockopt::subscribe, "");
    header_subscriptions_.push_back({headers, filter_topic, std::move(callback)});
}

void ZmqIn::send_loop() {
    while (running_) {
        std::vector<Message> batch;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            cv_.wait_for(lock, std::chrono::milliseconds(100), [this] {
                return !running_ || !msg_queue_.empty();
            });

            if (!running_) break;

            // 控制队列大小
            if (msg_queue_.size() > static_cast<size_t>(config_.max_queue_size * 1.2)) {
                std::cerr << "WARN: Queue overflow (" << msg_queue_.size()
                          << "), discarding old messages" << std::endl;
                while (msg_queue_.size() > config_.max_queue_size) {
                    msg_queue_.pop();
                }
            }

            // 批量取出
            size_t count = std::min(msg_queue_.size(), config_.batch_size);
            batch.reserve(count);
            for (size_t i = 0; i < count; ++i) {
                batch.push_back(std::move(msg_queue_.front()));
                msg_queue_.pop();
            }
        }

        // 批量发送
        for (auto& msg : batch) {
            zmq::pollitem_t items[] = {{*pub_sock_, 0, ZMQ_POLLOUT, 0}};
            if (zmq::poll(items, 1, std::chrono::milliseconds(config_.send_timeout_ms)) <= 0){
                std::cerr << "发送超时，丢弃消息: " << msg.topic << std::endl;
                continue;
            }

            try {
                zmq::multipart_t multipart;
                switch(msg.type) {
                    case ExchangeType::DIRECT:
                        multipart.addstr(msg.topic);
                        multipart.addstr(msg.content);
                        break;
                    case ExchangeType::TOPIC:
                        multipart.addstr(msg.routing_key + ":" + msg.topic);
                        multipart.addstr(msg.content);
                        break;
                    case ExchangeType::FANOUT:
                        multipart.addstr("");
                        multipart.addstr(msg.topic);
                        multipart.addstr(msg.content);
                        break;
                    case ExchangeType::HEADERS: {
                        std::string header_str;
                        for (const auto& [k, v] : msg.headers) {
                            header_str += k + "=" + v + ";";
                        }
                        multipart.addstr(header_str);
                        multipart.addstr(msg.topic);
                        multipart.addstr(msg.content);
                        break;
                    }
                }

                if (!multipart.send(*pub_sock_, ZMQ_DONTWAIT)) {
                    throw zmq::error_t();
                }
            } catch (const zmq::error_t& e) {
                std::cerr << "发送错误 (" << e.num() << "): " << e.what() << std::endl;
                if (!try_reconnect()) {
                    running_ = false;
                    break;
                }
            }
        }
    }
}

void ZmqIn::recv_loop() {
    zmq::pollitem_t items[] = {{*sub_sock_, 0, ZMQ_POLLIN, 0}};

    while (running_) {
        try {
            if (zmq::poll(items, 1, 100) <= 0) continue;

            zmq::multipart_t multipart(*sub_sock_);
            if (multipart.size() < 2) continue;

            std::string frame1 = multipart[0].to_string();
            std::string frame2 = multipart[1].to_string();
            std::string frame3 = multipart.size() > 2 ? multipart[2].to_string() : "";

            ExchangeType msg_type = ExchangeType::DIRECT;
            std::string topic, body;
            std::unordered_map<std::string, std::string> headers;

            if (multipart.size() == 2) {
                topic = frame1;
                body = frame2;
                if (topic.find(':') != std::string::npos) {
                    msg_type = ExchangeType::TOPIC;
                }
            } else {
                if (frame1.empty()) {
                    msg_type = ExchangeType::FANOUT;
                    topic = frame2;
                    body = frame3;
                } else {
                    msg_type = ExchangeType::HEADERS;
                    headers = parse_headers(frame1);
                    topic = frame2;
                    body = frame3;
                }
            }

            std::lock_guard<std::mutex> lock(sub_mutex_);

            // 处理普通订阅
            for (auto& [filter, callback] : subscriptions_[msg_type]) {
                if (msg_type == ExchangeType::TOPIC ||
                    msg_type == ExchangeType::DIRECT ||
                    msg_type == ExchangeType::FANOUT) {
                    callback(topic, body, {});
                }
            }

            // 处理HEADERS订阅
            for (auto& sub : header_subscriptions_) {
                if (headers_match(sub.headers, sub.filter_topic, topic, headers)) {
                    sub.callback(topic, body, headers);
                }
            }

        } catch (const zmq::error_t& e) {
            if (e.num() != ETERM) {
                std::cerr << "接收错误: " << e.what() << std::endl;
            }
        }
    }
}

std::unordered_map<std::string, std::string> ZmqIn::parse_headers(const std::string& str) {
    std::unordered_map<std::string, std::string> headers;
    size_t pos = 0;
    while (pos < str.size()) {
        size_t eq = str.find('=', pos);
        if (eq == std::string::npos) break;

        size_t end = str.find(';', eq);
        if (end == std::string::npos) end = str.size();

        headers[str.substr(pos, eq - pos)] = str.substr(eq + 1, end - eq - 1);
        pos = end + 1;
    }
    return headers;
}

bool ZmqIn::headers_match(
        const std::map<std::string, std::string>& filter,
        const std::string& filter_topic,
        const std::string& msg_topic,
        const std::unordered_map<std::string, std::string>& msg_headers) {
    if (!filter_topic.empty() && msg_topic != filter_topic) {
        return false;
    }

    for (const auto& [key, val] : filter) {
        auto it = msg_headers.find(key);
        if (it == msg_headers.end()) return false;

        if (val.find(',') != std::string::npos) {
            std::istringstream iss(val);
            std::string v;
            bool found = false;
            while (std::getline(iss, v, ',')) {
                v.erase(0, v.find_first_not_of(' '));
                v.erase(v.find_last_not_of(' ') + 1);
                if (it->second == v) {
                    found = true;
                    break;
                }
            }
            if (!found) return false;
        }
        else if (it->second != val) {
            return false;
        }
    }
    return true;
}

