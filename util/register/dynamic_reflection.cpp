//
// Created by Administrator on 2025/1/22 0022.
//
#include <iostream>
#include <tuple>
#include <stdexcept>
#include <cassert>
#include <string_view>
#include <optional>
#include <utility> // For std::forward
#include <unordered_map>
#include <functional>
#include <memory>
#include <any>
#include <type_traits> // For std::is_invocable
#include <nlohmann/json.hpp>
 // For std::is_invocable

using  namespace  std;

namespace dyrefl {

    // 这个宏用于创建字段信息
#define REFLECTABLE_PROPERTIES(TypeName, ...)  using CURRENT_TYPE_NAME = TypeName; \
    static constexpr auto properties() { return std::make_tuple(__VA_ARGS__); }
#define REFLECTABLE_MENBER_FUNCS(TypeName, ...) using CURRENT_FUNC_NAME = TypeName; \
    static constexpr auto member_funcs() { return std::make_tuple(__VA_ARGS__); }

// 这个宏用于创建属性信息，并自动将字段名转换为字符串
#define REFLEC_PROPERTY(Name) dyrefl::Property<decltype(&CURRENT_TYPE_NAME::Name), &CURRENT_TYPE_NAME::Name>(#Name)
#define REFLEC_FUNCTION(Func) dyrefl::Function<decltype(&CURRENT_FUNC_NAME::Func), &CURRENT_FUNC_NAME::Func>(#Func)

// 定义一个属性结构体，存储字段名称和值的指针
    template<typename T, T Value>
    struct Property {
        const char *name;

        constexpr Property(const char *name) : name(name) {}

        constexpr T get_value() const { return Value; }
    };

    template<typename T, T Value>
    struct Function {
        const char *name;

        constexpr Function(const char *name) : name(name) {}

        constexpr T get_func() const { return Value; }
    };

    // 使用 std::any 来处理不同类型的字段值和函数返回值
    template<typename T, typename Tuple, size_t N = 0>
    std::any __get_field_value_impl(T &obj, const char *name, const Tuple &tp) {
        if constexpr (N >= std::tuple_size_v<Tuple>) {
            return std::any();// Not Found!
        } else {
            const auto &prop = std::get<N>(tp);
            if (std::string_view(prop.name) == name) {
                return std::any(obj.*(prop.get_value()));
            } else {
                return __get_field_value_impl<T, Tuple, N + 1>(obj, name, tp);
            }
        }
    }

    // 使用 std::any 来处理不同类型的字段值和函数返回值
    template<typename T, size_t N = 0>
    std::any get_field_value(T &obj, const char *name) {
        return __get_field_value_impl(obj, name, T::properties());
    }

    // 使用 std::any 来处理不同类型的字段值和函数返回值
    template<typename T, typename Tuple, typename Value, size_t N = 0>
    std::any __assign_field_value_impl(T &obj, const char *name, const Value &value, const Tuple &tp) {
        if constexpr (N >= std::tuple_size_v<Tuple>) {
            return std::any();// Not Found!
        } else {
            const auto &prop = std::get<N>(tp);
            if (std::string_view(prop.name) == name) {
                if constexpr (std::is_assignable_v<decltype(obj.*(prop.get_value())), Value>) {
                    obj.*(prop.get_value()) = value;
                    return std::any(obj.*(prop.get_value()));
                } else {
                    assert(false);// 无法赋值 类型不匹配!!
                    return std::any();
                }
            } else {
                return __assign_field_value_impl < T, Tuple, Value, N + 1 > (obj, name, value, tp);
            }
        }
    }

    template<typename T, typename Value>
    std::any assign_field_value(T &obj, const char *name, const Value &value) {
        return __assign_field_value_impl(obj, name, value, T::properties());
    }

    // 成员函数调用相关:
    template<bool check_arg_typs = true, typename T, typename FuncTuple, size_t N = 0, typename... Args>
     std::any __invoke_member_func_impl(T &obj, const char *name, const FuncTuple &tp, Args &&... args) {
        if constexpr (N >= std::tuple_size_v<FuncTuple>) {
            return std::any();// Not Found!
        } else {
            const auto &func = std::get<N>(tp);
            if (std::string_view(func.name) == name) {
                if constexpr (std::is_invocable_v<decltype(func.get_func()), T &, Args...>) {
                    return std::invoke(func.get_func(), obj, std::forward<Args>(args)...);
                } else {
                    assert(!check_arg_typs);// 调用参数不匹配
                    return std::any();
                }
            } else {
                return __invoke_member_func_impl<check_arg_typs, T, FuncTuple, N + 1>(obj, name, tp,
                                                                                      std::forward<Args>(args)...);
            }
        }
    }

