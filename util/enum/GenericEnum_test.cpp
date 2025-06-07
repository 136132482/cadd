#include "GenericEnum.h"
#include "../../nopeoplecar/OrderEnums.h"

//#include "GenericEnum.cpp"
// 定义全局枚举对象
NumberEnum createVehicleEnum() {
    return NumberEnum()
            .add(101, {"四轮车", "快递", "快递车"})
            .add(102, {"四轮车", "冷藏", "冷藏车"})
            .add(201, {"无人机", "载重", "载重无人机"})
            .add(301, {"机器人", "清洁", "清洁机器人"});
}


// 定义全局枚举对象
NumberEnum createSTATUSEnum() {
    return NumberEnum()
            .add(101, {"成功", "0"})
            .add(102, {"失败", "-1"});
}




// 全局对象
NumberEnum VehicleEnum = createVehicleEnum();
NumberEnum StatusEnum  = createSTATUSEnum();

int main() {
    // 使用示例
    int code = 102;

    // 获取所有参数
    const auto& params = VehicleEnum.getParams(code);
    std::cout << "编码" << code << "的所有参数: ";
    for (const auto& p : params) {
        std::cout << p << " ";
    }
    const auto& params1 = StatusEnum.getParams(code);
    std::cout << "编码" << code << "的所有参数: ";
    for (const auto& p : params1) {
        std::cout << p << " ";
    }


    // 获取特定参数
    std::cout << "\n大类: " << VehicleEnum.getParam(code, 0);
    std::cout << "\n小类: " << VehicleEnum.getParam(code, 1);
    std::cout << "\n名称: " << VehicleEnum.getParam(code, 2);

    // 遍历所有枚举项
    std::cout << "\n\n所有车辆类型:";
    for (auto code : VehicleEnum.getAllCodes()) {
        std::cout << "\n- " << VehicleEnum.getParam(code, 2)
                  << " (编码:" << code << ")";
    }

    // 2. 多code查询
    std::vector<int> codes = {101, 201, 999}; // 999是无效code
    std::cout << "\n多code查询结果:\n";
    auto multiParams = VehicleEnum.getMultiParams(codes);
    for (const auto& params : multiParams) {
        for (const auto& p : params) {
            std::cout << p << " ";
        }
        std::cout << "\n";
    }

    // 3. 字符串形式的多code查询
    std::string codeStr = "101,102,301";
    std::cout << "\n字符串形式的多code查询:\n";
    auto paramsFromString = VehicleEnum.getMultiParamsFromString(codeStr);
    for (const auto& params : paramsFromString) {
        for (const auto& p : params) {
            std::cout << p << " ";
        }
        std::cout << "\n";
    }

    // 4. 状态枚举查询
    std::cout << "\n状态查询:\n";
    auto statusParams = StatusEnum.getMultiParamsFromString("101,102");
    for (const auto& params : statusParams) {
        std::cout << params[0] << " (" << params[1] << ")\n";
    }

    std::cout << "\n状态查询:\n";
    auto vestatusParams = VehicleEnum.getMultiParamsFromString("101,102");
    for (const auto& params : vestatusParams) {
        std::cout << params[0] << " (" << params[1] << ")\n";
    }
    // 示例1：不区分大小写匹配英文
    auto foodCodes = VehicleEnum.getCodesByParamValue(1, "food", true);
    std::cout << "Food相关订单code: ";
    for (int code : foodCodes) {
        std::cout << code << " "; // 输出: 102
    }

    // 示例2：不区分大小写匹配中文（自动精确匹配）
    auto vehicleCodes = VehicleEnum.getCodesByParamValue(0, "四轮车", true);
    std::cout << "\n四轮车类型订单code: ";
    for (int code : vehicleCodes) {
        std::cout << code << " "; // 输出: 101 102 301
    }

    // 示例3：精确匹配
    auto exactMatch = VehicleEnum.getCodesByParamValue(1, "MEDicine", false);
    std::cout << "\n精确匹配结果: ";
    for (int code : exactMatch) {
        std::cout << code << " "; // 输出: 201
    }

    auto exactMatch1 = OrderTypeEnum.getDistinctCapabilitiesFromString("101,102,401,501", 3);
    std::cout << "\n精确匹配结果: ";
    for (std::string& s : exactMatch1) {
        std::cout << s << " "; // 输出: 201
    }

    // 使用示例
    if (auto code = OrderTypeEnum.findCode("四轮车,鲜花,鲜花配送车,保温箱",4)) {
            std::cout << "车辆编码: " << *code << std::endl;
        }

    return 0;
}