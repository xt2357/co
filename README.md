# co
A toy coroutine lib for C++

/*
 * yield to another coroutine  
 * fail if target coroutine is not in Suspend state   
 * or there exists an exception being handled in current coroutine's context
 * (which means the yield_to statement is in a catch block)
*/
>bool co::yield_to(co::Routine &other);

## Improtant!!!
>Client code should never absorbing co::ForceUnwindingException otherwise it will cause incomplete stack unwinding
