#pragma once
#include "ZmqInstance.h"
#include <memory>
#include <mutex>
#include <unordered_map>

namespace ZmqGlobal {
    class InstanceManager {
    public:
        static ZmqIn& getPublisher(const std::string& endpoint) {
            static std::mutex mtx;
            static std::unordered_map<std::string, std::shared_ptr<ZmqIn>> instances;
            std::lock_guard<std::mutex> lock(mtx);
            auto& inst = instances[endpoint];
            if (!inst) {
                inst.reset(new ZmqIn(true, endpoint)); // 改用new避免make_shared问题
                inst->start();
            }
            return *inst;
        }

        static ZmqIn& getSubscriber(const std::string& endpoint) {
            static std::mutex mtx;
            static std::unordered_map<std::string, std::shared_ptr<ZmqIn>> instances;

            std::lock_guard<std::mutex> lock(mtx);
            auto& inst = instances[endpoint];
            if (!inst) {
                inst.reset(new ZmqIn(false, endpoint));
                inst->start();
            }
            return *inst;
        }
    };
}