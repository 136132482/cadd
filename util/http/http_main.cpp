#include "http_client.h"
#include <iostream>

int main() {
    // 1. GET带URL参数
    auto resp1 = HttpClient::Get("https://httpbin.org/get",
                                 {{"key1", "value1"}, {"key2", "value2"}},
                                 {{"User-Agent", "MyClient/1.0"}});
    std::cout << "GET with params:\n" << resp1 << "\n\n";

    // 2. POST JSON数据
    HttpClient::Request jsonReq{
            "https://httpbin.org/post",
            json{{"name", "John"}, {"age", 30}},
            {{"Content-Type", "application/json"}}
    };
    auto resp2 = HttpClient::Post(jsonReq);
    std::cout << "POST JSON:\n" << resp2 << "\n\n";

    // 3. POST表单数据
    HttpClient::Request formReq{
            "https://httpbin.org/post",
            HttpClient::FormData{{"username", "admin"}, {"password", "123456"}},
            {{"Content-Type", "application/x-www-form-urlencoded"}}
    };
    auto resp3 = HttpClient::Post(formReq);
    std::cout << "POST Form:\n" << resp3 << "\n\n";

    // 4. 异步PUT (Future方式)
    auto future = HttpClient::AsyncPut({
                                               "https://httpbin.org/put",
                                               std::string("This is raw data"),  // 显式转换
                                               {{"X-Custom-Header", "Test"}}
                                       });
    std::cout << "Async PUT result:\n" << future.get() << "\n\n";

    // 5. 异步DELETE (回调方式)
    HttpClient::DeleteAsync({
                                    "https://httpbin.org/delete",
                                    json{{"id", 123}},
                                    {{"Authorization", "Bearer token"}}
                            }, [](const std::string& resp, int code) {
        std::cout << "Async DELETE callback:\n" << resp << "\n";
    });

    return 0;
}