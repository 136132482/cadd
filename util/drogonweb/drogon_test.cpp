#include <drogon/drogon.h>
#include <jwt-cpp/jwt.h>
#include <iostream>
#include "../pgsql/SociDB.h"
#include "../pgsql/SociDemo.h"
#include "../json/JsonConversion.h"
#include "DrogonJsonAdapter.h"
#include "../Logger//Logger.h"

int main() {
    // ==================== 1. 强制初始化日志系统 ====================
    std::cout << "========================================\n"
              << "🚀 Drogon Server STARTING\n"
              << "========================================\n";
    Logger::Global().Info("========================================");
    Logger::Global().Info("🚀 Drogon Server STARTING");
    Logger::Global().Info("========================================");

    // ==================== 2. 初始化数据库连接 ====================
    try {
        Logger::Global().Debug("Initializing database connection...");
        std::cout << "尝试连接数据库...\n";

        SociDB::init("host=127.0.0.1 port=5432 dbname=pg_db user=root password=root", 4);

        Logger::Global().Info("✅ 数据库连接成功");
        std::cout << "✅ 数据库连接成功\n";
    } catch (const std::exception& e) {
        Logger::Global().Error("💥 数据库连接失败: " + std::string(e.what()));
        std::cerr << "💥 数据库连接失败: " << e.what() << "\n";
        return 1;
    }

    // ==================== 3. 注册路由 ====================
    // (1) 用户列表路由
    drogon::app().registerHandler(
            "/users",
            [](const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
                Logger::Global().Debug("GET /users 请求到达");
                std::cout << "处理 /users 请求\n";

                try {
                    // 获取分页参数
                    int page = req->getParameter("page").empty() ? 1 : std::stoi(req->getParameter("page"));
                    int limit = req->getParameter("limit").empty() ? 10 : std::stoi(req->getParameter("limit"));

                    Logger::Global().DebugFmt("分页参数: page=%d, limit=%d", page, limit);

                    // 查询数据库
                    auto users = SociDB::query<User>(
                            "SELECT id, username,email FROM users ORDER BY id DESC LIMIT " +
                            std::to_string(limit) + " OFFSET " + std::to_string((page - 1) * limit)
                    );

                    Logger::Global().InfoFmt("返回 %d 条用户数据", users.size());
                    cb(DrogonJsonAdapter::success(JsonConversion::toJson(users)));
                } catch (const std::exception& e) {
                    Logger::Global().Error("查询用户列表失败: " + std::string(e.what()));
                    auto resp = drogon::HttpResponse::newHttpResponse();
                    resp->setStatusCode(drogon::k500InternalServerError);
                    resp->setBody("服务器错误: " + std::string(e.what()));
                    cb(resp);
                }
            },
            {drogon::Get}
    );

    // (2) 用户注册路由
    drogon::app().registerHandler(
            "/register",
            [](const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
                Logger::Global().Debug("POST /register 请求到达");
                std::cout << "处理 /register 请求\n";

                auto resp = drogon::HttpResponse::newHttpResponse();
                try {
                    // 校验JSON格式
                    auto json = req->getJsonObject();
                    if (!json || json->empty()) {
                        throw std::runtime_error("无效的JSON格式");
                    }

                    // 校验必填字段
                    if (!json->isMember("username") || !json->isMember("password")|| !json->isMember("email")) {
                        throw std::runtime_error("必须提供username和password");
                    }

                    User user = JsonConversion::fromJson<User>(DrogonJsonAdapter::fromDrogon(*json));
                    user.password = drogon::utils::getMd5(user.password); // 密码加密

                    auto user_id = SociDB::insert<User>(user);
                    if (user_id.has_value()) {
                        auto ret = DrogonJsonAdapter::success({
                                                                      {"user_id", user_id.value()}
                                                              });
                        cb(ret);
                    } else {
                        auto ret = DrogonJsonAdapter::error(500);
                        cb(ret);
                    }

                    Logger::Global().DebugFmt("注册新用户: username=%s", user.username.c_str());

                } catch (...) {
                    try {
                        std::rethrow_exception(std::current_exception());
                    } catch (const std::exception& e) {
                        if (std::string(e.what()).find("duplicate") != std::string::npos) {
                            Logger::Global().Warning("用户名冲突: " + std::string(e.what()));
                            resp->setStatusCode(drogon::k409Conflict);
                            resp->setBody("用户名已存在");
                        }
                        else if (e.what() == std::string("无效的JSON格式")) {
                            Logger::Global().Warning("无效的JSON请求");
                            resp->setStatusCode(drogon::k400BadRequest);
                            resp->setBody(e.what());
                        }
                        else {
                            Logger::Global().Error("注册错误: " + std::string(e.what()));
                            resp->setStatusCode(drogon::k500InternalServerError);
                            resp->setBody("服务器错误: " + std::string(e.what()));
                        }
                        cb(resp);
                    }
                }
            },
            {drogon::Post}
    );

    // ==================== 4. 启动服务 ====================
    Logger::Global().Info("⏳ 启动服务: 0.0.0.0:8888");
    std::cout << "⏳ 启动服务: 0.0.0.0:8888\n";

    // 强制设置Drogon日志级别
    drogon::app().setLogLevel(trantor::Logger::kTrace);

    // 打印可用路由
    Logger::Global().Info("可用路由:");
    Logger::Global().Info("  GET    /users      - 获取用户列表");
    Logger::Global().Info("  POST   /register   - 注册新用户");
    std::cout << "可用路由:\n"
              << "  GET    /users      - 获取用户列表\n"
              << "  POST   /register   - 注册新用户\n";

    // 启动服务
    drogon::app()
            .addListener("0.0.0.0", 8888)
            .run();

    Logger::Global().Info("🛑 服务已停止");
    std::cout << "🛑 服务已停止\n";
    return 0;
}