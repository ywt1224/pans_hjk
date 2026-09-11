#ifndef PANS_INCLUDE_PANS_MUTEX_H
#define PANS_INCLUDE_PANS_MUTEX_H

#include <atomic> // 后面的那个 defined(__GLIBC__) ；如果没有包含这个头文件，就不会有定义，这两还是配套的
#include <mutex>

#if defined(__x86_64__) || defined(__i386__)
#include <immintrin.h> //这个头文件是干嘛的？
#elif defined(__aarch64__) || defined(__arm__)
#include <arm_acle.h>
#endif

#if defined(__linux__) && defined(__GLIBC__) //这个__GLIBC__关键字是干嘛的
#include <pthread.h>
#endif

inline void cpu_relax() noexcept //这个noexcept关键字是干嘛的
{
#if defined(__x86_64__) || defined(__i386__)
    _mm_pause();
#elif defined(__aarch64__) || defined(__arm__)
    __asm__ __volatile__("yield" ::: "memory");
#endif
}

namespace pans{

#if defined(__linux__) && defined(__GLIBC__)

class Spinlock
{
public:
    Spinlock() noexcept
    {// 第二个参数是决定 is nonzero the spinlock can be shared between different processes.
        pthread_spin_init(&m_mutex, PTHREAD_PROCESS_PRIVATE);
    }// 进程间共享是啥意思？

    ~Spinlock() noexcept //析构就没啥好说的
    {
        pthread_spin_destroy(&m_mutex);
    }

    //这两个是禁止拷贝构造吧，忘了，为什么要禁止呢
    Spinlock(const Spinlock&) = delete;
    Spinlock& operator=(const Spinlock&) = delete;

    void lock() noexcept
    {
        pthread_spin_lock(&m_mutex);
    }

    void unlock() noexcept
    {
        pthread_spin_unlock(&m_mutex);
    }

private:
    pthread_spinlock_t m_mutex{};
};

#else // 这个就是不是 Linux 平台就自己造一个呗，实现方式就是 TAS 然后优化了一下
        //内部不用每次都去重新设置值，只观察它的值是否就绪（就是类比锁是否释放了）
class Spinlock
{
public://这个别名又是干嘛的 后面好像也没用到，就是一个正常习惯，方便后面的使用
        //当然这里没用到这个别名
    using Lock = std::lock_guard<Spinlock>;

    Spinlock() noexcept = default;
    ~Spinlock() noexcept = default;

    Spinlock(const Spinlock&) = delete;
    Spinlock& operator=(const Spinlock&) = delete;

    void lock() noexcept
    {   //这个的用法和我们之前在操作系统里的用法还不太一样，这里传的是内存序
        //为什么不是传需要设置的值呢？
        while (m_mutex.test_and_set(std::memory_order_acquire))
        {//从这个 test 里的调用函数来看，就只是load了值
            while (m_mutex.test(std::memory_order_relaxed))
            {
                cpu_relax();
            }  
        }   
    }
    //这个关键字的作用又是啥
    [[nodiscard]] bool try_lock() noexcept
    {
        return !m_mutex.test_and_set(std::memory_order_acquire);
    } 

    void unlock() noexcept
    {
        m_mutex.clear(std::memory_order_release);
    }

private://设置原子变量的初始值就是0
    std::atomic_flag m_mutex = ATOMIC_FLAG_INIT;
};

#endif

} // namespace pans


#endif // PANS_INCLUDE_PANS_MUTEX_H