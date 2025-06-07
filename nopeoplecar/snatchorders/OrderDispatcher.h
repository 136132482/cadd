#include "../../util/mq/zmq/ZmqManager.h"
#include "../../util/mq/zmq/ZmqInstance.h"

#include "../ParamObj.h"
#include "../../util/pgsql/SociDB.h"
#include "../../util/date/DateUtil.h"
#include <string>
#include <chrono>
#include "../../util/redis/RedisPool.h"
#include "../../nopeoplecar/OrderEnums.h"
#include "../../api/geocode/geoApi.h"
#include <utility> // for std::pair
#include "../../util/comment/common.h" // for std::pair

std::atomic<int> total_expired_messages_cleared{0};

std::atomic<int> total_orders_published{0};
std::mutex publish_mutex; // 全局互斥锁
using namespace std::chrono_literals;


//static ZmqRouter& zmq_instance() {
//    static std::once_flag init_flag;
//    static ZmqRouter* router = nullptr;
//
//    std::call_once(init_flag, [] {
//        router = &ZmqRouter::instance();
//        ZmqRouter::instance().init_publisher("tcp://*:5556");
//    });
//    return *router; // 返回纯净的router实例
//}


// 查询待发布的订单
template<typename T>
static SociDB::PageResult<T> fetchPendingOrders(const int& page,const int& pageSize) {
    auto now = std::chrono::system_clock::now();
    auto pageresult=  SociDB::queryPage<Order>(
            "SELECT * FROM xc_uv_order ",
            page,
            pageSize,
            {{"status","0"}},  // 精确条件
            {},  // 范围条件
            {},  // 模糊条件
            {},  // IN条件
            {}, //补充sql
            {}, //分组
            {"created_at desc"}// 排序
    );

//    return SociDB::query<Order>(
//            "SELECT * FROM xc_uv_order "
//            "WHERE status = 0 "
//            "AND expire_time > ? "
//            "ORDER BY created_at LIMIT 1000",
//            now
//    );
    return pageresult;
}



std::pair<std::string, std::string> getAddressToPoint(const Order& order) {
    // Lambda封装缓存逻辑
    auto get_cached_address = [](const std::string& point) {
        auto res = CommonUtils::wkbHexToPoint(point);

        auto cached = RedisUtils::HGet("point_address", res);
        if (cached.empty()) {
            cached = Geocoder::reverse_geocode(res);
            if (!cached.empty()) {
                RedisUtils::HSet("point_address", res, cached);
            }
        }
        return cached.empty() ? "未知地址" : cached;
    };
    return {
            get_cached_address(order.pickup.value()),
            get_cached_address(order.delivery.value())
    };
}


// 订单消息模板
std::string make_order_template(const Order& order) {
    auto [pickup_addr, delivery_addr] = getAddressToPoint(order);

    nlohmann::json json;
    // 确保键是字符串类型（即使id是数字）
    json[std::to_string(order.id.value())] = nlohmann::json::object({
                                                                            {"订单编号", order.order_no.value_or("NULL")},
                                                                            {"订单类型", order.order_type.value_or("未知")},
                                                                            {"取货地点", pickup_addr},
                                                                            {"送货地点", delivery_addr},
                                                                            {"发布时间", DateUtil::format_time_point(order.created_at)},
                                                                            {"奖励金额", order.reward.value_or(0.0)},
                                                                            {"配送距离", order.distance.value_or(0)},
                                                                            {"剩余时间", DateUtil::random_ttl()}
                                                                    });

    std::cout << "- Body: " << json.dump(4) << std::endl;
    return json.dump();
}


// 判断订单是否已过期
bool is_order_expired(const std::optional<std::chrono::system_clock::time_point>& expire_time) {
    // 如果没有过期时间，默认视为已过期
    if (!expire_time) return true;
    // 检查当前时间是否超过过期时间
    return std::chrono::system_clock::now() > *expire_time;
}

// 批量发布订单
void publish_orders_with_stats(const std::vector<Order>& orders) {
    std::lock_guard<std::mutex> lock(publish_mutex); // 自动加锁
    if (orders.empty()) return;
//    auto& router = zmq_instance();// 确保ZMQ已初始化
    int valid_orders = 0;
    auto batch_start = std::chrono::high_resolution_clock::now();
    auto& pub = ZmqGlobal::InstanceManager::getPublisher("tcp://*:5556");
    for (const auto& order : orders) {
//        if (is_order_expired(order.expire_time)) continue;
//        messages.push_back({
//                                   "vehicle_orders",
//                                   make_order_template(order),
//                                   "",
//                                   {
////                                           {"order_id", std::to_string(order.id.value_or(0))},
//                                           {"type", std::to_string(code.value())}
//                                   }
//                           });
        pub.publish("vehicle_orders", make_order_template(order), ZmqIn::ExchangeType::HEADERS, {{"type", std::to_string(order.order_type_code.value())}, {"channel", "vehicle_orders"}});
        valid_orders++;
    }

    auto batch_end = std::chrono::high_resolution_clock::now();
    auto batch_duration = std::chrono::duration_cast<std::chrono::milliseconds>(batch_end - batch_start);

    if (valid_orders>0) {
//        router.publish_batch(messages,ZmqRouter::ExchangeType::HEADERS);
        total_orders_published += valid_orders;
        std::cout << "[性能统计]\n"
                  << "批次订单数: " << valid_orders << "\n"
                  << "批次总耗时: " << batch_duration.count() << "ms\n"
                  << "平均每个订单耗时: " << batch_duration.count()/valid_orders << "ms\n"
                  << "累计已发布订单: " << total_orders_published << "\n";
    }
}

