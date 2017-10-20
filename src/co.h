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

#include <iostream>

namespace co {

struct Context {
public:

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
            delete _stack;
        }
    }

private:

    ucontext_t _ucontext;
    char * _stack = nullptr;
    size_t kStackSize = (1 << 23) - 8;// cache miss avoidance
    
};

class Routine;
void set_running_routine(Routine &routine);
Routine &get_main_routine();

class Routine {

friend void routine_entry();
friend Routine &get_running_routine();
friend bool yield_to(Routine &);

public:

    Routine(const Routine &) = delete;
    Routine& operator=(const Routine &) = delete;

    typedef std::function<void(void)> Delegate;

    enum class State {Created, Running, Suspend, Dead};

    Routine():_logic([](){}), _state(State::Created) {}
    Routine(Delegate&& logic):_logic(std::move(logic)), _state(State::Created) {}
    ~Routine() {
        // std::cout << "destructor: " << this << std::endl;
        _state = State::Dead;
        for (auto r : _sub_routines) {
            RecursiveUnwindAndMarkDead(r);
            r->_parent = nullptr;
        }
        if (_parent) {
            assert(_parent->RemoveSubRoutine(*this));
        }
    }

    bool operator==(const Routine &other) { return this == &other; }
    bool operator!=(const Routine &other) { return this != &other; }

    //rvalue reference? yes it is designed the only way to pass a functor in 
    bool SetBehavior(Delegate&& logic) {
        if (State::Created != _state) {
            return false;
        }
        _logic = std::move(logic);
        return true;
    }

    State GetState() { return _state; }

private:

    static void RecursiveUnwindAndMarkDead(Routine *r) {
    	if (r->GetState() == Routine::State::Dead) {
    		return;
    	}
        for (auto sub : r->_sub_routines) {
            RecursiveUnwindAndMarkDead(sub);
        }
        if (r->GetState() != Routine::State::Created && *r != get_main_routine()) {
        	// TODO: unwind the coroutine stack of r whose state is suspend or running
        }
        r->SetState(State::Dead);
    }

    void SetState(State state) { _state = state; }

    bool AttachSubRoutine(Routine &sub_routine) {
        return _sub_routines.insert(&sub_routine).second;
    }

    bool RemoveSubRoutine(Routine &sub_routine) {
        return _sub_routines.erase(&sub_routine);
    }

    bool PrepareContextForFirstResume(void (*start_point)(), Routine &parent) {
        if (State::Created != _state || !parent.AttachSubRoutine(*this) || !_context.MakeContext(start_point, parent._context)) {
            return false;
        }
        _parent = &parent;
        _state = State::Suspend;
        return true;
    }

    bool Jump(Routine &other) {
        if (other.GetState() != State::Suspend) {
            return false;
        }
        _state = State::Suspend;
        set_running_routine(other);
        auto success = _context.SwapContext(other._context);
        if (!success) {
            set_running_routine(*this);
        }
        _state = State::Running;
        return success;
    }

private:
    Delegate _logic;
    State _state;
    Context _context;
    std::unordered_set<Routine*> _sub_routines;
    Routine *_parent = nullptr;
};


bool yield_to(Routine &other);
}