//
// Test additions to cdschecker2.
//

#include "common.h"
#include "librace2.h"

#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <stdio.h>
#include <unistd.h>


std::atomic<int> a{0};

librace::var<int> x=0,   y=1,   z=2;  // dependent data, check for races
librace::ref<int> rx=x,  ry=y,  rz=z; 
librace::ptr<int> px=&x, py=&y, pz=&z;

int xx=10, yy=20, zz=30;
librace::ptr pp(&x); // pp(&xx);


void fa(void *obj)
{
    x = 1; // aka store_32(&x, 1);
    
    // bug: use rl::mo_relaxed aka std::memory_order_relaxed
    // fix: use rl::mo_release aka std::memory_order_release
    a.store(1, std::memory_order_relaxed);
}

void fb(void *obj)
{
    // bug: use rl::mo_relaxed aka std::memory_order_relaxed
    // fix: use rl::mo_acquire aka std::memory_order_acquire
    if (1 == a.load(std::memory_order_relaxed))
    {
        x = 2; //aka store_32(&x, 2);
    }
}

int user_main(int argc, char **argv)
{

    MODEL_ASSERT(z == 2);
    MODEL_ASSERT(rz == 2);
    MODEL_ASSERT(*pz == 2);

    z += 1;    
    MODEL_ASSERT(z == 3);
    MODEL_ASSERT(rz == 3);
    MODEL_ASSERT(*pz == 3);
    
    rz += 1;    
    MODEL_ASSERT(z == 4);
    MODEL_ASSERT(rz == 4);
    MODEL_ASSERT(*pz == 4);
    
    *pz += 1;    
    MODEL_ASSERT(z == 5);
    MODEL_ASSERT(rz == 5);
    MODEL_ASSERT(*pz == 5);
    
    //sleep(15);
    a.store(0, std::memory_order_release);
    
    printf("Main thread: creating 2 threads\n");
    auto t1(std::jthread(fa, nullptr));
    auto t2(std::thread(fb, nullptr));

    //t1.join();
    t2.join();
    printf("Main thread is finished\n");

    return 0;
}

