#include <optional>
#include <string>
#include <chrono>
#include "../util/pgsql/soci_utils.h"



// 无人车信息结构体（所有字段改为optional）
struct UVehicle {
    std::optional<int64_t> id;
    std::optional<std::string> uv_code;
    std::optional<int32_t> model_type;
    std::optional<int32_t> status;
    std::optional<int32_t> battery;
    std::optional<std::string> capabilities;
    std::optional<std::string> location;
    std::optional<int32_t> version;
    std::optional<std::string> supported_types; // 支持的类型列表，逗号分隔
    std::optional<std::chrono::system_clock::time_point> heartbeat_time;
    std::optional<std::chrono::system_clock::time_point> created_at;
    std::optional<std::chrono::system_clock::time_point> updated_at;
    std::optional<int32_t> is_delete;
};

SOCI_MAP(UVehicle, "xc_uv_vehicle",
         soci_helpers::make_pair(&UVehicle::id, "uv_id", true),
         soci_helpers::make_pair(&UVehicle::uv_code, "uv_code"),
         soci_helpers::make_pair(&UVehicle::model_type, "model_type"),
         soci_helpers::make_pair(&UVehicle::status, "status"),
         soci_helpers::make_pair(&UVehicle::battery, "battery"),
         soci_helpers::make_pair(&UVehicle::capabilities, "capabilities"),
         soci_helpers::make_pair(&UVehicle::location, "location"),
         soci_helpers::make_pair(&UVehicle::version, "version"),
         soci_helpers::make_pair(&UVehicle::supported_types, "supported_types"),
         soci_helpers::make_pair(&UVehicle::heartbeat_time, "heartbeat_time"),
         soci_helpers::make_pair(&UVehicle::created_at, "created_at"),
         soci_helpers::make_pair(&UVehicle::updated_at, "updated_at"),
         soci_helpers::make_pair(&UVehicle::is_delete, "is_delete")
);

// 订单信息结构体（所有字段改为optional）
struct Order {
    std::optional<int64_t> id;
    std::optional<std::string> order_no;
    std::optional<int64_t> merchant_id;
    std::optional<double> reward;
    std::optional<int32_t> required_type;
    std::optional<std::string> pickup;
    std::optional<std::string> delivery;
    std::optional<int32_t> distance;
    std::optional<int32_t> status;
    std::optional<int32_t> version;
    std::optional<std::string> order_type;
    std::optional<int64_t> order_type_code;
    std::optional<std::chrono::system_clock::time_point> expire_time;
    std::optional<int64_t> uv_id;
    std::optional<std::chrono::system_clock::time_point> created_at;
    std::optional<std::chrono::system_clock::time_point> updated_at;
    std::optional<int32_t> is_delete;
};

SOCI_MAP(Order, "xc_uv_order",
         soci_helpers::make_pair(&Order::id, "order_id", true),
         soci_helpers::make_pair(&Order::order_no, "order_no"),
         soci_helpers::make_pair(&Order::merchant_id, "merchant_id"),
         soci_helpers::make_pair(&Order::reward, "reward"),
         soci_helpers::make_pair(&Order::required_type, "required_type"),
         soci_helpers::make_pair(&Order::pickup, "pickup"),
         soci_helpers::make_pair(&Order::delivery, "delivery"),
         soci_helpers::make_pair(&Order::distance, "distance"),
         soci_helpers::make_pair(&Order::status, "status"),
         soci_helpers::make_pair(&Order::version, "version"),
         soci_helpers::make_pair(&Order::order_type, "order_type"),
         soci_helpers::make_pair(&Order::order_type_code, "order_type_code"),
         soci_helpers::make_pair(&Order::expire_time, "expire_time"),
         soci_helpers::make_pair(&Order::uv_id, "uv_id"),
         soci_helpers::make_pair(&Order::created_at, "created_at"),
         soci_helpers::make_pair(&Order::updated_at, "updated_at"),
         soci_helpers::make_pair(&Order::is_delete, "is_delete")
);

// 抢单记录结构体（所有字段改为optional）
struct GrabLog {
    std::optional<int64_t> id;
    std::optional<int64_t> order_id;
    std::optional<int32_t> status;
    std::optional<int64_t> uv_id;
    std::optional<int32_t> result;
    std::optional<double> bid_amount;
    std::optional<int32_t> response_time;
    std::optional<std::chrono::system_clock::time_point> created_at;
    std::optional<std::chrono::system_clock::time_point> updated_at;
    std::optional<int32_t> is_delete;
};

SOCI_MAP(GrabLog, "xc_uv_grab_log",
         soci_helpers::make_pair(&GrabLog::id, "log_id", true),
         soci_helpers::make_pair(&GrabLog::order_id, "order_id"),
         soci_helpers::make_pair(&GrabLog::status, "status"),
         soci_helpers::make_pair(&GrabLog::uv_id, "uv_id"),
         soci_helpers::make_pair(&GrabLog::result, "result"),
         soci_helpers::make_pair(&GrabLog::bid_amount, "bid_amount"),
         soci_helpers::make_pair(&GrabLog::response_time, "response_time"),
         soci_helpers::make_pair(&GrabLog::created_at, "created_at"),
         soci_helpers::make_pair(&GrabLog::updated_at, "updated_at"),
         soci_helpers::make_pair(&GrabLog::is_delete, "is_delete")
);

