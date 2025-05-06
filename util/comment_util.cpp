//
// Created by Administrator on 2025/1/15 0015.
//
#include <iostream>
#include <map>
#include <string>
#include <nlohmann/json.hpp>
#include <vector>
#include <variant>
#include <type_traits>
#include "JsonValueUtil.cpp"

using namespace std;

using json = nlohmann::json;

class  CommentUtil {
public:
    //json转map  单层
    template<typename K, typename V>
    map<K, V> JsonDoDict(std::string jsonstr) {
        nlohmann::json json_str = nlohmann::json::parse(jsonstr);
        map<K, V> dataMap;
        for (auto &element: json_str.items()) {
            if (element.value().is_number_integer()) {
                dataMap[element.key()] = element.value().get<int>();
            } else if (element.value().is_number_float()) {
                dataMap[element.key()] = element.value().get<double>();
            } else if (element.value().is_boolean()) {
                dataMap[element.key()] = element.value().get<bool>();
            } else if (element.value().is_string()) {
                dataMap[element.key()] = element.value().get<std::string>();
            } else {
                // 处理其他类型或抛出异常
                std::cerr << "Unsupported type for key: " << element.key() << std::endl;
            }
        }


        // 输出 map 内容   单层
        for (const auto &pair: dataMap) {
            std::cout << pair.first << ": ";
            try {
                std::cout << std::any_cast<int>(pair.second) << std::endl;
            } catch (const std::bad_any_cast &) {
                try {
                    std::cout << std::any_cast<double>(pair.second) << std::endl;
                } catch (const std::bad_any_cast &) {
                    try {
                        std::cout << std::any_cast<float>(pair.second) << std::endl;
                    } catch (const std::bad_any_cast &) {
                        try {
                            std::cout << std::any_cast<bool>(pair.second) << std::endl;
                        } catch (const std::bad_any_cast &) {
                            try {
                                std::cout << std::any_cast<std::string>(pair.second) << std::endl;
                            } catch (const std::bad_any_cast &) {
                                std::cout << "Unknown type" << std::endl;
                            }
                        }
                    }
                }
            }
        }
        return dataMap;
    }


    void handle(const json &value) {
        if (value.is_string()) {
            std::cout << "String: " << value.get<std::string>() << std::endl;
        } else if (value.is_number()) {
            std::cout << "Number: " << value.get<double>() << std::endl;
        } else if (value.is_boolean()) {
            std::cout << "Bool: " << value.get<bool>() << std::endl;
        } else if (value.is_null()) {
            std::cout << "Null" << std::endl;
        } else if (value.is_array()) {
            std::cout << "Array:" << std::endl;
            for (const auto &item : value) {
                handle(item);
            }
        } else if (value.is_object()) {
            std::cout << "Object:" << std::endl;
            for (const auto &[key, val] : value.items()) {
                std::cout << key << ": ";
                handle(val);
            }
        }
    }


    template<typename T>
    struct is_string : std::false_type {
    };

    template<>
    struct is_string<std::string> : std::true_type {
    };


    template<typename T>
    std::string toString(const T &value) {
        if constexpr (std::is_same_v<T, std::string>) {
            return value;
        } else {
            return std::to_string(value);
        }
    }

    template<typename T>
    bool toString() {
        if constexpr (is_string<T>::value) {
            return false;
        } else {
            return true;
        }
    }

    //map转json
    template<typename K, typename V>
    std::string MapToJson(std::map<K, V> &map) {
        nlohmann::json json_str;
        if constexpr (is_string<K>::value) {
            for (auto &item: map) {
                json_str[item.first] = item.second;
            }
        } else {
            for (auto &item: map) {
                json_str[std::to_string(item.first)] = item.second;
            }
        }
        return json_str.dump();
    }


    //将字符串转为json
    nlohmann::json StrToJson(std::string &str) {
        nlohmann::json json_str = nlohmann::json::parse(str);
        return json_str;
    }

