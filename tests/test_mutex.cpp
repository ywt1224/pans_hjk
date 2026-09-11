#include <pans/mutex.h>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <latch>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <pans/macros.h>

constexpr int RUNS = 7;
u64 g_Counter = 0;
u64 g_target = 0;

class Spinlock
{//这个是因为当前是Linux，所以我们之前自己实现的就不会宏展开，但是这里需要测试，所以粘一遍
public:
    using Lock = std::lock_guard<Spinlock>;

    Spinlock() noexcept = default;
    ~Spinlock() noexcept = default;

    Spinlock(const Spinlock&) = delete;
    Spinlock& operator=(const Spinlock&) = delete;

    void lock() noexcept
    {
        while (m_mutex.test_and_set(std::memory_order_acquire))
        {
            while (m_mutex.test(std::memory_order_relaxed))
            {
                cpu_relax();
            }
        }
    }

    [[nodiscard]] bool try_lock() noexcept
    {
        return !m_mutex.test_and_set(std::memory_order_acquire);
    }

    void unlock() noexcept
    {
        m_mutex.clear(std::memory_order_release);
    }

private:
    std::atomic_flag m_mutex = ATOMIC_FLAG_INIT;
};

u64 OperationsForThread(std::size_t thread_index, std::size_t thread_count)
{//这个就是把总的操作数计算平均分配给thread_count 个线程，每个有多少
    //然后除不尽的话，就把余数，分给前面的线程，这也是传thread_index的意义
    const u64 base = g_target / thread_count;
    const u64 remainder = g_target % thread_count;
    return base + (thread_index < remainder ? 1 : 0);//这个就是把余数分给前面的线程，每个多一个操作
}

template <typename Operation>//这里的 Operation 就是每个线程需要执行的动作
std::chrono::nanoseconds BenchmarkThreads(std::size_t thread_count, Operation&& operation)
{//这就是把操作数平均分配到各个线程，然后 一起开始工作 注意一起开始工作是怎么实现的
    std::latch ready(static_cast<std::ptrdiff_t>(thread_count));
    std::latch start_gate(1);
    std::latch finished(static_cast<std::ptrdiff_t>(thread_count));
    
    std::vector<std::thread> threads;
    threads.reserve(thread_count);//提前分配需要的线程数量
    for (std::size_t thread_index = 0; thread_index < thread_count; ++thread_index)
    {
        threads.emplace_back([&,thread_index](){
            ready.count_down();//表示当前线程准备好了，但是要等全部线程准备好
            start_gate.wait();//这个就是等待全部线程准备好，由主线程发出
            operation(thread_index,OperationsForThread(thread_index, thread_count));
            finished.count_down();//表示当前线程已经操作完毕，作为计算结束时间的信号
        });
    } 
    
    ready.wait();//等待全部线程准备好
    const auto begin = std::chrono::steady_clock::now();
    start_gate.count_down();//全部线程一起开始工作，有抢锁
    finished.wait();//等待全部线程完成
    const auto end = std::chrono::steady_clock::now();

    for (std::thread& thread : threads)
    {
        thread.join();
    }

    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin);
}

template <typename Mutex>
std::chrono::nanoseconds BenchmarkMutex(std::size_t thread_count)
{//这个就是根据不同的传入锁，来计算耗时了，内部其实就是把需要进行的操作数平均分配到每个线程,所以就是需要计算一下，这也是OperationsForThread的作用
    Mutex mutex;
    g_Counter = 0;
    const auto elapsed = BenchmarkThreads(thread_count, [&mutex](std::size_t, u64 operations) {
            for(u64 index = 0; index < operations; ++index)
            {//这里把自己传入的Mutex交给 lock_guard 应该是要满足它的要求的吧
                std::lock_guard<Mutex> lock(mutex);
                ++g_Counter;
            }
        });

    if (g_Counter != g_target)
    {
        throw std::runtime_error("mutex correctness check failed");
    }

    return elapsed;
}

template <typename Benchmark>
double Run(Benchmark&& benchmark)//为啥是 && 这个引用
{//这个就是获取操作对应的执行时间。总共执行7次，取平均值作为一次的耗时
    long double total_nanoseconds = 0.0L;
    for (int run = 0; run < RUNS; ++run)
    {//.count成员又是啥？
        total_nanoseconds += static_cast<long double>(benchmark().count());
    }
    return static_cast<double>(total_nanoseconds / RUNS);
}

void PrintResult(std::string_view name, double total_nanoseconds)
{//这个就只是一个格式化打印，执行的操作和总的耗费时间
    const double nanoseconds_per_operation = total_nanoseconds / static_cast<double>(g_target);
    std::cout << std::left << std::setw(30) << name << std::right
              << total_nanoseconds << " ns \ttotal, "
              << nanoseconds_per_operation << " ns/op\n";
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: ./test_mutex count\n";
        return EXIT_FAILURE;
    }//获取目标操作次数
    g_target = std::stoull(argv[1]);//这个是干嘛，我知道就是获取运行命令的第二个参数，还有什么其它含义吗

    std::cout << std::fixed << std::setprecision(2);//这些是设置格式，基本用法是啥？包含在iomanip里面
    std::cout << "operations: " << g_target << ", average of " << RUNS << " runs\n\n";

    const std::vector<std::size_t> thread_counts = {1, 2, 4, 8, 10};  
    for (const std::size_t thread_count : thread_counts)
    {
        std::cout << "threads: " << thread_count << '\n';//这里捕获的方式为什么是&，全局变量不可以看到吗
        PrintResult("pthread_spinlock::Spinlock", Run([&]() { return BenchmarkMutex<pans::Spinlock>(thread_count); }));
        PrintResult("atomic_flag::Spinlock", Run([&]() { return BenchmarkMutex<Spinlock>(thread_count); }));
        // PrintResult("atomic_wait", Run([&]() { return BenchmarkMutex<AtomicWaitLock>(thread_count); }));
        PrintResult("std::mutex", Run([&]() { return BenchmarkMutex<std::mutex>(thread_count); }));
        std::cout << '\n';
    }

    return EXIT_SUCCESS;    
}