//#include <iostream>
//#include <tuple>
//#include <stdexcept>
//#include <cassert>
//#include <string_view>
//#include <optional>
//#include <utility> // For std::forward
//#include "Reflection.h"
//using namespace std;
//namespace refl {
//
//    // 这个宏用于创建字段信息
//// 定义REFLECTABLE_PROPERTIES宏
//#define REFLECTABLE_PROPERTIES(TypeName, ...) \
//    using CURRENT_TYPE_NAME = TypeName; \
//    static constexpr auto properties() { return std::make_tuple(__VA_ARGS__); }
//
//// 定义REFLECTABLE_MENBER_FUNCS宏
//#define REFLECTABLE_MENBER_FUNCS(TypeName, ...) \
//    using CURRENT_FUNC_NAME = TypeName; \
//    static constexpr auto member_funcs() { return std::make_tuple(__VA_ARGS__); }
//
//// 这个宏用于创建属性信息，并自动将字段名转换为字符串
//#define REFLEC_PROPERTY(Name) refl::Property<decltype(&CURRENT_TYPE_NAME::Name), &CURRENT_TYPE_NAME::Name>(#Name)
//#define REFLEC_FUNCTION(Func) refl::Function<decltype(&CURRENT_FUNC_NAME::Func), &CURRENT_FUNC_NAME::Func>(#Func)
//
//// 定义一个属性结构体，存储字段名称和值的指针
//    template <typename T, T Value>
//    struct Property {
//        const char* name;
//        constexpr Property(const char* name) : name(name) {}
//        constexpr T get_value() const { return Value; }
//    };
//    template <typename T, T Value>
//    struct Function {
//        const char* name;
//        constexpr Function(const char* name) : name(name) {}
//        constexpr T get_func() const { return Value; }
//    };
//
//    // 用于获取特定成员的值的函数，如果找不到名称，则返回默认值
//    template <typename T, typename Tuple, size_t N = 0, size_t RetTypeSize = 0>
//    constexpr void* __get_field_value_impl(T& obj, const char* name, const Tuple& tp) {
//        if constexpr (N >= std::tuple_size_v<Tuple>) {
//            return nullptr;
//        }
//        else {
//            const auto& prop = std::get<N>(tp);
//            if (std::string_view(prop.name) == name) {
//                cout<<RetTypeSize<<"="<<sizeof(prop.get_value())<<endl;
////                assert(RetTypeSize == sizeof(prop.get_value()));// 返回值类型传错了
//                return (void*)&(obj.*(prop.get_value()));
//            }
//            else {
//                return __get_field_value_impl<T, Tuple, N + 1, RetTypeSize>(obj, name, tp);
//            }
//        }
//    }
//
//    template <typename RetType, typename T, typename Tuple, size_t N = 0>
//    constexpr RetType* get_field_value(T& obj, const char* name, const Tuple& tp) {
//        return (RetType*)__get_field_value_impl<T, Tuple, N, sizeof(RetType)>(obj, name, tp);
//    }
//
//    // 成员函数相关:
//    template <typename RetType, typename T, typename FuncTuple, size_t N = 0, typename... Args>
//    constexpr std::optional<RetType> __invoke_member_func_impl(T& obj, const char* name, const FuncTuple& tp, Args&&... args) {
//        if constexpr (N >= std::tuple_size_v<FuncTuple>) {
//            //throw std::runtime_error(std::string(name) + " Function not found."); // 可以选择抛出异常或者通过optional判断是否成功
//            return std::optional<RetType>();
//        }
//        else {
//            const auto& func = std::get<N>(tp);
//            if (std::string_view(func.name) == name) {
//                return (obj.*(func.get_func()))(std::forward<Args>(args)...);
//            }
//            else {
//                return __invoke_member_func_impl<RetType, T, FuncTuple, N + 1>(obj, name, tp, std::forward<Args>(args)...);
//            }
//        }
//    }
//
//    template <typename RetType, typename T, typename... Args>
//    constexpr std::optional <RetType> invoke_member_func(T& obj, const char* name, Args&&... args) {
//        constexpr auto funcs = T::member_funcs();
//        return __invoke_member_func_impl<RetType>(obj, name, funcs, std::forward<Args>(args)...);
//    }
//
//
//    // 定义一个类型特征模板，用于获取属性信息
//    template <typename T>
//    struct For {
//        static_assert(std::is_class_v<T>, "Reflector requires a class type.");
//
//        // 遍历所有字段名称
//        template <typename Func>
//        static void for_each_propertie_name(Func&& func) {
//            constexpr auto props = T::properties();
//            std::apply([&](auto... x) {
//                ((func(x.name)), ...);
//            }, props);
//        }
//
//        // 遍历所有字段值
//        template <typename Func>
//        static void for_each_propertie_value(T& obj, Func&& func) {
//            constexpr auto props = T::properties();
//            std::apply([&](auto... x) {
//                ((func(x.name, obj.*(x.get_value()))), ...);
//            }, props);
//        }
//
//        // 遍历所有函数名称
//        template <typename Func>
//        static void for_each_member_func_name(Func&& func) {
//            constexpr auto props = T::member_funcs();
//            std::apply([&](auto... x) {
//                ((func(x.name)), ...);
//            }, props);
//        }
//    };
//
//}// namespace refl
//
//
//// =========================一下为使用示例代码====================================
//
//// 用户自定义的结构体，需要反射的字段使用REFLECTABLE宏来定义
//struct MyStruct {
//    int x{ 10 };
//    float y{ 20.5f };
//    int print() const {
//        std::cout << "MyStruct::print called! " << "x: " << x << ", y: " << y << std::endl;
//        return 666;
//    }
//
//    REFLECTABLE_PROPERTIES(MyStruct,REFLEC_PROPERTY(x),REFLEC_PROPERTY(y)
//    );
//    REFLECTABLE_MENBER_FUNCS(MyStruct,REFLEC_FUNCTION(print)
//    );
//};
//
//int main() {
//    MyStruct obj;
//
//    // 打印所有字段名称
//    refl::For<MyStruct>::for_each_propertie_name([](const char* name) {
//        std::cout << "Field name: " << name << std::endl;
//    });
//
//    // 打印所有字段值
//    refl::For<MyStruct>::for_each_propertie_value(obj, [](const char* name, auto&& value) {
//        std::cout << "Field " << name << " has value: " << value << std::endl;
//    });
//
//    // 打印所有函数名称
//    refl::For<MyStruct>::for_each_member_func_name([](const char* name) {
//        std::cout << "Member func name: " << name << std::endl;
//    });
//
//    // 获取特定成员的值，如果找不到成员，则返回默认值
//    auto x_value = get_field_value<int>(obj, "x", MyStruct::properties());
//    std::cout << "Field x has value: " << *x_value << std::endl;
//
//    auto y_value = get_field_value<float>(obj, "y", MyStruct::properties());
//    std::cout << "Field y has value: " << *y_value << std::endl;
//
//    auto z_value = get_field_value<int>(obj, "z", MyStruct::properties()); // "z" 不存在
//    std::cout << "Field z has value: " << z_value << std::endl;
//
//    // 通过字符串调用成员函数 'print'
//    auto print_ret = refl::invoke_member_func<int>(obj, "print");
//    std::cout << "print member return: " << print_ret.value() << std::endl;
//
//    return 0;
//}
//
//