    // 将Person对象转换为map 没有获取对象参数的方法 可以直接在类中进行宏定义
    template<typename K, typename V, class T>
    std::map<K, V> objectToMap(const T &obj) {
        std::map<K, V> result;

        return result;
    }


//
//    template<typename T, typename... Args>
//    void processParamsToMap(std::map<std::string, std::string>& map, const std::string& key, const T& value, Args&&... args) {
//        map[key] = toString(value);
//        processParamsToMap(map, std::forward<Args>(args)...);
//    }
//
//
//    template<typename... Args,typename K,typename V>
//    std::map<K, V> createMapFromParams(Args&&... args) {
//        std::map<K, V> result;
//        processParamsToMap(result, std::forward<Args>(args)...);
//        return result;
//    }

    //多层嵌套 json转map
//    template <typename K, typename V>
    std::map<std::string, std::any> JsonToMapMore(const nlohmann::json &json_str) {
        std::map<std::string, std::any> dataMap;
        for (auto &item: json_str.items()) {
            if (item.value().is_object()) {
                dataMap[item.key()] = JsonToMapMore(item.value());
            } else if (item.value().is_array()) {
//                std::vector<std::variant<std::string, double, int, bool, std::nullptr_t>> array;
                vector<any> array;
                for (const auto &element: item.value()) {
                    if (element.is_string()) {
                        array.emplace_back(element.get<std::string>());
                    } else if (element.is_number_float()) {
                        array.emplace_back(element.get<double>());
                    } else if (element.is_number_integer()) {
                        array.emplace_back(element.get<int>());
                    } else if (element.is_boolean()) {
                        array.emplace_back(element.get<bool>());
                    } else if (element.is_null()) {
                        array.emplace_back(nullptr);
                    } else if (element.is_object()) {
                        dataMap[item.key()] = JsonToMapMore(element);
                    }
                }
                dataMap[item.key()] = array;
            } else if (item.value().is_string()) {
                dataMap[item.key()] = item.value().get<std::string>();
            } else if (item.value().is_number_float()) {
                dataMap[item.key()] = item.value().get<double>();
            } else if (item.value().is_number_integer()) {
                dataMap[item.key()] = item.value().get<int>();
            } else if (item.value().is_boolean()) {
                dataMap[item.key()] = item.value().get<bool>();
            } else if (item.value().is_null()) {
                dataMap[item.key()] = nullptr;
            }
        }
        return dataMap;
    }

    template<class T>
    void printVector(T &obj) {
        for (size_t i = 0; i < obj.size(); ++i) {
            std::cout << obj[i] << " ";
        }
    }

    template<typename K, typename V>
    void printMap(map<K, V> &myMap) {
        for (auto it = myMap.begin(); it != myMap.end(); ++it) {
            std::cout << it->first << ":" << it->second;
        }
    }

    //这个没有问题
    template<typename T>
    void jsonToMap(const nlohmann::json &j, map<string, T> &m) {
        for (auto it = j.begin(); it != j.end(); ++it) {
            if (it->is_primitive()) {
                m[it.key()] = it->get<T>();
            } else if (it->is_object()) {
                map<string, T> nestedMap;
                jsonToMap<T>(it.value(), nestedMap);
                m[it.key()] = nestedMap; // 这里需要类型转换，但通常不推荐这样做
            } else if (it->is_array()) {
                vector<T> vec;
                for (const auto &item: it.value()) {
                    vec.push_back(item.get<T>());
                }
                m[it.key()] = vec; // 这里也需要类型转换，但通常不推荐这样做
            }
        }
    }


    // 定义一个递归函数来处理 std::map 和 std::vector 的转换
//    template<typename T>
//    nlohmann::json mapToJson(const T& map);

    template<typename K, typename V>
    nlohmann::json mapToJson(const std::map<K, V> &map) {
        nlohmann::json j;
        for (const auto &pair: map) {
            j[pair.first] = mapToJson(pair.second);
        }
        return j;
    }

    template<typename T>
    nlohmann::json mapToJson(const std::vector<T> &vec) {
        nlohmann::json j = nlohmann::json::array();
        for (const auto &item: vec) {
            j.push_back(mapToJson(item));
        }
        return j;
    }

    template<typename T>
    nlohmann::json mapToJson(const T &value) {
        return value; // 基本类型直接返回
    }


