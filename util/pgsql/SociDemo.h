#ifndef CADD_SOCIDEMO_H
#define CADD_SOCIDEMO_H

#pragma once
#include <soci/soci.h>
#include <soci/postgresql/soci-postgresql.h>
#include <memory>
#include <optional>
#include <vector>
#include <functional>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include "soci_utils.h"
#include "SociDB.h"
#include "../ssl/CryptoUtil.h"
#include <nlohmann/json.hpp> // 需要安装json库


struct User {
    std::optional<int>  id;
    std::string username;
    std::string email;
    std::string password;
    std::optional<int> status;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point updated_at;
};


struct Product {
    std::optional<int> id;
    std::optional<std::string> name;
    std::optional<double> price;
    std::optional<int> categoryId;
    std::optional<int> stock;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point updated_at;
};



struct Order {
    std::optional<int>  id;
    std::optional<int> userId;
    std::optional<double> total_amount;
    std::optional<int> status;  // 新增状态字段
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point updated_at;

    Order(int user_id,double total_amount, int status):
        userId(user_id),total_amount(total_amount),status(status){}

    Order() = default;

};



struct OrderDetail {
    std::optional<int>  id;
    std::optional<int>  productId;
    std::optional<int>  quantity;
    std::optional<double> price;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point updated_at;
};

// 四表联合查询结果结构体
struct OrderFullInfo {
    std::optional<int>  order_id;
    std::optional<double> total_amount;
    std::optional<int>  order_status;
    std::chrono::system_clock::time_point order_created_at;

    std::optional<int>  user_id;
    std::string username;
    std::string email;

    std::optional<int>  product_id;
    std::string product_name;
    std::optional<double> product_price;

    std::optional<int>  quantity;
    std::optional<double> detail_price;
};


// User 映射
SOCI_MAP(User,"users",
         soci_helpers::make_pair(&User::id, "id",true),
         soci_helpers::make_pair(&User::username, "username"),
         soci_helpers::make_pair(&User::email, "email"),
         soci_helpers::make_pair(&User::password, "password"),
         soci_helpers::make_pair(&User::status, "status"),
         soci_helpers::make_pair(&User::created_at, "created_at"),
         soci_helpers::make_pair(&User::updated_at, "updated_at")
);

// Product 映射
SOCI_MAP(Product,"products",
         soci_helpers::make_pair(&Product::id, "id",true),
         soci_helpers::make_pair(&Product::name, "name"),
         soci_helpers::make_pair(&Product::price, "price"),
         soci_helpers::make_pair(&Product::categoryId, "category_id"),
         soci_helpers::make_pair(&Product::stock, "stock"),
         soci_helpers::make_pair(&Product::created_at, "created_at"),
         soci_helpers::make_pair(&Product::updated_at, "updated_at")
);

// Order 映射
SOCI_MAP(Order,"orders",
         soci_helpers::make_pair(&Order::id, "id",true),
         soci_helpers::make_pair(&Order::userId, "user_id"),
         soci_helpers::make_pair(&Order::total_amount, "total_amount"),
         soci_helpers::make_pair(&Order::status, "status"),
         soci_helpers::make_pair(&Order::created_at, "created_at"),
         soci_helpers::make_pair(&Order::updated_at, "updated_at")
);

// OrderDetail 映射
SOCI_MAP(OrderDetail,"order_details",
         soci_helpers::make_pair(&OrderDetail::id, "order_id",true),
         soci_helpers::make_pair(&OrderDetail::productId, "product_id"),
         soci_helpers::make_pair(&OrderDetail::quantity, "quantity"),
         soci_helpers::make_pair(&OrderDetail::price, "price"),
         soci_helpers::make_pair(&OrderDetail::created_at, "created_at"),
         soci_helpers::make_pair(&OrderDetail::updated_at, "updated_at")
);

