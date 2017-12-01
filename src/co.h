/*
MIT License

Copyright (c) 2017 Tao Xiong(xt2357@gmail.com)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#pragma once
#include <ucontext.h>
#include <stdlib.h>

#include <cassert>

#include <utility>
#include <unordered_set>
#include <functional>
#include <memory>
#include <exception>
#include <iostream>

namespace co {


struct Context {
public:

    Context() = default;

    // Context(Context &&other) {
    //     _stack = other._stack;
    //     _ucontext = other._ucontext;
    //     other._stack = nullptr;
    // }

    // Context& operator=(Context &&other) {
    //     if (this != &other) {
    //         free(_stack);
    //         _stack = other._stack;
    //         _ucontext = other._ucontext;
    //         other._stack = nullptr;
    //     }
    //     return *this;
    // }

    // make context which start at start_point and start_point return to start_point_return_to
    bool MakeContext(void (*start_point)(), Context &start_point_return_to) {
        if (_stack || -1 == getcontext(&_ucontext)) {
            return false;
        }
        _stack = static_cast<char*>(malloc(kStackSize));
        if (!_stack) {
            return false;
        }
        _ucontext.uc_stack.ss_sp = _stack;
        _ucontext.uc_stack.ss_size = kStackSize;
        _ucontext.uc_link = &start_point_return_to._ucontext;
        makecontext(&_ucontext, start_point, 0);
        return true;
    }

    bool SwapContext(const Context &other) {
        if (-1 == swapcontext(&_ucontext, &other._ucontext)) {
            return false;
        }
        return true;
    }

    ~Context() {
        if (_stack) {
            free(_stack);
        }
    }

private:

    ucontext_t _ucontext;
    char * _stack = nullptr;
    size_t kStackSize = (1 << 23) - 8;// cache miss avoidance
    
};

class _Routine;
void set_running_routine(_Routine &_Routine);
_Routine *get_running_routine();
_Routine *get_main_routine();


// client code should never absorbing this exception otherwise it will cause incomplete stack unwinding
class ForceUnwindingException
{
public:
    ForceUnwindingException(_Routine &routine_to_unwind):_routine_to_unwind(routine_to_unwind) {}
    _Routine &_routine_to_unwind;
};

typedef std::function<void(void)> Delegate;

class _Routine {

friend void routine_entry();
friend _Routine *get_running_routine();
friend bool yield_to(_Routine &);

public:

    _Routine(const _Routine &) = delete;
    _Routine& operator=(const _Routine &) = delete;

    /*
    _Routine is non-movable
    cauz stack of its'context will contain `this` pointers of old _Routine objects 
    which will be invalid after moving them
    (moving will cause change of the address of _Routine objects)
    */

    // _Routine(_Routine &&other) {
    //     std::cout << "move ctor: move to " << this << std::endl;
    //     _logic = std::move(other._logic);
    //     _state = std::move(other._state);
    //     _context = std::move(other._context);
    //     _sub_routines = std::move(other._sub_routines);
    //     _parent = std::move(other._parent);
    //     _force_unwind = std::move(other._force_unwind);
    //     _rethrow_exception = std::move(other._rethrow_exception);
    //     _exception = std::move(other._exception);
    //     other._logic = [](){};
    //     other._state = State::Dead;
    //     other._parent = nullptr;
    //     other._force_unwind = false;
    //     other._rethrow_exception = false;
    //     if (_parent) {
    //         assert(_parent->RemoveSubRoutine(other));
    //         assert(_parent->AttachSubRoutine(*this));
    //     }
    //     for (auto &son : _sub_routines) {
    //         son->_parent = this;
    //     }
    // }
    // _Routine& operator=(_Routine &&other) {
    //     std::cout << "move =" << std::endl;
    //     if (this != &other) {
    //         if (_parent) {
    //             assert(_parent->RemoveSubRoutine(*this));
    //         }
    //         for (auto r : _sub_routines) {
    //             RecursiveUnwindAndMarkDead(*r);
    //             r->_parent = nullptr;
    //         }
    //         _sub_routines.clear();
    //         RecursiveUnwindAndMarkDead(*this);
    //         _logic = std::move(other._logic);
    //         _state = std::move(other._state);
    //         _context = std::move(other._context);
    //         _sub_routines = std::move(other._sub_routines);
    //         _parent = std::move(other._parent);
    //         _force_unwind = std::move(other._force_unwind);
    //         _rethrow_exception = std::move(other._rethrow_exception);
    //         _exception = std::move(other._exception);
    //         if (_parent) {
    //             assert(_parent->RemoveSubRoutine(other));
    //             assert(_parent->AttachSubRoutine(*this));
    //         }
    //         for (auto &son : _sub_routines) {
    //             son->_parent = this;
    //         }
    //     }
    //     return *this;
    // }

    

    enum class State {Created, Prepared, Running, Suspend, Dead};

    _Routine():_logic([](){}), _state(State::Created), _context(new Context()) {  std::cout << "construct:" << this << "empty logic" << std::endl; }
    _Routine(Delegate&& logic):_logic(std::move(logic)), _state(State::Created), _context(new Context()) { std::cout << "construct:" << this << std::endl;}
    _Routine(const Delegate &logic):_logic(logic), _state(State::Created), _context(new Context()) {}
    ~_Routine() {
        //std::cout << "destructor: " << this << std::endl;
        std::cout << "detor:" << int(_state) << " " << this << std::endl;
        if (_parent) {
            std::cout << "move pointer from parent" << std::endl;
            assert(_parent->RemoveSubRoutine(*this));
        }
        for (auto r : _sub_routines) {
            RecursiveUnwindAndMarkDead(*r);
            r->_parent = nullptr;
        }
        _sub_routines.clear();
        std::cout << "sub routines cleared" << std::endl;
        RecursiveUnwindAndMarkDead(*this);
    }

    bool operator==(const _Routine &other) { return this == &other; }
    bool operator!=(const _Routine &other) { return this != &other; }

    //rvalue reference? yes it is designed the only way to pass a functor in 
    bool SetBehavior(Delegate&& logic) {
        if (State::Created != _state) {
            return false;
        }
        _logic = std::move(logic);
        return true;
    }

    bool SetBehavior(const Delegate& logic) {
        if (State::Created != _state) {
            return false;
        }
        _logic = logic;
        return true;
    }

    State GetState() { return _state; }