    void PrintAnyParm(const std::map<std::string, std::any> myMap) {
        // 打印结果（仅作示例，实际使用时可能需要更复杂的处理）
        for (const auto &[key, value]: myMap) {
            std::cout << key << ": ";
            if (value.type() == typeid(std::string)) {
                std::cout << std::any_cast<std::string>(value);
            } else if (value.type() == typeid(double)) {
                std::cout << std::any_cast<double>(value);
            } else if (value.type() == typeid(int)) {
                std::cout << std::any_cast<int>(value);
            } else if (value.type() == typeid(bool)) {
                std::cout << std::any_cast<bool>(value);
            } else if (value.type() == typeid(nullptr)) {
                std::cout << "null";
            } else if (value.type() == typeid(std::vector<std::string>)) {
                auto result = std::any_cast<std::vector<std::string>>(value);
                printVector(result);
            } else if (value.type() == typeid(std::vector<double>)) {
                auto result = std::any_cast<std::vector<double>>(value);
                printVector(result);
            } else if (value.type() == typeid(std::vector<int>)) {
                auto result = std::any_cast<std::vector<int>>(value);
                printVector(result);
            } else if (value.type() == typeid(std::vector<char>)) {
                auto result = std::any_cast<std::vector<int>>(value);
                printVector(result);
            } else if (value.type() == typeid(std::vector<nullptr_t>)) {
                auto result = std::any_cast<std::vector<nullptr_t>>(value);
                printVector(result);
            } else if (value.type() == typeid(std::map<std::string, std::string>)) {
                auto result = std::any_cast<std::map<std::string, std::string>>(value);
                printMap(result);
            } else if (value.type() == typeid(std::map<std::string, double>)) {
                auto result = std::any_cast<std::map<std::string, double>>(value);
                printMap(result);
            } else if (value.type() == typeid(std::map<std::string, int>)) {
                auto result = std::any_cast<std::map<std::string, int>>(value);
                printMap(result);
            } else if (value.type() == typeid(std::map<std::string, bool>)) {
                auto result = std::any_cast<std::map<std::string, bool>>(value);
                printMap(result);
            } else if (value.type() == typeid(std::map<std::string, std::nullptr_t>)) {
                auto result = std::any_cast<std::map<std::string, std::nullptr_t>>(value);
                printMap(result);
            } else {
                // 处理其他类型或抛出异常
                std::cerr << "Unsupported type for key: " << key << std::endl;
            }
            std::cout << std::endl;
        }
    }

};
// 特化模板类，用于处理 MyObject
template<>
struct JsonValueUtil::ToJson<MyObject> {
    static json convert(const MyObject &obj) {
        json j;
        j["id"] = obj.id;
        j["info"] = obj.info;
        j["attributes"] = ToJson<decltype(obj.attributes)>::convert(obj.attributes);
        j["maplist"] = ToJson<decltype(obj.maplist)>::convert(obj.maplist);
        j["otherObj"] = ToJson<decltype(obj.otherObj)>::convert(obj.otherObj);
        return j;
    }
};


// 特化模板类，用于处理 MyObject
template<>
struct JsonValueUtil::FromJson<MyObject> {
    static MyObject jsonToObject(const json &j, MyObject &obj) {
        obj.id= FromJson<decltype(obj.id)>::jsonToObject(j["id"]);
        obj.info=FromJson<decltype(obj.info)>::jsonToObject(j["info"]);
        obj.attributes= FromJson<decltype(obj.attributes)>::jsonToObject(j["attributes"]);
        obj.maplist= FromJson<decltype(obj.maplist)>::jsonToObject(j["maplist"]);
        obj.otherObj=  FromJson<decltype(obj.otherObj)>::jsonToObject(j["otherObj"]);
        return obj;
    }
};




