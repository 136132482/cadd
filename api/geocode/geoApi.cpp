#include "geoApi.h"
#include "../../util/http/http_client.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <boost/algorithm/string/join.hpp>

Geocoder::Geocoder() : api_key_("97e00b3c420529181dc6e2ecb55a77a6") {}

Geocoder& Geocoder::instance() {
    static Geocoder instance;
    return instance;
}
std::string Geocoder::api_key() {
    static Geocoder instance;
    return instance.api_key_;
}


 std::string Geocoder::geocode(const std::string& address) {
     auto api_key=Geocoder::api_key();

     std::string url = "https://restapi.amap.com/v3/geocode/geo?address=" +
                      address + "&key=" + api_key;

    auto response = MyHttpClient::Get(url, {}, {{"Accept", "application/json"}});

    try {
        auto json = nlohmann::json::parse(response);
        if (json["status"] == "1" && !json["geocodes"].empty()) {
            std::string location = json["geocodes"][0]["location"];
            size_t comma_pos = location.find(',');
            if (comma_pos != std::string::npos) {
                return "POINT(" + location.replace(comma_pos, 1, " ") + ")";
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Geocoding failed: " << e.what()
                  << "\nResponse: " << response << std::endl;
    }

    return "POINT(0 0)";
}


std::string Geocoder::reverse_geocode(const std::string& location) {
    auto api_key = Geocoder::api_key();

    // 处理POINT格式或直接坐标
    std::string coord = location;
    if (coord.find("POINT(") != std::string::npos) {
        coord = coord.substr(6, coord.size() - 7); // 去掉POINT(和)
        std::replace(coord.begin(), coord.end(), ' ', ','); // 空格转逗号
    }

    std::string url = "https://restapi.amap.com/v3/geocode/regeo?location=" +
                      coord + "&key=" + api_key + "&extensions=base&output=JSON";

    auto response = MyHttpClient::Get(url, {}, {{"Accept", "application/json"}});

    try {
        auto json = nlohmann::json::parse(response);

        // 检查API响应状态
        if (json["status"] != "1") {
            throw std::runtime_error("API error: " + json["info"].get<std::string>());
        }

        // 多层地址信息回退机制
        const auto& regeocode = json["regeocode"];

        // 1. 优先使用格式化地址（处理可能为数组的情况）
        if (regeocode.contains("formatted_address")) {
            if (regeocode["formatted_address"].is_string()) {
                return regeocode["formatted_address"].get<std::string>();
            }
            if (regeocode["formatted_address"].is_array() && !regeocode["formatted_address"].empty()) {
                return regeocode["formatted_address"][0].get<std::string>();
            }
        }

        // 2. 拼接详细地址组件
        if (regeocode.contains("addressComponent")) {
            const auto& addr = regeocode["addressComponent"];
            std::vector<std::string> parts;

            if (addr["country"].is_string()) parts.push_back(addr["country"].get<std::string>());
            if (addr["province"].is_string()) parts.push_back(addr["province"].get<std::string>());
            if (addr["city"].is_string()) parts.push_back(addr["city"].get<std::string>());
            if (addr["district"].is_string()) parts.push_back(addr["district"].get<std::string>());
            if (addr["township"].is_string()) parts.push_back(addr["township"].get<std::string>());

            if (!parts.empty()) {
                return boost::algorithm::join(parts, "");
            }
        }

        // 3. 街道级信息
        if (regeocode.contains("streetNumber")) {
            const auto& street = regeocode["streetNumber"];
            if (street["street"].is_string() && street["number"].is_string()) {
                return street["street"].get<std::string>() + street["number"].get<std::string>();
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Reverse geocoding failed: " << e.what()
                  << "\nResponse: " << response << std::endl;
    }

    return "未知地址 (" + coord + ")";  // 包含原始坐标信息
}

int main() {
    // 测试地址（包含中文和空格）
    std::string geo = Geocoder::geocode("北京市海淀区中关村大街1号");
    std::cout << geo << std::endl; // 输出: POINT(116.31604 39.98293)

    std::string addr1 = Geocoder::reverse_geocode("116.31604,39.98293");
    std::string addr2 =  Geocoder::instance().reverse_geocode("POINT(116.31604 39.98293)");

    std::cout << "地址1: " << addr1 << std::endl;
    std::cout << "地址2: " << addr2 << std::endl;
    return 0;
}