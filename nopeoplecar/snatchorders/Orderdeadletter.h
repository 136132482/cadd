#include <zmq.hpp>
#include <chrono>
#include <iostream>
#include <vector>
#include <atomic>
#include <atomic>
#include <nlohmann/json.hpp>
#include <fstream>
#include "../../util/redis/RedisPool.h"
#include "../../util/date/DateUtil.h"
#include "../../util/pgsql//SociDB.h"

#pragma pack(push, 1)
struct MessageHeader {
    int64_t timestamp;
    int32_t msg_id;
    int16_t version;
    char reserved[6];
};
#pragma pack(pop)

struct MessageStats {
    std::atomic<int> deadletter_size{0};
    std::atomic<int> processed_count{0};
};
static MessageStats stats;

class ZmqCleaner {
public:
    ZmqCleaner(zmq::context_t& ctx, const std::vector<std::string>& endpoints)
            : ctx_(ctx) {
        for (const auto& ep : endpoints) {
            auto sock = std::make_unique<zmq::socket_t>(ctx_, ZMQ_SUB);
            sock->connect(ep);
            sock->setsockopt(ZMQ_SUBSCRIBE, "", 0);
            sock->setsockopt(ZMQ_RCVHWM, 1000);
            sockets_.emplace_back(std::move(sock));
            std::cout << "[" << DateUtil::getCurrentTime() << "] 监控端口:" << ep << "\n";
        }
    }

    size_t cleanup_expired(int timeout_sec = 300) {
        size_t count = 0;
        zmq::pollitem_t items[sockets_.size()];

        for (size_t i = 0; i < sockets_.size(); ++i) {
            items[i] = { *sockets_[i], 0, ZMQ_POLLIN, 0 };
        }

        if (zmq::poll(items, sockets_.size(), 100) > 0) {
            zmq::message_t msg;
            for (size_t i = 0; i < sockets_.size(); ++i) {
                while (sockets_[i]->recv(msg, zmq::recv_flags::dontwait)) {
                    if (is_expired(msg, timeout_sec) && deadletter_store(msg)) {
                        count++;
                    }
                }
            }
        }
        return count;
    }

    static void process_deadletters(int batch_size = 100) {
        auto keys = RedisUtils::Keys("deadletter:*");
        int processed = 0;

        for (const auto& key : keys) {
            if (processed >= batch_size) break;

            try {
                auto data = RedisUtils::HGetAll(key);
                if (process_message(data)) {
                    RedisUtils::Del(key);
                    stats.deadletter_size--;
                    processed++;
                }
            } catch (const std::exception& e) {
                log_processing_error(key, e.what());
            }
        }

        if (processed > 0) {
            std::cout << "[" << DateUtil::getCurrentTime()
                      << "] 已处理死信: " << processed << "条\n";
        }
    }

    static void maintain_deadletters() {
        std::cout << "[" << DateUtil::getCurrentTime()
                  << "] 开始死信队列维护...\n";

        auto keys = RedisUtils::Keys("deadletter:*");
        size_t processed = 0;
        size_t failed = 0;

        for (const auto &key: keys) {
            try {
                // 只处理剩余生命期小于12小时的
                if (RedisUtils::GetTTL(key) < 3600 * 12) {
                    auto data = RedisUtils::HGetAll(key);

                    if (archive_to_local(key, data)) {
                        if (RedisUtils::Del(key)) {
                            stats.deadletter_size--;
                            processed++;
                        } else {
                            failed++;
                            std::cerr << "[" << DateUtil::getCurrentTime()
                                      << "] Redis删除失败:" << key << "\n";
                        }
                    } else {
                        failed++;
                        RedisUtils::HSet("deadletter_errors", key, "archive_failed");
                    }
                }
            } catch (const std::exception &e) {
                failed++;
                std::cerr << "[" << DateUtil::getCurrentTime()
                          << "] 处理异常:" << e.what() << "\n";
            }
        }

        // 记录统计信息
        RedisUtils::HMSet("deadletter_maintenance", {
                {"last_run",  std::to_string(time(nullptr))},
                {"processed", std::to_string(processed)},
                {"failed",    std::to_string(failed)}
        });

        // 存储空间告警
        size_t total_size = processed * 1024; // 假设每条约1KB
        if (total_size > 100 * 1024 * 1024) { // 超过100MB报警
            std::cerr << "[" << DateUtil::getCurrentTime()
                      << "] [ALERT] 死信归档体积过大:"
                      << total_size / 1024 / 1024 << "MB\n";
        }
    }

private:
    static bool is_expired(const zmq::message_t& msg, int timeout_sec) {
        if (msg.size() < sizeof(MessageHeader)) return false;
        auto now = std::chrono::system_clock::now();
        auto msg_time = std::chrono::system_clock::time_point(
                std::chrono::milliseconds(msg.data<MessageHeader>()->timestamp)
        );
        return (now - msg_time) > std::chrono::seconds(timeout_sec);
    }

