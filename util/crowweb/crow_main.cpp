#include <crow.h>
#include <atomic>
#include "../http/http_client.h"  // 使用自定义的HTTP客户端

std::atomic<int> taskId(0);

// 回调处理函数
void handleCallbackResponse(const std::string& response, int status) {
    std::cout << "回调响应状态码: " << status << "\n响应内容: " << response << "\n";
}

int main() {
    crow::SimpleApp app;

    // 1. 提交异步任务（带回调URL）
    CROW_ROUTE(app, "/api/task").methods("POST"_method)([](const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json) return crow::response(400, "Bad Request");

        int id = ++taskId;
        std::string callbackUrl = json["callback_url"].s();

        // 模拟异步处理
        std::thread([id, callbackUrl]() {
            std::this_thread::sleep_for(std::chrono::seconds(2)); // 模拟处理耗时

            // 如果提供了回调URL，使用HttpClient发送回调
            if (!callbackUrl.empty()) {
                MyHttpClient::Request req{
                        callbackUrl,
                        crow::json::wvalue{{"task_id", id}, {"status", "done"}}.dump(),
                        {
                                {"Authorization", "Bearer your_token_here"},
                                {"Content-Type", "application/json"},
                                {"Accept", "application/json"}
                        }
                };

                // 使用异步方式发送回调
                MyHttpClient::PostAsync(req, handleCallbackResponse);
            }
        }).detach();

        return crow::response(202, crow::json::wvalue{{"task_id", id}}.dump());
    });

    // 2. 接收外部回调
    CROW_ROUTE(app, "/api/callback").methods("POST"_method,"OPTIONS"_method)([](const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json) return crow::response(400);
        std::cout << "收到回调: " << req.body << "\n";
        // 这里可以触发其他逻辑（如通知客户端）
        return crow::response(200);
    });

    // 3. 启动服务
    app.port(9999)
            .bindaddr("0.0.0.0") // 允许外部访问
            .multithreaded().run();
}