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


void gaoshi() {
    Obj obj {"obj3"};
    cout << "main:" << int(co::get_main_routine().GetState()) << endl;
    cout << "r1:" << int(r1.GetState()) << endl;
    cout << "r2:" << int(r2.GetState()) << endl;
    yield_to(co::get_main_routine());
}


void throw_exception() {
    throw std::exception();
}

void g_f1() {
    Obj obj {"obj1"};
    cout << "main:" << int(co::get_main_routine().GetState()) << endl;
    cout << "r1:" << int(r1.GetState()) << endl;
    cout << "r2:" << int(r2.GetState()) << endl;
    cout << "f1: 1" << endl;
    try {
        co::yield_to(r2);
    }
    catch(...) {
        cout << "catched!" << endl;
        // throw_exception();
        // assert(yield_to(co::get_main_routine()));
    }
    assert(yield_to(co::get_main_routine()));
    cout << "reenter f1 main:" << int(co::get_main_routine().GetState()) << endl;
    cout << "r1:" << int(r1.GetState()) << endl;
    cout << "r2:" << int(r2.GetState()) << endl;
    gaoshi();
    cout << "f1: 2" << endl;
    co::yield_to(r2);
}

void g_f2() {
    Obj obj {"obj2"};
    cout << "main:" << int(co::get_main_routine().GetState()) << endl;
    cout << "r1:" << int(r1.GetState()) << endl;
    cout << "r2:" << int(r2.GetState()) << endl;
    cout << "f2: 1" << endl;
    throw 10;
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
    // auto f1 = []() {
    //     Obj obj {"obj1"};
    //     cout << "f1: 1" << endl;
    //     co::yield_to(r2);
    //     cout << "f1: 2" << endl;
    //     co::yield_to(r2);
    // };
    // auto f2 = []() {
    //     Obj obj {"obj2"};
    //     cout << "f2: 1" << endl;
    //     co::yield_to(r1);
    //     cout << "f2: 2" << endl;
    // };
    function<void()> func1 = Func(1), func2 = Func(2);
    r1.SetBehavior(g_f1);
    r2.SetBehavior(g_f2);
    cout << "main:" << int(co::get_main_routine().GetState()) << endl;
    cout << "r1:" << int(r1.GetState()) << endl;
    cout << "r2:" << int(r2.GetState()) << endl;
    // try {
    //     throw 20;
    // }
    // catch(...) {
    //     cout << (bool)(current_exception()) << endl;
    // }
    // cout << (bool)(current_exception()) << endl;
    try {
        co::yield_to(r1);
        // throw;
    }
    catch(...) {
        cout << "catched in main" << endl;
    }
    cout << "returned to main" << endl;
    cout << "main:" << int(co::get_main_routine().GetState()) << endl;
    cout << "r1:" << int(r1.GetState()) << endl;
    cout << "r2:" << int(r2.GetState()) << endl;
    return 0;
}