// 使用SOCI_MAP宏注册映射（完全复用现有模板）
SOCI_MAP(OrderFullInfo, "order_full_info_view",  // 视图名可自定义
         soci_helpers::make_pair(&OrderFullInfo::order_id, "order_id"),
         soci_helpers::make_pair(&OrderFullInfo::total_amount, "total_amount"),
         soci_helpers::make_pair(&OrderFullInfo::order_status, "order_status"),
         soci_helpers::make_pair(&OrderFullInfo::order_created_at, "order_created_at"),

         soci_helpers::make_pair(&OrderFullInfo::user_id, "user_id"),
         soci_helpers::make_pair(&OrderFullInfo::username, "username"),
         soci_helpers::make_pair(&OrderFullInfo::email, "email"),

         soci_helpers::make_pair(&OrderFullInfo::product_id, "product_id"),
         soci_helpers::make_pair(&OrderFullInfo::product_name, "product_name"),
         soci_helpers::make_pair(&OrderFullInfo::product_price, "product_price"),

         soci_helpers::make_pair(&OrderFullInfo::quantity, "quantity"),
         soci_helpers::make_pair(&OrderFullInfo::detail_price, "detail_price")
)


class SociDemo {
public:
    static void demo() {
        // 初始化连接池
        SociDB::init();

        queryone();

        // 1. 新增操作示例
        insertDemo();

//        // 2. 修改操作示例
        updateDemo();
//
//        // 3. 删除操作示例
        deleteDemo();
//
//        // 4. 查询操作示例
        queryDemo();
    }
    static void bulkOperationsDemo() {
        std::cout << "\n===== 批量操作示例 =====\n";

//        // 1. 批量新增示例
        bulkInsertDemo();
//
//        // 2. 批量修改示例
        bulkUpdateDemo();
//
//        // 3. 批量删除示例
        bulkDeleteDemo();
    }

private:
    static  void  queryone(){
        User searchUser;
        searchUser.username = "张三";
        auto result = SociDB::queryOne<User>(searchUser);
        std::cout << "对象 " << result->username << "" << std::endl;
        auto result1 = SociDB::queryById<Product>(1); // 已知存在的产品ID
        std::cout << "对象 " << result1->price.value() << "" << std::endl;
    }


    static void insertDemo() {
        std::cout << "\n===== 新增操作示例 =====\n";
        // 单条插入用户
        // 使用示例
        User newUser{
                .username = "王五",
                .email = "smmsssx@qq.com",
                .password = CryptoUtil::md5("password123"),
                .status = 1
        };

        if (auto flag = SociDB::insert(newUser)) {
            std::cout << "插入成功，ID: " << flag.value() << std::endl;
        }


        std::vector<User> users = {
                {.username = "user9",
                        .email = "email9@test.com",
                        .password = CryptoUtil::md5("pass1"),
                        .status = 1},

                {.username = "user10",
                        .email = "email10@test.com",
                        .password = CryptoUtil::md5("pass2"),
                        .status = 1},

                {.username = "user11",
                        .email = "email11@test.com",
                        .password = CryptoUtil::md5("pass3"),
                        .status = 0}
        };

        if (auto res=SociDB::bulkInsert(users)) {
            std::cout << "成功插入 " << res->size() << " 条记录，ID为：\n";
            for (int id : *res) {
                std::cout << id << " ";
            }
            std::cout << std::endl;
        }

// 批量产品插入
        std::vector<Product> products = {
                {.name="Laptop",
                 .price= 1299.99,
                 .categoryId=1,
                 .stock=10},
                {.name="Mouse",
                 .price=29.99,
                        .categoryId=1,
                        .stock=100},
                {.name="Keyboard",
                 .price=59.99,
                        .categoryId=1,
                        .stock=50,
                }
        };

        if (SociDB::bulkInsert(products)) {
            std::cout << "成功批量插入 " << products.size() << " 个产品" << std::endl;
        }

        Order  order(1,199.87, 1);

        // 单条插入订单
        if (auto flag=SociDB::insert<Order>(order)) {
            std::cout << "新增订单ID: " << flag.value() << std::endl;
        }
    }

