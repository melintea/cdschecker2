#include <stdio.h>
#include <threads.h>
#include <stdatomic.h>

#include "librace.h"

atomic_int x;
atomic_int y;
atomic_int z;

static void a(void *obj)
{
        int r4=atomic_load_explicit(&y, memory_order_relaxed);
        int r1=atomic_fetch_add_explicit(&x, 2, memory_order_relaxed);
        printf("r1=%d\n",r1);
        printf("r4=%d\n",r4);
}

static void b(void *obj)
{
        int r2=atomic_fetch_add_explicit(&x, 2, memory_order_relaxed);
        atomic_store_explicit(&y, 1, memory_order_relaxed);
        //      atomic_store_explicit(&z, 1, memory_order_relaxed);
        printf("r2=%d\n",r2);
}

static void c(void *obj)
{
        //      int r1=atomic_load_explicit(&z, memory_order_acquire);
        int r3=atomic_load_explicit(&x, memory_order_acquire);
        printf("r3=%d\n",r3);
}

static void d(void *obj)
{
        atomic_store_explicit(&x, 1, memory_order_release);
}

int user_main(int argc, char **argv)
{
        thrd_t t1, t2, t3, t4;

        atomic_init(&x, 0);
        atomic_init(&y, 0);
        //      atomic_init(&z, 0);

        printf("Main thread: creating 2 threads\n");
        thrd_create(&t1, (thrd_start_t)&a, NULL);
        thrd_create(&t2, (thrd_start_t)&b, NULL);
        thrd_create(&t3, (thrd_start_t)&c, NULL);
        thrd_create(&t4, (thrd_start_t)&d, NULL);

        thrd_join(t1);
        thrd_join(t2);
        thrd_join(t3);
        thrd_join(t4);
        printf("Main thread is finished\n");

        return 0;
}
