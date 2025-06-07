#include "../util/task/TaskScheduler.h"
#include "../util/pgsql/SociDB.h"
#include "ParamObj.h"
#include "OrderEnums.h"
#include <atomic>
#include <iomanip>
#include <iostream> // Added for std::cout
#include <chrono>   // Added for std::chrono
#include <vector>
#include <string>
#include <mutex>
#include "../api/geocode/geoApi.h"
#include "../util/redis/RedisPool.h"

std::mutex db_mutex; // 数据库操作互斥锁

// 定义全局枚举对象


std::string generateOrderNumber() {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();

    return "ORD-" + std::to_string(timestamp) +
           "-" + std::to_string(safe_random(1000, 9999));
}

// Order type enum
enum class NoCarOrderType {
    DAILY_GOODS = 101,  // 日常百货
    FOOD = 102,         // 餐饮
    MEDICINE = 201,     // 医药
    DELIVERY = 301,     // 快递
    ELECTRONICS = 401,  // 电子产品
    FROZEN_FOOD = 501,  // 冷冻食品
    DOCUMENTS = 601,    // 文件
    FLOWERS = 701,      // 鲜花
    CLOTHING = 801,     // 服装
    BOOKS = 901         // 图书
};

// Order creation function
Order createRandomOrder(NoCarOrderType type) {
    Order order;
    order.order_no = generateOrderNumber();
    order.merchant_id = safe_random(1, 1000);
//    order.reward = std::round(safe_random(10.0, 500.0) * 100) / 100; // 保留2位小数
    order.reward = std::stod((std::ostringstream() << std::fixed << std::setprecision(2) << std::round(safe_random(10.0, 500.0) * 100) / 100).str());

    order.status = 0; // Using enum value
    order.distance = safe_random(1, 50);
//    order.required_type=safe_random(1,3);
    order.version = 1;
    order.is_delete = 0;
    // Get type info from enum
    auto type_code = static_cast<int>(type);
    const auto& params = OrderTypeEnum.getParams(type_code);
    if (!params.empty()) {
        order.order_type = boost::algorithm::join(params, ",");
    }
    order.order_type_code=type_code;
    // Set timestamps
    auto now = std::chrono::system_clock::now();
    order.created_at = now;
    order.updated_at = now;
    order.expire_time = now + std::chrono::hours(safe_random(1, 72));  // UTC+8 时间
    // Random locations
    std::vector<std::string> locations = {
            "北京市朝阳区", "上海市浦东新区", "广州市天河区",
            "深圳市南山区", "成都市武侯区", "杭州市余杭区"
    };
    auto pickup_loacation  = locations[safe_random(0, static_cast<int>(locations.size()-1))];
    auto delivery_loacation  = locations[safe_random(0, static_cast<int>(locations.size()-1))];
    auto pickup_adr=RedisUtils::HGet("geo",pickup_loacation);
    if(!pickup_adr.empty()){
        order.pickup=pickup_adr;
    }else{
        order.pickup =Geocoder::geocode(pickup_loacation);
        RedisUtils::HSet("geo", pickup_loacation,order.pickup.value());
    }
    auto delivery_adr=RedisUtils::HGet("geo",delivery_loacation);
    if(!delivery_adr.empty()){
        order.delivery =delivery_adr;
    }else{
        order.delivery  =Geocoder::geocode(delivery_loacation);
        RedisUtils::HSet("geo", delivery_loacation,order.delivery.value());
    }
    return order;
}

// Status description function
std::string getStatusDescription(int status_code) {
    auto params = OrderStatusEnum.getParams(status_code);
    return params.empty() ? "未知状态" : params[0];
}


// 批量创建订单（线程安全）
void batchCreateOrders(std::vector<Order>& orders) {
    std::lock_guard<std::mutex> lock(db_mutex);
    try {
        Logger::Global().Info("开始批量创建 " + std::to_string(orders.size()) + " 个订单");
        auto start = std::chrono::steady_clock::now();
        // Execute bulk insert and get optional result
        auto result = SociDB::bulkInsert(orders);
        auto duration = std::chrono::steady_clock::now() - start;
        Logger::Global().Info("批量创建完成，耗时: " +
                              std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(duration).count()) + "ms");
        // Check if optional contains value
        if (result.has_value()) {
            const auto& inserted_ids = result.value(); // Get the vector from optional
            // Log the inserted IDs
            if (!inserted_ids.empty()) {
                std::vector<std::string> id_strings;
                id_strings.reserve(inserted_ids.size());
                for (const auto& id : inserted_ids) {
                    id_strings.push_back(std::to_string(id));
                }
                if (inserted_ids.size() <= 10) {
                    Logger::Global().Info("成功创建订单ID: " +
                                          boost::algorithm::join(id_strings, ", "));
                } else {
                    Logger::Global().Debug("成功创建 " + std::to_string(inserted_ids.size()) +
                                           " 个订单，前10个ID: " +
                                           boost::algorithm::join(
                                                   std::vector<std::string>(id_strings.begin(),
                                                                            id_strings.begin() + std::min(10, (int)id_strings.size())),
                                                   ", "));
                }
            } else {
                Logger::Global().Warning("批量插入操作返回了空ID集合");
            }
        } else {
            Logger::Global().Warning("批量插入操作未返回ID集合");
        }
    } catch (const std::exception& e) {
        Logger::Global().Error("批量创建订单失败: " + std::string(e.what()));
    }
}


