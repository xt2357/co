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

    Context():_stack(new char[kStackSize]) {}

    // make context which start at start_point and start_point return to start_point_return_to
    bool MakeContext(void (*start_point)(), Context &start_point_return_to) noexcept {
        assert(_stack);
        if (!_stack || -1 == getcontext(&_ucontext)) {
            return false;
        }
        _ucontext.uc_stack.ss_sp = _stack;
        _ucontext.uc_stack.ss_size = kStackSize;
        _ucontext.uc_link = &start_point_return_to._ucontext;
        makecontext(&_ucontext, start_point, 0);
        return true;
    }

    bool SwapContext(const Context &other) noexcept {
        if (-1 == swapcontext(&_ucontext, &other._ucontext)) {
            return false;
        }
        return true;
    }

    ~Context() noexcept {
        if (_stack) {
            delete [] _stack;
        }
    }

private:

    ucontext_t _ucontext;
    char * _stack;
    static const size_t kStackSize = (1 << 23) - 8;// cache miss avoidance
    
};

class _Routine;
void set_running_routine(_Routine &_Routine) noexcept;
_Routine &get_running_routine() noexcept;
_Routine &get_main_routine() noexcept;


// client code should never absorbing this exception otherwise it will cause incomplete stack unwinding
class ForceUnwindingException
{
public:
    ForceUnwindingException(_Routine &routine_to_unwind):_routine_to_unwind(routine_to_unwind) {}
    _Routine &_routine_to_unwind;
};

using Delegate = std::function<void(void)>;
enum class State {Created, Prepared, Running, Suspend, Dead};

class _Routine {

friend void routine_entry() noexcept;
friend _Routine &get_running_routine() noexcept;
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

    _Routine():_logic([](){}), _state(State::Created) { /* std::cout << "construct:" << this << "empty logic" << std::endl; */}
    _Routine(Delegate&& logic):_logic(std::move(logic)), _state(State::Created) { /*std::cout << "construct:" << this << std::endl;*/}
    _Routine(const Delegate &logic):_logic(logic), _state(State::Created) {}
    ~_Routine() noexcept {
        //std::cout << "destructor: " << this << std::endl;
        // std::cout << "detor:" << int(_state) << " " << this << std::endl;
        if (_parent) {
            // std::cout << "move pointer from parent" << std::endl;
            assert(_parent->RemoveSubRoutine(*this));
        }
        // std::cout << "sub routines cleared" << std::endl;
        RecursiveUnwindAndMarkDead(*this);
    }

    bool operator==(const _Routine &other) { return this == &other; }
    bool operator!=(const _Routine &other) { return this != &other; }

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

    State GetState() noexcept { return _state; }

    void enable_syscall_hook(bool enable) { _enable_syscall_hook = enable; }
    bool enable_syscall_hook() { return _enable_syscall_hook; }

private:

    static void RecursiveUnwindAndMarkDead(_Routine &r) {
        // std::cout << "RecursiveUnwindAndMarkDead" << std::endl;
        if (r.GetState() == State::Dead) {
            return;
        }
        for (auto sub : r._sub_routines) {
            RecursiveUnwindAndMarkDead(*sub);
            sub->_parent = nullptr;
        }
        // r._sub_routines.clear();
        // here means the thread is destructing
        if (r.GetState() == State::Running && r == get_main_routine()) {
            r.SetState(State::Dead);
            return;
        }
        // we can not handle a running coroutine which is not main_routine
        assert(r.GetState() != State::Running);
        if (r.GetState() == State::Suspend && r != get_main_routine()) {
            // std::cout << "unwind" << std::endl;
            // unwind the coroutine stack of r whose state is suspend
            r._force_unwind = true;
            // return here after unwinding
            assert(get_running_routine()._context.SwapContext(r._context));
        }
        r.SetState(State::Dead);
    }

    void SetState(State state) noexcept { _state = state; }

    bool AttachSubRoutine(_Routine &sub_routine) {
        return _sub_routines.insert(&sub_routine).second;
    }

    bool RemoveSubRoutine(_Routine &sub_routine) {
        return _sub_routines.erase(&sub_routine);
    }

    bool PrepareContextForFirstResume(void (*start_point)(), _Routine &parent) noexcept {
        if (State::Created != _state || !parent.AttachSubRoutine(*this) || !_context.MakeContext(start_point, parent._context)) {
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
        auto success = _context.SwapContext(other._context);
        if (!success) {
            // std::cout << "not success Jump" << std::endl;
            set_running_routine(*this);
            other.SetState(state_backup);
        }
        // unwind to break point
        if (_force_unwind) {
            // std::cout << "force unwind" << std::endl;
            throw ForceUnwindingException(*this);
        }
        // throw to break point
        if (_rethrow_exception) {
            // std::cout << "rethrow!" << std::endl;
            _rethrow_exception = false;
            std::rethrow_exception(_exception);
        }
        // return or normal yield to break point
        return success;
    }

private:
    Delegate _logic;
    State _state;
    Context _context;
    std::unordered_set<_Routine*> _sub_routines;
    _Routine *_parent = nullptr;
    bool _force_unwind = false, _rethrow_exception = false, _enable_syscall_hook = false;
    std::exception_ptr _exception;
};


class Routine
{
public:
    Routine(): _ptr(new _Routine{}) {}
    Routine(Delegate &&logic): _ptr(new _Routine {std::move(logic)}) { /*std::cout << "move ctor" << std::endl;*/}
    Routine(const Delegate &logic): _ptr(new _Routine {logic}) {}

    _Routine& operator *() const { return *_ptr; }

    bool SetBehavior(Delegate&& logic) { return _ptr->SetBehavior(std::move(logic)); }

    bool SetBehavior(const Delegate& logic) { return _ptr->SetBehavior(logic); }

    State GetState() noexcept { return _ptr->GetState(); }

    void enable_syscall_hook(bool enable) { _ptr->enable_syscall_hook(enable); }
    bool enable_syscall_hook() { return _ptr->enable_syscall_hook(); }

    bool operator==(const Routine &other) const { return _ptr == other._ptr; }
    bool operator!=(const Routine &other) const { return _ptr != other._ptr; }
    bool operator <(const Routine &other) const { return _ptr < other._ptr; }
    bool operator<=(const Routine &other) const { return _ptr <= other._ptr; }
    bool operator >(const Routine &other) const { return _ptr > other._ptr; }
    bool operator>=(const Routine &other) const { return _ptr >= other._ptr; }

    size_t hash() const { return std::hash<std::unique_ptr<co::_Routine>>()(_ptr); }
    
private:
    std::unique_ptr<_Routine> _ptr;
};


bool yield_to(_Routine &other);
bool yield_to(Routine &other);

}

namespace std
{
    template <>
    struct hash<co::Routine>
    {
        size_t operator()(const co::Routine& r) const
        {
            return r.hash();
        }
    };
}