    static void updateDemo() {
        std::cout << "\n===== 修改操作示例 =====\n";

        // 更新用户信息
        User user;
        user.id = 1; // 已有用户ID
        user.username = "new_username";
        user.email = "new_email@example.com";
        user.status = 2;

        if (SociDB::update(user)) {
            std::cout << "用户更新成功" << std::endl;
        } else {
            std::cerr << "用户更新失败" << std::endl;
        }

// 更新产品信息
        Product product;
        product.id = 7; // 已有产品ID
        product.price = 99.99;
        product.stock = 100;

        if (SociDB::update(product)) {
            std::cout << "产品更新成功" << std::endl;
        } else {
            std::cerr << "产品更新失败" << std::endl;
        }

        // 更新订单状态
        Order order;
        order.id = 789; // 已有订单ID
        order.status = 3; // 已发货

        if (SociDB::update(order)) {
            std::cout << "订单状态更新成功" << std::endl;
        } else {
            std::cerr << "订单状态更新失败" << std::endl;
        }


        // 批量更新用户状态
        std::vector<User> usersToUpdate = {
                {.id = 101, .status = 1}, // 注意：需要C++20支持指定初始化
                {.id = 102, .status = 1},
                {.id = 103, .status = 2}
        };

        if (SociDB::bulkUpdate(usersToUpdate)) {
            std::cout << "批量用户状态更新成功" << std::endl;
        } else {
            std::cerr << "批量用户状态更新失败" << std::endl;
        }

       // 批量调整产品价格
        std::vector<Product> productsToUpdate;
        productsToUpdate.push_back(Product{.id = 13, .price = 59.99}); // C++20
        productsToUpdate.push_back(Product{.id = 14, .price = 79.99});
        productsToUpdate.push_back(Product{.id = 15, .price = 89.99});

        if (SociDB::bulkUpdate(productsToUpdate)) {
            std::cout << "批量产品价格更新成功" << std::endl;
        } else {
            std::cerr << "批量产品价格更新失败" << std::endl;
        }

        // 批量更新订单状态（传统初始化方式）
        std::vector<Order> ordersToUpdate;
        Order o1; o1.id = 15; o1.status = 4; // 已完成
        Order o2; o2.id = 16; o2.status = 4;
        ordersToUpdate.push_back(o1);
        ordersToUpdate.push_back(o2);

        if (SociDB::bulkUpdate(ordersToUpdate)) {
            std::cout << "批量订单状态更新成功" << std::endl;
        } else {
            std::cerr << "批量订单状态更新失败" << std::endl;
        }



    }

    static void deleteDemo() {
        std::cout << "\n===== 删除操作示例 =====\n";

        // 单条删除测试用户 - 修改为通过User模板获取表信息
        if (SociDB::remove<User>(1)) {  // 假设test_user_id是测试用户的ID
            std::cout << "测试用户删除成功\n";
        }

        // 批量删除低价产品
        std::vector<std::int64_t > productIdsToDelete;

       // 查询用户(保持不变)
        auto users = SociDB::query<User>("SELECT * FROM users where id =6");
        std::cout << "找到 " << users.size() << " 条" << std::endl;
        for (const auto& p : users) {
            std::cout << "ID: " << p.id.value() << " 名称: " << p.status.value()
                      << " 价格: " << p.email << " 库存: " << soci_helpers::to_db_string(p.created_at) << std::endl;
        }
       // 查询低价产品(保持不变)
        std::optional<float> price=100.0;
        std::optional<int> stock=10;
        auto cheapProducts = SociDB::query<Product>(
                "SELECT * FROM products WHERE price < ? AND stock > ?",
                price,  // price参数
                stock      // stock参数
        );

        // 添加调试输出
        std::cout << "找到 " << cheapProducts.size() << " 条低价产品" << std::endl;
        for (const auto& p : cheapProducts) {
            std::cout << "ID: " << p.id.value() << " 名称: " << p.name.value()
                      << " 价格: " << p.price.value() << " 库存: " << p.stock.value() << std::endl;
        }

      // 收集要删除的产品ID(保持不变)
        for (const auto& p : cheapProducts) {
            productIdsToDelete.push_back(p.id.value());
        }

       // 批量删除 - 修改为通过Product模板获取表信息
        if (!productIdsToDelete.empty() && SociDB::bulkRemove<Product>(productIdsToDelete)) {
            std::cout << "批量删除" << productIdsToDelete.size()
                      << "个低价产品成功\n";
        }


        bool success = SociDB::restore<User>(123);

        std::vector<int64_t > order_ids = {101, 102, 103, 104, 105};
        bool success1 = SociDB::bulkRestore<Order>(order_ids);
    }

