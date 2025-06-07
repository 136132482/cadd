// asio_pool.hpp
#pragma once
#include <asio.hpp>
#include <queue>
#include <memory>

class AsioConnectionPool {
public:
    using SocketPtr = std::shared_ptr<asio::ip::tcp::socket>;

    AsioConnectionPool(asio::io_context& io, const std::string& host, int port, size_t pool_size)
            : io_(io), endpoint_(asio::ip::make_address(host), port) {
        for (size_t i = 0; i < pool_size; ++i) {
            auto sock = std::make_shared<asio::ip::tcp::socket>(io_);
            sock->async_connect(endpoint_, [this, sock](const asio::error_code& ec) {
                if (!ec) available_.push(sock);
            });
        }
    }

    // 获取连接（超时机制）
    SocketPtr acquire(std::chrono::milliseconds timeout = 100ms) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (cond_.wait_for(lock, timeout, [this] { return !available_.empty(); })) {
            auto sock = available_.front();
            available_.pop();
            return sock;
        }
        return nullptr;
    }

    // 释放连接
    void release(SocketPtr sock) {
        std::lock_guard<std::mutex> lock(mutex_);
        available_.push(sock);
        cond_.notify_one();
    }

private:
    asio::io_context& io_;
    asio::ip::tcp::endpoint endpoint_;
    std::queue<SocketPtr> available_;
    std::mutex mutex_;
    std::condition_variable cond_;
};