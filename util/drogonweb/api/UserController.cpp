#pragma once
#include "UserController.h"
#include <chrono> // 添加这行
#include "../../http/http_client.h"
#include "../../http/FormParser.h"


    // 1. 获取用户列表
     void UserController::getUsers(const HttpRequestPtr& req,
                  std::function<void(const HttpResponsePtr&)>&& callback)
    {
        try {
            int page = req->getParameter("page").empty() ? 1 : std::stoi(req->getParameter("page"));
            int limit = req->getParameter("limit").empty() ? 10 : std::stoi(req->getParameter("limit"));

            auto users = SociDB::query<User>(
                    "SELECT id, username, email FROM users ORDER BY id DESC LIMIT " +
                    std::to_string(limit) + " OFFSET " + std::to_string((page - 1) * limit)
            );
            callback(DrogonJsonAdapter::success(JsonConversion::toJson(users)));
        } catch (const std::exception& e) {
            callback(DrogonJsonAdapter::error(k500InternalServerError,
                                              "Server error: " + std::string(e.what())));
        }
    }

    // 2. 创建用户
     void UserController::createUser(const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback)
    {
        try {
            auto json = req->getJsonObject();
            if (!json || json->empty()) {
                callback(DrogonJsonAdapter::error(k400BadRequest, "Invalid JSON"));
                return;
            }

            User newUser{
                    .username = (*json)["username"].asString(),
                    .email = (*json)["email"].asString(),
                    .password = utils::getMd5((*json)["password"].asString()),
                    .status = 1
            };

            auto result = SociDB::insert(newUser);
            if (!result) {
                callback(DrogonJsonAdapter::error(k500InternalServerError, "Insert failed"));
                return;
            }

            callback(DrogonJsonAdapter::success({
                                                        {"user_id", result.value()},
                                                        {"status", "created"}
                                                }));
        } catch (const soci::postgresql_soci_error& e) {
            callback(DrogonJsonAdapter::error(
                    std::string(e.what()).find("duplicate") != std::string::npos ?
                    k409Conflict : k500InternalServerError,
                    e.what()
            ));
        } catch (const std::exception& e) {
            callback(DrogonJsonAdapter::error(k500InternalServerError, e.what()));
        }
    }

    // 3. 更新用户
     void UserController::updateUser(const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback,
                    int id)
    {
        try {
            auto json = req->getJsonObject();
            if (!json || json->empty()) {
                callback(DrogonJsonAdapter::error(k400BadRequest, "Invalid JSON"));
                return;
            }
            User user;
            user.id = id;
            user.email = (*json)["email"].asString();

            if (!SociDB::update(user)) {
                callback(DrogonJsonAdapter::error(k500InternalServerError, "Update failed"));
                return;
            }

            callback(DrogonJsonAdapter::success({
                                                        {"message", "User updated successfully"}
                                                }));
        } catch (const std::exception& e) {
            callback(DrogonJsonAdapter::error(k500InternalServerError, e.what()));
        }
    }

    // 4. 删除用户
     void UserController::deleteUser(const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback,
                                     int id)
    {
        try {
//            int id = std::stoi(req->getParameter("id"));
            bool deletionResult = SociDB::remove<User>(id);

            if (deletionResult) {
                callback(DrogonJsonAdapter::success({
                                                            {"message", "User deleted successfully"},
                                                            {"user_id", id}
                                                    }));
            } else {
                callback(DrogonJsonAdapter::error(
                        k404NotFound,
                        "User not found or already deleted",
                        {{"requested_id", id}}
                ));
            }
        } catch (const std::invalid_argument& e) {
            callback(DrogonJsonAdapter::error(
                    k400BadRequest,
                    "Invalid user ID format",
                    {{"error_details", e.what()}}
            ));
        } catch (const std::exception& e) {
            callback(DrogonJsonAdapter::error(
                    k500InternalServerError,
                    "Internal server error during deletion",
                    {{"error_details", e.what()}}
            ));
        }
    }

    // 5. 同步JSON POST
     void UserController::jsonPost(const HttpRequestPtr& req,
                  std::function<void(const HttpResponsePtr&)>&& callback)
    {
        try {
            auto json = req->getJsonObject();
            if (!json) {
                callback(DrogonJsonAdapter::error(k400BadRequest, "Invalid JSON"));
                return;
            }
            std::string username = (*json)["username"].asString();
            std::string email = (*json)["email"].asString();
            callback(DrogonJsonAdapter::success({
                                                        {"status", "created"},
                                                        {"username", username},
                                                        {"email", email}
                                                }));
        } catch (...) {
            callback(DrogonJsonAdapter::error(k500InternalServerError, "Processing failed"));
        }
    }

    // 6. 同步表单POST
     void UserController::formPost(const HttpRequestPtr& req,
                  std::function<void(const HttpResponsePtr&)>&& callback)
    {
        try {
            auto form_data = FormParser::Parse(req);
            std::string username = form_data["username"];
            std::string password = form_data["password"];

            if (username.empty() || password.empty()) {
                callback(DrogonJsonAdapter::error(k400BadRequest, "Missing parameters"));
                return;
            }
            callback(DrogonJsonAdapter::success({
                                                        {"status", "form_received"},
                                                        {"username", username}
                                                }));
        } catch (...) {
            callback(DrogonJsonAdapter::error(k500InternalServerError, "Form processing failed"));
        }
    }

    // 7. 修改后的异步JSON POST方法
    void UserController::asyncJsonPost(
            const HttpRequestPtr& req,
            std::function<void(const HttpResponsePtr&)>&& callback)
    {
        try {
            // 1. 立即返回处理中状态
            static std::atomic<int> taskId(0);
            int currentTaskId = ++taskId;
            TaskManager::instance().setTask(currentTaskId, "processing");

            Json::Value resp;
            resp["task_id"] = currentTaskId;
            resp["status"] = "processing";
            resp["check_status"] = "/api/v1/tasks/" + std::to_string(currentTaskId);
            callback(HttpResponse::newHttpJsonResponse(resp));

            // 2. 使用自定义HttpClient（通过命名空间限定）
            std::string callbackUrl = req->getHeader("X-Callback-Url");
            if (!callbackUrl.empty()) {
                app().getLoop()->runInLoop([=]() {
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                    TaskManager::instance().setTask(currentTaskId, "completed");

                    MyHttpClient::Request req{  // 全局命名空间限定
                            callbackUrl,
                            json{{"task_id", currentTaskId}, {"status", "completed"}}.dump(),
                            {{"Content-Type", "application/json"}}
                    };
                    MyHttpClient::PostAsync(req, [](const std::string&, int){});
                });
            }
        } catch (...) {
            callback(DrogonJsonAdapter::error(k500InternalServerError, "Async task failed"));
        }
    }

void UserController::getTaskStatus(
        const HttpRequestPtr& req,
        std::function<void(const HttpResponsePtr&)>&& callback,
        const std::string& task_id)
{
    Json::Value responseJson;
    try {
        int id = std::stoi(task_id);
        auto status = TaskManager::instance().getTask(id);

        responseJson["task_id"] = id;
        responseJson["status"] = status;

        auto resp = HttpResponse::newHttpJsonResponse(responseJson);
        callback(resp);
    } catch (...) {
        responseJson["error"] = "Invalid task ID";
        auto resp = HttpResponse::newHttpJsonResponse(responseJson);
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
    }
}