    template<typename T, typename... Args>
     std::any invoke_member_func(T &obj, const char *name, Args &&... args) {
        constexpr auto funcs = T::member_funcs();
        return __invoke_member_func_impl(obj, name, funcs, std::forward<Args>(args)...);
    }

    template<typename T, typename... Args>
     std::any invoke_member_func_type_safe(T &obj, const char *name, Args &&... args) {
        constexpr auto funcs = T::member_funcs();
        return __invoke_member_func_impl<true>(obj, name, funcs, std::forward<Args>(args)...);
    }


    // 定义一个类型特征模板，用于获取属性信息
    template<typename T>
    struct For {
        static_assert(std::is_class_v<T>, "Reflector requires a class type.");

        // 遍历所有字段名称
        template<typename Func>
        static void for_each_propertie_name(Func &&func) {
            constexpr auto props = T::properties();
            std::apply([&](auto... x) {
                ((func(x.name)), ...);
            }, props);
        }

        // 遍历所有字段名称并返回字段名称集合
        static std::vector<std::string> for_each_propertie_name_return() {
            constexpr auto props = T::properties();
            std::vector<std::string> names;
            std::apply([&](auto... x) {
                ((names.push_back(x.name)), ...);
            }, props);
            return names;
        }




        // 遍历所有字段值
        template<typename Func>
        static void for_each_propertie_value(T &obj, Func &&func) {
            constexpr auto props = T::properties();
            std::apply([&](auto... x) {
                ((func(x.name, obj.*(x.get_value()))), ...);
            }, props);
        }

        // 遍历所有函数名称
        template<typename Func>
        static void for_each_member_func_name(Func &&func) {
            constexpr auto props = T::member_funcs();
            std::apply([&](auto... x) {
                ((func(x.name)), ...);
            }, props);
        }
    };

}

    // ===============================================================

    // 以下是动态反射机制的支持代码：
namespace dynamic {

        // 为每个类型定义注册变量，这段宏需要出现在cpp中。
#define REGEDIT_DYNAMIC_REFLECTABLE(TypeName)\
        const bool TypeName::is_registered = [] { \
            static dynamic::TypeRegistryEntry<TypeName> entry; \
            return true; \
        }();
        // 用于注册类型信息的宏
#define DECL_DYNAMIC_REFLECTABLE(TypeName) \
        friend class dynamic::TypeRegistryEntry<TypeName>; \
        static std::string_view static_type_name() { return #TypeName; } \
        virtual std::string_view get_type_name() const override { return static_type_name(); } \
        static std::unique_ptr<::dynamic::IReflectable> create_instance() { return std::make_unique<TypeName>(); } \
        static const bool is_registered; \
        std::any get_field_value_by_name(const char* name) const override { \
            return dyrefl::get_field_value(*this, name); \
        } \
        std::any invoke_member_func_by_name(const char* name) override { \
            return dyrefl::invoke_member_func(*static_cast<TypeName*>(this), name); \
        }\
        std::any invoke_member_func_by_name(const char* name, std::any param1) override { \
            return dyrefl::invoke_member_func(*static_cast<TypeName*>(this), name, param1); \
        }\
        std::any invoke_member_func_by_name(const char* name, std::any param1, std::any param2) override { \
            return dyrefl::invoke_member_func(*static_cast<TypeName*>(this), name, param1, param2); \
        }\
        std::any invoke_member_func_by_name(const char* name, std::any param1, std::any param2, std::any param3) override { \
            return dyrefl::invoke_member_func(*static_cast<TypeName*>(this), name, param1, param2, param3); \
        }\
        std::any invoke_member_func_by_name(const char* name, std::any param1, std::any param2, std::any param3, std::any param4) override { \
            return dyrefl::invoke_member_func(*static_cast<TypeName*>(this), name, param1, param2, param3, param4); \
        }\


        // 反射基类
        class IReflectable {
        public:
            virtual ~IReflectable() = default;

            virtual std::string_view get_type_name() const = 0;

            virtual std::any get_field_value_by_name(const char *name) const = 0;

            virtual std::any invoke_member_func_by_name(const char *name) = 0;

            virtual std::any invoke_member_func_by_name(const char *name, std::any param1) = 0;

            virtual std::any invoke_member_func_by_name(const char *name, std::any param1, std::any param2) = 0;

            virtual std::any
            invoke_member_func_by_name(const char *name, std::any param1, std::any param2, std::any param3) = 0;

            virtual std::any
            invoke_member_func_by_name(const char *name, std::any param1, std::any param2, std::any param3,
                                       std::any param4) = 0;
            // 不能无限增加，会增加虚表大小。最多支持4个参数的调用。
        };

        // 类型注册工具
        class TypeRegistry {
        public:
            using CreatorFunc = std::function<std::unique_ptr<IReflectable>()>;

            static TypeRegistry &instance() {
                static TypeRegistry registry;
                return registry;
            }

            void register_type(const std::string_view type_name, CreatorFunc creator) {
                creators[type_name] = std::move(creator);
            }

            std::unique_ptr<IReflectable> create(const std::string_view type_name) {
                if (auto it = creators.find(type_name); it != creators.end()) {
                    return it->second();
                }
                return nullptr;
            }

        private:
            std::unordered_map<std::string_view, CreatorFunc> creators;
        };


        // 用于在静态区域注册类型的辅助类
        template<typename T>
        class TypeRegistryEntry {
        public:
            TypeRegistryEntry() {
                ::dynamic::TypeRegistry::instance().register_type(T::static_type_name(), &T::create_instance);
            }
        };
}




// 用户自定义的结构体
class MyStruct : public ::dynamic::IReflectable {
    // 如果不需要动态反射，可以不从public refl::dynamic::IReflectable派生
public:
    int x{ 10 };
    double y{ 20.5f };
    int print() const {
        std::cout << "MyStruct::print called! " << "x: " << x << ", y: " << y << std::endl;
        return 666;
    }
    // 如果需要支持动态调用，参数必须是std::any，并且不能超过4个参数。
    int print_with_arg(std::any param) const {
        std::cout << "MyStruct::print called! " << " arg is: " << std::any_cast<int>(param) << std::endl;
        return 888;
    }