    static void queryDemo() {
        std::cout << "\n===== 查询操作示例 =====\n";

        // 1. 基础查询
        auto activeUsers = SociDB::query<User>(
                "SELECT * FROM users WHERE status = 1");
        std::cout << "活跃用户数量: " << activeUsers.size() << "\n";

        // 2. 分页查询产品
        auto productPage = SociDB::queryPage<OrderFullInfo>(
                "SELECT "
                "o.id as order_id, o.total_amount, o.status as order_status, "
                "o.created_at as order_created_at, "
                "u.id as user_id, u.username, u.email, "
                "p.id as product_id, p.name as product_name, p.price as product_price, "
                "od.quantity, od.unit_price as detail_price "
                "FROM orders o "
                "JOIN users u ON o.user_id = u.id "
                "JOIN order_details od ON o.id = od.order_id "
                "JOIN products p ON od.product_id = p.id",
                1,  // 页码
                10, // 每页数量
                {},  // 精确条件
                {},  // 范围条件
                {},  // 模糊条件
                {},  // IN条件
                {" p.name ='智能手机' "}, //补充sql
                {}, //分组
                {"o.created_at desc"}// 排序
        );
        std::cout << "电子产品第1页: " << productPage.items.size()
                  << "/" << productPage.total_items << "\n";



        auto [session,rows] =  SociDB::queryRaw(
           "SELECT o.id as order_id, u.username, p.name as product_name"
           " FROM orders o JOIN users u ON o.user_id = u.id"
           " join order_details od  on od.order_id = o.id"
           " JOIN products p ON od.product_id = p.id"
           " WHERE o.status = 1"
           " LIMIT 100"
        );

        for (const auto& row : rows) {
            int id = row.get<int>("order_id");
            std::string name = row.get<std::string>("product_name");
            std::string username = row.get<std::string>("username");
            std::cout << id << ": " << name <<":"<<username<< "\n";
        }


        auto resultMap = SociDB::queryToJson("SELECT o.id as order_id, u.username, o.total_amount as amount, "
                                             "to_char(o.created_at, 'YYYY-MM-DD HH24:MI') as create_time "
                                             "FROM orders o JOIN users u ON o.user_id = u.id");
        for (const auto& row : resultMap) {  // 遍历每一行
            for (const auto& [col, val] : row.items()) {  // 遍历每列
                std::cout << col << ": ";
                if (val.is_string()) {
                    std::cout << val.get<std::string>();
                } else if (val.is_number()) {
                    std::cout << val.dump(); // 保持原始数值格式
                } else if (val.is_null()) {
                    std::cout << "NULL";
                }
                std::cout << "\n";
            }
            std::cout << "--------\n"; // 行分隔符
        }



//        auto orders1 = SociDB::queryAdvanced<OrderDetail>(
//                "SELECT o.id as order_id, u.username, o.total_amount as amount, "
//                "to_char(o.created_at, 'YYYY-MM-DD HH24:MI') as create_time "
//                "FROM orders o JOIN users u ON o.user_id = u.id");
//
//        for (const auto& o : orders1) {
//            std::cout << "订单#" << o.orderId.value() << " 产品:" << o.productId.value()
//                      << " 数量:" << o.quantity.value() << " 时间:" << soci_helpers::to_db_string(o.created_at) << "\n";
//        }


        auto results = SociDB::queryAdvanced<OrderFullInfo>(
                // 多表连接SQL（使用字段别名匹配DTO）
                "SELECT "
                "o.id as order_id, o.total_amount, o.status as order_status, "
                "o.created_at as order_created_at, "
                "u.id as user_id, u.username, u.email, "
                "p.id as product_id, p.name as product_name, p.price as product_price, "
                "od.quantity, od.unit_price as detail_price "
                "FROM orders o "
                "JOIN users u ON o.user_id = u.id "
                "JOIN order_details od ON o.id = od.order_id "
                "JOIN products p ON od.product_id = p.id",
                // 以下参数用法与单表查询完全一致
                {{"o.status", "1"}},                      // 精确条件
                {{"o.created_at", {"2024-01-01", "2025-12-31"}}}, // 范围条件
                {{"p.name", "Laptop"}},                      // 模糊条件
                {},                                       // IN条件
                {},                                       //补充条件
                {},                                       // 分组
                "o.created_at DESC",                  // 排序
                100                                       // 限制条数
        );

        for (const auto& order : results) {
            std::cout << "订单ID: " << order.order_id.value_or(0)
                      << " | 用户: " << order.username
                      << " | 商品: " << order.product_name
                      << " × " << order.quantity.value_or(0)
                      << " | 单价: " << order.detail_price.value_or(0.0)
                      << " | 时间: "
                      << std::chrono::system_clock::to_time_t(order.order_created_at)
                      << "\n";
        }

        // 准备查询条件
        std::vector<User> userList = {
                {.username="user111",.status= 1},  // 查username/email/status
                {.username="user222",.status= 1}, // 查email/password
                {.username="user333",.status= 1}  // 查username/status
        };

        // 执行查询
        auto users = SociDB::queryList<User>(userList);

        // 处理结果
        for (const auto& u : users) {
            std::cout << "找到用户: " << u.username
                      << " (状态: " << (u.status ? std::to_string(*u.status) : "NULL")
                      << ")" << std::endl;
        }
    }

