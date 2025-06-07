#include "EnumSystem.h"
#include "EnumSystem.cpp"


int main() {
    initEnums();
    EnumSystem& es = EnumSystem::instance();

    // 1. 遍历所有枚举值
    std::cout << "所有车辆类型:\n";
    std::vector<VehicleType> types = es.getAllValues<VehicleType>();
    for (size_t i = 0; i < types.size(); ++i) {
        std::cout << "- " << es.getParamByIndex(types[i], 0)
                  << " (code: " << es.getParamByIndex(types[i], 2) << ")\n";
    }

    // 2. 通过code查询
    std::cout << "\ncode 102 对应的车辆: "
              << es.getParamByIndex(es.getByCode<VehicleType>(102), 0) << "\n";

    // 3. 按索引访问参数
    std::cout << "\n卡车详细信息:\n";
    std::cout << "名称: " << es.getParamByIndex(TRUCK, 0) << "\n";
    std::cout << "类型: " << es.getParamByIndex(TRUCK, 1) << "\n";
    std::cout << "排量: " << es.getParamByIndex(TRUCK, 3) << "\n";


    // 1. 通过多个code查询
    std::vector<int> codes = {101, 201};
    std::vector<VehicleType> typeList = es.getByCodes<VehicleType>(codes);
    std::cout << "通过多个code查询结果:\n";
    for (auto type : typeList) {
        std::cout << "- " << es.getParamByIndex(type, 0) << "\n";
    }

    // 2. 通过拼接字符串查询
    std::string codeStr = "102,201";
    auto types2 = es.getByCodeString<VehicleType>(codeStr);
    std::cout << "\n通过字符串查询结果:\n";
    for (auto type : types2) {
        std::cout << "- " << es.getParamByIndex(type, 0) << "\n";
    }

    // 3. 获取多个code对应的完整参数
    auto paramsList = es.getMultiParams<VehicleType>({101, 102});
    std::cout << "\n多个code的参数:\n";
    for (auto& params : paramsList) {
        for (auto& p : params) {
            std::cout << p << " ";
        }
        std::cout << "\n";
    }

    return 0;
}