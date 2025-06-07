#include "../../http/http_client.h"
#include <iostream>
#include <chrono>
#include <thread>
#include "../../ssl/CryptoUtil.h"

// 响应处理回调函数
void handleResponse(const std::string& response, int status) {
    std::cout << "Status: " << status << "\nResponse: " << response << "\n\n";
}

// 基础认证头
MyHttpClient::Headers getBaseHeaders() {
    return {
            {"Authorization", "Bearer your_token_here"},
            {"Accept", "application/json"}
    };
}

// JSON请求头
MyHttpClient::Headers getJsonHeaders() {
    auto headers = getBaseHeaders();
    headers["Content-Type"] = "application/json";
    return headers;
}

// 表单请求头
MyHttpClient::Headers getFormHeaders() {
    auto headers = getBaseHeaders();
    headers["Content-Type"] = "application/x-www-form-urlencoded";
    return headers;
}

void demoGetUsers() {
    std::cout << "=== GET User List ===\n";
    // 同步方式
    auto response = MyHttpClient::Get(
            "http://localhost:8888/api/v1/getUsers",
            {{"page", "1"}, {"limit", "10"}},
            getBaseHeaders()  // GET请求不需要Content-Type
    );
    std::cout << "Sync Response:\n" << response << "\n";
    // 异步Future方式
    auto future = MyHttpClient::AsyncGet(
            "http://localhost:8888/api/v1/getUsers",
            {{"page", "2"}, {"limit", "5"}},
            getBaseHeaders()
    );
    std::cout << "Async Future Response:\n" << future.get() << "\n";

    // 异步回调方式
    MyHttpClient::GetAsync(
            "http://localhost:8888/api/v1/getUsers",
            handleResponse,
            {{"page", "1"}, {"limit", "20"}},
            getBaseHeaders()
    );
}

void demoCreateUser() {
    std::cout << "=== Create User (JSON) ===\n";
    MyHttpClient::Request jsonReq{
            "http://localhost:8888/api/v1/createUser",
            json{{"username", "Alice"}, {"email", "alice@example.com"},{"password","123456"}}.dump(),
            getJsonHeaders()  // JSON请求使用特定的Content-Type
    };
    // 同步方式
    auto response = MyHttpClient::Post(jsonReq);
    std::cout << "Sync Response:\n" << response << "\n";
    // 异步方式
    MyHttpClient::PostAsync(jsonReq, handleResponse);
}

void demoUpdateUser() {
    std::cout << "=== Update User ===\n";
    int userId = 200; // 实际业务中应该从其他地方获取
    MyHttpClient::Request putReq{
            "http://localhost:8888/api/v1/updateUser/200",
            json{{"username", "Alice Updated"}, {"email", "alice.updated@example.com"}}.dump(),
            getJsonHeaders()  // PUT请求也使用JSON Content-Type
    };

    // 同步方式
    auto response = MyHttpClient::Put(putReq);
    std::cout << "Sync Response:\n" << response << "\n";

    // 异步方式
    MyHttpClient::PutAsync(putReq, [](const std::string& resp, int status) {
        std::cout << "Update completed. Status: " << status << "\n";
    });
}

void demoDeleteUser() {
    std::cout << "=== Delete User ===\n";
    MyHttpClient::Request delReq{
            "http://localhost:8888/api/v1/deleteUser/200",
            {}, // 空数据
            getBaseHeaders()  // DELETE请求不需要Content-Type
    };
    // 同步方式
    auto response = MyHttpClient::Delete(delReq);
    std::cout << "Sync Response:\n" << response << "\n";
    // 异步方式
    auto future = MyHttpClient::AsyncDelete(delReq);
    std::cout << "Async Future Response:\n" << future.get() << "\n";
}

void demoJsonPost() {
    std::cout << "=== JSON POST ===\n";
    MyHttpClient::Request req{
            "http://localhost:8888/api/v1/users/json",
            json{{"action", "login"}, {"username", "admin"}, {"password", "secret"}}.dump(),
            getJsonHeaders()  // 明确指定JSON Content-Type
    };
    MyHttpClient::PostAsync(req, handleResponse);
}

void demoFormPost() {
    std::cout << "=== Form POST ===\n";
    MyHttpClient::Request req{
            "http://localhost:8888/api/v1/users/form",
            MyHttpClient::FormData{{"username", "admin"},{"password", "secret"},{"remember", "true"}},
            getFormHeaders()  // 表单请求使用特定的Content-Type
    };

    auto response = MyHttpClient::Post(req);
    std::cout << "Form Response:\n" << response << "\n";
}




