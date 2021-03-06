#include <cstdlib>

#include <iostream>
#include <vector>
#include <list>
#include <set>
#include <unordered_set>
#include <chrono>

#include "../src/co.h"

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
    throw 20;
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

void test() {
    cout << (std::current_exception() == nullptr) << endl;
    co::Routine r;
    cout << co::yield_to(r) << endl;
    cout << "test finish" << endl;
}

struct HandlerException : public std::exception {
public:
    HandlerException(std::string message) noexcept :
        _message(std::move(message)) {}
    const char * what() const noexcept override { return _message.c_str(); }

protected:
    std::string _message;
};

void yield_back(){
    // cout << "yield_to new routine" << endl;
    co::yield_to(co::get_main_routine());
}

void stack_overflow(int a) {
    // cout << "stack_overflow" << endl;
    stack_overflow(a);
}

void print(int a) {
    std::cout << a << std::endl;
}

void show_exception(exception_ptr ptr) {
    if (ptr == nullptr) {
        cout << "null exception" << endl;
        return;
    }
    try {
        rethrow_exception(ptr);
    }
    catch (exception &e) {
        cout << e.what() << endl;
    }
}

void throw_something() {
    try {
        throw HandlerException("hahaha");
    }
    catch (...) {
        return;
    }
}

int main() 
{
    try {
        throw_something();
    }
    catch (const std::exception &e) {
        cout << e.what() << endl;
    }
    show_exception(current_exception());
    try {
        throw HandlerException("main");
    }
    catch (const exception& e) {
        show_exception(current_exception());
        co::Routine r {[](){
            try {
                throw HandlerException("routine");
            }
            catch (...) {
                show_exception(current_exception());

                co::yield_to(co::get_main_routine());
            }
        }};
        co::yield_to(r);
        show_exception(current_exception());
    }
    show_exception(current_exception());
    cout << uncaught_exception() << endl;


    co::Routine hook {[&](){
        co::get_running_routine().enable_syscall_hook(true);
        int x = 1;
        malloc(10);
        cin >> x;
        co::yield_to(co::get_main_routine());
    }};
    co::yield_to(hook);
    
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
    co::get_main_routine();
    vector<co::Routine> routines;
    // construct: 8us
    auto t1 = chrono::high_resolution_clock::now();
    for (int i = 0; i < 10; ++i) {
        // cout << "new routine created" << endl;
        routines.emplace_back(yield_back);
        // routines.back().GetState();
        co::yield_to(routines.back());
    }
    auto t2 = chrono::high_resolution_clock::now();
    cout << chrono::duration_cast<chrono::duration<double>>((t2 - t1)).count() << endl;
    int i;
    cin >> i; 
    co::Routine testr {[](){
        while (1) {
            co::yield_to(co::get_main_routine());
        }
    }};
    // switch: 280ns
    t1 = chrono::high_resolution_clock::now();
    for (int i = 0; i < 10000; ++i) {
        (co::yield_to(testr));
    }
    t2 = chrono::high_resolution_clock::now();
    cout << "swtich performance: " << chrono::duration_cast<chrono::duration<double>>((t2 - t1)).count() << endl;

    function<void()> func1 = Func(1), func2 = Func(2);
    co::Routine cr {func1};
    co::Routine cr2 {func1};
    cout << "equal:" << (cr == cr) << endl;
    cout << "no equal: " << (cr != cr2) << endl;
    cr = std::move(cr2);
    set<co::Routine> set_r;
    unordered_set<co::Routine> unset_r;
    r1.SetBehavior(g_f1);
    r2.SetBehavior(g_f2);
    cout << "main:" << int(co::get_main_routine().GetState()) << endl;
    cout << "r1:" << int(r1.GetState()) << endl;
    cout << "r2:" << int(r2.GetState()) << endl;
    try {
        throw 20;
    }
    catch(...) {
        cout << (bool)(current_exception()) << endl;
        cout << "test ex" << endl;
        try {
            throw current_exception();
        }
        catch (...) {
            
        }
        cout << (bool)(current_exception()) << endl;

    }
    cout << (bool)(current_exception()) << endl;
    try {
        co::yield_to(r1);
        // co::start_routine(std::bind(g_f1));
        // throw;
    }
    catch(...) {
        cout << "catched in main" << endl;
        cout << (std::current_exception() == nullptr) << endl;
        try {
            test();
        }
        catch (...)
        {}
    }
    cout << (std::current_exception() == nullptr) << endl;
    cout << "returned to main" << endl;
    cout << "main:" << int(co::get_main_routine().GetState()) << endl;
    cout << "r1:" << int(r1.GetState()) << endl;
    cout << "r2:" << int(r2.GetState()) << endl;
    // stack_overflow(2);
    co::Routine so {std::bind(print, 1)};
    co::yield_to(so);
    return 0;
}
