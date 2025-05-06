#include <drogon/drogon.h>
#include <jwt-cpp/jwt.h>

using namespace drogon;

int main() {
    // 1. 初始化数据库连接
    auto db = app().createDbClient("host=127.0.0.1 port=5432 dbname=mydb user=postgres password=123456", 4);

    // 2. 创建用户路由
    app().registerHandler(
            "/users",
            [db](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& cb) {
                // 获取查询参数
                int page = req->getParameter("page", "1").as<int>();
                int limit = req->getParameter("limit", "10").as<int>();

                // 执行原生SQL查询
                db->execSqlAsync(
                        "SELECT id, username FROM users ORDER BY id DESC LIMIT $1 OFFSET $2",
                        [cb](const orm::Result& r) {
                            Json::Value users(Json::arrayValue);
                            for (const auto& row : r) {
                                Json::Value user;
                                user["id"] = row["id"].as<int>();
                                user["username"] = row["username"].as<std::string>();
                                users.append(user);
                            }
                            cb(HttpResponse::newHttpJsonResponse(users));
                        },
                        [cb](const orm::DrogonDbException& e) {
                            auto resp = HttpResponse::newHttpResponse();
                            resp->setStatusCode(k500InternalServerError);
                            resp->setBody("Database error: " + std::string(e.base().what()));
                            cb(resp);
                        },
                        limit,
                        (page - 1) * limit
                );
            },
            {Get}
    );

    // 3. 用户注册路由
    app().registerHandler(
            "/register",
            [db](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& cb) {
                auto json = req->getJsonObject();
                if (!json || !json->isMember("username") || !json->isMember("password")) {
                    auto resp = HttpResponse::newHttpResponse();
                    resp->setStatusCode(k400BadRequest);
                    resp->setBody("Missing username or password");
                    return cb(resp);
                }

                std::string hashedPwd = utils::getMd5((*json)["password"].asString());

                db->execSqlAsync(
                        "INSERT INTO users (username, password) VALUES ($1, $2) RETURNING id",
                        [cb](const orm::Result& r) {
                            Json::Value ret;
                            ret["user_id"] = r[0]["id"].as<int>();
                            cb(HttpResponse::newHttpJsonResponse(ret));
                        },
                        [cb](const orm::DrogonDbException& e) {
                            auto resp = HttpResponse::newHttpResponse();
                            resp->setStatusCode(k409Conflict);
                            resp->setBody("Username already exists");
                            cb(resp);
                        },
                        (*json)["username"].asString(),
                        hashedPwd
                );
            },
            {Post}
    );

    // 4. 启动服务
    app()
            .setLogLevel(trantor::Logger::kWarn)
            .addListener("0.0.0.0", 8888)
            .run();

    return 0;
}