#ifndef FACTORY_MANAGER_H
#define FACTORY_MANAGER_H

#include "GenericFactory.h"
#include <map>
#include <mutex>
#include <memory>

class FactoryManager {
public:
    static FactoryManager& instance() {
        static FactoryManager instance;
        return instance;
    }

    void registerFactory(std::unique_ptr<IFactory> factory) {
        std::lock_guard<std::mutex> lock(mutex_);
        factories_[factory->getFactoryType()] = std::move(factory);
    }

    std::unique_ptr<IProduct> createProduct(const std::string& factoryType,
                                            const std::vector<std::any>& args = {}) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = factories_.find(factoryType);
        if (it == factories_.end()) {
            throw std::runtime_error("Factory not registered: " + factoryType);
        }
        return it->second->create(args);
    }

private:
    FactoryManager() = default;
    std::map<std::string, std::unique_ptr<IFactory>> factories_;
    std::mutex mutex_;
};

#endif // FACTORY_MANAGER_H