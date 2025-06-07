#include "http_client.h"
#include <curl/curl.h>
#include <sstream>
#include <future>
#include <thread>
#include <cstring>
#include <iostream>

namespace {
    size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
        size_t total = size * nmemb;
        output->append((char*)contents, total);
        return total;
    }

    std::string BuildQueryString(const MyHttpClient::UrlParams& params) {
        std::ostringstream oss;
        for (const auto& [key, value] : params) {
            if (!oss.str().empty()) oss << "&";

            // 对键和值进行双重编码保护
            char* escaped_key = curl_easy_escape(nullptr, key.c_str(), key.size());
            char* escaped_val = curl_easy_escape(nullptr, value.c_str(), value.size());

            oss << escaped_key << "=" << escaped_val;

            curl_free(escaped_key);
            curl_free(escaped_val);
        }

        // 调试输出（发布时移除）
        std::cout << "Encoded Query: " << oss.str() << std::endl;
        return oss.str();
    }

    void SetupCurlHeaders(CURL* curl, const MyHttpClient::Headers& headers) {
        struct curl_slist* headerList = nullptr;

        // 默认添加UTF-8编码头（如果用户未指定）
        if (headers.find("Content-Type") == headers.end()) {
            headerList = curl_slist_append(headerList,
                                           "Content-Type: application/x-www-form-urlencoded; charset=utf-8");
        }
        // 添加用户自定义头
        for (const auto& [key, value] : headers) {
            std::string header = key + ": " + value;
            // 自动补全charset
            if (key == "Content-Type" && value.find("charset=") == std::string::npos) {
                header += "; charset=utf-8";
            }
            headerList = curl_slist_append(headerList, header.c_str());
        }
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
    }

    void SetupCurlData(CURL* curl, const MyHttpClient::Request& req) {
        std::visit([curl](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, MyHttpClient::FormData>) {
                // 废弃旧的BuildQueryString方式，改用mime API
                curl_mime* mime = curl_mime_init(curl);
                for (const auto& [key, value] : arg) {
                    curl_mimepart* part = curl_mime_addpart(mime);
                    curl_mime_name(part, key.c_str());
                    curl_mime_data(part, value.c_str(), CURL_ZERO_TERMINATED);
                }
                curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
            }
            else if constexpr (std::is_same_v<T, json>) {
                std::string jsonStr = arg.dump();
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonStr.c_str());
                curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, jsonStr.length());
                // 必须设置JSON的Content-Type
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER,
                                 curl_slist_append(nullptr, "Content-Type: application/json"));
            }
            else if constexpr (std::is_same_v<T, std::string>) {
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, arg.c_str());
                curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, arg.length());
            }
        }, req.data);
    }
}

class MyHttpClient::Impl {
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

std::shared_ptr<MyHttpClient::Impl> MyHttpClient::GetCurlHandle() {
    static auto instance = std::make_shared<Impl>();
    return instance;
}

// 同步GET
std::string MyHttpClient::Get(const std::string& url,
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
std::string MyHttpClient::Post(const Request& request) {
    auto curl = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>(
            curl_easy_init(), curl_easy_cleanup);

    curl_easy_setopt(curl.get(), CURLOPT_URL, request.url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_POST, 1L);
    SetupCurlHeaders(curl.get(), request.headers);
    SetupCurlData(curl.get(), request);

    return GetCurlHandle()->PerformRequest(curl.get());
}

std::string MyHttpClient::Put(const Request& request) {
    auto curl = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>(
            curl_easy_init(), curl_easy_cleanup);

    curl_easy_setopt(curl.get(), CURLOPT_URL, request.url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_CUSTOMREQUEST, "PUT");
    SetupCurlHeaders(curl.get(), request.headers);
    SetupCurlData(curl.get(), request);

    return GetCurlHandle()->PerformRequest(curl.get());
}

std::string MyHttpClient::Delete(const Request& request) {
    auto curl = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>(
            curl_easy_init(), curl_easy_cleanup);

    curl_easy_setopt(curl.get(), CURLOPT_URL, request.url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_CUSTOMREQUEST, "DELETE");
    SetupCurlHeaders(curl.get(), request.headers);
    SetupCurlData(curl.get(), request);

    return GetCurlHandle()->PerformRequest(curl.get());
}

// 异步实现（Future方式）
std::future<std::string> MyHttpClient::AsyncGet(const std::string& url,
                                              const UrlParams& params,
                                              const Headers& headers) {
    return std::async(std::launch::async, [=] { return Get(url, params, headers); });
}

std::future<std::string> MyHttpClient::AsyncPost(const Request& request) {
    return std::async(std::launch::async, [=] { return Post(request); });
}

std::future<std::string> MyHttpClient::AsyncPut(const Request& request) {
    return std::async(std::launch::async, [=] { return Put(request); });
}

std::future<std::string> MyHttpClient::AsyncDelete(const Request& request) {
    return std::async(std::launch::async, [=] { return Delete(request); });
}

std::string MyHttpClient::PostMultipartForm(const std::string& url,
                                            const FormData& form_data,
                                            const Headers& headers) {
    auto curl = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>(
            curl_easy_init(), curl_easy_cleanup);

    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL handle");
    }

    // 1. 创建mime结构
    curl_mime* mime = curl_mime_init(curl.get());

    // 2. 添加表单字段
    for (const auto& [key, value] : form_data) {
        curl_mimepart* part = curl_mime_addpart(mime);
        curl_mime_name(part, key.c_str());
        curl_mime_data(part, value.c_str(), CURL_ZERO_TERMINATED);
    }

    // 3. 设置CURL选项
    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_MIMEPOST, mime);

