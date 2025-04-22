//#include "GenericFactory.h"
//
//GenericFactory& GenericFactory::instance() {
//    static GenericFactory instance;
//    return instance;
//}
//
//void GenericFactory::registerFactory(FactoryType type, FactoryCreator creator) {
//    std::lock_guard<std::mutex> lock(mutex_);
//
//    // 验证工厂创建器
//    auto factory = creator();
//    if (!factory) {
//        throw std::invalid_argument("Invalid factory creator");
//    }
//
//    // 存储工厂创建器
//    factoryCreators_[type] = std::move(creator);
//    auto& storedFactory = factoryCreators_[type];
//
//    // 2. 修正CREATE操作的lambda签名
//    operations_[{type, OperationType::CREATE}] =
//            [&storedFactory](IProduct*, const std::vector<std::any>& args) -> std::any {
//                try {
//                    auto product = storedFactory()->create(args);
//                    // 3. 正确构造包含unique_ptr的any对象
//                    return std::make_any<std::shared_ptr<IProduct>>(std::move(product));
//                } catch (const std::exception& e) {
//                    throw std::runtime_error("Create failed: " + std::string(e.what()));
//                }
//            };
//
//    // 4. 修正PROCESS操作的lambda签名
//    operations_[{type, OperationType::PROCESS}] =
//            [&storedFactory](IProduct* p, const std::vector<std::any>& args) -> std::any {
//                if (!p) throw std::invalid_argument("Null product pointer");
//                try {
//                    storedFactory()->process(p, args);
//                    return {}; // 返回空的any
//                } catch (const std::exception& e) {
//                    throw std::runtime_error("Process failed: " + std::string(e.what()));
//                }
//            };
//
//    // 5. 修正DESTROY操作的lambda签名
//    operations_[{type, OperationType::DESTROY}] =
//            [&storedFactory](IProduct* p, const std::vector<std::any>&) -> std::any {
//                if (!p) throw std::invalid_argument("Null product pointer");
//                try {
//                    storedFactory()->destroy(p);
//                    return {}; // 返回空的any
//                } catch (const std::exception& e) {
//                    throw std::runtime_error("Destroy failed: " + std::string(e.what()));
//                }
//            };
//}
//
//
//void GenericFactory::registerOperation(FactoryType type, OperationType op, OperationFunc func) {
//    std::lock_guard<std::mutex> lock(mutex_);
//    operations_[{type, op}] = std::move(func);
//}
//
//std::unique_ptr<IProduct> GenericFactory::createProductImpl(FactoryType type,
//                                                            const std::vector<std::any>& args) {
//    std::lock_guard<std::mutex> lock(mutex_);
//
//    if (factoryCreators_.find(type) == factoryCreators_.end()) {
//        throw std::runtime_error("Factory not registered");
//    }
//
//    // 直接调用工厂创建器创建产品
//    return factoryCreators_[type]()->create(args);
//}
//
//std::any GenericFactory::execute(FactoryType type, OperationType op,
//                                 IProduct* product,
//                                 const std::vector<std::any>& args) {
//    // 获取操作处理器（加锁保护）
//    OperationFunc handler;
//    {
//        std::lock_guard<std::mutex> lock(mutex_);
//        auto opKey = std::make_pair(type, op);
//        auto it = operations_.find(opKey);
//        if (it == operations_.end()) {
//            throw std::runtime_error("Operation not registered");
//        }
//        handler = it->second;
//    }
//
//
//    // 处理产品创建（不加锁）
//    std::unique_ptr<IProduct> tempProduct;
//    if (product == nullptr) {
//        tempProduct = createProductImpl(type, args); // 调用公共接口
//        product = tempProduct.get();
//    }
//
//    // 执行操作（不加锁）
//    return handler(product, args);
//}