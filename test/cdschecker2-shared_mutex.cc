//
// Test additions to cdschecker2.
//

#include "common.h"
#include "librace2.h"
#include "stacktrace.h"

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

int global_var{-321};

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

struct mtx_test_data
{
    librace::var<int>* _cx{nullptr};
    std::shared_mutex* _smtx{nullptr};
};

void freader(void *obj)
{
    utils::scope_debug um("freader\n");

    mtx_test_data* pdata((mtx_test_data*)obj);
    MODEL_ASSERT(pdata && pdata->_cx);
    MODEL_ASSERT(pdata && pdata->_smtx);

    librace::ptr<int> pcx(pdata->_cx);

    std::shared_lock rlock(*pdata->_smtx);
    [[maybe_unused]] int val(*pcx);
}


void fwriter(void *obj)
{
    utils::scope_debug um("fwriter\n");

    mtx_test_data* pdata((mtx_test_data*)obj);
    MODEL_ASSERT(pdata && pdata->_cx);
    MODEL_ASSERT(pdata && pdata->_smtx);

    librace::ptr<int> pcx(pdata->_cx);

    std::unique_lock wlock(*pdata->_smtx);
    *pcx += 1;
}

int user_main(int argc, char **argv)
{
    utils::scope_debug um("user_main\n");
    //print_stacktrace();

#if 0
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
#endif

#if 0
    {
        utils::scope_debug s("=== fa/fb relaxed bug\n");
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

    //
    // shared_mutex test
    // No error but it will hog the machine.
    //
    
    int local_var{-123};
    //model_print("int local_var %p\n", &local_var);

    std::shared_mutex smtx; //shared_mutex cannot be used as a global var (ModelChecker limitation)
    //model_print("smtx %p\n", &smtx);
    
    librace::var<int> cx=0;

    constexpr int NLOOP = 1;
    constexpr int NTHRD = 1;

//    { std::shared_lock rlock(smtx); }
//    { std::unique_lock wlock(smtx); }

#if 1
    {
        utils::scope_debug s("=== old type functions\n");
        cx = 0;

        mtx_test_data testdata{
            ._cx = &cx,
            ._smtx = &smtx
        };

        auto t1(std::thread(freader, (void*)&testdata)); 
        auto t2(std::thread(fwriter, (void*)&testdata));

        t1.join();
        t2.join();

        MODEL_ASSERT( cx == NTHRD*NLOOP );
        cx = 0;
    }
#endif

#if 0
    {
        utils::scope_debug s("=== buggy emplace/move\n");

        std::vector<std::thread> thrs; // moveable threads
        thrs.reserve(2*NTHRD);

        model_print("--push_back\n");
        for (int i = 0; i < NTHRD; ++i) {
            thrs.push_back(std::thread([&](){
                for (int j = 0; j < NLOOP; ++j) {
                    //model_print("lambda1 smtx=%p\n", &smtx);
                    MODEL_ASSERT(global_var == -321);
                    //model_print("lambda1 local_var=%p\n", &local_var);
                    MODEL_ASSERT(local_var  == -123);
                    std::shared_lock rlock(smtx);
                    [[maybe_unused]] int val(cx);
                }
            }));
        }

        model_print("--NO emplace_back: buggy\n");
        for (int i = 0; i < NTHRD; ++i) {
            thrs.push_back(std::thread([&](){ //emplace_back([&](){
                for (int j = 0; j < NLOOP; ++j) {
                    //model_print("emplace_back lambda2 smtx=%p\n", &smtx); 
                    MODEL_ASSERT(global_var == -321);
                    MODEL_ASSERT(local_var  == -123);
                    std::unique_lock wlock(smtx);
                    //model_print("l2\n");
                    cx += 1;
                }
            }) );
        }

        //model_print("--cleanup\n");
        for (auto& t : thrs) {
            t.join();
        }
        MODEL_ASSERT( cx == NTHRD*NLOOP );
        cx = 0;
    }
#endif

#if 0
    {
        utils::scope_debug s("=== placement new\n");

        std::array<std::thread, 2*NTHRD> thrs;
        for (int i = 0; i < NTHRD; ++i) {
            //TODO: std::destroy_at / std::construct_at 
            thrs[i].~thread(); // (&thrs[i])->~thread() 
            new (&thrs[i]) std::thread([&](){
                for (int j = 0; j < NLOOP; ++j) {
                    //model_print("lambda3 smtx=%p\n", &smtx);
                    MODEL_ASSERT(global_var == -321);
                    MODEL_ASSERT(local_var  == -123);
                    std::shared_lock rlock(smtx);
                    [[maybe_unused]] int val(cx);
                }
            });
        }
        for (int i = NTHRD; i < 2*NTHRD; ++i) {
            thrs[i].~thread(); 
            new (&thrs[i]) std::thread([&](){
                for (int j = 0; j < NLOOP; ++j) {
                    //model_print("lambda4 smtx=%p\n", &smtx);
                    MODEL_ASSERT(global_var == -321);
                    MODEL_ASSERT(local_var  == -123);
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

    printf("Main thread is finished\n");
    return 0;
}

