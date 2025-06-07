#pragma once
#include <drogon/HttpResponse.h>
#include <nlohmann/json.hpp>
#include <json/value.h>
#include <json/reader.h>
#include <json/writer.h>

// 前向声明
//namespace drogonHttp {
//    class HttpResponse;
//    using HttpResponsePtr = std::shared_ptr<HttpResponse>;
//}

class DrogonJsonAdapter {
public:
    // 明确使用drogon命名空间
    static Json::Value toDrogon(const nlohmann::json& j) {
        Json::Value value;
        Json::Reader reader;
        reader.parse(j.dump(), value);
        return value;
    }

    static nlohmann::json fromDrogon(const Json::Value& v) {
        Json::FastWriter writer;
        return nlohmann::json::parse(writer.write(v));
    }

    static drogon::HttpResponsePtr createResponse(const nlohmann::json& j,
                                                  drogon::HttpStatusCode code = drogon::k200OK) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse(toDrogon(j));
        resp->setStatusCode(code);  // 单独设置状态码
        return resp;
    }

    static drogon::HttpResponsePtr success(const nlohmann::json& data = {}) {
        nlohmann::json ret{
                {"code", 0},
                {"message", "success"},
                {"data", data}
        };
        return createResponse(ret);  // 默认200状态码
    }

    static drogon::HttpResponsePtr error(int code,
                                         const std::string& message={},
                                         const nlohmann::json& details = {}) {
        nlohmann::json ret{
                {"code", code},
                {"message", message}
        };
        if (!details.empty()) {
            ret["details"] = details;
        }
        return createResponse(ret, static_cast<drogon::HttpStatusCode>(code));
    }
};