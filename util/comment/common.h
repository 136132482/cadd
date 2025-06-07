#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include <random>
#include <numeric>
//#include <gdal/ogr_geometry.h>

class CommonUtils {
public:
    // 随机选择1到max_count个元素
    template<typename T>
    static std::vector<T> randomSelect(const std::vector<T>& collection, int max_count = 3) {
        if (collection.empty()) return {};

        std::vector<T> result;
        int select_count = std::min(1 + (rand() % max_count), (int)collection.size());
        std::sample(collection.begin(), collection.end(),
                    std::back_inserter(result),
                    select_count,
                    std::mt19937(std::random_device()()));
        return result;
    }

    // 通用版本（数值类型）
    // 字符串版本
    static std::string joinToString(const std::vector<std::string>& collection,
                             const std::string& delimiter = ",") {
        std::string result;
        for (const auto& item : collection) {
            if (!result.empty()) result += delimiter;
            result += item;
        }
        return result;
    }

    static auto parseOrderType(const std::string& order_type) {
        std::vector<std::string> parts;
        boost::split(parts, order_type, boost::is_any_of(","));
        return std::make_pair(parts, parts.size());
    }

    // 数值类型版本
    template<typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
    static std::string joinToString(const std::vector<T>& collection,
                             const std::string& delimiter = ",") {
        std::string result;
        for (const auto& item : collection) {
            if (!result.empty()) result += delimiter;
            result += std::to_string(item);
        }
        return result;
    }

    // 从字符串分割（"1,2,3" -> vector<int>{1,2,3}）
    static std::vector<int> splitToInt(const std::string& str,
                                       char delimiter = ',') {
        std::vector<int> result;
        size_t start = 0, end = str.find(delimiter);

        while (end != std::string::npos) {
            result.push_back(std::stoi(str.substr(start, end - start)));
            start = end + 1;
            end = str.find(delimiter, start);
        }
        if (!str.empty()) result.push_back(std::stoi(str.substr(start)));

        return result;
    }

    // 线程安全版本（使用自定义随机引擎）
    template<typename T>
    static std::vector<T> threadSafeRandomSelect(
            const std::vector<T>& collection,
            std::mt19937& gen,
            int max_count = 3)
    {
        if (collection.empty()) return {};

        std::vector<T> result;
        std::uniform_int_distribution<> dis(1, std::min(max_count, (int)collection.size()));
        std::sample(collection.begin(), collection.end(),
                    std::back_inserter(result),
                    dis(gen),
                    gen);
        return result;
    }


    // 解析WKB十六进制字符串为经纬度
    static std::pair<double, double> parsePostGISPoint(const std::string& hex) {
        if (hex.length() < 32) return {0, 0};

        // 提取经度部分（最后16字节的前8字节）
        std::string lon_hex = hex.substr(16, 16);
        uint64_t lon_bits;
        std::stringstream(lon_hex) >> std::hex >> lon_bits;
        double lon = *reinterpret_cast<double*>(&lon_bits);

        // 提取纬度部分（最后16字节的后8字节）
        std::string lat_hex = hex.substr(32, 16);
        uint64_t lat_bits;
        std::stringstream(lat_hex) >> std::hex >> lat_bits;
        double lat = *reinterpret_cast<double*>(&lat_bits);

        return {lon, lat};
    }

//
//    std::string wkbHexToWkt(const std::string& wkbHex) {
//        // 1. 十六进制转二进制
//        OGRGeometry* geom;
//        size_t len = wkbHex.length() / 2;
//        unsigned char* wkb = new unsigned char[len];
//        for(size_t i=0; i<len; i++) {
//            sscanf(wkbHex.substr(i*2,2).c_str(), "%2hhx", &wkb[i]);
//        }
//
//        // 2. 解析几何对象
//        OGRGeometryFactory::createFromWkb(wkb, nullptr, &geom);
//
//        // 3. 生成WKT
//        char* wkt;
//        geom->exportToWkt(&wkt);
//        std::string result(wkt);
//
//        // 4. 清理内存
//        OGRGeometryFactory::destroyGeometry(geom);
//        delete[] wkb;
//        CPLFree(wkt);
//
//        return result; // 如 "POINT (116.404 39.915)"
//    }

// 预处理：确保输入是合法的WKB格式

// 将PostGIS的十六进制WKB转换为"POINT(lon lat)"
    static std::string wkbHexToPoint(const std::string& wkbHex) {
        // 1. 检查最小长度 (POINT+SRID至少25字节=50字符)
        if (wkbHex.length() < 50 || wkbHex.substr(0, 2) != "01") {
            throw std::runtime_error("Invalid WKB length or format");
        }

        // 2. 提取坐标部分（跳过9字节：1字节序+4类型+4SRID）
        auto hexToDouble = [](const std::string& hex) {
            if (hex.length() != 16) throw std::runtime_error("Invalid coordinate length");

            // 小端序解析（PostGIS默认）
            uint64_t bits;
            std::stringstream ss;
            for (int i = 7; i >= 0; --i) { // 反向字节序处理
                ss << hex.substr(i*2, 2);
            }
            ss >> std::hex >> bits;

            double value;
            memcpy(&value, &bits, sizeof(double));
            return value;
        };

        // 3. 解析经纬度
        double lon = hexToDouble(wkbHex.substr(18, 16)); // 经度（字节9-16）
        double lat = hexToDouble(wkbHex.substr(34, 16));  // 纬度（字节17-24）

        // 4. 有效性检查
        if (lon < -180 || lon > 180 || lat < -90 || lat > 90) {
            throw std::runtime_error("Coordinate out of valid range");
        }

        // 5. 生成WKT
        std::ostringstream oss;
        oss << "POINT(" << std::setprecision(9) << lon << " " << lat << ")";
        return oss.str();
    }
};