#include "co.h"

thread_local bool co::Env::tls_initialized;
thread_local co::Routine co::Env::tls_main_routine;
thread_local co::Routine * co::Env::tls_cur_routine;