    // 4. 设置头信息（复用现有函数）
    SetupCurlHeaders(curl.get(), headers);

    // 5. 执行请求（复用现有函数）
    std::string response = GetCurlHandle()->PerformRequest(curl.get());

    // 6. 清理资源
    curl_mime_free(mime);

    return response;
}

// 异步实现（回调方式）
void MyHttpClient::GetAsync(const std::string& url,
                          Callback callback,
                          const UrlParams& params,
                          const Headers& headers) {
    std::thread([=] { callback(Get(url, params, headers), 200); }).detach();
}

void MyHttpClient::PostAsync(const Request& request, Callback callback) {
    std::thread([=] { callback(Post(request), 200); }).detach();
}

void MyHttpClient::PutAsync(const Request& request, Callback callback) {
    std::thread([=] { callback(Put(request), 200); }).detach();
}

void MyHttpClient::DeleteAsync(const Request& request, Callback callback) {
    std::thread([=] { callback(Delete(request), 200); }).detach();
}

// 调试回调函数（复用之前的实现）
int DebugCallback(CURL* handle, curl_infotype type,
                  char* data, size_t size,
                  void* userptr) {
    if (type == CURLINFO_TEXT || type == CURLINFO_HEADER_IN ||
        type == CURLINFO_HEADER_OUT || type == CURLINFO_DATA_IN) {
        std::cerr << "[CURL DEBUG] ";
        std::cerr.write(data, size);
    }
    return 0;
}

// 异步上传的线程函数
void UploadThread(const std::string& url,
                  const std::string& filePath,
                  const std::string& fieldName,
                  std::function<void(std::string, int)> callback) {
    try {
        CURL* curl = curl_easy_init();
        if (!curl) {
            throw std::runtime_error("CURL initialization failed");
        }

        // 设置调试输出
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, DebugCallback);

        // 准备MIME
        curl_mime* mime = curl_mime_init(curl);
        curl_mimepart* part = curl_mime_addpart(mime);
        curl_mime_name(part, fieldName.c_str());
        curl_mime_filedata(part, filePath.c_str());

        // 设置请求选项
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);

        // 接收响应
        std::string response;
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        // 执行请求
        CURLcode res = curl_easy_perform(curl);

        // 获取状态码
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        // 清理资源
        curl_mime_free(mime);
        curl_easy_cleanup(curl);

        // 处理结果
        if (res != CURLE_OK) {
            throw std::runtime_error(curl_easy_strerror(res));
        }

        // 打印调试摘要
        std::cout << "[ASYNC UPLOAD SUMMARY]\n"
                  << "URL: " << url << "\n"
                  << "File: " << filePath << "\n"
                  << "Status: " << http_code << "\n"
                  << "Response size: " << response.size() << " bytes\n";

        callback(response, http_code);
    } catch (const std::exception& e) {
        std::cerr << "[ASYNC UPLOAD ERROR] " << e.what() << std::endl;
        callback(e.what(), 500);
    }
}


// Future风格的异步上传
std::future<std::string> MyHttpClient::AsyncUpload(const std::string& url,
                                                   const std::string& filePath,
                                                   const std::string& fieldName) {
    return std::async(std::launch::async, [=]() {
        std::promise<std::string> promise;
        UploadThread(url, filePath, fieldName,
                     [&](std::string response, int status) {
                         if (status >= 200 && status < 300) {
                             promise.set_value(response);
                         } else {
                             promise.set_exception(
                                     std::make_exception_ptr(
                                             std::runtime_error("HTTP " + std::to_string(status))
                                     )
                             );
                         }
                     });
        return promise.get_future().get();
    });
}

// 回调风格的异步上传
void MyHttpClient::UploadAsync(const std::string& url,
                               const std::string& filePath,
                               std::function<void(std::string, int)> callback,
                               const std::string& fieldName) {
    std::thread(UploadThread, url, filePath, fieldName, callback).detach();
}
