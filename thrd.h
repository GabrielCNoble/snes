#ifndef THRD_H
#define THRD_H

#include <stdint.h>

struct thrd_t
{
    uintptr_t   rsp;
    void *      data;
    void *      stack;
};

/* 4MB stack */
#define THRD_STACK_SIZE 0x400000

extern void thrd_Switch(struct thrd_t *from, struct thrd_t *to);

extern void thrd_Start(struct thrd_t *thread);

struct thrd_t *thrd_Create(void (*thrd_func)(struct thrd_t *thread));

void thrd_Destroy(struct thrd_t *thread);



#endif