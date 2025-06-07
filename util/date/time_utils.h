// time_utils.h
#pragma once
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace time_utils {

    // 跨平台安全的localtime封装
    inline   tm* safe_localtime(const time_t* timer, tm* result) {
#if defined(_WIN32) || defined(_WIN64)
        return localtime_s(result, timer) ? nullptr : result;
#else
        return localtime_r(timer, result);
#endif
    }


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

    // 将时间戳(秒)格式化为字符串
    inline std::string FormatTimestamp(time_t timestamp,
                                       const std::string& format = "%Y-%m-%d %H:%M:%S") {
        // 线程安全的时间转换
        struct tm tm;
#ifdef _WIN32
        localtime_s(&tm, &timestamp);
#else
        localtime_r(&timestamp, &tm);
#endif

        char buffer[80];
        std::strftime(buffer, sizeof(buffer), format.c_str(), &tm);
        return buffer;
    }

    template<typename Clock>
    static std::string FormatTime(const std::chrono::time_point<Clock>& tp,
                                  const std::string& format = "%Y-%m-%d %H:%M:%S") {
        auto tt = Clock::to_time_t(tp);

        // 线程安全的时间转换
        struct tm tm;
#ifdef _WIN32
        localtime_s(&tm, &tt);
#else
        localtime_r(&tt, &tm);
#endif

        char buffer[80];
        std::strftime(buffer, sizeof(buffer), format.c_str(), &tm);
        return buffer;
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
}
