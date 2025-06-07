// CallbackNotifier.h
#pragma once
#include <drogon/HttpClient.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <functional>

namespace drogon {
    class HttpClient;
    using HttpClientPtr = std::shared_ptr<HttpClient>;
}

class CallbackNotifier
{
public:
    // 定义回调函数类型
    using CallbackFunc = std::function<void(drogon::ReqResult,
                                            const drogon::HttpResponsePtr&)>;

    // 单例模式
    static CallbackNotifier& instance() {
        static CallbackNotifier instance;
        return instance;
    }

    // 发送回调通知
    void notify(const std::string& url,
                const drogon::HttpResponsePtr& resp,
                CallbackFunc customCallback = nullptr)
    {
        auto client = drogon::HttpClient::newHttpClient(url);
        auto req = drogon::HttpRequest::newHttpJsonRequest(*resp->getJsonObject());
        req->setMethod(drogon::Post);

        auto callback = customCallback ? customCallback : defaultCallback;
        client->sendRequest(req, callback);
    }

private:
    CallbackNotifier() = default;

    // 默认回调处理
    static void defaultCallback(drogon::ReqResult result,
                                const drogon::HttpResponsePtr& resp)
    {
        if (result != drogon::ReqResult::Ok) {
            LOG_ERROR << "Callback failed: " << result;
        } else {
            LOG_INFO << "Callback response: " << resp->getBody();
        }
    }
};