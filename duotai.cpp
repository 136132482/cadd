//
// Created by Administrator on 2025/1/14 0014.
//
#include <iostream>
using namespace std;



class  Animal{
public:
    //����
     virtual  void sound() const=0;
    //��������
    virtual  ~Animal(){
//        cout<<"����"<<endl;
    };
};

class  Dog: public Animal{
public:
    void sound()const  override{
        cout << "����" << endl;
    }

    ~Dog() override {
        cout << "���ٹ�" << endl;
    }
};

class Cat : public Animal {
public:
    // ��д sound ����
    void sound()const override  {
        cout << "è��" << endl;
    }

    ~Cat() override{
        cout << "����è" << endl;
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