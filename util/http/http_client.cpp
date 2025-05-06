#include "http_client.h"
#include <curl/curl.h>
#include <sstream>
#include <future>
#include <thread>
#include <cstring>

namespace {
    size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
        size_t total = size * nmemb;
        output->append((char*)contents, total);
        return total;
    }

    std::string BuildQueryString(const HttpClient::UrlParams& params) {
        std::ostringstream oss;
        for (auto it = params.begin(); it != params.end(); ++it) {
            if (it != params.begin()) oss << "&";
            oss << curl_easy_escape(nullptr, it->first.c_str(), it->first.length())
                << "="
                << curl_easy_escape(nullptr, it->second.c_str(), it->second.length());
        }
        return oss.str();
    }

    void SetupCurlHeaders(CURL* curl, const HttpClient::Headers& headers) {
        struct curl_slist* headerList = nullptr;
        for (const auto& h : headers) {
            headerList = curl_slist_append(headerList, (h.first + ": " + h.second).c_str());
        }
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
    }

    void SetupCurlData(CURL* curl, const HttpClient::Request& req) {
        std::visit([curl](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, json>) {
                std::string jsonStr = arg.dump();
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonStr.c_str());
                curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, jsonStr.length());
            }
            else if constexpr (std::is_same_v<T, HttpClient::FormData>) {
                std::string formData = BuildQueryString(arg);
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, formData.c_str());
                curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, formData.length());
            }
            else if constexpr (std::is_same_v<T, std::string>) {
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, arg.c_str());
                curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, arg.length());
            }
        }, req.data);
    }
}

class HttpClient::Impl {
public:
    Impl() { curl_global_init(CURL_GLOBAL_DEFAULT); }
    ~Impl() { curl_global_cleanup(); }

    std::string PerformRequest(CURL* curl) {
        std::string response;
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            throw std::runtime_error(curl_easy_strerror(res));
        }

        return response;
    }


};

std::shared_ptr<HttpClient::Impl> HttpClient::GetCurlHandle() {
    static auto instance = std::make_shared<Impl>();
    return instance;
}

// 同步GET
std::string HttpClient::Get(const std::string& url,
                            const UrlParams& params,
                            const Headers& headers) {
    auto curl = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>(
            curl_easy_init(), curl_easy_cleanup);

    std::string fullUrl = url;
    if (!params.empty()) {
        fullUrl += (url.find('?') == std::string::npos ? "?" : "&") + BuildQueryString(params);
    }

    curl_easy_setopt(curl.get(), CURLOPT_URL, fullUrl.c_str());
    SetupCurlHeaders(curl.get(), headers);

    return GetCurlHandle()->PerformRequest(curl.get());
}

// 同步POST/PUT/DELETE
std::string HttpClient::Post(const Request& request) {
    auto curl = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>(
            curl_easy_init(), curl_easy_cleanup);

    curl_easy_setopt(curl.get(), CURLOPT_URL, request.url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_POST, 1L);
    SetupCurlHeaders(curl.get(), request.headers);
    SetupCurlData(curl.get(), request);

    return GetCurlHandle()->PerformRequest(curl.get());
}

std::string HttpClient::Put(const Request& request) {
    auto curl = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>(
            curl_easy_init(), curl_easy_cleanup);

    curl_easy_setopt(curl.get(), CURLOPT_URL, request.url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_CUSTOMREQUEST, "PUT");
    SetupCurlHeaders(curl.get(), request.headers);
    SetupCurlData(curl.get(), request);

    return GetCurlHandle()->PerformRequest(curl.get());
}

std::string HttpClient::Delete(const Request& request) {
    auto curl = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>(
            curl_easy_init(), curl_easy_cleanup);

    curl_easy_setopt(curl.get(), CURLOPT_URL, request.url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_CUSTOMREQUEST, "DELETE");
    SetupCurlHeaders(curl.get(), request.headers);
    SetupCurlData(curl.get(), request);

    return GetCurlHandle()->PerformRequest(curl.get());
}

// 异步实现（Future方式）
std::future<std::string> HttpClient::AsyncGet(const std::string& url,
                                              const UrlParams& params,
                                              const Headers& headers) {
    return std::async(std::launch::async, [=] { return Get(url, params, headers); });
}

std::future<std::string> HttpClient::AsyncPost(const Request& request) {
    return std::async(std::launch::async, [=] { return Post(request); });
}

std::future<std::string> HttpClient::AsyncPut(const Request& request) {
    return std::async(std::launch::async, [=] { return Put(request); });
}

std::future<std::string> HttpClient::AsyncDelete(const Request& request) {
    return std::async(std::launch::async, [=] { return Delete(request); });
}

// 异步实现（回调方式）
void HttpClient::GetAsync(const std::string& url,
                          Callback callback,
                          const UrlParams& params,
                          const Headers& headers) {
    std::thread([=] { callback(Get(url, params, headers), 200); }).detach();
}

void HttpClient::PostAsync(const Request& request, Callback callback) {
    std::thread([=] { callback(Post(request), 200); }).detach();
}

void HttpClient::PutAsync(const Request& request, Callback callback) {
    std::thread([=] { callback(Put(request), 200); }).detach();
}

void HttpClient::DeleteAsync(const Request& request, Callback callback) {
    std::thread([=] { callback(Delete(request), 200); }).detach();
}