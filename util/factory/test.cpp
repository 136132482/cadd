#include "FactoryInitializer.h"



//int main() {
//    auto& factory = GenericFactory::instance();
//
//    // 1. 注册工厂和操作
//    factory.registerFactory<MathProduct>(FactoryType::MATH_FACTORY);
//    MathProductExtensions::registerAll(factory);
//
//    // 2. 创建产品实例
//    auto mathProduct = factory.createProduct(FactoryType::MATH_FACTORY);
////    auto* mathProduct = dynamic_cast<MathProduct*>(product.get());
//
//    // 3. 测试基本运算
//    std::cout << "=== 基本运算测试 ===" << std::endl;
//    int sum = factory.executeOperation<int, int, int>(
//            FactoryType::MATH_FACTORY, "add", mathProduct.get(), 3, 4);
//    std::cout << "3 + 4 = " << sum << std::endl;
//
//    double productVal = factory.executeOperation<double, double, double>(
//            FactoryType::MATH_FACTORY, "multiply", mathProduct.get(), 2.5, 4.0);
//    std::cout << "2.5 * 4.0 = " << productVal << std::endl;
//
//    factory.executeOperation<void, const std::string&>(
//            FactoryType::MATH_FACTORY, "print", mathProduct.get(), "Hello from print!");
//
//    // 4. 测试静态方法
//    std::cout << "\n=== 静态方法测试 ===" << std::endl;
//    std::string name = factory.executeOperation<std::string>(
//            FactoryType::MATH_FACTORY, "getNameStatic", nullptr);
//    std::cout << "Static name: " << name << std::endl;
//
//    // 5. 测试容器操作
//    std::cout << "\n=== 容器操作测试 ===" << std::endl;
//
//    // 处理vector
//    std::vector<int> inputVec = {1, 2, 3};
//    auto processedVec = factory.executeOperation<std::vector<int>, const std::vector<int>&>(
//            FactoryType::MATH_FACTORY, "processVector", mathProduct.get(), inputVec);
//
//    std::cout << "Processed vector: ";
//    for (int num : processedVec) std::cout << num << " ";
//    std::cout << std::endl;
//
//    // 处理map
//    std::unordered_map<std::string, std::list<double>> inputMap = {
//            {"A", {1.0, 2.0}},
//            {"B", {3.0, 4.0}}
//    };
//    auto processedMap = factory.executeOperation<std::map<std::string, std::vector<double>>,const std::unordered_map<std::string, std::list<double>>&>(
//            FactoryType::MATH_FACTORY, "processMap", mathProduct.get(), inputMap);
//
//    std::cout << "Processed map:\n";
//    for (const auto& [key, vec] : processedMap) {
//        std::cout << key << ": ";
//        for (double val : vec) std::cout << val << " ";
//        std::cout << std::endl;
//    }
//
//    return 0;
//}


int main() {
    auto& factory = GenericFactory::instance();

    // 一行代码初始化所有工厂
    InitializeAllFactories(factory);

    // 使用数学工厂
    auto mathProduct = factory.createProduct(FactoryType::MATH_FACTORY);

    auto* product = dynamic_cast<MathProduct*>(mathProduct.get());
    auto name=product->getName();
    std::cout << "product name : " << name << std::endl;

    auto sum = factory.executeOperation<int, int, int>(
            FactoryType::MATH_FACTORY, "add", mathProduct.get(), 2, 3);
    std::cout << "Math result: " << sum << std::endl;

    // 使用物理工厂
    auto physicsProduct = factory.createProduct(FactoryType::PHYSICS_FACTORY);
    auto force = factory.executeOperation<double, double, double>(
            FactoryType::PHYSICS_FACTORY, "calculateForce", physicsProduct.get(), 5.0, 9.8);
    std::cout << "Physics result: " << force << std::endl;

    return 0;
}