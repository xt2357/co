#include "co.h"
#include <cassert>

#include <iostream>

namespace co {

thread_local bool tls_initialized;
thread_local Routine tls_main_routine;
thread_local Routine * tls_cur_routine;

Routine *get_running_routine() {
    if (!tls_initialized) {
        tls_main_routine.SetState(Routine::State::Running);
        tls_cur_routine = &tls_main_routine;
        // std::cout << "tls cor: " << &tls_main_routine << std::endl;
        tls_initialized = true;
    }
    return tls_cur_routine;
}

bool in_main_routine() {
    return get_running_routine() == &tls_main_routine;
}

void set_running_routine(Routine *routine) {
    tls_cur_routine = routine;
}

void routine_entry() {
    auto r = get_running_routine();
    // TODO: what if when _logic throws?
    r->_logic();
    r->SetState(Routine::State::Dead);
    // make all sub_routines dead
    // std::cout << r << std::endl;
    // std::cout << "r->_sub_routines size: " << r->_sub_routines.size() << std::endl;
    // for (auto sub : r->_sub_routines) {
    //     std::cout << sub << std::endl;
    // }
    for (auto sub : r->_sub_routines) {
        Routine::RecursiveMarkDead(sub);
    }
    set_running_routine(r->_parent);
}

bool yield_to(Routine &other) {
    auto cur_routine = get_running_routine();
    if (other.GetState() == Routine::State::Created) {
        if (!other.PrepareContextForFirstResume(&routine_entry, *cur_routine)) {
            return false;
        }
    }
    if (cur_routine == &other) {
        return true;
    }
    return get_running_routine()->Jump(other);
}

}