#include "../../util/mq/zmq/ZmqManager.h"
#include "../../util/mq/zmq/ZmqInstance.h"
#include "../ParamObj.h"
#include "../../util/pgsql/SociDB.h"
#include "../../util/date/DateUtil.h"
#include "../../util/redis/RedisPool.h"
#include "../../nopeoplecar/OrderEnums.h"
#include <string>
#include <chrono>
#include <condition_variable>
#include <BS_thread_pool.hpp>

// 使用带优先级的线程池（数值越大优先级越高）
static BS::thread_pool<BS::tp::priority> global_pool(100); // 4个核心线程

// 优先级常量定义
constexpr BS::pr HIGH_PRIORITY{10};
constexpr BS::pr MID_PRIORITY{5};
constexpr BS::pr LOW_PRIORITY{1};

using namespace std::chrono_literals; // 添加这行


class OrderCache {
public:
    // 存储订单到指定车辆的哈希表
    static bool cache_order(const std::int64_t & vehicle_id,
                            const std::string& order_key,
                            const std::string& order_value) {
        return RedisUtils::HSet(
                get_vehicle_key(vehicle_id),
                order_key,
                order_value
        );
    }

    // 从指定车辆缓存中移除订单
    static bool remove_order(const std::int64_t& vehicle_id,
                             const std::string& order_key) {
        return RedisUtils::HDel(
                get_vehicle_key(vehicle_id),
                order_key
        ) > 0;
    }

    // 获取指定车辆的所有订单
    static std::unordered_map<std::string, std::string> get_vehicle_orders(const std::int64_t& vehicle_id) {
        return RedisUtils::HGetAll(get_vehicle_key(vehicle_id));
    }

    // 获取指定车辆的特定订单
    static std::optional<std::string> get_order(const std::int64_t& vehicle_id,const std::string& order_key) {
        auto value = RedisUtils::HGet(
                get_vehicle_key(vehicle_id),
                order_key
        );
        return value.empty() ? std::nullopt : std::optional(value);
    }

//private:
    // 生成车辆专属的Redis键
    static std::string get_vehicle_key(const std::int64_t & vehicle_id) {
        if (vehicle_id <= 0) {
            throw std::runtime_error("Invalid vehicle_id");
        }
        return "vehicle_orders:" + std::to_string(vehicle_id);
    }
};


class VehicleClient {
private:
    struct OrderMeta {
        std::chrono::system_clock::time_point expire_time;
    };
    std::unordered_map<std::string, OrderMeta> available_orders_; // 非静态

    static inline std::mutex orders_mutex_;
    std::thread grab_thread_;
    std::atomic<bool> running_{true};
    int64_t vehicle_id_;
    std::thread sub_thread_;  // 消息订阅线程


    std::mutex mtx_;
    std::condition_variable cv_;
    std::shared_ptr<zmq::context_t> zmq_context_; // ZMQ上下文需要统一管理

    // 定义返回值结构体
    struct GrabResult {
        bool success;
        std::string message;
        std::string order_id;
        std::chrono::system_clock::time_point timestamp;
    };

    static inline std::mutex instances_mutex_;
    static inline std::unordered_map<int64_t, VehicleClient*> instances_;


    std::atomic<bool> zmq_ready_{false};  // 新增订阅状态标志
    std::condition_variable sub_cv_;       // 新增条件变量



    //            std::vector<std::string> to_process;
    ////            {
    ////                std::lock_guard<std::mutex> lock(orders_mutex_);
    ////                for (auto& [order_id, meta] : available_orders_) {
    ////                    to_process.push_back(order_id);
    ////                }
    ////            }
    ////            for (const auto& order_id : to_process) {
    ////                grab_order(order_id);
    ////            }


