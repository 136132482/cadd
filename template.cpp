//
// Created by Administrator on 2025/1/14 0014.
//
#include <iostream>
#include "duotai.cpp"
using namespace std;

template<typename T>
class Base {
public:
    virtual void showType() const = 0;
};



template<class T>
class paisheng:public Base<T>{

public:
    void showType()const override{
        std::cout << "Derived with type: " << typeid(T).name() << std::endl;
    }


};

template<class T1>
void func(T1 &obj){
    obj->sound();
    delete obj;
}







int main(){
    //显性 无法对其进行多态处理
   Base<Animal> *base=new paisheng<Animal>();

    Animal *animal= new Cat();
    func(animal);
    base->showType();
}

