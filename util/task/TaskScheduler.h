#pragma once  // 添加头文件保护

#include <croncpp.h>
#include <iostream>
#include <chrono>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <atomic>
#include <functional>
#include <thread>
#include "../date/time_utils.h"
#include "../Logger/Logger.h"
#include "../date/DateUtil.h"

// 线程池实现
// 线程池实现（带日志）
class ThreadPool {
public:
    explicit ThreadPool(size_t threads, const std::string& name = "ThreadPool")
            : stop(false), logger(name) {
        logger.Info("线程池初始化，工作线程数: " + std::to_string(threads));

        for(size_t i = 0; i < threads; ++i) {
            workers.emplace_back([this, i] {
                logger.Debug("工作线程启动: " + std::to_string(i));

                while(true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->condition.wait(lock, [this] {
                            return this->stop || !this->tasks.empty();
                        });

                        if(this->stop && this->tasks.empty()) {
                            logger.Debug("工作线程停止: " + std::to_string(i));
                            return;
                        }

                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }

                    try {
                        auto start = std::chrono::steady_clock::now();
                        task(); // 执行任务
                        auto end = std::chrono::steady_clock::now();
                        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
                        logger.Trace("任务执行耗时: " + std::to_string(duration.count()) + "ms");
                    } catch(const std::exception& e) {
                        logger.Error("任务执行异常: " + std::string(e.what()));
                    }
                }
            });
        }
    }

    // 添加任务到队列
    template<class F>
    void enqueue(F&& f, const std::string& description = "") {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            tasks.emplace(std::forward<F>(f));
            logger.Debug(description.empty() ? "添加匿名任务" : "添加任务: " + description);
        }
        condition.notify_one();
    }

    ~ThreadPool() {
        logger.Info("线程池销毁中...");
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for(std::thread &worker: workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        logger.Info("线程池已销毁");
    }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    std::atomic<bool> stop;
    Logger logger; // 日志记录器
};

// 异步任务调度器（带完整日志）
class AsyncCronScheduler {
public:
    AsyncCronScheduler(size_t thread_pool_size = 4)
            : pool(thread_pool_size, "CronWorker"),
              running(false),
              logger("CronScheduler") {
        logger.Info("定时调度器初始化，线程池大小: " + std::to_string(thread_pool_size));
    }

    // 添加定时任务
    void add_task(const std::string& cron_expr, std::function<void()> task, const std::string& task_name = "") {
        std::lock_guard<std::mutex> lock(scheduler_mutex);
        try {
            auto expr = cron::make_cron(cron_expr);
            tasks.emplace_back(expr, std::move(task));
            logger.Info("添加定时任务: " + task_name + " Cron表达式: " + cron_expr);
        } catch(const std::exception& e) {
            logger.Error("解析Cron表达式失败: " + cron_expr + " 错误: " + e.what());
        }
    }

    // 启动调度器
    void start() {
        logger.Info("启动定时调度器...");
        running = true;
        scheduler_thread = std::thread([this]() {
            logger.Debug("调度线程开始运行");

            std::unordered_map<size_t, time_t> last_execution;
            size_t task_index = 0;

            while(running) {
                auto now = std::chrono::system_clock::now();
                auto now_time_t = std::chrono::system_clock::to_time_t(now);

                // 线程安全的时间转换
                std::tm now_tm;
                localtime_s(&now_tm, &now_time_t);

                {
                    std::lock_guard<std::mutex> lock(scheduler_mutex);
                    task_index = 0;

                    for(auto& [cron_expr, task] : tasks) {
                        try {
                            auto prev_time_t = now_time_t - 1;
                            std::tm prev_tm;
                            localtime_s(&prev_tm, &prev_time_t);

                            bool should_trigger = false;
                            auto next_for_now = cron::cron_next(cron_expr, now_tm);
                            auto next_for_prev = cron::cron_next(cron_expr, prev_tm);

                            if(std::mktime(&next_for_prev) != std::mktime(&next_for_now)) {
                                if(last_execution[task_index] != std::mktime(&next_for_now)) {
                                    should_trigger = true;
                                    last_execution[task_index] = std::mktime(&next_for_now);
                                    logger.Debug("触发任务: " + std::to_string(task_index));
                                }
                            }

                            if(should_trigger) {
                                pool.enqueue(task, "定时任务[" + std::to_string(task_index) + "]");
                            }
                        } catch(const std::exception& e) {
                            logger.Error("任务调度异常: " + std::string(e.what()));
                        }
                        task_index++;
                    }
                }

                // 动态调整检查间隔
                auto next_check = now + std::chrono::milliseconds(50);
                std::this_thread::sleep_until(next_check);
            }
            logger.Debug("调度线程停止运行");
        });
    }

    // 停止调度器
    void stop() {
        if(running) {
            logger.Info("停止定时调度器...");
            running = false;
            if(scheduler_thread.joinable()) {
                scheduler_thread.join();
            }
            logger.Info("定时调度器已停止");
        }
    }

    ~AsyncCronScheduler() {
        stop();
    }

private:
    ThreadPool pool;
    std::thread scheduler_thread;
    std::atomic<bool> running;
    std::vector<std::pair<cron::cronexpr, std::function<void()>>> tasks;
    std::mutex scheduler_mutex;
    Logger logger; // 日志记录器
};