    //                auto uv_order= SociDB::query<Order>("select * from xc_uv_order where  status = 1 and uv_id = ? and is_delete = 0 ",vehicle_id_);
//                if(uv_order.size()>100){
//                    std::cout << "抢单数已超过当前配送最大数量，请先完成订单..." << std::endl;
//                    break;
//                }
    // 私有方法
    void grab_processor() {
        constexpr int MAX_IDLE_CYCLES = 5;      // 5次空循环后进入深度休眠
        constexpr auto SHORT_SLEEP = 100ms;     // 常规检查间隔
        constexpr auto LONG_SLEEP = 5s;         // 无订单时的休眠时长

        int idle_cycles = 0;
        std::cout << "\n====== 抢单线程启动 ======\n"
                  << "线程ID: " << std::this_thread::get_id() << "\n"
                  << "车辆ID: " << vehicle_id_ << "\n"
                  << "=========================\n";
        try {
            while (running_) {
                // 1. 检查停止信号（带锁保护）
//                {
//                    std::unique_lock<std::mutex> lock(mtx_);
//                    if (cv_.wait_for(lock, SHORT_SLEEP, [this] { return !running_; })) {
//                        break; // 收到停止信号
//                    }
//                }
                // 2. 获取当前订单
                auto orders = OrderCache::get_vehicle_orders(vehicle_id_);
                // 3. 无订单时的处理
                if (orders.empty()) {
                    if (++idle_cycles >= MAX_IDLE_CYCLES) {
                        // 进入深度休眠（仍可被唤醒）
                        std::unique_lock<std::mutex> lock(mtx_);
                        cv_.wait_for(lock, LONG_SLEEP, [this] {
                            return !running_ ||
                                   !OrderCache::get_vehicle_orders(vehicle_id_).empty();
                        });
                        idle_cycles = 0; // 重置计数器
                    }
                    continue;
                }
                // 4. 有订单时的处理
                idle_cycles = 0; // 重置空闲计数器
                for (auto& [order_id, order_json] : orders) {
                    if (!running_) break; // 快速响应停止信号
                    auto result = grab_order(order_id);
                    if (!result.success) {
                        OrderCache::remove_order(vehicle_id_,order_id);
                        std::cerr << "订单处理失败: " << order_id
                                  << " 原因: " << result.message << "\n";
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "\n!!!!!! 线程异常 !!!!!!\n"
                      << "异常信息: " << e.what() << "\n"
                      << "线程ID: " << std::this_thread::get_id() << "\n"
                      << "!!!!!!!!!!!!!!!!!!!!!!\n";
            running_ = false;
        }
        std::cout << "====== 抢单线程终止 ======\n";
    }


    GrabResult grab_order(const std::string& order_id) {
        auto now = std::chrono::system_clock::now();
        std::cout << "[" << std::chrono::system_clock::to_time_t(now)
                  << "] 开始处理订单: " << order_id << std::endl;

        // 1. 获取Redis分布式锁
        std::string lock_key = "order_lock:" + order_id;
        RedisUtils::Lock lock(lock_key, 1000);
        std::cout << "尝试获取锁: " << lock_key << std::endl;

        if (!lock.TryLock()) {
            std::cout << "获取锁失败，订单可能正在被其他进程处理: " << order_id << std::endl;
            return {false, "抢单冲突，请重试", order_id, now};
        }
        std::cout << "成功获取分布式锁" << std::endl;

        // 2. 检查订单状态
        int64_t num = std::stoll(order_id);
        std::cout << "查询订单状态，订单ID: " << num << std::endl;
//
//        auto vec = SociDB::query<Order>(
//                "SELECT * FROM xc_uv_order WHERE order_id=? FOR UPDATE",
//                num
//        );
//        auto order =vec.empty() ? std::nullopt : std::optional(vec.front());
        auto order = SociDB::queryById<Order>(num);
        if (!order) {
            std::cout << "订单不存在: " << order_id << std::endl;
            return {false, "订单不存在", order_id, now};
        }

        if (order->status.value() != 0) {
            std::cout << "订单已被抢，当前状态: " << order->status.value()
                      << ", 当前接单车辆: " << order->uv_id.value_or(-1) << std::endl;
            return {false, "订单已被抢", order_id, now};
        }

        std::optional<int64_t> log_id;
        std::optional<int64_t> task_id;
        // 3. 开始事务
//        SociDB::Session sess;
//        soci::transaction tr(sess.get());  // 关键点：事务开始

        try {
            // 3. 更新订单状态（乐观锁）
            int new_version = order->version.value() + 1;
            std::cout << "尝试更新订单，新版本号: " << new_version << std::endl;

            auto result = SociDB::update(
                    "UPDATE xc_uv_order SET "
                    "status = 1, uv_id = ?, version = ? , updated_at = ? "
                    "WHERE order_id = ? AND version = ? and is_delete = 0 ",
                    vehicle_id_, new_version, now,num,
                    order->version.value()
            );

            if (result == 0) {
                std::cout << "抢单失败（版本冲突或条件不满足）" << std::endl;
                return {false, "抢单失败（版本冲突）", order_id, now};
            }

            auto end_time = std::chrono::system_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - now);
            long response_time_ms = duration.count();

//            tr.commit();
            // 6. 清理和通知
            remove_expired_order(order_id);
            notify_order_acquisition(*order);
            std::cout << "抢单成功处理完成: " << order_id << std::endl;

            auto& pub = ZmqGlobal::InstanceManager::getPublisher("tcp://*:5558");
            nlohmann::json task_msg = {
                    {"order_id", order_id},
                    {"uv_id", vehicle_id_},
                    {"response_time_ms", response_time_ms},
                    {"order_type_code", order->order_type_code.value()},
                    {"order_reward", order->reward.value()}
            };
            pub.publish("order_log_task", task_msg.dump());

            return {true, "抢单成功", order_id, now};
        } catch (const std::exception& e) {
            std::cerr << "抢单过程发生异常: " << e.what() << std::endl;
            return {false, std::string("系统错误: ") + e.what(), order_id, now};
        }
    }






    void remove_expired_order(const std::string& order_id) const {
        std::lock_guard<std::mutex> lock(orders_mutex_);
        OrderCache::remove_order(vehicle_id_, order_id);
//        available_orders_.erase(order_id);
        // 可选：打印日志确认操作
        std::cout << "已从Redis移除过期订单: " << order_id << std::endl;
    }

    static void notify_order_acquisition(Order  &order) {
        auto& pub = ZmqGlobal::InstanceManager::getPublisher("tcp://*:5557");
        pub.publish("order_update", std::to_string(order.id.value()), ZmqIn::ExchangeType::HEADERS, {{"type", std::to_string(order.order_type_code.value())}, {"channel", "update_orders"}});
    }

//    static void notify_order_acquisition(Order& order) {
//        auto code = OrderTypeEnum.findCode(order.order_type.value(), 3);
//        // 构造消息
//        ZmqRouter::Message msg{
//                "order_update",                             // topic
//                std::to_string(order.id.value()),           // content
//                "",                                         // routing_key
//                {{"type", std::to_string(code.value())},    // headers
//                 {"channel", "update_orders"}},
//                0,                                          // priority
//                ZmqRouter::ExchangeType::HEADERS             // type
//        };
//        // 异步发送（关键修改点）
//        ZmqRouter::instance().publish_async(msg);
//
//        // 可选的日志记录
//        std::cout << "[Async] Order notification queued: "
//                  << order.id.value() << std::endl;
//    }

    void cache_new_order(const std::string& order_key) {
        try {
            std::lock_guard<std::mutex> lock(orders_mutex_);
            available_orders_[order_key] = {
                    .expire_time = std::chrono::system_clock::now() +
                                   std::chrono::seconds(1800)
            };
        } catch (...) {
            Logger::Global().Error("订单缓存失败");
        }
    }

    // 消息订阅线程函数
    void message_subscriber()  {
        // 初始化ZMQ订阅（只执行一次）
        try {
            std::cout << "正在初始化消息订阅..." << std::endl;
            init_vehicle_subscriptions(vehicle_id_);
            std::cout << "消息订阅初始化成功" << std::endl;
            // 2. 设置订阅完成标志

            std::mutex mtx;
            std::unique_lock<std::mutex> lock(mtx);
            cv_.wait(lock, [this] { return !running_; });

//            zmq_ready_ = true;
//            sub_cv_.notify_all(); // 关键点：通知主线程
        } catch (const zmq::error_t& e) {
            std::cerr << "ZMQ订阅失败: " << e.what();
            throw std::runtime_error("ZMQ初始化失败");
        }
    }



public:
    explicit VehicleClient(int64_t id) : running_(true), vehicle_id_(id),zmq_context_(std::make_shared<zmq::context_t>(1)){
        std::lock_guard<std::mutex> lock(instances_mutex_);
        instances_[id] = this;
    }

    void start() {
//        std::lock_guard<std::mutex> lock(mtx_);
        if (running_) {
            if (!running_) {
                std::cerr << "错误：尝试在已停止状态启动\n";
                return;
            }
            // 更详细的启动日志
            try {
//                sub_thread_ = std::thread(&VehicleClient::message_subscriber, this);
                global_pool.detach_task([this]{VehicleClient::message_subscriber();},HIGH_PRIORITY);
//                // 等待订阅完成
//                std::unique_lock<std::mutex> lock(mtx_);
//                sub_cv_.wait_for(lock, 5s, [this]{
//                    return zmq_ready_.load();
//                });
                std::this_thread::sleep_for(std::chrono::milliseconds(100));

                std::cout << "正在创建抢单线程..." << std::endl;
//                grab_thread_ = std::thread(&VehicleClient::grab_processor, this);
                global_pool.detach_task([this]{VehicleClient::grab_processor();},HIGH_PRIORITY);

//                std::cout << "抢单线程创建成功，线程ID: " << grab_thread_.get_id() << std::endl;
            } catch (const std::exception& e) {
                running_ = false;
                std::cerr << "初始化失败: " << e.what() << std::endl;
                throw; // 重新抛出异常
            }
        }
    }

    void stop() {
        // 1. 设置停止标志并唤醒所有线程
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (!running_) return;
            running_ = false;
            cv_.notify_all(); // 唤醒抢单线程中的条件变量等待
        }
        // 2. 定义线程安全停止函数
        auto safe_stop_thread = [](std::thread& t) {
            if (t.joinable()) {
                if (t.get_id() != std::this_thread::get_id()) {
                    // 正常等待线程结束（带超时）
                    constexpr auto timeout = std::chrono::seconds(3);
                    auto start = std::chrono::steady_clock::now();

                    while (t.joinable() &&
                           (std::chrono::steady_clock::now() - start) < timeout) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    }

                    // 超时后的处理
                    if (t.joinable()) {
                        std::cerr << "线程停止超时，强制分离 (Thread ID: "
                                  << t.get_id() << ")\n";
                        t.detach(); // 最后手段
                    }
                } else {
                    t.detach(); // 避免自我死锁
                }
            }
        };

        // 3. 停止所有工作线程（按创建逆序）
//        safe_stop_thread(grab_thread_);  // 先停止抢单线程
//        safe_stop_thread(sub_thread_);   // 再停止订阅线程

        // 4. 清理资源（线程安全）
        try {
//            RedisUtils::ReleaseAllConnections(); // 确保所有连接释放
            // 4.1 清理Redis缓存（先于ZMQ关闭）
            RedisUtils::Del(OrderCache::get_vehicle_key(vehicle_id_));
            std::cout << "Redis缓存已清理 (VehicleID: " << vehicle_id_ << ")\n";

            if (zmq_context_) {
                zmq_context_->close();
                zmq_context_.reset(); // 手动释放（可选）
            }
            std::cout << "ZMQ资源已释放\n";

        } catch (const zmq::error_t& e) {
            std::cerr << "ZMQ关闭异常: " << e.what() << "\n";
        }
        // 5. 状态确认
        std::cout << "服务已完全停止\n";
        std::cout << "线程状态: "
//                  << "订阅线程=" << (sub_thread_.joinable() ? "活跃" : "已停止")
//                  << ", 抢单线程=" << (grab_thread_.joinable() ? "活跃" : "已停止")
                  << "\n";
    }