// 配送任务结构体（所有字段改为optional）
struct DeliveryTask {
    std::optional<int64_t> id;
    std::optional<int64_t> order_id;
    std::optional<int64_t> uv_id;
    std::optional<int32_t> actual_distance;
    std::optional<std::chrono::system_clock::time_point> start_time;
    std::optional<std::chrono::system_clock::time_point> end_time;
    std::optional<int32_t> status;
    std::optional<std::chrono::system_clock::time_point> created_at;
    std::optional<std::chrono::system_clock::time_point> updated_at;
    std::optional<int32_t> is_delete;
};

SOCI_MAP(DeliveryTask, "xc_uv_delivery",
         soci_helpers::make_pair(&DeliveryTask::id, "task_id", true),
         soci_helpers::make_pair(&DeliveryTask::order_id, "order_id"),
         soci_helpers::make_pair(&DeliveryTask::uv_id, "uv_id"),
         soci_helpers::make_pair(&DeliveryTask::actual_distance, "actual_distance"),
         soci_helpers::make_pair(&DeliveryTask::start_time, "start_time"),
         soci_helpers::make_pair(&DeliveryTask::end_time, "end_time"),
         soci_helpers::make_pair(&DeliveryTask::status, "status"),
         soci_helpers::make_pair(&DeliveryTask::created_at, "created_at"),
         soci_helpers::make_pair(&DeliveryTask::updated_at, "updated_at"),
         soci_helpers::make_pair(&DeliveryTask::is_delete, "is_delete")
);

// 四表联合查询结果结构体（所有字段改为optional）
struct OrderFullInfo {
    // 订单信息
    std::optional<int64_t> order_id;
    std::optional<std::string> order_no;
    std::optional<double> reward;
    std::optional<int16_t> order_status;
    std::optional<std::chrono::system_clock::time_point> order_created_at;
    std::optional<std::chrono::system_clock::time_point> order_updated_at;
    std::optional<int16_t> order_is_delete;

    // 无人车信息
    std::optional<int64_t> uv_id;
    std::optional<std::string> uv_code;
    std::optional<int16_t> uv_status;
    std::optional<std::chrono::system_clock::time_point> uv_created_at;
    std::optional<std::chrono::system_clock::time_point> uv_updated_at;
    std::optional<int16_t> uv_is_delete;

    // 抢单记录
    std::optional<int64_t> grab_log_id;
    std::optional<int16_t> grab_result;
    std::optional<double> bid_amount;
    std::optional<std::chrono::system_clock::time_point> grab_updated_at;
    std::optional<int16_t> grab_is_delete;

    // 配送任务
    std::optional<int64_t> task_id;
    std::optional<int32_t> actual_distance;
    std::optional<int16_t> delivery_status;
    std::optional<std::chrono::system_clock::time_point> delivery_updated_at;
    std::optional<int16_t> delivery_is_delete;
};

SOCI_MAP(OrderFullInfo, "联合查询结果",
// 订单字段
         soci_helpers::make_pair(&OrderFullInfo::order_id, "order_id"),
         soci_helpers::make_pair(&OrderFullInfo::order_no, "order_no"),
         soci_helpers::make_pair(&OrderFullInfo::reward, "reward"),
         soci_helpers::make_pair(&OrderFullInfo::order_status, "status"),
         soci_helpers::make_pair(&OrderFullInfo::order_created_at, "order_created_at"),
         soci_helpers::make_pair(&OrderFullInfo::order_updated_at, "order_updated_at"),
         soci_helpers::make_pair(&OrderFullInfo::order_is_delete, "order_is_delete"),

// 无人车字段
         soci_helpers::make_pair(&OrderFullInfo::uv_id, "uv_id"),
         soci_helpers::make_pair(&OrderFullInfo::uv_code, "uv_code"),
         soci_helpers::make_pair(&OrderFullInfo::uv_status, "uv_status"),
         soci_helpers::make_pair(&OrderFullInfo::uv_created_at, "uv_created_at"),
         soci_helpers::make_pair(&OrderFullInfo::uv_updated_at, "uv_updated_at"),
         soci_helpers::make_pair(&OrderFullInfo::uv_is_delete, "uv_is_delete"),

// 抢单记录字段
         soci_helpers::make_pair(&OrderFullInfo::grab_log_id, "log_id"),
         soci_helpers::make_pair(&OrderFullInfo::grab_result, "result"),
         soci_helpers::make_pair(&OrderFullInfo::bid_amount, "bid_amount"),
         soci_helpers::make_pair(&OrderFullInfo::grab_updated_at, "grab_updated_at"),
         soci_helpers::make_pair(&OrderFullInfo::grab_is_delete, "grab_is_delete"),

// 配送任务字段
         soci_helpers::make_pair(&OrderFullInfo::task_id, "task_id"),
         soci_helpers::make_pair(&OrderFullInfo::actual_distance, "actual_distance"),
         soci_helpers::make_pair(&OrderFullInfo::delivery_status, "delivery_status"),
         soci_helpers::make_pair(&OrderFullInfo::delivery_updated_at, "delivery_updated_at"),
         soci_helpers::make_pair(&OrderFullInfo::delivery_is_delete, "delivery_is_delete")
);