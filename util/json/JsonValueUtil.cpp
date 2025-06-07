//
// Created by Administrator on 2025/1/15 0015.
//


//    nlohmann::json mapToJsonV(const std::map<std::string, JsonValue>& map);
#include <nlohmann/json.hpp>
#include <iostream>
#include <utility>
#include <vector>
#include <variant>
#include <map>
#include <sstream>
//#include "comment_util.cpp"
#include <type_traits>
#include <string>
#include  "MyObject.h"
#include <tuple>

using namespace std;
using json = nlohmann::json;
//CommentUtil *comment =new CommentUtil();


//using JsonValue = std::variant<int, double, bool, std::string, std::vector<std::variant<int, double, bool, std::string>>, std::map<std::string, std::variant<int, double, bool, std::string>>>;


class  JsonValueUtil{


public:

    // 基本模板类，用于处理非容器和非特殊类型的数据
    template<typename T>
    struct ToJson{
        static json convert(const T& value) {
            return json(value);
        }
    };

// 特化模板类，用于处理 std::map
    template<typename K, typename V>
    struct ToJson<std::map<K, V>> {
        static json convert(const std::map<K, V>& map) {
            json j;
            for (const auto& [k, v] : map) {
                    j[ToJson<K>::convert(k)] = ToJson<V>::convert(v);
            }
            return j;
        }
    };

// 特化模板类，用于处理 std::vector
    template<typename T>
    struct ToJson<std::vector<T>> {
        static json convert(const std::vector<T>& vec) {
            json j = json::array();
            for (const auto& item : vec) {
                j.push_back(ToJson<T>::convert(item));
            }
            return j;
        }
    };

// 特化模板类，用于处理 std::list
    template<typename T>
    struct ToJson<std::list<T>> {
        static json convert(const std::list<T>& list) {
            json j = json::array();
            for (const auto& item : list) {
                j.push_back(ToJson<T>::convert(item));
            }
            return j;
        }
    };

    template <typename T>
    struct FromJson{
        static T jsonToObject(const json &j) {
            return j.get<T>();                                 // 将json对象转换为指定类型对象
        }
    };

    // 遍历所有字段值并返回字段名称和值的键值对集合
    template <typename T>
    static std::map<std::string, nlohmann::json> for_each_propertie_value_return(T &obj) {
        constexpr auto props = T::properties();
        std::map<std::string, nlohmann::json> values;
        std::apply([&](auto... x) {
            ((values[x.name] = JsonValueUtil::ToJson<decltype(obj.*(x.get_value()))>::convert(obj.*(x.get_value()))), ...);
        }, props);
        return values;
    }

    template <typename T>
    static T for_each_propertie_value_json(T &obj, json &j) {
        constexpr auto props = T::properties();
        std::apply([&](auto... x) {
            ((void)(
                    [&]() {
                        using MemberType = decltype(obj.*(x.get_value()));
                        if constexpr (std::is_reference<MemberType>::value) {
                            obj.*(x.get_value()) = FromJson<std::remove_reference_t<MemberType>>::jsonToObject(j[x.name]);
                        } else {
                            obj.*(x.get_value()) = FromJson<MemberType>::jsonToObject(j[x.name]);
                        }
                    }(), ...));
        }, props);
        return obj;
    }


    template <typename T>
    static std::map<std::string, std::any> for_each_propertie_value_return_any(T &obj) {
        constexpr auto props = T::properties();
        std::map<std::string, std::any> values;
        std::apply([&](auto... x) {
            ((values[x.name] = std::any(obj.*(x.get_value()))), ...);
        }, props);
        return values;
    }

    template<class T>
    struct Fromobj {
        static T jsonToObject(T &obj, json &j) {
            constexpr auto props = T::properties();
            constexpr std::size_t size = std::tuple_size_v<decltype(props)>;
            for (size_t i = 0; i < size; i++) {
                const auto &prop = std::get<0>(props);
                obj.*(prop.get_value()) = FromJson<decltype(obj.*(prop.get_value()))>::jsonToObject(j[prop.name]);
            }
//            auto apply = [&](auto&&... args) {
//                ((obj.*(args.name) = FromJson<decltype(obj.*(args.name))>::jsonToObject(j[args.name])), ...);
//            };
//            std::apply(apply, props);
//            return obj;
        }
    };


