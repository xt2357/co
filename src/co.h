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

#include <utility>

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


// TODO: wrap biz logic into a functor

class Routine {

friend void first_resume();

public:

	typedef void (*Delegate)();

	enum class State {Created, Running, Suspend, Dead};

	Routine():_state(State::Created) {}
	Routine(Delegate logic):_logic(std::move(logic)), _state(State::Created) {}
	~Routine() {
		_state = State::Dead;
		// TODO: delete all sub routines: stack rewinding
	}

	bool PrepareContextForFirstResume(void (*start_point)(), Routine &parent) {
		if (State::Created != _state) {
			return false;
		}
		if (!_context.MakeContext(start_point, parent._context)) {
			return false;
		}
		_state = State::Suspend;
		return true;
	}

	bool Jump(Routine &other) {
		if (other.GetState() != State::Suspend) {
			return false;
		}
		_state = State::Suspend;
		auto success = _context.SwapContext(other._context);
		_state = State::Running;
		return success;
	}

	State GetState() { return _state; }
	void SetState(State state) { _state = state; }

private:

	

private:
	Delegate _logic;
	State _state;
	Context _context;
};


class Env {
public:
	static Routine *GetCurRoutine() {
		if (!tls_initialized) {
			tls_main_routine.SetState(Routine::State::Running);
			tls_cur_routine = &tls_main_routine;
			tls_initialized = true;
		}
		return tls_cur_routine;
	}
	static bool InMainRoutine() {
		return GetCurRoutine() == &tls_main_routine;
	}
private:
	static thread_local bool tls_initialized;
	static thread_local Routine tls_main_routine;
	static thread_local Routine *tls_cur_routine;
};

bool yield_to(Routine &other);

}