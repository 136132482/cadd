#ifndef ENUMSYSTEM_H
#define ENUMSYSTEM_H

#include <map>
#include <vector>
#include <string>
#include <stdexcept>
#include <sstream>  // 添加这个头文件

class EnumSystem {
public:
    static EnumSystem& instance() {
        static EnumSystem instance;
        return instance;
    }

    template <typename T>
    void add(T value, const std::vector<std::string>& params) {
        getStore<T>().add(value, params);

        if (params.size() >= 3) {
            int code = atoi(params[2].c_str());
            getCodeMap<T>()[code] = value;
        }
    }

    template <typename T>
    const std::vector<std::string>& getParams(T value) {
        return getStore<T>().get(value);
    }

    template <typename T>
    std::string getParamByIndex(T value, size_t index) {
        const std::vector<std::string>& params = getParams(value);
        if (index >= params.size()) {
            throw std::out_of_range("参数索引越界");
        }
        return params[index];
    }

    template <typename T>
    T getByCode(int code) {
        typename std::map<int, T>::iterator it = getCodeMap<T>().find(code);
        if (it == getCodeMap<T>().end()) {
            throw std::out_of_range("无效的code值");
        }
        return it->second;
    }

    template <typename T>
    std::vector<T> getAllValues() {
        std::vector<T> values;
        const typename EnumStore<T>::ItemMap& items = getStore<T>().getAllItems();
        for (typename EnumStore<T>::ItemMap::const_iterator it = items.begin();
             it != items.end(); ++it) {
            values.push_back(it->first);
        }
        return values;
    }

    // 新增：通过多个code获取枚举值集合
    template <typename T>
    std::vector<T> getByCodes(const std::vector<int>& codes) {
        std::vector<T> result;
        for (size_t i = 0; i < codes.size(); ++i) {
            try {
                result.push_back(getByCode<T>(codes[i]));
            } catch (const std::out_of_range&) {
                // 忽略无效code
            }
        }
        return result;
    }

    // 新增：通过拼接字符串获取枚举值集合
    template <typename T>
    std::vector<T> getByCodeString(const std::string& codeStr, char delimiter = ',') {
        std::vector<int> codes;
        std::stringstream ss(codeStr);
        std::string item;

        while (std::getline(ss, item, delimiter)) {
            try {
                codes.push_back(atoi(item.c_str()));
            } catch (...) {
                // 忽略无效数字
                throw std::out_of_range("无效的枚举值");
            }
        }

        return getByCodes<T>(codes);
    }

    // 新增：通过多个code获取参数集合
    template <typename T>
    std::vector<std::vector<std::string>> getMultiParams(const std::vector<int>& codes) {
        std::vector<std::vector<std::string>> result;
        auto values = getByCodes<T>(codes);
        for (size_t i = 0; i < values.size(); ++i) {
            result.push_back(getParams(values[i]));
        }
        return result;
    }

private:
    template <typename T>
    class EnumStore {
    public:
        typedef std::map<T, std::vector<std::string> > ItemMap;

        void add(T value, const std::vector<std::string>& params) {
            m_items[value] = params;
        }

        const std::vector<std::string>& get(T value) const {
            typename ItemMap::const_iterator it = m_items.find(value);
            if (it == m_items.end()) {
                throw std::out_of_range("无效的枚举值");
            }
            return it->second;
        }

        const ItemMap& getAllItems() const {
            return m_items;
        }

    private:
        ItemMap m_items;
    };

    template <typename T>
    struct TypeInfo {
        static EnumStore<T> store;
        static std::map<int, T> codeMap;
    };

    template <typename T>
    EnumStore<T>& getStore() {
        return TypeInfo<T>::store;
    }

    template <typename T>
    std::map<int, T>& getCodeMap() {
        return TypeInfo<T>::codeMap;
    }
};

// 静态成员初始化
template <typename T>
typename EnumSystem::EnumStore<T> EnumSystem::TypeInfo<T>::store;

template <typename T>
std::map<int, T> EnumSystem::TypeInfo<T>::codeMap;

#endif // ENUMSYSTEM_H