//
// Test additions to cdschecker2.
//

#include "common.h"
#include "librace2.h"

#include <array>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>
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
    //x = 1; // aka store_32(&x, 1);
    rx = 1;
    
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
        //x = 2; //aka store_32(&x, 2);
	*px = 2;
    }
}

int user_main(int argc, char **argv)
{
    utils::scope_print um("user_main\n");

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

#if 0
    {
        utils::scope_print s("=== fa/fb relaxed bug\n");
        //
        // This exposes a relaxed bug with fa & fb
        //
        
        a.store(0, std::memory_order_release);
        
        printf("Main thread: creating 2 threads\n");
        auto t1(std::jthread(fa, (void*)nullptr)); // FIXME: -fpermissive instead of casting?
        auto t2(std::thread(fb, (void*)nullptr));

        t1.join();
        t2.join();
    }
#endif

#if 0
    {
        utils::scope_print s("=== well behaved threads\n");

        std::thread ta([&](){
                model_print("ta\n");
             });
        std::thread tb([&](){
                model_print("tb\n");
             });

        ta.join();
        tb.join();
    }
    {
        utils::scope_print s("=== well behaved threads\n");

        std::jthread ta([&](){
                model_print("jta\n");
             });
        std::jthread tb([&](){
                model_print("jtb\n");
             });
    }
#endif

#if 0
    {
        utils::scope_print s("=== buggy:not joined thread \n");

        std::thread ta([&](){
                model_print("ta not joined\n");
             });
        std::thread tb([&](){
                model_print("tb\n");
             });

        //ta.join();
        tb.join();
    }
#endif

#if 0
    //
    // shared_mutex test
    // No error but it will hog the machine.
    //
    
    std::shared_mutex smtx; //shared_mutex cannot be used as a global var (ModelChecker limitation)
    model_print("smtx %p\n", &smtx);
    librace::var<int> cx=0;
    constexpr int NLOOP = 1;
    constexpr int NTHRD = 1;

    { std::shared_lock rlock(smtx); }
    { std::unique_lock wlock(smtx); }

    {
        std::jthread ta([&](){
                for (int j = 0; j < NLOOP; ++j) {
                    model_print("lambda1 %p\n", &smtx);
                }
             });
        std::jthread tb(std::move(ta));
        tb.join();
        ta.join();
    }

    #if 1
    {
        utils::scope_print s("emplace/move\n");

        std::vector<std::thread> thrs; // moveable threads
        thrs.reserve(2*NTHRD);

        model_print("--push_back\n");
        for (int i = 0; i < NTHRD; ++i) {
            thrs.push_back(std::thread([&](){
                for (int j = 0; j < NLOOP; ++j) {
                    model_print("lambda1 %p\n", &smtx);
                    std::shared_lock rlock(smtx);
                    [[maybe_unused]] int val(cx);
                }
            }));
        }

        model_print("--emplace_back\n");
        for (int i = 0; i < NTHRD; ++i) {
            thrs.emplace_back([&](){
                for (int j = 0; j < NLOOP; ++j) {
                    model_print("emplace_back lambda2 %p\n", &smtx);
                    std::unique_lock wlock(smtx);
                    model_print("l2\n");
                    cx += 1;
                }
            });
        }

        model_print("--cleanup\n");
        for (auto& t : thrs) {
            t.join();
        }
        MODEL_ASSERT( cx == NTHRD*NLOOP );
        cx = 0;
    }
    #endif

    #if 1
    {
        utils::scope_print s("placement new\n");

        std::array<std::thread, 2*NTHRD> thrs;
        for (int i = 0; i < NTHRD; ++i) {
            //TODO: std::destroy_at / std::construct_at 
            thrs[i].~thread(); // (&thrs[i])->~thread() 
            new (&thrs[i]) std::thread([&](){
                for (int j = 0; j < NLOOP; ++j) {
                    model_print("lambda3 %p\n", &smtx);
                    std::shared_lock rlock(smtx);
                    [[maybe_unused]] int val(cx);
                }
            });
        }
        for (int i = NTHRD; i < 2*NTHRD; ++i) {
            thrs[i].~thread(); 
            new (&thrs[i]) std::thread([&](){
                for (int j = 0; j < NLOOP; ++j) {
                    model_print("lambda4 %p\n", &smtx);
                    std::unique_lock wlock(smtx);
                    cx += 1;
                }
            });
        }
        for (auto& t : thrs) {
            t.join();
        }
        for (auto& t : thrs) {
            t.~thread();
        }
        MODEL_ASSERT( cx == NTHRD*NLOOP );
        cx = 0;
    }
    #endif

#endif
    
    printf("Main thread is finished\n");
    return 0;
}

