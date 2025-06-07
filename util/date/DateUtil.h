#pragma once

#include <chrono>
#include <ctime>
#include <string>
#include <stdexcept>
#include "time_utils.h"
#include <random>

class DateUtil {
public:
    // 日期结构体
    struct Date {
        int year;
        int month;  // 1-12
        int day;    // 1-31
        int hour;
        int minute;
        int second;

        // 转换为time_t
        time_t to_time_t() const;
    };

    // 静态方法声明
    static std::string getCurrentTime();
    static Date now();
    static Date add_natural_months(const Date& date, int months);
    static Date add_cycle_months(const Date& date, int months, int fixed_day = 1);
    static Date add_days(const Date& date, int days);
    static int days_between(const Date& start, const Date& end);
    static bool is_leap_year(int year);
    static int days_in_month(int year, int month);
    static int day_of_week(const Date& date);
    static Date add_weeks(const Date& date, int weeks);
    static Date month_start(const Date& date);
    static Date next_month_start(const Date& date);
    static Date prev_month_start(const Date& date);
    static std::string format(const Date& date, const char* fmt = "%Y-%m-%d %H:%M:%S");
    static bool compaCurr(const std::chrono::system_clock::time_point& tp);
    static std::string format_time_point(const std::optional<std::chrono::system_clock::time_point>& tp);
    static std::string random_ttl();
};