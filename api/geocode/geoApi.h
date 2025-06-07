#pragma once
#include <string>

class Geocoder {
public:
    // 禁用拷贝和赋值
    Geocoder(const Geocoder&) = delete;
    Geocoder& operator=(const Geocoder&) = delete;

    // 获取单例实例
    static Geocoder& instance();
    static std::string api_key();

    // 地理编码函数
     static std::string geocode(const std::string& address);
    static std::string reverse_geocode(const std::string& location); // 新增逆向地理编码


private:
    Geocoder(); // 私有构造函数
    const std::string api_key_;

};