//
// Created by Administrator on 2025/1/14 0014.
//
#include <iostream>
using namespace std;



class  Animal{
public:
    //抽象
     virtual  void sound() const=0;
    //析构函数
    virtual  ~Animal(){
//        cout<<"销毁"<<endl;
    };
};

class  Dog: public Animal{
public:
    void sound()const  override{
        cout << "狗叫" << endl;
    }

    ~Dog() override {
        cout << "销毁狗" << endl;
    }
};

class Cat : public Animal {
public:
    // 重写 sound 方法
    void sound()const override  {
        cout << "猫叫" << endl;
    }

    ~Cat() override{
        cout << "销毁猫" << endl;
    }
};


//int main(){
//    Animal *animal;
//    animal = new Dog();
//    animal->sound();
//    delete animal;
//
//
//    animal=new Cat();
//    animal->sound();
//    delete animal;
//
//    return 0;
//}