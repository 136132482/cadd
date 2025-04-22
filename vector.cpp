//
// Created by Administrator on 2025/1/14 0014.
//
#include <vector>
#include <iostream>
#include <nlohmann/json.hpp>

class vector_bean{
public:

    struct  MyStruct{
        int id;
        std::string name;
    };


    void  get_vector(){
        std::vector<MyStruct> myVector;
        myVector.push_back({1,"章三"});
        myVector.push_back({2,"李四"});
        myVector.push_back({3,"王五"});

        for(int i=0;i<myVector.size();i++){
            std::cout<<myVector[i].id<<"："<<myVector[i].name<<std::endl;
        }

        for(auto &item:myVector){
            std::cout << "ID: " << item.id << ", Name: " << item.name << std::endl;
        }
    }





};
int main(){
    vector_bean vectorBean;
    vectorBean.get_vector();
}