    // 从 JSON 转换为对象的辅助函数模板
    template<typename T>
    static void fromJson(const nlohmann::json& j, T& obj) {
        if constexpr (std::is_arithmetic_v<T> || std::is_same_v<T, std::string>) {
            obj = j.get<T>();
        } else if constexpr (std::is_same_v<T, std::vector<typename T::value_type>>) {
            obj.clear();
            for (const auto& item : j) {
                obj.push_back(fromJson<typename T::value_type>(item));
            }
        } else if constexpr (std::is_same_v<T, std::map<typename T::key_type, typename T::mapped_type>>) {
            obj.clear();
            for (const auto& [key, value] : j.items()) {
                obj[std::stoi(key)] = fromJson<typename T::mapped_type>(value);
            }
        } else {
            obj.clear();
            for (const auto& [key, value] : j.items()) {
                obj[key] = fromJson<typename T::mapped_type>(value);
            }
        }
    };

    static std::map<std::string, MyObject> ParamToMap(){
             // 创建一个多层嵌套的 map
            std::list<std::string> info = {"心心相系", "心心相系", "心心相系"};
            std::vector<std::list<std::string>> vectorlist={{"msg5", "msg5", "msg6"},{"msg7", "msg8", "msg9"}};
            std::map<std::string, std::vector<int>>maplist_info={{"msg5",{7,8,9}},{"msg5",{4,5,6}}};

            std::map<std::string, std::vector<std::list<std::string>>>maplist={{"msg11",vectorlist},{"msg11",vectorlist}};
            std::map<string, std::map<std::string, std::vector<std::list<std::string>>>> params = {{"1", maplist},{"2", maplist}};

             std::map<std::string, MyObject> data1 = {
            {"object1", MyObject(1, "Object One", {{"attr1", 10}, {"attr2", 20}}
            ,maplist_info,params)},
            {"object2", MyObject(2, "Object Two", {{"attr3", 30}, {"attr4", 40}}
            ,maplist_info,params)}
    };
        return data1;
     }

    static  MyObject ParamToObj(){
        // 创建一个多层嵌套的 map
        std::list<std::string> listinfo = {"心心相系", "心心相系", "心心相系"};
        std::vector<std::list<std::string>> vectorlist={{"msg5", "msg5", "msg6"},{"msg7", "msg8", "msg9"}};
        std::map<std::string, std::vector<int>>maplist_info={{"msg5",{7,8,9}},{"msg5",{4,5,6}}};

        std::map<std::string, std::vector<std::list<std::string>>>maplist={{"msg11",vectorlist},{"msg11",vectorlist}};
        std::map<string, std::map<std::string, std::vector<std::list<std::string>>>> params = {{"1", maplist},{"2", maplist}};

        MyObject obj(1, "Object One", {{"attr1", 10}, {"attr2", 20}}
                        ,maplist_info,params);
        return obj;
    }


};



//// 特化模板类，用于处理 MyObject
//template<>
//struct JsonValueUtil::ToJson<MyObject> {
//    static json convert(const MyObject &obj) {
//        json j;
//        j["id"] = obj.id;
//        j["listinfo"] = obj.name;
//        j["attributes"] = ToJson<decltype(obj.attributes)>::convert(obj.attributes);
//        j["maplist"] = ToJson<decltype(obj.maplist)>::convert(obj.maplist);
//        j["otherObj"] = ToJson<decltype(obj.otherObj)>::convert(obj.otherObj);
//        return j;
//    }
//};




//int main() {

//
//
//
////    // 创建一些测试数据
////    std::map<std::string, MyObject> data = {
////            {"张三", MyObject(1, "张三", {{"王五", 10}, {"孙八3", 20}})},
////            {"李四", MyObject(2, "李四", {{"赵六", 30}, {"陈9", 40}})}
////    };
//

//
//    // 将 map 转换为 JSON
//    json j = JsonValueUtil::ToJson<decltype(data1)>::convert(data1);
//
//    // 输出 JSON 字符串
//    std::cout << j.dump(4) << std::endl; // 格式化输出，缩进为4个空格
//
//    return 0;
//}