#pragma once
#include <nlohmann/json.hpp>
#include "../DrogonJsonAdapter.h"
#include "../../json/JsonConversion.h"
#include "../../pgsql/SociDB.h"
#include "../../pgsql/SociDemo.h"
#include <drogon/HttpClient.h>
#include <drogon/HttpAppFramework.h>
#include <drogon/HttpController.h>
#include <drogon/HttpSimpleController.h>
#include "CallbackNotifier.h"
#include <atomic>
#include <thread>
#include "TaskManager.h"


using namespace drogon;
using json = nlohmann::json;

/**
 * @brief 用户管理控制器
 *
 * 提供用户相关的RESTful API接口，包括：
 * - 用户列表查询
 * - 用户创建/更新/删除
 * - 同步/异步数据处理
 * - 回调通知功能
 */
class UserController : public HttpController<UserController, false>
{
public:
    // 禁用拷贝和移动
    UserController() = default;
    UserController(const UserController&) = delete;
    UserController& operator=(const UserController&) = delete;
    /// 路由注册宏
    METHOD_LIST_BEGIN
    // 用户基础CRUD接口
    ADD_METHOD_TO(UserController::getUsers, "/api/v1/getUsers", Get, "AuthFilter", "LogFilter");
    ADD_METHOD_TO(UserController::createUser, "/api/v1/createUser", Post, "AuthFilter", "LogFilter");
    ADD_METHOD_TO(UserController::updateUser, "/api/v1/updateUser/{id}", Put, "AuthFilter", "LogFilter");
    ADD_METHOD_TO(UserController::deleteUser, "/api/v1/deleteUser/{id}", Delete, "AuthFilter", "LogFilter");

    // 特殊处理接口
    ADD_METHOD_TO(UserController::jsonPost, "/api/v1/users/json", Post, "AuthFilter");
    ADD_METHOD_TO(UserController::formPost, "/api/v1/users/form", Post, "AuthFilter");
    ADD_METHOD_TO(UserController::asyncJsonPost, "/api/v1/async", Post, "AuthFilter");
    ADD_METHOD_TO(UserController::getTaskStatus, "/api/v1/tasks/{task_id}", Get, "AuthFilter");

        // 预留文件上传接口（已注释）
    // ADD_METHOD_TO(UserController::fileUpload, "/api/v1/upload", Post, "AuthFilter");
    METHOD_LIST_END

    /**
     * @brief 获取用户列表（分页）
     * @param req HTTP请求对象
     * @param callback 回调函数
     */
    static void getUsers(const HttpRequestPtr& req,
                         std::function<void(const HttpResponsePtr&)>&& callback);

    /**
     * @brief 创建新用户
     * @param req HTTP请求对象
     * @param callback 回调函数
     */
    static void createUser(const HttpRequestPtr& req,
                           std::function<void(const HttpResponsePtr&)>&& callback);

    /**
     * @brief 更新用户信息
     * @param req HTTP请求对象
     * @param callback 回调函数
     */
    static void updateUser(const HttpRequestPtr& req,
                           std::function<void(const HttpResponsePtr&)>&& callback,int id);

    /**
     * @brief 删除用户
     * @param req HTTP请求对象
     * @param callback 回调函数
     */
    static void deleteUser(const HttpRequestPtr& req,
                           std::function<void(const HttpResponsePtr&)>&& callback,int id);

    /**
     * @brief 处理JSON格式的POST请求
     * @param req HTTP请求对象
     * @param callback 回调函数
     */
    static void jsonPost(const HttpRequestPtr& req,
                         std::function<void(const HttpResponsePtr&)>&& callback);

    /**
     * @brief 处理表单格式的POST请求
     * @param req HTTP请求对象
     * @param callback 回调函数
     */
    static void formPost(const HttpRequestPtr& req,
                         std::function<void(const HttpResponsePtr&)>&& callback);

    /**
     * @brief 异步处理JSON请求（带回调通知）
     * @param req HTTP请求对象
     * @param callback 回调函数
     */
    static void asyncJsonPost(const HttpRequestPtr& req,
                              std::function<void(const HttpResponsePtr&)>&& callback);


    static void getTaskStatus(
            const HttpRequestPtr& req,
            std::function<void(const HttpResponsePtr&)>&& callback,
            const std::string& task_id);

private:

};