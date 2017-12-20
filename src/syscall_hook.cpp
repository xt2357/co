#include "co.h"
#include <stdio.h>
#include <stdint.h>
#include <dlfcn.h>                               /* header required for dlsym() */
 
/* lcheck() is for memory leak check; its code is not shown
 here */
extern "C" {

void lcheck(void);
void* malloc(size_t size)
{
    static void* (*my_malloc)(size_t) = NULL;
    if (!my_malloc)
    my_malloc = (void* (*)(size_t))dlsym(RTLD_NEXT, "malloc");  /* returns the object reference for malloc */
    
    if (!co::get_running_routine()->enable_syscall_hook()) {
        return my_malloc(size);
    }
    
    printf("inside shared object...\n");
    void *p = my_malloc(size);               /* call malloc() using function pointer my_malloc */ 
    printf("malloc(%d) = %p\n", (int)size, p);
    lcheck();                                /* calling do_your_stuff function */
    printf("returning from shared object...\n");
    return p;
}
void lcheck(void)
{
    printf("displaying memory leaks...\n");
    /* do required stuff here */
}

}