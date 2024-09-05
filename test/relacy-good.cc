//
// Same test as the relacy one. Correct.
//

#include "common.h"
#include "librace2.h"

#include <atomic>
#include <threads.h>
#include <stdio.h>
#include <unistd.h>


std::atomic<int> a{0};
librace::var<int> x = 0; // dependent data, check for races


void fa(void *obj)
{
    x = 1; //store_32(&x, 1);
    // bug: use rl::mo_relaxed aka std::memory_order_relaxed
    // fix: use rl::mo_release aka std::memory_order_release
    a.store(1, std::memory_order_release);
}

void fb(void *obj)
{
    // bug: use rl::mo_relaxed aka std::memory_order_relaxed
    // fix: use rl::mo_acquire aka std::memory_order_acquire
    if (1 == a.load(std::memory_order_acquire))
    {
        x = 2; //store_32(&x, 2);
    }
}

int user_main(int argc, char **argv)
{
    //sleep(15);
    a.store(0, std::memory_order_release);
    
    thrd_t t1, t2;

    printf("Main thread: creating 2 threads\n");
    thrd_create(&t1, (thrd_start_t)&fa, NULL);
    thrd_create(&t2, (thrd_start_t)&fb, NULL);

    thrd_join(t1);
    thrd_join(t2);
    printf("Main thread is finished\n");

    return 0;
}