    static void bulkInsertDemo() {
        std::vector<User> users = {
                {.username = "user1",
                        .email = "email1@test.com",
                        .password = CryptoUtil::md5("pass1"),
                        .status = 1},

                {.username = "user2",
                        .email = "email2@test.com",
                        .password = CryptoUtil::md5("pass2"),
                        .status = 1},

                {.username = "user3",
                        .email = "email3@test.com",
                        .password = CryptoUtil::md5("pass3"),
                        .status = 0}
        };

        std::vector<OrderDetail> oderDetailList = {
                {.productId= 1,
                        .price = 22.5
                },

                {.productId= 1,
                    .price = 22.5},

                {.productId= 1,
                        .price = 22.5}
        };

        SociDB::bulkInsert(oderDetailList);


        if (SociDB::bulkInsert(users)) {
            std::cout << "成功批量插入 " << users.size() << " 个用户" << std::endl;

            // 验证插入结果
            auto insertedUsers = SociDB::query<User>(
                    "SELECT * FROM users WHERE username LIKE 'batch_user%'");
            std::cout << "验证结果: 找到" << insertedUsers.size()
                      << "个测试用户\n";
        }

        // 批量新增产品
//        std::vector<std::tuple<std::string, double, int, int>> products = {
//                {"Bulk Product A", 99.99, 1, 100},
//                {"Bulk Product B", 199.99, 2, 50},
//                {"Bulk Product C", 299.99, 1, 75},
//                {"Bulk Product D", 399.99, 3, 30}
//        };
//
//        if (SociDB::bulkInsert<Product>("products", products)) {
//            std::cout << "批量插入" << products.size() << "个产品成功\n";
//        }
    }