    ~VehicleClient() {
        stop(); // 析构时自动停止
        std::lock_guard<std::mutex> lock(instances_mutex_);
        instances_.erase(vehicle_id_);
    }

    // 业务消息处理函数（与ZmqRouter回调签名匹配）
    static void order_message_handler(
            const std::string& topic,
            const std::string& msg,
            const std::unordered_map<std::string, std::string>& headers)
    {
        // 1. 记录收到消息
        std::cout << "[" << DateUtil::getCurrentTime() << "] "
                  << "Received order on topic: " << topic
                  << ", size: " << msg.size() << " bytes" << std::endl;
        // 2. 解析并存储到Redis
//        global_pool.detach_task([&headers, &msg]{
        try {
            auto vehicle = headers.find("vehicle_id");
            int64_t vehicle_id;
            if (vehicle != headers.end()) {
                 vehicle_id = std::stoll(vehicle->second);
            }

            VehicleClient* instance = nullptr;
            {
                std::lock_guard<std::mutex> lock(instances_mutex_);
                auto it = instances_.find(vehicle_id);
                if (it == instances_.end()) return;
                instance = it->second;
            }
            // 操作实例时使用实例内部的锁
            std::lock_guard<std::mutex> inst_lock(instance->mtx_);
            instance->cv_.notify_all();

            auto j = nlohmann::json::parse(msg);
            for (auto& [order_id, order_data] : j.items()) {
                OrderCache::cache_order(vehicle_id, order_id, order_data.dump());
            }
            // 3. 触发后续业务处理
        } catch (const std::exception& e) {
            std::cerr << "Order process failed: " << e.what() << std::endl;
        }
//        });
    }

//    static ZmqRouter& zmq_sub_instance() {
//        static std::once_flag init_flag;
//        static ZmqRouter* router = nullptr;
//
//        std::call_once(init_flag, [] {
//            router = &ZmqRouter::instance();
//            router->init_subscriber("tcp://localhost:5556");
//        });
//        return *router; // 返回纯净的router实例
//    }
//
//    static ZmqRouter& zmq_pub_instance() {
//        static std::once_flag init_flag;
//        static ZmqRouter* router = nullptr;
//
//        std::call_once(init_flag, [] {
//            router = &ZmqRouter::instance();
//            ZmqRouter::instance().init_publisher("tcp://*:5556");
//        });
//        return *router; // 返回纯净的router实例
//    }