private:

    static void RecursiveUnwindAndMarkDead(_Routine &r) {
        std::cout << "RecursiveUnwindAndMarkDead" << std::endl;
        if (r.GetState() == _Routine::State::Dead) {
            return;
        }
        for (auto sub : r._sub_routines) {
            RecursiveUnwindAndMarkDead(*sub);
        }
        // here means the thread is destructing
        if (r.GetState() == State::Running && r == *get_main_routine()) {
            r.SetState(State::Dead);
            return;
        }
        // we can not handle a running coroutine which is not main_routine
        assert(r.GetState() != State::Running);
        if (r.GetState() == _Routine::State::Suspend && r != *get_main_routine()) {
            std::cout << "unwind" << std::endl;
            // unwind the coroutine stack of r whose state is suspend
            r._force_unwind = true;
            // return here after unwinding
            assert(get_running_routine()->_context->SwapContext(*r._context));
        }
        r.SetState(State::Dead);
    }

    void SetState(State state) { _state = state; }

    bool AttachSubRoutine(_Routine &sub_routine) {
        return _sub_routines.insert(&sub_routine).second;
    }

    bool RemoveSubRoutine(_Routine &sub_routine) {
        return _sub_routines.erase(&sub_routine);
    }

    bool PrepareContextForFirstResume(void (*start_point)(), _Routine &parent) {
        if (State::Created != _state || !parent.AttachSubRoutine(*this) || !_context->MakeContext(start_point, *parent._context)) {
            return false;
        }
        _parent = &parent;
        _state = State::Prepared;
        return true;
    }

    bool Jump(_Routine &other) {
        if (std::current_exception() || (other.GetState() != State::Suspend && other.GetState() != State::Prepared)) {
            return false;
        }
        _state = State::Suspend;
        auto state_backup = other.GetState();
        set_running_routine(other);
        other.SetState(State::Running);
        auto success = _context->SwapContext(*other._context);
        if (!success) {
            std::cout << "not success Jump" << std::endl;
            set_running_routine(*this);
            other.SetState(state_backup);
        }
        // unwind to break point
        if (_force_unwind) {
            std::cout << "force unwind" << std::endl;
            throw ForceUnwindingException(*this);
        }
        // throw to break point
        if (_rethrow_exception) {
            std::cout << "rethrow!" << std::endl;
            _rethrow_exception = false;
            std::rethrow_exception(_exception);
        }
        // return or normal yield to break point
        return success;
    }

private:
    Delegate _logic;
    State _state;
    std::unique_ptr<Context> _context;
    std::unordered_set<_Routine*> _sub_routines;
    _Routine *_parent = nullptr;
    bool _force_unwind = false, _rethrow_exception = false;
    std::exception_ptr _exception;
};

// using Routine = std::unique_ptr<_Routine>;

class Routine
{
public:
    Routine(): _ptr(new _Routine{}) {}
    Routine(Delegate &&logic): _ptr(new _Routine {std::move(logic)}) {}
    Routine(const Delegate &logic): _ptr(new _Routine {logic}) {}

    _Routine& operator *() const { return *_ptr; }
    _Routine* operator ->() const {return _ptr.get(); }

    bool operator==(const Routine &other) { return **this == *other; }
    bool operator!=(const Routine &other) { return **this != *other; }
    
private:
    std::unique_ptr<_Routine> _ptr;
};

bool yield_to(_Routine &other);
bool yield_to(Routine &other);
bool yield_to(_Routine *other);


// struct StartRoutineFailException: public std::exception {
//     virtual const char * what() { return "start_routine fail"; }
// };

// template <typename Callable>
// auto start_routine(Callable &&func) -> decltype(func()) {
//     bool is_void = std::is_same<decltype(func()), void>::value;
//     decltype(func()) *result = nullptr;
//     if (!is_void) {
//         result = new decltype(func());
//     }
//     _Routine r {[&]() {
//         if (is_void) {
//             func();
//         }
//         else {
//             *result = func();
//         }
//     }};
//     if (!yield_to(r)) {
//         throw StartRoutineFailException();
//     }
//     if (!is_void) {
//         decltype(func) res = *result;
//         delete result;
//         return res;
//     }
//     else {
//         return;
//     }
// }


}