//动态反射注册类(不使用动态反射可以去除)
REGEDIT_DYNAMIC_REFLECTABLE(MyObject)
//动态反射注册类(不使用动态反射可以去除)
REGEDIT_DYNAMIC_REFLECTABLE(MyStruct)
int main(){
    CommentUtil comment;
    MyObject obj;

    dyrefl::For<MyObject>::for_each_propertie_name([](const char* name) {
        std::cout << "Field name: " << name << std::endl;
    });

//
//    std::string json_str = R"({"name": "John", "age": 30, "is_student": false})";
//    std::map<std::string, std::any> dataMap=comment.JsonDoDict<std::string,std::any>(json_str);
//
//    std::string json_str1 = R"({"name": "张三", "age": "30", "is_student": "false"})";
//    std::map<std::string, std::string> dataMap1=comment.JsonDoDict<std::string,std::string>(json_str1);
//
//    std::string json_str2 = R"({"key1": 100, "key2": 200, "key3": 300})";
//    std::map<std::string, std::any> dataMap2=comment.JsonDoDict<std::string,std::any>(json_str2);
//
//
//    std::string json_str3 = R"({"key1": 100.1, "key2": 200.2, "key3": 300.3})";
//    std::map<std::string, std::any> dataMap3=comment.JsonDoDict<std::string,std::any>(json_str3);
//    for(auto& item:dataMap2){
//        std::cout << item.first << ": " << item.second << std::endl;
//    }

    std::map<int, int> exampleMap = {
            {1, 3},
            {2, 5},
            {3, 7}
    };

    nlohmann:: json json_str4 = comment.MapToJson(exampleMap);
    std::cout << json_str4 << std::endl; // 格式化输出，缩进为4个空格

    std::map<std::string, std::any> dataMap4=comment.JsonDoDict<std::string,std::any>(json_str4);

    std::string jsonString = R"({
        "name": "John",
        "age": 30,
        "isStudent": false,
        "address": {
            "city": "New York",
            "zipcode": "10001"
        },
        "courses": ["Math", "Science"],
        "grades": [95, 88.5, 76]
    })";
    nlohmann::json json_str= comment.StrToJson(jsonString);
//    std::map<std::string, std::any> myMap = comment.JsonToMapMore(json_str);
//    comment.PrintAnyParm(myMap);

    map<string, nlohmann::json> result;

    comment.jsonToMap<nlohmann::json>(json_str,result);

    // 打印结果
    for (const auto& pair : result) {
        comment.handle(pair);
        cout << pair.first << ": " << pair.second << endl;
    }

    nlohmann::json json_str1=comment.mapToJson(result);

    cout<<json_str1.dump()<<endl;

        // 将 map 转换为 JSON
    json j = JsonValueUtil::ToJson<decltype(JsonValueUtil::ParamToMap())>::convert(JsonValueUtil::ParamToMap());

    obj=JsonValueUtil::for_each_propertie_value_json(obj,j["object1"]);


    // 输出 JSON 字符串
    std::cout << j.dump(4) << std::endl; // 格式化输出，缩进为4个空格

    map<string, json> result1;

    comment.jsonToMap<json>(j,result1);

    for (const auto& pair : result1) {
        cout << pair.first << ": " << pair.second << endl;
    }
    json pJson=j["object1"]["otherObj"];
    cout<<pJson.dump()<<endl;
    std::map<std::string, std::map<std::string, std::vector<std::list<std::string>>>>otherObj=pJson.get<decltype(MyObject::otherObj)>();

//    cout<<otherObj.begin()->second.begin()->first<<endl;

    obj=JsonValueUtil::FromJson<MyObject>::jsonToObject(j["object1"],obj);

    vector<std::string>vector1=dyrefl::For<MyObject>::for_each_propertie_name_return();
    for(auto&item:vector1){
        cout<<item<<endl;
    }

    std::map<string, json> maps=JsonValueUtil::for_each_propertie_value_return(obj);
    for(auto&[key,value]:maps){
        std::cout << "Field " << key << " has value: " << value.dump(4) << std::endl;
    }


     JsonValueUtil::Fromobj<MyObject>::jsonToObject(obj,j["object1"]);

    // 打印所有字段值
    dyrefl::For<MyObject>::for_each_propertie_value(obj, [](const char* name, auto&& value) {
        json str=JsonValueUtil::ToJson<decltype(value)>::convert(value);
        std::cout << "Field " << name << " has value: " << str.dump(4) << std::endl;
    });




    cout<<obj.otherObj.begin()->second.begin()->first<<endl;



    return 0;
}