    static void order_update_handler(
            const std::string& topic,
            const std::string& msg,
            const std::unordered_map<std::string, std::string>& headers)
    {
//        global_pool.detach_task([&headers, &msg] {
            try {
                std::int64_t order_id = std::stoll(msg);
                auto order = SociDB::queryById<Order>(order_id);
                std::cout << "[" << DateUtil::getCurrentTime() << "] "
                          << "收到订单状态更新: order_id=" << order_id
                          << ", new_status=" << order->status.value() << std::endl;
                auto vehicle = headers.find("vehicle_id");
                int64_t vehicle_id;
                if (vehicle != headers.end()) {
                    vehicle_id = std::stoll(vehicle->second);
                }
                // 根据状态处理
                switch (order->status.value()) {
                    case 1: // 已接单
                        handle_order_accepted(msg, vehicle_id);
                        break;
                    case 2: // 已完成
//                    handle_order_completed(order_id);
                        break;
                    case -1: // 已取消
//                    handle_order_cancelled(order_id);
                        break;
                    default:
                        std::cerr << "未知订单状态: " << order->status.value() << std::endl;
                }
            } catch (const std::exception &e) {
                std::cerr << "订单更新处理失败: " << e.what() << std::endl;
            }
//        });
    }

    // 具体处理函数
    static void handle_order_accepted(const std::string & order_id,const std::int64_t& vehicle_id) {
        // 从Redis移除已接单订单
        OrderCache::remove_order(vehicle_id, order_id);
        // 更新本地缓存
//        {
//            std::lock_guard<std::mutex> lock(orders_mutex_);
//            available_orders_.erase(order_id);
//        }
        std::cout << "订单已接单处理完成: " << order_id << std::endl;
    }


