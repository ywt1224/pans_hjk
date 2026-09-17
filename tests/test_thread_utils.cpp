#include <cassert>
#include <string>
#include <thread>
#include <iostream>

#if defined(__linux__)
#include <pthread.h>
#endif

#include <pans/utils/thread_utils.h>

int main()
{
    std::cout << "1: main thread id: " << pans::GetThreadId() << std::endl;
    std::cout << "2: main thread name: " << pans::GetThreadName() << std::endl;
    sleep(30); // 有足够的时间去控制台查看，输入命令：top -H -p <pid>
    pans::SetThreadName("main_thread");
    std::cout << "3: main thread name: " << pans::GetThreadName() << std::endl;
    sleep(10);
    pans::SetThreadName("花间客真帅"); // 一个汉字三个占字节
    std::cout << "4: main thread name: " << pans::GetThreadName() << std::endl;
    sleep(10);
    pans::SetThreadName("磐石磐石坚如磐石");
    std::cout << "5: main thread name: " << pans::GetThreadName() << std::endl;
    sleep(10);
    std::thread worker([]() {
        std::cout << "6: main thread id: " << pans::GetThreadId() << std::endl;
        std::cout << "7. son thread name: " << pans::GetThreadName() << std::endl;
        sleep(10);
        pans::SetThreadName("son-thread-long-name");
        std::cout << "8. son thread name: " << pans::GetThreadName() << std::endl;
        sleep(10);
        pans::SetThreadName("");
        std::cout << "9. son thread name: " << pans::GetThreadName() << std::endl;
    });
    worker.join();

    return 0;
}

