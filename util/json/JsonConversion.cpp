#include "JsonConversion.h"
#include "../pgsql/SociDemo.h"
#include <iostream>

int main() {
    // 1. 创建测试对象
    User user;
    user.id = 1;
    user.username = "test";
    user.email = "test@example.com";
    user.created_at = std::chrono::system_clock::now();

    // 2. 转换为JSON
    auto j = JsonConversion::toJson(user);

    // 3. 打印JSON内容
    std::cout << "=== 转换得到的JSON ===" << std::endl;
    std::cout << j.dump(4) << std::endl;

    // 4. 详细遍历打印
    std::cout << "\n=== 详细字段分析 ===" << std::endl;
    for (auto& [key, value] : j.items()) {
        std::cout << "字段 '" << key << "': ";

        if (value.is_null()) {
            std::cout << "NULL";
        } else if (value.is_string()) {
            std::cout << "字符串值: \"" << value.get<std::string>() << "\"";
        } else if (value.is_number_integer()) {
            std::cout << "整数值: " << value.get<int>();
        } else if (value.is_boolean()) {
            std::cout << "布尔值: " << std::boolalpha << value.get<bool>();
        }
        // 可以继续添加其他类型判断...

        std::cout << std::endl;
    }

    // 5. 转换回对象
    User user2 = JsonConversion::fromJson<User>(j);

    // 6. 验证转换结果
    std::cout << "\n=== 转换回对象验证 ===" << std::endl;
    std::cout << "id: " << user2.id.value() << std::endl;
    std::cout << "username: " << user2.username << std::endl;
    std::cout << "email: " <<  user2.email  << std::endl;

    // 格式化输出时间
    auto tt = std::chrono::system_clock::to_time_t(user2.created_at);
    std::tm tm = *std::localtime(&tt);
    std::cout << "created_at: " << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << std::endl;

    // 7. 验证数据一致性
    bool success = (user.id == user2.id) &&
                   (user.username == user2.username) &&
                   (user.email == user2.email);

    std::cout << "\n验证结果: " << (success ? "成功" : "失败") << std::endl;

    return 0;
}