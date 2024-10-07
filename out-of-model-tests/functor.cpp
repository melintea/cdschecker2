//==============================================================================
// Poor man's function & task (cdschecker has limited std support 
// and std::function is messing with the memory addresses being checked)
//==============================================================================


#include <array>
#include <atomic>
#include <mutex>
#include "thread"
#include "shared_mutex"
#include <vector>
#include <stdio.h>
#include <unistd.h>


class Foo
{

public:

    int bar(int a, int b)
    {
        return a + b;
    }
};



int somefunction(int a, int b)
{
    return a + b;
}

int someTask1(int x)
{
    std::cout << "x=" << x << std::endl;
    return x;
}

void someTask2(int x, int* py)
{
    std::cout << "someTask2 x=" << x << std::endl;
    std::cout << "someTask2 py = 0x" << py << '\n';
}

void fa(void *obj)
{
}

void fb(void *obj)
{
}

int user_main(int argc, char** argv)
{
    utils::scope_print um("user_main\n");

    constexpr int NLOOP = 1;
    constexpr int NTHRD = 1;
    int cx = 0;
    std::cout << "&cx = 0x" << &cx << '\n';

    std::shared_mutex smtx; //shared_mutex cannot be used as a global var (ModelChecker limitation)
    model_print("smtx %p\n", &smtx);

#if 1
    printf("Main thread: creating 2 threads\n");
    auto t1(std::jthread(fa, (void*)nullptr)); //<-- FIXME nullptr to (void*)0
    auto t2(std::thread(fb, (void*)nullptr));

    //t1.join();
    t2.join();
#endif
#if 1
    std::cout << "==== thread \n";
    
    {
        std::jthread ta([&](){
                for (int j = 0; j < NLOOP; ++j) {
                    model_print("lambda1 %p\n", &cx);
                }
             });
        std::jthread tb(std::move(ta));
        tb.join();
        ta.join();
    }

    {
        utils::scope_print s("=== buggy emplace/move\n");

        std::vector<std::thread> thrs; // moveable threads
        thrs.reserve(2*NTHRD);

        model_print("--push_back\n");
        for (int i = 0; i < NTHRD; ++i) {
            thrs.push_back(std::thread([&](){
                for (int j = 0; j < NLOOP; ++j) {
                    model_print("lambda1 %p\n", &cx);
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
                    model_print("emplace_back lambda2 %p\n", &cx);
                    model_print("emplace_back lambda2 %p\n", &smtx); //BUG: addr  of smtx
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
    
    {
        utils::scope_print s("=== placement new\n");

        std::array<std::thread, 2*NTHRD> thrs;
        for (int i = 0; i < NTHRD; ++i) {
            //TODO: std::destroy_at / std::construct_at 
            thrs[i].~thread(); // (&thrs[i])->~thread() 
            new (&thrs[i]) std::thread([&](){
                for (int j = 0; j < NLOOP; ++j) {
                    model_print("lambda3 %p\n", &cx);
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
                    model_print("lambda4 %p\n", &cx);
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
    
    { // task
        int  a  = 15;
        int* pa = &a;
        int  b  = 1;
        std::cout << "&a = 0x" << pa << '\n';

        // Lambda 
        std::thread thread1 = std::thread([&]() {
            a += 5;
            std::cout << "&a = 0x" << pa << '\n';
            assert(pa == &a);
        });
        //assert(thread1._ptdata->_tid.priv == 101);
        std::cout << a << std::endl; // 20
        assert(a == 20);

        // Top level function reference
        a = 5;
        std::thread  thread2 = std::thread(someTask2, 42, &a);
        //assert(thread2._ptdata->_tid.priv == 101);
        std::cout << a << std::endl; // 5
        assert(a == 5);
    }
#endif

    return 0;
}

int main(int argc, char** argv)
{
    return user_main(argc, argv);
}


