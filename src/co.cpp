#include "co.h"
#include <cassert>

#include <memory>
#include <iostream>

namespace co {

thread_local bool tls_initialized;
thread_local Routine tls_main_routine;
thread_local Routine * tls_cur_routine;

Routine &get_running_routine() {
    if (!tls_initialized) {
        tls_main_routine.SetState(Routine::State::Running);
        tls_cur_routine = &tls_main_routine;
        // std::cout << "tls cor: " << &tls_main_routine << std::endl;
        tls_initialized = true;
    }
    return *tls_cur_routine;
}

bool in_main_routine() {
    return &get_running_routine() == &tls_main_routine;
}

void set_running_routine(Routine &routine) {
    tls_cur_routine = &routine;
}

Routine & get_main_routine() {
    if (!tls_initialized) {
        get_running_routine();
    }
    return tls_main_routine;
}

void routine_entry() {
    auto& r = get_running_routine();
    std::unique_ptr<void, std::function<void(void*)>> always_do(nullptr, [&r](void*){
		r.SetState(Routine::State::Dead);
        for (auto sub : r._sub_routines) {
            Routine::RecursiveUnwindAndMarkDead(*sub);
            sub->_parent = nullptr;
        }
        r._sub_routines.clear();
        set_running_routine(*r._parent);
        r._parent->SetState(Routine::State::Running);
	});
    // what if when _logic throws? do some context switching and rethrow it
    try {
        r._logic();
    }
    // unwind this coroutine
    catch (ForceUnwindingException &e) {
        e._routine_to_unwind._context.SwapContext(get_running_routine()._context);
    }
    catch (...) {
        throw;
    }
}

bool yield_to(Routine &other) {
    auto& cur_routine = get_running_routine();
    if (other.GetState() == Routine::State::Created) {
        if (!other.PrepareContextForFirstResume(&routine_entry, cur_routine)) {
            return false;
        }
    }
    if (&cur_routine == &other) {
        return true;
    }
    return get_running_routine().Jump(other);
}

}