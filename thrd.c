#include "thrd.h"
#include <stdlib.h>

struct thrd_t *thrd_Create(void (*thrd_func)(struct thrd_t *thread))
{
    struct thrd_t *thread = calloc(1, sizeof(struct thrd_t));
    thread->stack = calloc(1, THRD_STACK_SIZE);
    uintptr_t *stack = (uintptr_t *)((uintptr_t)thread->stack + THRD_STACK_SIZE);
    stack--;
    uintptr_t stack_base = (uintptr_t)stack;

    /* thrd_Start parameter */
    // *stack_top = thread;
    /* this will get executed the first time we switch into a thread */
    // stack_top--;
    // *stack_top = thrd_Start;


    /* we'll enter the thread func the first time thrd_Switch gets called with this
    thread. The parameter will be in rdi (on linux) / rcx (on windows) */
    stack--;
    *stack = (uintptr_t)thrd_func;
    /* rbp */
    stack--;
    *stack = stack_base;
    /* rax */
    stack--;
    *stack = 0;
    /* rbx */
    stack--;
    *stack = 0;
    /* rcx */
    stack--;
    *stack = 0;
    /* rdx */
    stack--;
    *stack = 0;
    /* rdi */
    stack--;
    *stack = (uintptr_t)thread;
    /* rsi */
    stack--;
    *stack = 0;
    /* r9 */
    stack--;
    *stack = 0;
    /* r10 */
    stack--;
    *stack = 0;
    /* r11 */
    stack--;
    *stack = 0;
    /* r12 */
    stack--;
    *stack = 0;
    /* r13 */
    stack--;
    *stack = 0;
    /* r14 */
    stack--;
    *stack = 0;
    /* r15 */
    stack--;
    *stack = 0;
    
    thread->rsp = (uintptr_t)stack;
    
    
    
    
    
    
    
    

    return thread;
}

void thrd_Destroy(struct thrd_t *thread)
{

}