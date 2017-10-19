#include <iostream>

#include "co.h"

using namespace std;



class Obj {
public:
    Obj(const char * s):_s(s) { cout << s << ": obj constructed" << endl;}
    ~Obj() { cout << _s << ": obj destructed" << endl;}
private:
    const char *_s;
};

co::Routine r1, r2;

void g_f1() {
    Obj obj {"obj1"};
    cout << "f1: 1" << endl;
    co::yield_to(r2);
    cout << "f1: 2" << endl;
    co::yield_to(r2);
}

void g_f2() {
    Obj obj {"obj2"};
    cout << "f2: 1" << endl;
    co::yield_to(r1);
    cout << "f2: 2" << endl;
    co::Routine sub {[](){cout<<"sub"<<endl;}};
    co::yield_to(sub);
}

struct Func {
    Func(int v):x(v){}
    void operator()() {
        cout << x << endl;
    }
    int x = 0;
};

int main() 
{
    auto f1 = []() {
        Obj obj {"obj1"};
        cout << "f1: 1" << endl;
        co::yield_to(r2);
        cout << "f1: 2" << endl;
        co::yield_to(r2);
    };
    auto f2 = []() {
        Obj obj {"obj2"};
        cout << "f2: 1" << endl;
        co::yield_to(r1);
        cout << "f2: 2" << endl;
    };
    function<void()> func1 = Func(1), func2 = Func(2);
    r1.SetBehavior(std::move(func1));
    r2.SetBehavior(std::move(func2));
    func1();
    co::yield_to(r1);
    cout << "returned to main" << endl;
    return 0;
}