    static void order_update_logAndTask(
            const std::string& topic,
            const std::string& msg,
            const std::unordered_map<std::string, std::string>& headers)
    {
        auto now = std::chrono::system_clock::now();
        std::optional<int64_t> log_id;
        std::optional<int64_t> task_id;
        bool flag= true;
        try {
            auto j = nlohmann::json::parse(msg);
            auto order_id = j["order_id"].get<std::string>();       // 明确转换为int64_t
            auto vehicle_id = j["uv_id"].get<int64_t>();       // 同上
            auto response_time_ms = j["response_time_ms"].get<long>();
            auto order_type_code = j["order_type_code"].get<std::int64_t>();
            auto order_reward = j["order_reward"].get<double>();
            std::string type_code = std::to_string(order_type_code);  // 安全转换
            // 4. 记录抢单日志
            GrabLog log;
            log.order_id = std::stoll(order_id);
            log.uv_id = vehicle_id;
            log.status = 1;
            log.result = 1;
            log.created_at = now;
            log.updated_at = now;
            log.bid_amount = order_reward;
            log.response_time=response_time_ms;
            log.is_delete=0;
            log_id = SociDB::insert(log);
            if(!log_id){
                std::cerr << "抢单日志记录失败: " << log_id.value() << std::endl;
                flag= false;
            }
            std::cout << "抢单日志记录成功，日志ID: " << log_id.value() << std::endl;
            DeliveryTask task;
            task.order_id = std::stoll(order_id);
            task.uv_id = vehicle_id;
            task.status = 1;
            task.start_time=now;
            task.created_at = now;
            task.updated_at = now;
            task.is_delete=0;
            task_id = SociDB::insert(task);
            if(!task_id){
                std::cerr << "抢单任务记录失败: " << log_id.value() << std::endl;
                flag= false;
            }
            std::cout << "配送任务创建成功，任务ID: " << task_id.value() << std::endl;
            if(!flag){
                SociDB::update("UPDATE xc_uv_order SET status=0, version=0, uv_id =NULL WHERE order_id=? AND status=1 ",order_id);
                // 6.2 清理已插入数据
                if (log_id) SociDB::remove<GrabLog>(*log_id);
                if (task_id) SociDB::remove<DeliveryTask>(*task_id);
                // 6.3 重发消息
                ZmqGlobal::InstanceManager::getPublisher("tcp://*:5557")
                        .publish("order_retry", order_id, ZmqIn::ExchangeType::HEADERS, {
                                {"type", type_code},
                                {"channel", "retry_orders"}
                        });
            }
        } catch (const std::exception &e) {
            std::cerr << "订单更新处理失败: " << e.what() << std::endl;
        }
    }







