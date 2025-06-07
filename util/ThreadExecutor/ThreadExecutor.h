#pragma once
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>
#include <future>
#include <atomic>
#include <memory>
#include <stdexcept>

class ThreadExecutor {
public:
    enum class ExecutionMode {
        SYNC,    // 同步执行
        ASYNC    // 异步执行
    };

    // 配置参数
    struct Config {
        ExecutionMode mode;  // 移除默认初始化
        size_t max_threads;
        size_t max_queue_size;

        // 提供构造函数替代默认初始化
        Config(ExecutionMode m = ExecutionMode::ASYNC,
               size_t threads = std::thread::hardware_concurrency(),
               size_t queue = 1000)
                : mode(m), max_threads(threads), max_queue_size(queue) {}
    };

    explicit ThreadExecutor(const Config& config = Config());
    ~ThreadExecutor();

    // 禁止拷贝和移动
    ThreadExecutor(const ThreadExecutor&) = delete;
    ThreadExecutor& operator=(const ThreadExecutor&) = delete;


    // 控制方法
    void start();
    void stop();
    void waitAll();
    void setConfig(const Config& config);

    // 在ThreadExecutor.h的类定义后添加这些模板方法实现

    template <typename F, typename... Args>
    auto execute(F&& func, Args&&... args)
    -> std::future<typename std::result_of<F(Args...)>::type> {

        using ReturnType = typename std::result_of<F(Args...)>::type;
        using PackagedTask = std::packaged_task<ReturnType()>;

        auto task = std::make_shared<PackagedTask>(
                std::bind(std::forward<F>(func), std::forward<Args>(args)...)
        );

        std::future<ReturnType> result = task->get_future();

        if (config_.mode == ExecutionMode::SYNC) {
            (*task)(); // 同步执行
        } else {
            auto wrapper = [task]() { (*task)(); };

            std::unique_lock<std::mutex> lock(queue_mutex_);
            condition_.wait(lock, [this]() {
                return tasks_.size() < config_.max_queue_size || stop_;
            });

            if (stop_) {
                throw std::runtime_error("ThreadExecutor is stopping");
            }

            tasks_.push({std::move(wrapper), nullptr});
            active_tasks_++;
            condition_.notify_one();
        }

        return result;
    }

    template <typename F, typename... Args>
    void executeBatch(const std::vector<std::tuple<Args...>>& tasks,
                      F&& func,
                      std::function<void(size_t, size_t)> progress = nullptr)
    {
        std::vector<std::future<void>> futures;
        futures.reserve(tasks.size());

        for (size_t i = 0; i < tasks.size(); ++i) {
            // 使用lambda捕获当前task的引用
            auto executeTask = [i, &tasks, &func, &progress, this]() {
                try {
                    // 关键修改：使用std::apply正确展开tuple参数
                    std::apply(func, tasks[i]);

                    if (progress) {
                        progress(i + 1, tasks.size());
                    }
                } catch (...) {
                    if (progress) {
                        progress(i + 1, tasks.size());
                    }
                    throw; // 重新抛出异常
                }
            };

            if (config_.mode == ExecutionMode::SYNC) {
                executeTask();
            } else {
                auto promise = std::make_shared<std::promise<void>>();
                futures.emplace_back(promise->get_future());

                {
                    std::unique_lock<std::mutex> lock(queue_mutex_);
                    condition_.wait(lock, [this]() {
                        return tasks_.size() < config_.max_queue_size || stop_;
                    });

                    if (stop_) return;

                    tasks_.push({
                                        std::move(executeTask),
                                        std::move(promise)
                                });
                    active_tasks_++;
                }
                condition_.notify_one();
            }
        }

        if (config_.mode == ExecutionMode::ASYNC) {
            for (auto& future : futures) {
                future.wait(); // 等待所有任务完成
            }
        }
    }
private:
    struct Task {
        std::function<void()> func;
        std::shared_ptr<std::promise<void>> promise;
    };

    void workerThread();

    Config config_;
    std::vector<std::thread> workers_;
    std::queue<Task> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_{false};
    std::atomic<size_t> active_tasks_{0};
    std::atomic<bool> initialized_{false};

};

#include <vector>
#include <list>
#include <tuple>
#include <iostream>


template<typename... Args>
class TaskGrouper {
public:
    using TaskType = std::tuple<Args...>;
    using TaskGroupType = std::vector<TaskType>;
    using TaskGroupsType = std::list<TaskGroupType>;

    // 构造函数接受任务列表和分组大小
    TaskGrouper(std::vector<TaskType> tasks, size_t groupSize = 5)
            : tasks_(std::move(tasks)), groupSize_(groupSize) {
        groupTasks();
    }

    // 获取分组结果
    const TaskGroupsType& getGroups() const { return taskGroups_; }

    // 遍历处理每个组
    template<typename F>
    void forEachGroup(F&& handler) {
        for (const auto& group : taskGroups_) {
            handler(group);
        }
    }

private:
    void groupTasks() {
        for (auto it = tasks_.begin(); it != tasks_.end();) {
            auto next = std::distance(it, tasks_.end()) >= groupSize_ ?
                        it + groupSize_ : tasks_.end();
            taskGroups_.emplace_back(it, next);
            it = next;
        }
    }

    std::vector<TaskType> tasks_;
    size_t groupSize_;
    TaskGroupsType taskGroups_;
};
