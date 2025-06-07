#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <boost/json.hpp>
#include <iostream>
#include <string>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace json = boost::json;

using tcp = net::ip::tcp;

// 结构体表示HTTP响应
struct HttpResponse {
    int status;
    std::string body;
    json::value json;
};

// 辅助函数：发送HTTP请求并返回响应
HttpResponse makeHttpRequest(
        const std::string& host,
        const std::string& port,
        const std::string& target,
        http::verb method,
        const std::string& body = "",
        const std::vector<std::pair<std::string, std::string>>& headers = {}
) {
    try {
        // 初始化I/O上下文和TCP解析器
        net::io_context ioc;
        tcp::resolver resolver(ioc);
        beast::tcp_stream stream(ioc);

        // 解析主机名和端口
        auto const results = resolver.resolve(host, port);
        stream.connect(results);

        // 构造HTTP请求
        http::request<http::string_body> req{method, target, 11}; // HTTP/1.1
        req.set(http::field::host, host);
        req.set(http::field::user_agent, "Boost.Beast");

        // 添加自定义头部
        for (const auto& [key, value] : headers) {
            req.set(key, value);
        }

        // 设置请求体
        if (!body.empty()) {
            req.body() = body;
            req.prepare_payload();
        }

        // 发送请求
        http::write(stream, req);

        // 接收响应
        beast::flat_buffer buffer;
        http::response<http::dynamic_body> res;
        http::read(stream, buffer, res);

        // 关闭连接
        beast::error_code ec;
        stream.socket().shutdown(tcp::socket::shutdown_both, ec);

        // 忽略连接关闭错误
        if (ec && ec != beast::errc::not_connected) {
            throw beast::system_error{ec};
        }

        // 解析响应
        std::string response_body = beast::buffers_to_string(res.body().data());
        json::value json_response;
        try {
            json_response = json::parse(response_body);
        } catch (...) {
            // 如果不是JSON格式，保持原始响应体
        }

        return HttpResponse{
                static_cast<int>(res.result_int()),
                response_body,
                json_response
        };
    } catch (const std::exception& e) {
        return HttpResponse{
                500,
                "Error: " + std::string(e.what()),
                json::value()
        };
    }
}

// 判断请求是否成功（仅HTTP状态码200为成功）
bool isRequestSuccess(const HttpResponse& response) {
    return response.status == 200; // 仅200为成功
}

// 测试 /users 接口
void testUsersEndpoint() {
    std::cout << "\n=== 测试 /users 接口 ===\n";

    // 测试正常情况
    auto response = makeHttpRequest(
            "127.0.0.1", "8888", "/users?page=1&limit=5", http::verb::get
    );

    if (isRequestSuccess(response)) {
        std::cout << "✅ /users 测试成功 (HTTP 200)\n";
        std::cout << "响应内容: " << response.body << std::endl;
    } else {
        std::cerr << "❌ /users 测试失败 (HTTP " << response.status << ")\n";
        std::cerr << "错误信息: " << response.body << std::endl;
    }

    // 测试错误情况（无效参数）
    response = makeHttpRequest(
            "127.0.0.1", "8888", "/users?page=invalid&limit=5", http::verb::get
    );

    if (!isRequestSuccess(response)) {
        std::cout << "✅ 正确捕获无效参数 (HTTP " << response.status << ")\n";
        std::cout << "错误响应: " << response.body << std::endl;
    } else {
        std::cerr << "❌ 未能正确捕获无效参数\n";
    }
}

// 测试 /register 接口
void testRegisterEndpoint() {
    std::cout << "\n=== 测试 /register 接口 ===\n";

    // 测试正常注册
    auto response = makeHttpRequest(
            "127.0.0.1", "8888", "/register", http::verb::post,
            R"({"username":"testuser", "email":"test@example.com", "password":"123456"})",
            {{"Content-Type", "application/json"}}
    );

    if (isRequestSuccess(response)) {
        std::cout << "✅ 注册成功 (HTTP 200)\n";
        std::cout << "响应内容: " << response.body << "\n\n";
    } else {
        std::cerr << "❌ 注册失败 (HTTP " << response.status << ")\n";
        std::cerr << "错误信息: " << response.body << "\n\n";
    }

    // 测试重复用户名
    response = makeHttpRequest(
            "127.0.0.1", "8888", "/register", http::verb::post,
            R"({"username":"testuser", "email":"new@example.com", "password":"123456"})",
            {{"Content-Type", "application/json"}}
    );

    if (!isRequestSuccess(response)) {
        std::cout << "✅ 正确捕获重复用户名 (HTTP " << response.status << ")\n";
        std::cout << "错误响应: " << response.body << "\n\n";
    } else {
        std::cerr << "❌ 未能捕获重复用户名\n\n";
    }

    // 测试无效邮箱
    response = makeHttpRequest(
            "127.0.0.1", "8888", "/register", http::verb::post,
            R"({"username":"newuser", "email":"invalid-email", "password":"123456"})",
            {{"Content-Type", "application/json"}}
    );

    if (!isRequestSuccess(response)) {
        std::cout << "✅ 正确捕获无效邮箱 (HTTP " << response.status << ")\n";
        std::cout << "错误响应: " << response.body << "\n\n";
    } else {
        std::cerr << "❌ 未能捕获无效邮箱\n\n";
    }

    // 测试密码太短
    response = makeHttpRequest(
            "127.0.0.1", "8888", "/register", http::verb::post,
            R"({"username":"newuser", "email":"valid@example.com", "password":"123"})",
            {{"Content-Type", "application/json"}}
    );

    if (!isRequestSuccess(response)) {
        std::cout << "✅ 正确捕获密码过短 (HTTP " << response.status << ")\n";
        std::cout << "错误响应: " << response.body << "\n\n";
    } else {
        std::cerr << "❌ 未能捕获密码过短\n\n";
    }
}

int main() {
    std::cout << "=== 开始测试（确保服务已运行在127.0.0.1:8888）===\n";
    testUsersEndpoint();
    testRegisterEndpoint();
    std::cout << "\n=== 测试完成 ===" << std::endl;
    return 0;
}