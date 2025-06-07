// TaskManager.h
#pragma once
#include <shared_mutex>
#include <unordered_map>
#include <drogon/drogon.h>

class TaskManager {
public:
    static TaskManager& instance() {
        static TaskManager inst;
        return inst;
    }

    void setTask(int id, std::string status) {
        std::unique_lock lock(mutex_);
        tasks_[id] = std::move(status);
    }

    std::string getTask(int id) const {
        std::shared_lock lock(mutex_);
        return tasks_.count(id) ? tasks_.at(id) : "pending";
    }

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<int, std::string> tasks_;
};