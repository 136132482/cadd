// websocket_client.h
#pragma once
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <functional>
#include <memory>
#include <queue>
#include <iostream>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace asio = boost::asio;
using tcp = boost::asio::ip::tcp;

class MyWebSocketClient {
public:
    using MessageHandler = std::function<void(const std::string&)>;
    using ErrorHandler = std::function<void(const std::string&)>;

    MyWebSocketClient(const std::string& host, const std::string& port);
    ~MyWebSocketClient();

    void connect(
            const std::string& path = "/",
            MessageHandler msgHandler = nullptr,
            ErrorHandler errHandler = nullptr
    );

    void send(const std::string& message);
    void close();
    bool isConnected() const;

private:
    void doConnect(const std::string& path);
    void doRead();
    void doWrite();
    void fail(const std::string& reason);

    std::string host_;
    std::string port_;
    asio::io_context ioc_;
    websocket::stream<tcp::socket> ws_;
    beast::flat_buffer buffer_;
    std::queue<std::string> writeQueue_;
    MessageHandler msgHandler_;
    ErrorHandler errHandler_;
    std::thread ioThread_;
    std::atomic<bool> connected_{false};
};