#ifndef GENERIC_FACTORY_H
#define GENERIC_FACTORY_H

#include <any>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>
#include <list>
#include <map>
#include <unordered_map>

enum class FactoryType { MATH_FACTORY, FRUIT_FACTORY, JSON_FACTORY,PHYSICS_FACTORY };

class IProduct {
public:
    virtual ~IProduct() = default;
    virtual std::string getName() const = 0;
};

class GenericFactory {
public:
    static GenericFactory& instance() {
        static GenericFactory instance;
        return instance;
    }

    GenericFactory(const GenericFactory&) = delete;
    GenericFactory& operator=(const GenericFactory&) = delete;

    // 注册产品工厂
    template <typename ProductT>
    void registerFactory(FactoryType type) {
        registerFactory(type, [] { return std::make_unique<ProductT>(); });
    }

    void registerFactory(FactoryType type, std::function<std::unique_ptr<IProduct>()> creator) {
        std::lock_guard<std::mutex> lock(mutex_);
        factoryCreators_[type] = std::move(creator);
    }

    // 创建产品
    std::unique_ptr<IProduct> createProduct(FactoryType type) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = factoryCreators_.find(type);
        if (it == factoryCreators_.end()) {
            throw std::runtime_error("Factory not registered");
        }
        return it->second();
    }

    // ========== 注册操作的各种重载 ==========

    // 1. 注册普通成员函数（非const）
    template <typename ProductT, typename Ret, typename... Args>
    void registerOperation(FactoryType type, const std::string& opName, Ret (ProductT::*func)(Args...)) {
        registerImpl<ProductT>(type, opName,
                               [func](IProduct* p, const std::vector<std::any>& args) -> std::any {
                                   return invokeMemberFunction<ProductT, Ret, Args...>(func, p, args);
                               });
    }

    // 2. 注册const成员函数
    template <typename ProductT, typename Ret, typename... Args>
    void registerOperation(FactoryType type, const std::string& opName, Ret (ProductT::*func)(Args...) const) {
        registerImpl<ProductT>(type, opName,
                               [func](IProduct* p, const std::vector<std::any>& args) -> std::any {
                                   return invokeMemberFunction<const ProductT, Ret, Args...>(func, p, args);
                               });
    }

    // 3. 注册静态成员函数或普通函数
    template <typename Ret, typename... Args>
    void registerOperation(FactoryType type, const std::string& opName, Ret (*func)(Args...)) {
        registerImpl<void>(type, opName,
                           [func](IProduct*, const std::vector<std::any>& args) -> std::any {
                               return invokeStaticFunction<Ret, Args...>(func, args);
                           });
    }

    // 4. 注册无返回值成员函数
    template <typename ProductT, typename... Args>
    void registerOperation(FactoryType type, const std::string& opName, void (ProductT::*func)(Args...)) {
        registerImpl<ProductT>(type, opName,
                               [func](IProduct* p, const std::vector<std::any>& args) -> std::any {
                                   invokeMemberFunction<ProductT, void, Args...>(func, p, args);
                                   return {};
                               });
    }

    // 5. 注册无返回值静态函数
    template <typename... Args>
    void registerOperation(FactoryType type, const std::string& opName, void (*func)(Args...)) {
        registerImpl<void>(type, opName,
                           [func](IProduct*, const std::vector<std::any>& args) -> std::any {
                               invokeStaticFunction<void, Args...>(func, args);
                               return {};
                           });
    }

    // ========== 执行操作 ==========

    template <typename Ret, typename... Args>
    Ret executeOperation(FactoryType type, const std::string& opName, IProduct* product, Args... args) {
        auto op = getOperation(type, opName);
        std::vector<std::any> anyArgs = {args...};
        std::any result = op(product, anyArgs);
        if constexpr (!std::is_same_v<Ret, void>) {
            return std::any_cast<Ret>(result);
        }
    }

    template <typename... Args>
    void executeOperation(FactoryType type, const std::string& opName, IProduct* product, Args... args) {
        auto op = getOperation(type, opName);
        std::vector<std::any> anyArgs = {args...};
        op(product, anyArgs);
    }

private:
    GenericFactory() = default;

    template <typename... Args, size_t... Is>
    static auto unpackArgsImpl(const std::vector<std::any>& args, std::index_sequence<Is...>) {
        return std::make_tuple(std::any_cast<Args>(args[Is])...);
    }

    template <typename... Args>
    static auto unpackArgs(const std::vector<std::any>& args) {
        return unpackArgsImpl<Args...>(args, std::index_sequence_for<Args...>{});
    }

    template <typename ProductT>
    void registerImpl(FactoryType type, const std::string& opName,
                      std::function<std::any(IProduct*, const std::vector<std::any>&)> func) {
        std::lock_guard<std::mutex> lock(mutex_);
        operations_[{type, opName}] = std::move(func);
    }

    template <typename ProductT, typename Ret, typename... Args, typename F>
    static std::any invokeMemberFunction(F&& f, IProduct* p, const std::vector<std::any>& args) {
        if (args.size() != sizeof...(Args)) {
            throw std::runtime_error("Argument count mismatch");
        }

        try {
            auto* product = dynamic_cast<ProductT*>(p);
            if (!product) throw std::runtime_error("Product type mismatch");

            if constexpr (std::is_same_v<Ret, void>) {
                std::apply([&](auto&&... unpackedArgs) {
                    (product->*f)(std::forward<decltype(unpackedArgs)>(unpackedArgs)...);
                }, unpackArgs<Args...>(args));
                return {};
            } else {
                return std::apply([&](auto&&... unpackedArgs) {
                    return std::any((product->*f)(std::forward<decltype(unpackedArgs)>(unpackedArgs)...));
                }, unpackArgs<Args...>(args));
            }
        } catch (const std::bad_any_cast& e) {
            throw std::runtime_error(std::string("Argument type mismatch: ") + e.what());
        }
    }

    template <typename Ret, typename... Args, typename F>
    static std::any invokeStaticFunction(F&& f, const std::vector<std::any>& args) {
        if (args.size() != sizeof...(Args)) {
            throw std::runtime_error("Argument count mismatch");
        }

        try {
            if constexpr (std::is_same_v<Ret, void>) {
                std::apply([&](auto&&... unpackedArgs) {
                    f(std::forward<decltype(unpackedArgs)>(unpackedArgs)...);
                }, unpackArgs<Args...>(args));
                return {};
            } else {
                return std::apply([&](auto&&... unpackedArgs) {
                    return std::any(f(std::forward<decltype(unpackedArgs)>(unpackedArgs)...));
                }, unpackArgs<Args...>(args));
            }
        } catch (const std::bad_any_cast& e) {
            throw std::runtime_error(std::string("Argument type mismatch: ") + e.what());
        }
    }

    std::function<std::any(IProduct*, const std::vector<std::any>&)>
    getOperation(FactoryType type, const std::string& opName) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = operations_.find({type, opName});
        if (it == operations_.end()) {
            throw std::runtime_error("Operation not registered");
        }
        return it->second;
    }

    std::map<FactoryType, std::function<std::unique_ptr<IProduct>()>> factoryCreators_;
    std::map<std::pair<FactoryType, std::string>,
            std::function<std::any(IProduct*, const std::vector<std::any>&)>> operations_;
    std::mutex mutex_;
};

#endif // GENERIC_FACTORY_H