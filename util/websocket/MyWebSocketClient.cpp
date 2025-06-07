// websocket_client.cpp
#include "MyWebSocketClient.h"
#include <iostream>

MyWebSocketClient::MyWebSocketClient(const std::string& host, const std::string& port)
        : host_(host), port_(port), ws_(ioc_) {}

MyWebSocketClient::~MyWebSocketClient() {
    close();
    if (ioThread_.joinable()) ioThread_.join();
}

void MyWebSocketClient::connect(const std::string& path, MessageHandler msgHandler, ErrorHandler errHandler) {
    msgHandler_ = std::move(msgHandler);
    errHandler_ = std::move(errHandler);

    ioThread_ = std::thread([this, path]() {
        try {
            doConnect(path);
            ioc_.run();
        } catch (const std::exception& e) {
            fail(e.what());
        }
    });
}

void MyWebSocketClient::doConnect(const std::string& path) {
    tcp::resolver resolver(ioc_);
    auto const results = resolver.resolve(host_, port_);

    asio::connect(ws_.next_layer(), results.begin(), results.end());

    ws_.handshake(host_, path);
    connected_ = true;

    doRead();
}

void MyWebSocketClient::doRead() {
    ws_.async_read(buffer_,
                   [this](beast::error_code ec, size_t) {
                       if (ec) return fail(ec.message());

                       if (msgHandler_) {
                           msgHandler_(beast::buffers_to_string(buffer_.data()));
                       }
                       buffer_.consume(buffer_.size());
                       doRead();
                   });
}

void MyWebSocketClient::send(const std::string& message) {
    asio::post(ioc_,
               [this, message]() {
                   bool writeInProgress = !writeQueue_.empty();
                   writeQueue_.push(message);
                   if (!writeInProgress) {
                       doWrite();
                   }
               });
}

void MyWebSocketClient::doWrite() {
    ws_.async_write(asio::buffer(writeQueue_.front()),
                    [this](beast::error_code ec, size_t) {
                        if (ec) return fail(ec.message());

                        writeQueue_.pop();
                        if (!writeQueue_.empty()) {
                            doWrite();
                        }
                    });
}

void MyWebSocketClient::close() {
    if (connected_) {
        asio::post(ioc_, [this]() {
            ws_.async_close(websocket::close_code::normal,
                            [this](beast::error_code ec) {
                                if (ec) fail(ec.message());
                                connected_ = false;
                            });
        });
    }
}

void MyWebSocketClient::fail(const std::string& reason) {
    if (errHandler_) errHandler_(reason);
    connected_ = false;
}

bool MyWebSocketClient::isConnected() const {
    return connected_;
}