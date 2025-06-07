#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <stdexcept>
#include <iostream>
#include <unordered_set>
#include <optional>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/trim.hpp>

class NumberEnum {
public:
    // 添加枚举项时同时构建全字符串映射
    NumberEnum& add(int code, const std::vector<std::string>& params) {
        items[code] = params;
        return *this;
    }

    std::optional<int> findCode(const std::string& input_str, int field_count = -1) const {
        // 分割输入字符串
        std::vector<std::string> input_parts;
        boost::split(input_parts, input_str, boost::is_any_of(","));

        // 遍历所有枚举项
        for (const auto& [code, params] : items) {
            // 计算实际需要比较的字段数
            int compare_count = (field_count == -1) ?
                                std::min(params.size(), input_parts.size()) :
                                std::min(field_count, static_cast<int>(std::min(params.size(), input_parts.size())));

            bool match = true;
            for (int i = 0; i < compare_count; ++i) {
                // 特殊处理最后一个字段（能力字段）
                if (i == 3 && params.size() > 3 && input_parts.size() > 3) {
                    // 分割枚举值的能力字段
                    std::vector<std::string> enum_capabilities;
                    boost::split(enum_capabilities, params[3], boost::is_any_of("/"));

                    // 分割输入的能力字段
                    std::vector<std::string> input_capabilities;
                    boost::split(input_capabilities, input_parts[3], boost::is_any_of(",/"));

                    // 检查输入能力是否全部包含在枚举能力中
                    for (const auto& cap : input_capabilities) {
                        if (std::find(enum_capabilities.begin(), enum_capabilities.end(), cap) == enum_capabilities.end()) {
                            match = false;
                            break;
                        }
                    }
                }
                    // 普通字段直接比较
                else if (params[i] != input_parts[i]) {
                    match = false;
                    break;
                }
            }

            if (match) {
                return code;
            }
        }
        return std::nullopt;
    }
    // 获取所有数字键
    std::vector<int> getAllCodes() const {
        std::vector<int> codes;
        for (const auto& pair : items) {
            codes.push_back(pair.first);
        }
        return codes;
    }

    // 通过数字获取参数
    const std::vector<std::string>& getParams(int code) const {
        if (auto it = items.find(code); it != items.end()) {
            return it->second;
        }
        throw std::out_of_range("Invalid code");
    }

    std::vector<int> getCodesByParamValue(size_t index,
                                                 const std::string& value,
                                                 bool ignoreCase = true)
    {
        std::vector<int> result;

        auto compare = [&](const std::string& a, const std::string& b) {
            if (ignoreCase) {
                return std::equal(a.begin(), a.end(), b.begin(), b.end(),
                                  [](char c1, char c2) {
                                      return std::tolower(c1) == std::tolower(c2);
                                  });
            }
            return a == b;
        };

        for (const auto& item : items) {
            if (index < item.second.size() &&
                compare(item.second[index], value)) {
                result.push_back(item.first);
            }
        }

        return result;
    }




    // 通过数字和索引获取特定参数
    const std::string& getParam(int code, size_t index) const {
        const auto& params = getParams(code);
        if (index >= params.size()) {
            throw std::out_of_range("Param index out of range");
        }
        return params[index];
    }

    // 新增：通过多个code获取参数集合
    std::vector<std::vector<std::string>> getMultiParams(const std::vector<int>& codes) const {
        std::vector<std::vector<std::string>> result;
        for (int code : codes) {
            if (items.count(code)) {
                result.push_back(getParams(code));
            }
        }
        return result;
    }

    // 新增：通过code字符串获取参数集合（格式："101,102,201"）
    std::vector<std::vector<std::string>> getMultiParamsFromString(const std::string& codeStr, char delimiter = ',') const {
        std::vector<int> codes;
        size_t start = 0;
        size_t end = codeStr.find(delimiter);

        while (end != std::string::npos) {
            try {
                int code = std::stoi(codeStr.substr(start, end - start));
                codes.push_back(code);
            } catch (...) {
                // 忽略无效数字
            }
            start = end + 1;
            end = codeStr.find(delimiter, start);
        }

        // 处理最后一个code
        try {
            int code = std::stoi(codeStr.substr(start));
            codes.push_back(code);
        } catch (...) {
            // 忽略无效数字
        }
        return getMultiParams(codes);
    }


    std::vector<std::string> getDistinctCapabilitiesFromString(
            const std::string& codeStr,
            size_t capabilityIndex = 3
    ) const {
        std::unordered_set<std::string> all_capabilities;
        // 1. 自动检测车辆ID分隔符（支持 / 或 ,）
        char idDelimiter = (codeStr.find('/') != std::string::npos) ? '/' : ',';
        // 2. 解析车辆ID
        std::vector<std::string> codes;
        boost::split(codes, codeStr, boost::is_any_of(std::string(1, idDelimiter)));
        for (const auto& codeStr : codes) {
            try {
                int code = std::stoi(codeStr);
                if (auto params = getParams(code); capabilityIndex < params.size()) {
                    // 3. 自动检测能力字段分隔符
                    std::string caps = params[capabilityIndex];
                    char capDelimiter = (caps.find('/') != std::string::npos) ? '/' : ',';
                    // 4. 分割能力字段
                    std::vector<std::string> capabilities;
                    boost::split(capabilities, caps, boost::is_any_of(std::string(1, capDelimiter)));
                    for (const auto& cap : capabilities) {
                        all_capabilities.insert(boost::algorithm::trim_copy(cap));
                    }
                }
            } catch (...) {}
        }
        return {all_capabilities.begin(), all_capabilities.end()};
    }

private:
    std::unordered_map<int, std::vector<std::string>> items;
    std::unordered_map<std::string, int> full_string_map; // 新增全字符串映射

};