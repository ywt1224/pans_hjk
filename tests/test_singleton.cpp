#include <iostream>
#include <thread>
#include <vector>

#define SINGLETON_VERSION_MEYERS 1
#define SINGLETON_VERSION_PURE_ACCESSOR 2
#define SINGLETON_VERSION_INHERITANCE 3

#ifndef SINGLETON_VERSION
#define SINGLETON_VERSION SINGLETON_VERSION_INHERITANCE
#endif

// -------------版本一------------------

#if SINGLETON_VERSION == SINGLETON_VERSION_MEYERS

class Singleton {
public:
    Singleton(const Singleton&) = delete;
    Singleton(Singleton&&) = delete;
    Singleton& operator=(const Singleton&) = delete;
    Singleton& operator=(Singleton&&) = delete;

    // 唯一全局访问点
    static Singleton& Instance()
    {
        static Singleton instance; // 局部静态变量，C++11之后线程安全
        return instance;
    }

    void log(const std::string& msg)
    {
        std::cout << "[LOG]: " << msg << std::endl;
    }

// 私有构造函数，阻止类外创建对象
private:
    Singleton() = default;
    ~Singleton() = default;
};

int main()
{
    Singleton::Instance().log("Hello Meyers Singleton.1");
    return 0;
}

#elif SINGLETON_VERSION == SINGLETON_VERSION_PURE_ACCESSOR

// -------------版本二------------------
template<typename T>
class Singleton
{
public:
    static inline T& Instance()
    {
        static T instance; // 局部静态变量，C++11之后线程安全
        return instance;
    }
private:
    Singleton() = delete;
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;
    Singleton(Singleton&&) = delete;
    Singleton& operator=(Singleton&&) = delete;
};

class A
{
public:
    void log(const std::string& msg)
    {
        std::cout << "[LOG]: " << msg << std::endl;
    }

private:
    A() = default;
    ~A() = default;

    A(const A&) = delete;
    A(A&&) = delete;
    A& operator=(const A&) = delete;
    A& operator=(A&&) = delete;

    friend class Singleton<A>;
};

using SA = Singleton<A>;

int main()
{
    SA::Instance().log("Hello Meyers Singleton.2");
    return 0;
}

#elif SINGLETON_VERSION == SINGLETON_VERSION_INHERITANCE

// --------------版本三，也是pans库采纳的方式，继承单例模板类-------------

#include <pans/singleton.hpp>

class A : public pans::Singleton<A>
{
    friend class pans::Singleton<A>;
public:
    void log(const std::string& msg)
    {
        std::cout << "[LOG]: " << msg << " data=" << m_data << std::endl;
    }
private:
    // A() = default; // 此类构造函数不对类对象进行修改，类对象创建即可用，编译器不会加锁
    // ~A() = default;

    // 欲使编译器加锁，必须产生一个编译器无法忽略的行为
    A()
    {
        std::puts("A::A() -- only once");
        m_data++;
    }
    ~A()
    {
        std::puts("A::~A() -- only once");
    }
private:
    int m_data{0};
};

int main()
{
    std::vector<std::thread> threads;
    threads.reserve(8);

    for (int i = 0; i < 8; ++i)
    {
        threads.emplace_back([] {
            A::Instance().log("Hello Meyers Singleton.3");
        });
    }

    for (auto& thread : threads)
    {
        thread.join();
    }
}

#endif

// 汇编单个文件
// g++ -DSINGLETON_VERSION=1 -S test_singleton.cpp -o test_singleton.s -I../pans/include
// __cxa_guard_acquire
// __cxa_guard_release

