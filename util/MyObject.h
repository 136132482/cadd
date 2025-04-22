//
// Created by Administrator on 2025/1/20 0020.
//
#include <string>
#include <vector>
#include <map>
#include <list>
//#include "AIGCJson.hpp"
#include "register/dynamic_reflection.cpp"

using namespace std;
//using namespace aigc;

class MyObject:public::dynamic::IReflectable {
public:
    int id{};
    std::string info;
    std::map<std::string, int> attributes;
    std::map<std::string, std::vector<int>>maplist;
    std::map<std::string, std::map<std::string, std::vector<std::list<std::string>>>>otherObj;
    MyObject()=default;
    MyObject(int id, const char *name, const std::map<std::string, int>& attributes)
            : id(id), info(name), attributes(attributes) {};
    MyObject(int id, const char *name, const std::map<std::string, int>& attributes, std::map<std::string, std::vector<int>>&maplist,
             std::map<std::string, std::map<std::string, std::vector<std::list<std::string>>>>&otherObj)
    : id(id), info(name), attributes(attributes),maplist(maplist),otherObj(otherObj) {};

//    AIGC_JSON_HELPER(id, name,attributes,maplist,otherObj) //成员注册
////    AIGC_JSON_HELPER_DEFAULT("id=123")
//    AIGC_JSON_HELPER_BASE((MyObject*)this) //基类注册

    REFLECTABLE_PROPERTIES(MyObject,REFLEC_PROPERTY(info),REFLEC_PROPERTY(attributes),
                           REFLEC_PROPERTY(maplist),REFLEC_PROPERTY(otherObj),REFLEC_PROPERTY(id)
    );
    REFLECTABLE_MENBER_FUNCS(MyObject);
    DECL_DYNAMIC_REFLECTABLE(MyObject)//动态反射的支持，如果不需要动态反射，可以去掉这行代码
};



class DataParam{
public:
    std::string  project;
    MyObject myobject;
    DataParam(std::string project,MyObject &object):project(std::move(project)), myobject(object){};

//    AIGC_JSON_HELPER(project, myobject) //成员注册
//    AIGC_JSON_HELPER_DEFAULT("test=123")
};




