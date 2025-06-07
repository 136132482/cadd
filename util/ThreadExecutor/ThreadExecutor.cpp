#include <iostream>
#include "ThreadExecutor.h"

ThreadExecutor::ThreadExecutor(const Config& config) : config_(config) {
    initialized_ = true;
    start();
}

ThreadExecutor::~ThreadExecutor() {
    if (initialized_) {
        stop();
        waitAll();

        // 确保所有工作线程完成
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }
}

void ThreadExecutor::start() {
    if (!workers_.empty()) return;

    std::lock_guard<std::mutex> lock(queue_mutex_);
    for (size_t i = 0; i < config_.max_threads; ++i) {
        workers_.emplace_back(&ThreadExecutor::workerThread, this);
    }
}

void ThreadExecutor::stop() {
    if (!initialized_) return;

    stop_ = true;
    condition_.notify_all();
}

void ThreadExecutor::waitAll() {
    if (!initialized_) return;

    // 增加超时机制避免死等
    auto start = std::chrono::steady_clock::now();
    while (active_tasks_ > 0) {
        if (std::chrono::steady_clock::now() - start > std::chrono::seconds(5)) {
            std::cerr << "Warning: Timeout waiting for tasks to complete" << std::endl;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void ThreadExecutor::setConfig(const Config& config) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    config_ = config;
}

void ThreadExecutor::workerThread() {
    while (initialized_ && !stop_) {
        Task task;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            condition_.wait(lock, [this]() {
                return !tasks_.empty() || stop_ || !initialized_;
            });

            if ((stop_ && tasks_.empty()) || !initialized_) {
                return;
            }

            if (!tasks_.empty()) {
                task = std::move(tasks_.front());
                tasks_.pop();
            }
        }

        if (task.func) {
            try {
                task.func();
            } catch (...) {
                // 捕获所有异常避免线程崩溃
            }
            if (task.promise) {
                task.promise->set_value();
            }
            active_tasks_--;
        }
    }
}