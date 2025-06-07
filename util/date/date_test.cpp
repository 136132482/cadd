#include "DateUtil.h"
#include <vector>
#include <iostream>
#include <thread>
#include <cassert>

void test_cross_platform() {
    // 测试日期计算
    auto d1 = DateUtil::now();
    auto d2 = DateUtil::add_days(d1, 30);
    auto days=DateUtil::days_between(d1, d2);
    if (days != 30) {
        std::cout << "测试失败: days_between 结果应为 30" << std::endl;
    }

    // 测试月末处理
    auto end_of_feb = DateUtil::Date{2024, 2, 28};
    auto next_month = DateUtil::add_natural_months(end_of_feb, 1);
    if (next_month.month != 3 || next_month.day != 28) {
        std::cout << "测试失败: 月末处理不正确" << std::endl;
    }

    // 测试闰年
    if (!DateUtil::is_leap_year(2024)) {
        std::cout << "测试失败: 2024年应为闰年" << std::endl;
    }
    if (DateUtil::is_leap_year(2023)) {
        std::cout << "测试失败: 2023年不应为闰年" << std::endl;
    }

    // 测试线程安全
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([]{
            auto t = DateUtil::now();
            std::cout << DateUtil::format(t) << std::endl;
        });
    }
    for (auto& t : threads) t.join();
}

// 测试主函数
void test_date_util() {
    using std::cout;
    using std::endl;

    // 1. 获取当前时间
    auto now = DateUtil::now();
    cout << "当前时间: " << DateUtil::format(now) << endl;
    /* 输出示例: 当前时间: 2023-08-20 15:30:45 */

    // 2. 自然月加减（保持月末特性）
    auto date1 = DateUtil::Date{2023, 1, 31};
    auto next_month = DateUtil::add_natural_months(date1, 1);
    cout << "自然月加法: 2023-01-31 +1个月 = "
         << next_month.year << "-" << next_month.month << "-" << next_month.day << endl;
    /* 输出: 自然月加法: 2023-01-31 +1个月 = 2023-2-28 */

    // 3. 周期月加减（固定日期）
    auto cycle_next = DateUtil::add_cycle_months(date1, 1, 15);
    cout << "周期月加法: 2023-01-31 +1个月(固定15号) = "
         << DateUtil::format(cycle_next) << endl;
    /* 输出: 周期月加法: 2023-01-31 +1个月(固定15号) = 2023-02-15 00:00:00 */

    // 4. 天数加减
    auto date2 = DateUtil::Date{2023, 2, 28};
    auto after_3days = DateUtil::add_days(date2, 3);
    cout << "天数加法: 2023-02-28 +3天 = "
         << DateUtil::format(after_3days) << endl;
    /* 输出: 天数加法: 2023-02-28 +3天 = 2023-03-03 00:00:00 */

    // 5. 计算日期间隔
    auto date3 = DateUtil::Date{2023, 1, 1};
    auto date4 = DateUtil::Date{2023, 12, 31};
    cout << "天数差: 2023-01-01 到 2023-12-31 = "
         << DateUtil::days_between(date3, date4) << "天" << endl;
    /* 输出: 天数差: 2023-01-01 到 2023-12-31 = 364天 */

    // 6. 闰年判断
    cout << "闰年检测: 2024年? " << std::boolalpha
         << DateUtil::is_leap_year(2024) << endl;
    /* 输出: 闰年检测: 2024年? true */

    // 7. 获取月份天数
    cout << "2023年2月天数: " << DateUtil::days_in_month(2023, 2) << endl;
    /* 输出: 2023年2月天数: 28 */

    // 8. 计算星期几
    auto date5 = DateUtil::Date{2023, 8, 20};
    cout << "2023-08-20 是星期" << DateUtil::day_of_week(date5) << endl;
    /* 输出: 2023-08-20 是星期0 (周日) */

    // 9. 周运算
    auto next_week = DateUtil::add_weeks(date5, 1);
    cout << "周运算: 2023-08-20 +1周 = "
         << DateUtil::format(next_week) << endl;
    /* 输出: 周运算: 2023-08-20 +1周 = 2023-08-27 00:00:00 */

    // 10. 月首/末操作
    cout << "当月首日: " << DateUtil::format(DateUtil::month_start(date5)) << endl;
    cout << "下月首日: " << DateUtil::format(DateUtil::next_month_start(date5)) << endl;
    cout << "上月首日: " << DateUtil::format(DateUtil::prev_month_start(date5)) << endl;
    /* 输出:
       当月首日: 2023-08-01 00:00:00
       下月首日: 2023-09-01 00:00:00
       上月首日: 2023-07-01 00:00:00 */
}

// 边界测试
void test_edge_cases() {
    // 1. 闰年2月测试
    auto leap_feb = DateUtil::Date{2024, 2, 29};
    auto next_month = DateUtil::add_natural_months(leap_feb, 1);
    assert(next_month.month == 3 && next_month.day == 31);

    // 2. 跨年测试
    auto year_end = DateUtil::Date{2023, 12, 31};
    auto next_year = DateUtil::add_natural_months(year_end, 1);
    assert(next_year.year == 2024 && next_year.month == 1 && next_year.day == 31);

    // 3. 短月测试
    auto jan_30 = DateUtil::Date{2023, 1, 30};
    auto feb_day = DateUtil::add_natural_months(jan_30, 1);
    assert(feb_day.month == 2 && feb_day.day == 28);
}

int main() {
    test_date_util();
    test_edge_cases();
    std::cout << "所有测试通过!" << std::endl;
    return 0;
}



//int main() {
//    test_cross_platform();
//    return 0;
//}