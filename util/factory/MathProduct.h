// MathProduct.h
#include "GenericFactory.h"
#include <vector>
#include <map>
#include <iostream>

class MathProduct : public IProduct {
public:
    std::string getName() const override { return "MathProduct"; }

    // 各种类型的成员函数示例
    int add(int a, int b) { return a + b; }
    double multiply(double a, double b) const { return a * b; }
    void print(const std::string& msg) { std::cout << msg << std::endl; }
    static std::string getNameStatic() { return "MathProduct"; }

    // 容器参数示例
    std::vector<int> processVector(const std::vector<int>& vec) {
        std::vector<int> result;
        for (int v : vec) result.push_back(v * 2);
        return result;
    }

    // 嵌套容器示例
    std::map<std::string, std::vector<double>> processMap(const std::unordered_map<std::string, std::list<double>>& input) {
        std::map<std::string, std::vector<double>> result;
        for (const auto& [k, v] : input) {
            result[k] = {v.begin(), v.end()};
        }
        return result;
    }
};

// 注册扩展
namespace MathProductExtensions {
    inline void registerAll(GenericFactory& factory) {
        // 注册各种类型的操作
        factory.registerOperation<MathProduct, int, int,int>(
                FactoryType::MATH_FACTORY, "add", &MathProduct::add);

        factory.registerOperation<MathProduct, double, double, double>(
                FactoryType::MATH_FACTORY, "multiply", &MathProduct::multiply);

//        factory.registerOperation(
//                FactoryType::MATH_FACTORY, "print", &MathProduct::print);
        factory.registerOperation<MathProduct, void, const std::string&>(
                FactoryType::MATH_FACTORY, "print", &MathProduct::print);

        factory.registerOperation<std::string>(
                FactoryType::MATH_FACTORY, "getNameStatic", &MathProduct::getNameStatic);

        // 注册容器操作
        factory.registerOperation<MathProduct, std::vector<int>,const std::vector<int>&>(
                FactoryType::MATH_FACTORY, "processVector", &MathProduct::processVector);

        factory.registerOperation<MathProduct, std::map<std::string, std::vector<double>>,const std::unordered_map<std::string, std::list<double>>&>(
                FactoryType::MATH_FACTORY, "processMap", &MathProduct::processMap);
    }
}