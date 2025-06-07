#include "DateUtil.h"


// Date 结构体方法实现
time_t DateUtil::Date::to_time_t() const {
    tm tm{};
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_sec = second;
    return mktime(&tm);
}

// 静态方法实现
std::string DateUtil::getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

DateUtil::Date DateUtil::now() {
    auto now = std::chrono::system_clock::now();
    time_t tt = std::chrono::system_clock::to_time_t(now);
    tm tm;
    time_utils::safe_localtime(&tt,&tm);
    return {
            tm.tm_year + 1900,
            tm.tm_mon + 1,
            tm.tm_mday,
            tm.tm_hour,
            tm.tm_min,
            tm.tm_sec
    };
}

DateUtil::Date DateUtil::add_natural_months(const Date& date, int months) {
    // 1. 计算基础年月
    int total_months = date.year * 12 + (date.month - 1) + months;
    int new_year = total_months / 12;
    int new_month = (total_months % 12) + 1;

    // 2. 判断原日期是否月末
    bool is_original_end_of_month =
            (date.day == days_in_month(date.year, date.month));

    // 3. 计算新日
    int new_day;
    if (is_original_end_of_month) {
        new_day = days_in_month(new_year, new_month); // 保持月末
    } else {
        new_day = std::min(date.day, days_in_month(new_year, new_month));
    }

    return {new_year, new_month, new_day,
            date.hour, date.minute, date.second};
}

DateUtil::Date DateUtil::add_cycle_months(const Date& date, int months, int fixed_day) {
    Date result = add_natural_months(date, months);
    result.day = fixed_day;
    return result;
}

DateUtil::Date DateUtil::add_days(const Date& date, int days) {
    time_t tt = date.to_time_t();
    tt += days * 86400; // 24 * 3600
    tm tm;
    time_utils::safe_localtime(&tt, &tm);

    return {
            tm.tm_year + 1900,
            tm.tm_mon + 1,
            tm.tm_mday,
            date.hour,
            date.minute,
            date.second
    };
}

int DateUtil::days_between(const Date& start, const Date& end) {
    return static_cast<int>(
            (end.to_time_t() - start.to_time_t()) / 86400
    );
}

bool DateUtil::is_leap_year(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int DateUtil::days_in_month(int year, int month) {
    static const int days[] = {31,28,31,30,31,30,
                               31,31,30,31,30,31};
    if(month == 2 && is_leap_year(year))
        return 29;
    return days[month-1];
}

int DateUtil::day_of_week(const Date& date) {
    tm tm{};
    tm.tm_year = date.year - 1900;
    tm.tm_mon = date.month - 1;
    tm.tm_mday = date.day;
    mktime(&tm);
    return tm.tm_wday;
}

DateUtil::Date DateUtil::add_weeks(const Date& date, int weeks) {
    return add_days(date, weeks * 7);
}

DateUtil::Date DateUtil::month_start(const Date& date) {
    return {date.year, date.month, 1, 0, 0, 0};
}

DateUtil::Date DateUtil::next_month_start(const Date& date) {
    return add_natural_months(month_start(date), 1);
}

DateUtil::Date DateUtil::prev_month_start(const Date& date) {
    return add_natural_months(month_start(date), -1);
}

std::string DateUtil::format(const Date& date, const char* fmt) {
    tm tm{};
    tm.tm_year = date.year - 1900;
    tm.tm_mon = date.month - 1;
    tm.tm_mday = date.day;
    tm.tm_hour = date.hour;
    tm.tm_min = date.minute;
    tm.tm_sec = date.second;
    mktime(&tm);

    char buf[128];
    strftime(buf, sizeof(buf), fmt, &tm);
    return buf;
}

bool DateUtil::compaCurr(const std::chrono::system_clock::time_point& tp) {
    auto now_c = std::chrono::system_clock::to_time_t(tp);
    std::tm tm;
    localtime_s(&tm, &now_c);
    std::tm next_day = tm;
    next_day.tm_mday++;
    mktime(&next_day);
    if(next_day.tm_mon != tm.tm_mon){
        return false;
    }
    return true;
}


// 时间点转字符串
std::string DateUtil::format_time_point(const std::optional<std::chrono::system_clock::time_point>& tp) {
    if (!tp) return "";

    time_t tt = std::chrono::system_clock::to_time_t(tp.value());
    tm tm;
    time_utils::safe_localtime(&tt, &tm);

    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return buf;
}

// 生成随机剩余时间（单位：秒）
std::string DateUtil::random_ttl() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(60, 3600); // 随机1分钟到1小时

    int seconds = dis(gen);
    return std::to_string(seconds) + "秒";
}