# co
A toy coroutine lib for C++

// yield to another coroutine
// fail if target coroutine is not in Suspend state 
// or there exists an exception handling(in a catch block) in current coroutine right now.
>bool co::yield_to(co::Routine &other);

## Improtant!!!
>Client code should never absorbing co::ForceUnwindingException otherwise it will cause incomplete stack unwinding
