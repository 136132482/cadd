//
// Created by Administrator on 2025/1/17 0017.
//

#ifndef __BASE_H__
#define __BASE_H__
#include <string>
#include <map>
#include <iostream>

// 使用模板，每个基类单独生成一个 ClassRegister
// 好处是需要反射的类不需要去继承 Object 对象
// ClassRegister 用来管理类名->类构造函数的映射，对外提供根据类名构造对象对函数
template<typename ClassName>
class ClassRegister {
public:
    typedef ClassName* (*Constructor)(void);
private:
    typedef std::map<std::string, Constructor> ClassMap;
    ClassMap constructor_map_;
public:
    // 添加新类的构造函数
    void AddConstructor(const std::string class_name, Constructor constructor) {
        typename ClassMap::iterator it = constructor_map_.find(class_name);
        if (it != constructor_map_.end()) {
            std::cout << "error!";
            return;
        }
        constructor_map_[class_name] = constructor;
    }
    // 根据类名构造对象
    ClassName* CreateObject(const std::string class_name) const {
        typename ClassMap::const_iterator it = constructor_map_.find(class_name);
        if (it == constructor_map_.end()) {
            return nullptr;
        }
        return (*(it->second))();
    }
};

// 用来保存每个基类的 ClassRegister static 对象，用于全局调用
template <typename ClassName>
ClassRegister<ClassName>& GetRegister() {
    static ClassRegister<ClassName> class_register;
    return class_register;
}

// 每个类的构造函数，返回对应的base指针
template <typename BaseClassName, typename SubClassName>
BaseClassName* NewObject() {
    return new SubClassName();
}

// 为每个类反射提供一个 helper，构造时就完成反射函数对注册
template<typename BaseClassName>
class ClassRegisterHelper {
public:
    ClassRegisterHelper(
            const std::string sub_class_name,
            typename ClassRegister<BaseClassName>::Constructor constructor) {
        GetRegister<BaseClassName>().AddConstructor(sub_class_name, constructor);
    }
    ~ClassRegisterHelper(){}
};

// 提供反射类的注册宏，使用时仅提供基类类名和派生类类名
#define RegisterClass(base_class_name, sub_class_name) \
  static ClassRegisterHelper<base_class_name> \
      sub_class_name##_register_helper( \
          #sub_class_name, NewObject<base_class_name, sub_class_name>);

// 创建对象的宏
#define CreateObject(base_class_name, sub_class_name_as_string) \
  GetRegister<base_class_name>().CreateObject(sub_class_name_as_string)

#endif