    static void bulkUpdateDemo() {
        std::cout << "\n===== 批量更新操作示例 =====\n";

        // 1. 批量更新用户状态
//        {
//            // 查询需要更新的用户
//            auto usersToUpdate = SociDB::query<User>(
//                    "SELECT id FROM users WHERE status = 0"); // 查询状态为0的用户
//
//            if (!usersToUpdate.empty()) {
//                std::vector<User> updateData;
//                for (auto& user : usersToUpdate) {
//                    user.status = 1; // 将状态改为1
//                    updateData.push_back(user);
//                }
//
//                if (SociDB::bulkUpdate(updateData)) {
//                    std::cout << "成功批量更新 " << updateData.size()
//                              << " 个用户状态 (0→1)\n";
//
//                    // 验证更新结果
//                    auto updatedUsers = SociDB::query<User>(
//                            "SELECT * FROM users WHERE id IN (" +
//                            joinIds(usersToUpdate) + ") AND status = 1");
//                    std::cout << "验证结果: " << updatedUsers.size()
//                              << " 个用户状态已更新\n";
//                }
//            } else {
//                std::cout << "没有找到需要更新状态的用户\n";
//            }
//        }

        // 2. 批量调整产品价格
        {
            // 查询需要调价的产品
            auto productsToUpdate = SociDB::query<Product>(
                    "SELECT id, price FROM products WHERE category_id = 1 and name ='智能手机'"); // 查询类别1的产品

            if (!productsToUpdate.empty()) {
                std::vector<Product> updateData;
                for (auto& product : productsToUpdate) {
                    product.price = product.price.value() * 1.1; // 价格上调10%
                    updateData.push_back(product);
                }

                if (SociDB::bulkUpdate(updateData)) {
                    std::cout << "成功批量调整 " << updateData.size()
                              << " 个产品价格 (+10%)\n";

                    // 验证更新结果
                    auto updatedProducts = SociDB::query<Product>(
                            "SELECT id, price FROM products WHERE id IN (" +
                            joinIds(productsToUpdate) + ")");

                    int count = 0;
                    for (const auto& p : updatedProducts) {
                        auto original = findOriginalPrice(productsToUpdate, p.id.value());
                        if (original && p.price.value() == original.value() * 1.1) {
                            count++;
                        }
                    }
                    std::cout << "验证结果: " << count << " 个产品价格已调整\n";
                }
            } else {
                std::cout << "没有找到需要调价的产品\n";
            }
        }

        // 3. 批量更新订单状态
        {
            // 查询需要更新的订单
            auto ordersToUpdate = SociDB::query<Order>(
                    "SELECT id FROM orders WHERE status = 1 AND created_at < NOW() - INTERVAL '7 days'");

            if (!ordersToUpdate.empty()) {
                std::vector<Order> updateData;
                for (auto& order : ordersToUpdate) {
                    order.status = 2; // 将状态改为2(已发货)
                    updateData.push_back(order);
                }

                if (SociDB::bulkUpdate(updateData)) {
                    std::cout << "成功批量更新 " << updateData.size()
                              << " 个订单状态 (1→2)\n";

                    // 验证更新结果
                    auto updatedOrders = SociDB::query<Order>(
                            "SELECT * FROM orders WHERE id IN (" +
                            joinIds(ordersToUpdate) + ") AND status = 2");
                    std::cout << "验证结果: " << updatedOrders.size()
                              << " 个订单状态已更新\n";
                }
            } else {
                std::cout << "没有找到需要更新状态的订单\n";
            }
        }
    }

// 辅助函数：将对象列表的ID拼接成逗号分隔的字符串
    template<typename T>
    static std::string joinIds(const std::vector<T>& items) {
        std::stringstream ss;
        for (size_t i = 0; i < items.size(); ++i) {
            if (i != 0) ss << ",";
            ss << items[i].id.value();
        }
        return ss.str();
    }

// 辅助函数：查找原始价格
    static std::optional<double> findOriginalPrice(const std::vector<Product>& products, int id) {
        for (const auto& p : products) {
            if (p.id.value() == id) {
                return p.price;
            }
        }
        return std::nullopt;
    }



    static void bulkDeleteDemo() {
        // 先查询要删除的测试用户
        auto usersToDelete = SociDB::query<User>(
                "SELECT id FROM users WHERE username LIKE 'batch_user%'");

        if (!usersToDelete.empty()) {
            std::vector<std::int64_t > userIds;
            for (const auto& u : usersToDelete) {
                userIds.push_back(u.id.value());
            }

            if (SociDB::bulkRemove<User>(userIds)) {
                std::cout << "批量删除" << userIds.size() << "个测试用户成功\n";

                // 验证删除结果
                auto remaining = SociDB::query<User>(
                        "SELECT * FROM users WHERE username LIKE 'batch_user%'");
                std::cout << "验证结果: 剩余" << remaining.size() << "个测试用户\n";
            }
        }

        // 批量删除低价产品
        std::vector<std::int64_t> productIdsToDelete;
        auto cheapProducts = SociDB::query<Product>(
                "SELECT id FROM products WHERE price < 200");

        for (const auto& p : cheapProducts) {
            productIdsToDelete.push_back(p.id.value());
        }

        if (!productIdsToDelete.empty() &&
            SociDB::bulkRemove<Product>(productIdsToDelete)) {
            std::cout << "批量删除" << productIdsToDelete.size()
                      << "个低价产品成功\n";
        }
    }
};



#endif //CADD_SOCIDB_H