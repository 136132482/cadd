// time_utils.h
#pragma once
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace time_utils {

    inline std::tm gmtime(const std::time_t& time) {
        std::tm tm;
#if defined(_WIN32) || defined(_WIN64)
        gmtime_s(&tm, &time); // Windows版本
#else
        gmtime_r(&time, &tm); // Linux/macOS版本
#endif
        return tm;
    }

    // 跨平台时间函数
    inline std::tm safe_gmtime(std::time_t time) {
        std::tm tm;
#if defined(_WIN32)
        gmtime_s(&tm, &time);
#else
        gmtime_r(&time, &tm);
#endif
        return tm;
    }

    inline std::tm localtime(const std::time_t& time) {
        std::tm tm;
#if defined(_WIN32) || defined(_WIN64)
        localtime_s(&tm, &time); // Windows版本
#else
        localtime_r(&time, &tm); // Linux/macOS版本
#endif
        return tm;
    }

} // namespace time_utils



namespace soci_datetime {
    // time_point → 数据库字符串
    static std::string to_string(const std::chrono::system_clock::time_point& tp) {
        std::time_t t = std::chrono::system_clock::to_time_t(tp);
        std::tm tm = *std::localtime(&t);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }

    // 字符串 → time_point
    static std::chrono::system_clock::time_point from_string(const std::string& s) {
        std::tm tm = {};
        std::istringstream iss(s);
        iss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
        return std::chrono::system_clock::from_time_t(std::mktime(&tm));
    }
}}