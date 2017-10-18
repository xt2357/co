#include "co.h"

thread_local bool co::Env::tls_initialized;
thread_local co::Routine co::Env::tls_main_routine;
thread_local co::Routine * co::Env::tls_cur_routine;

namespace co {

// TODO: logic for first entering, catch force_rewinding_exception
static void first_resume() {
	Env::GetCurRoutine()->_logic();
}

bool yield_to(Routine &other) {
	auto cur_routine = Env::GetCurRoutine();
	if (other.GetState() == Routine::State::Created) {
		if (!other.PrepareContextForFirstResume(&first_resume, *cur_routine)) {
			return false;
		}
	}
	if (cur_routine == &other) {
		return true;
	}
	return Env::GetCurRoutine()->Jump(other);
}

}