#pragma once
#include <string>
#include <map>
#include <functional>
#include <memory>
#include <variant>
#include <nlohmann/json.hpp>
#include <future>      // 必须包含
using json = nlohmann::json;

class HttpClient {
public:
    // 请求数据类型
    using FormData = std::map<std::string, std::string>;
    using UrlParams = std::map<std::string, std::string>;
    using Headers = std::map<std::string, std::string>;

    // 统一请求参数
    struct Request {
        std::string url;
        std::variant<json, FormData, std::string> data;
        Headers headers;
    };


    // 构造/析构
    HttpClient();
    ~HttpClient();

    // 同步请求
    static std::string Get(const std::string& url,
                           const UrlParams& params = {},
                           const Headers& headers = {});

    static std::string Post(const Request& request);
    static std::string Put(const Request& request);
    static std::string Delete(const Request& request);

    // 异步Future方式
    static std::future<std::string> AsyncGet(const std::string& url,
                                             const UrlParams& params = {},
                                             const Headers& headers = {});

    static std::future<std::string> AsyncPost(const Request& request);
    static std::future<std::string> AsyncPut(const Request& request);
    static std::future<std::string> AsyncDelete(const Request& request);

    // 异步回调方式
    using Callback = std::function<void(const std::string& response, int status)>;
    static void GetAsync(const std::string& url,
                         Callback callback,
                         const UrlParams& params = {},
                         const Headers& headers = {});

    static void PostAsync(const Request& request, Callback callback);
    static void PutAsync(const Request& request, Callback callback);
    static void DeleteAsync(const Request& request, Callback callback);

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_; // 使用unique_ptr需要定义析构函数
    static std::shared_ptr<Impl> GetCurlHandle(); // 关键声明


};