void demoMultipartForm() {
    try {
        std::cout << "=== Multipart Form POST Demo ===\n";
        // 1. 准备表单数据
        MyHttpClient::FormData formData = {
                {"username", "admin"},
                {"password", "secret123"},
                {"email","136132482@qq.com"}
        };
        // 3. 发送请求
        std::string response = MyHttpClient::PostMultipartForm(
                "http://localhost:8888/api/v1/users/form",
                formData,
                getFormHeaders()  // 表单请求使用特定的Content-Type
        );

        // 4. 处理响应
        std::cout << "=== 服务器响应 ===\n"
                  << "状态码: 200\n"
                  << "响应体: " << response << "\n";
    } catch (const std::exception& e) {
        std::cerr << "!!! 请求失败: " << e.what() << std::endl;
    }
}


void demoAsyncProcessing() {
    std::cout << "=== Async with Callback ===\n";

    // 1. 使用智能指针保持回调生命周期
    auto callback = std::make_shared<std::function<void(const std::string&, int)>>(
            [](const std::string& resp, int status) {
                if(status != 200) {
                    std::cerr << "Request failed! Status: " << status << std::endl;
                    return;
                }
                try {
                    auto j = json::parse(resp);
                    if (!j.contains("task_id")) {
                        throw std::runtime_error("Missing task_id in response");
                    }
                    std::string taskId = std::to_string(j["task_id"].get<int>());  // 显式转换
                    // 轮询逻辑保持不变
                    auto pollStatus = [taskId]() {
                        while (true) {
                            try {
                                auto statusResp = MyHttpClient::Get(
                                        "http://localhost:8888/api/v1/tasks/" + taskId,
                                        {},
                                        {{"Accept", "application/json"}}
                                );
                                auto status = json::parse(statusResp);
                                std::cout << "[" << taskId << "] Status: "
                                          << status["status"] << std::endl;
                                if (status["status"] == "completed") break;
                            } catch (const std::exception& e) {
                                std::cerr << "Polling error: " << e.what() << std::endl;
                            }
                            std::this_thread::sleep_for(std::chrono::seconds(1));
                        }
                    };
                    std::thread(pollStatus).detach();
                } catch (const std::exception& e) {
                    std::cerr << "Processing error: " << e.what() << std::endl;
                }
            }
    );

    // 2. 准备请求
    MyHttpClient::Request req{
            "http://localhost:8888/api/v1/async",
            json{{"data", "large_data_processing"}},
            {
                    {"Content-Type", "application/json"},
                    {"Authorization", "Bearer your_token"},
                    {"X-Callback-Url", "http://localhost:9999/api/callback"}
            }
    };

    // 3. 提交请求（保持原有回调）
    MyHttpClient::PostAsync(req, *callback);

    // 4. 主线程等待（示例等待10秒）
    std::this_thread::sleep_for(std::chrono::seconds(10));
    std::cout << "Main thread exiting..." << std::endl;
}

int main() {
    try {
        demoGetUsers();
        std::this_thread::sleep_for(std::chrono::seconds(1));

        demoCreateUser();
        std::this_thread::sleep_for(std::chrono::seconds(1));

        demoUpdateUser();
        std::this_thread::sleep_for(std::chrono::seconds(1));

        demoDeleteUser();
        std::this_thread::sleep_for(std::chrono::seconds(1));

        demoJsonPost();
        std::this_thread::sleep_for(std::chrono::seconds(1));

        demoFormPost();
        std::this_thread::sleep_for(std::chrono::seconds(1));

//        demoMultipartForm();

//        demoAsyncProcessing();

        // 等待异步操作完成
//        std::this_thread::sleep_for(std::chrono::seconds(5));

//        auto future  =MyHttpClient::AsyncUpload("http://localhost:8888/api/upload", R"(F:\test_voice\pic\1.jpeg)");
//        std::cout << "上传成功！服务器响应：" << future.get() << std::endl;

        MyHttpClient::UploadAsync("http://localhost:8888/api/upload", R"(F:\test_voice\pic\3.jpeg)",
                                  [](std::string response, int status) {
                                      if (status == 200) {
                                          std::cout << "上传成功: " << response << std::endl;
                                      } else {
                                          std::cerr << "上传失败(" << status << "): " << response << std::endl;
                                      }
                                  }
        );
        std::this_thread::sleep_for(std::chrono::seconds(5));
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}