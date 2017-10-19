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

int main() 
{
    co::Routine r1, r2;
    auto f1 = [&r2]() {
        Obj obj {"obj1"};
        cout << "f1: 1" << endl;
        co::yield_to(r2);
        cout << "f1: 2" << endl;
        co::yield_to(r2);
    };
    auto f2 = [&r1]() {
        Obj obj {"obj2"};
        cout << "f2: 1" << endl;
        co::yield_to(r1);
        cout << "f2: 2" << endl;
    };
    r1.SetLogic(f1);
    r2.SetLogic(f2);
    co::yield_to(r1);
    return 0;
}