    static bool deadletter_store(const zmq::message_t& msg) {
        try {
            const auto* header = msg.data<MessageHeader>();
            std::string key = "deadletter:" + std::to_string(header->msg_id);

            size_t body_size = std::min(msg.size() - sizeof(MessageHeader), size_t(1024 * 1024));

            RedisUtils::HMSetWithTTL(key, {
                    {"timestamp", std::to_string(header->timestamp)},
                    {"msg_id", std::to_string(header->msg_id)},
                    {"data", std::string(msg.data<char>() + sizeof(MessageHeader), body_size)}
            }, 86400);

            stats.deadletter_size++;
            return true;
        } catch (...) {
            return false;
        }
    }


    static bool archive_to_local(const std::string& key,
                          const std::unordered_map<std::string, std::string>& data) {
        try {
            // 1. 创建归档目录
            const std::string archive_dir = "/var/deadletter/";
            if (!std::filesystem::exists(archive_dir)) {
                std::filesystem::create_directories(archive_dir);
            }

            // 2. 生成带日期的文件名
            auto now = std::chrono::system_clock::now();
            auto in_time_t = std::chrono::system_clock::to_time_t(now);
            std::stringstream ss;
            ss << std::put_time(std::localtime(&in_time_t), "%Y%m%d");

            std::string filename = archive_dir + ss.str() + "_" + key + ".json";

            // 3. 写入JSON文件
            std::ofstream out(filename);
            if (!out) {
                std::cerr << "[" << DateUtil::getCurrentTime()
                          << "] 无法创建文件:" << filename << "\n";
                return false;
            }

            nlohmann::json j(data);
            out << j.dump(2); // 美化输出

            // 4. 验证写入成功
            if (out.tellp() <= 0) {
                std::cerr << "[" << DateUtil::getCurrentTime()
                          << "] 归档文件为空:" << filename << "\n";
                return false;
            }

            std::cout << "[" << DateUtil::getCurrentTime()
                      << "] 归档成功:" << filename
                      << " 大小:" << out.tellp() << "B\n";
            return true;

        } catch (const std::exception& e) {
            std::cerr << "[" << DateUtil::getCurrentTime()
                      << "] 归档异常:" << e.what() << "\n";
            return false;
        }
    }

// 修改maintain_deadletters函数


    static bool process_message(const std::unordered_map<std::string, std::string>& data) {
        // 需要添加header解析和错误处理
        try {
            MessageHeader header;
            header.timestamp = std::stol(data.at("timestamp"));
            header.msg_id = std::stoi(data.at("msg_id"));

            if (data.count("order_id") > 0) {
                std::cout << "处理订单死信 ID:" << header.msg_id
                          << " 订单号:" << data.at("order_id") << "\n";
                return true;
            }
        } catch (...) {
            return false;
        }
        return false;
    }

    static void log_processing_error(const std::string& key, const std::string& error) {
        RedisUtils::HSet("deadletter:errors", key, error);
    }

    zmq::context_t& ctx_;
    std::vector<std::unique_ptr<zmq::socket_t>> sockets_;
};

// 使用示例
//int main() {
//    zmq::context_t ctx(1);
//    std::vector<std::string> endpoints = {
//            "tcp://192.168.1.100:5555",
//            "tcp://192.168.1.101:5556"
//    };
//
//    ZmqCleaner cleaner(ctx, endpoints);
//
//    // 定时任务示例（实际使用您的调度器）
//    while (true) {
//        cleaner.cleanup_expired();
//        ZmqCleaner::process_deadletters();
//        std::this_thread::sleep_for(std::chrono::seconds(5));
//    }
//
//    return 0;
//}