    REFLECTABLE_PROPERTIES(MyStruct,REFLEC_PROPERTY(x),REFLEC_PROPERTY(y)
    );
    REFLECTABLE_MENBER_FUNCS(MyStruct,
                             REFLEC_FUNCTION(print),
                             REFLEC_FUNCTION(print_with_arg)
    );

    DECL_DYNAMIC_REFLECTABLE(MyStruct)//动态反射的支持，如果不需要动态反射，可以去掉这行代码
};




int main() {
    MyStruct obj;
    // # 静态反射部分：
    // 打印所有字段名称
    dyrefl::For<MyStruct>::for_each_propertie_name([](const char* name) {
        std::cout << "Field name: " << name << std::endl;
    });

    // 打印所有字段值
    dyrefl::For<MyStruct>::for_each_propertie_value(obj, [](const char* name, auto&& value) {
        std::cout << "Field " << name << " has value: " << value << std::endl;
    });

    // 打印所有函数名称
    dyrefl::For<MyStruct>::for_each_member_func_name([](const char* name) {
        std::cout << "Member func name: " << name << std::endl;
    });

    // 获取特定成员的值，如果找不到成员，则返回默认值
    auto x_value = dyrefl::get_field_value(obj, "x");
    std::cout << "Field x has value: " << std::any_cast<int>(x_value) << std::endl;

    auto y_value = dyrefl::get_field_value(obj, "y");
    std::cout << "Field y has value: " << std::any_cast<double>(y_value) << std::endl;

    //修改值：
    dyrefl::assign_field_value(obj, "y", 33.33f);
    y_value = dyrefl::get_field_value(obj, "y");
    std::cout << "Field y has modifyed,new value is: " << std::any_cast<double>(y_value) << std::endl;

    auto z_value = dyrefl::get_field_value(obj, "z"); // "z" 不存在
    if (z_value.type().name() == std::string_view("int")) {
        std::cout << "Field z has value: " << std::any_cast<int>(z_value) << std::endl;
    }

    // 通过字符串调用成员函数 'print'
    auto print_ret = dyrefl::invoke_member_func_type_safe(obj, "print");
    std::cout << "print member return: " << std::any_cast<int>(print_ret) << std::endl;


    std::cout << "---------------------" << std::endl;

    // 动态反射部分(动态反射完全不需要知道类型MyStruct的定义)：
    // 动态创建 MyStruct 实例并调用方法
    auto instance = dynamic::TypeRegistry::instance().create("MyStruct");
    if (instance) {
        std::cout << "Dynamic instance type: " << instance->get_type_name() << std::endl;
        // 这里可以调用 MyStruct 的成员方法
        auto x_value2 = instance->get_field_value_by_name("x");
        std::cout << "Field x has value: " << std::any_cast<int>(x_value2) << std::endl;

        instance->invoke_member_func_by_name("print");
        instance->invoke_member_func_by_name("print_with_arg", 10);
        instance->invoke_member_func_by_name("print_with_arg", 20, 222);//这个调用会失败，命中断言，因为print_with_arg只接受一个函数
    }
    return 0;
}