    // 业务订阅初始化（可选调用）
    static void init_vehicle_subscriptions(const std::int64_t & vehicle_id) {

//        auto& router = zmq_sub_instance(); // 复用已初始化的实例
        auto  uvehicle= SociDB::queryById<UVehicle>(vehicle_id);
        auto codes=uvehicle->supported_types;
        // 包装原始handler，注入vehicle_id到headers
        auto wrapped_handler = [&vehicle_id](const std::string& topic,
                                  const std::string& msg,
                                  const std::unordered_map<std::string, std::string>& headers) {
            auto new_headers = headers;
            new_headers["vehicle_id"] = std::to_string(vehicle_id);
            order_message_handler(topic, msg, new_headers);
        };

        auto wrapped_update_handler = [&vehicle_id](const std::string& topic,
                                         const std::string& msg,
                                         const std::unordered_map<std::string, std::string>& headers) {
            auto new_headers = headers;
            new_headers["vehicle_id"] = std::to_string(vehicle_id);
            order_update_handler(topic, msg, new_headers);
        };


        auto wrapped_update_log_task = [&vehicle_id](const std::string& topic,
                                                    const std::string& msg,
                                                    const std::unordered_map<std::string, std::string>& headers) {
            auto new_headers = headers;
            new_headers["vehicle_id"] = std::to_string(vehicle_id);
            order_update_logAndTask(topic, msg, new_headers);
        };



        auto& sub = ZmqGlobal::InstanceManager::getSubscriber("tcp://localhost:5556");
        auto& update_sub = ZmqGlobal::InstanceManager::getSubscriber("tcp://localhost:5557");
        auto& task_sub = ZmqGlobal::InstanceManager::getSubscriber("tcp://localhost:5558");


        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        task_sub.subscribe({"order_log_task"},wrapped_update_log_task,
                             ZmqIn::ExchangeType::DIRECT);


        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        update_sub.subscribe_headers(
                {{"type",codes.value()},{"channel","retry_orders"}},
                wrapped_handler,
                "order_retry");

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        update_sub.subscribe_headers(
                {{"type",codes.value()},{"channel","update_orders"}},
                wrapped_update_handler,
                "order_update");



        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // HEADERS模式订阅（车辆类型匹配）
        sub.subscribe_headers(
                {{"type",codes.value()},{"channel","vehicle_orders"}},
                wrapped_handler,
                "vehicle_orders");


    }
};


