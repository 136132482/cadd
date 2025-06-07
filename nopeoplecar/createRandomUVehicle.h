// UVehicleUtils.h
#pragma once
#include "ParamObj.h"
#include <random>
#include "../util/redis/RedisPool.h"
#include "../api/geocode/geoApi.h"
#include "../util/task/TaskScheduler.h"
#include "../util/pgsql/SociDB.h"
#include "OrderEnums.h"
#include <mutex>
#include "../util/comment/common.h"

std::mutex db_mutex; // 数据库操作互斥锁

// 生成随机无人车编号
std::string generateUVCode() {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
    return "UV-" + std::to_string(timestamp) +
           "-" + std::to_string(safe_random(100, 999));
}

// 创建随机无人车
UVehicle createRandomUVehicle() {
    UVehicle vehicle;
    vehicle.uv_code = generateUVCode();
    vehicle.model_type = safe_random(1, 3); // 1-四轮车 2-无人机 3-机器人
    vehicle.status = safe_random(0, 2); // 0-待机 1-忙碌 2-维护
    vehicle.battery = safe_random(20, 100); // 电量百分比
    vehicle.version = 1;
    vehicle.is_delete = 0;

    switch(vehicle.model_type.value()) {
        case 1: { // 四轮车
            auto all_code1 = OrderTypeEnum.getCodesByParamValue(0, "四轮车");
            vehicle.supported_types = CommonUtils::joinToString(CommonUtils::randomSelect(all_code1));

            break;
        }
        case 2: { // 无人机
            auto all_code2 = OrderTypeEnum.getCodesByParamValue(0, "无人机");
            vehicle.supported_types = CommonUtils::joinToString(CommonUtils::randomSelect(all_code2));
            break;
        }
        case 3: { // 机器人
            auto all_code3 = OrderTypeEnum.getCodesByParamValue(0, "机器人");
            vehicle.supported_types = CommonUtils::joinToString(CommonUtils::randomSelect(all_code3));
            break;
        }
        default:
            vehicle.supported_types = "";
            break;
    }

    auto capabilitiesCode=OrderTypeEnum.getDistinctCapabilitiesFromString(vehicle.supported_types.value(),3);
    vehicle.capabilities=CommonUtils::joinToString(capabilitiesCode);

    // 随机位置
    std::vector<std::string> locations = {
            "北京市朝阳区", "上海市浦东新区", "广州市天河区",
            "深圳市南山区", "成都市武侯区", "杭州市余杭区"
    };
    auto location = locations[safe_random(0, static_cast<int>(locations.size()-1))];

    // 获取或缓存地理编码
    auto location_adr = RedisUtils::HGet("geo", location);
    if(!location_adr.empty()) {
        vehicle.location = location_adr;
    } else {
        vehicle.location = Geocoder::geocode(location);
        RedisUtils::HSet("geo", location, vehicle.location.value());
    }

    // 时间戳
    auto now = std::chrono::system_clock::now();
    vehicle.created_at = now;
    vehicle.updated_at = now;
    vehicle.heartbeat_time = now;

    return vehicle;
}

// 批量创建无人车
void batchCreateUVehicles(std::vector<UVehicle>& vehicles) {
    std::lock_guard<std::mutex> lock(db_mutex);
    try {
        Logger::Global().Info("开始批量创建 " + std::to_string(vehicles.size()) + " 辆无人车");
        auto start = std::chrono::steady_clock::now();

        auto result = SociDB::bulkInsert(vehicles);
        auto duration = std::chrono::steady_clock::now() - start;

        Logger::Global().Info("批量创建完成，耗时: " +
                              std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(duration).count()) + "ms");

        if (result.has_value()) {
            const auto& inserted_ids = result.value();
            if (!inserted_ids.empty()) {
                std::vector<std::string> id_strings;
                id_strings.reserve(inserted_ids.size());
                for (const auto& id : inserted_ids) {
                    id_strings.push_back(std::to_string(id));
                }

                if (inserted_ids.size() <= 10) {
                    Logger::Global().Info("成功创建无人车ID: " +
                                          boost::algorithm::join(id_strings, ", "));
                } else {
                    Logger::Global().Debug("成功创建 " + std::to_string(inserted_ids.size()) +
                                           " 辆无人车，前10个ID: " +
                                           boost::algorithm::join(
                                                   std::vector<std::string>(id_strings.begin(),
                                                                            id_strings.begin() + std::min(10, (int)id_strings.size())),
                                                   ", "));
                }
            }
        }
    } catch (const std::exception& e) {
        Logger::Global().Error("批量创建无人车失败: " + std::string(e.what()));
    }
}