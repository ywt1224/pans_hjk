#ifndef PANS_INCLUDE_PANS_SINGLETON_HPP
#define PANS_INCLUDE_PANS_SINGLETON_HPP

namespace pans {

template <typename T>
class Singleton {
   
public:
    static T& Instance()
    {
        static T instance; // 局部静态变量，C++11之后线程安全
        return instance;
    }

private:
    // 单例对象不需要拷贝或移动
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;
    Singleton(Singleton&&) = delete;
    Singleton& operator=(Singleton&&) = delete;

protected:// 子类继承需要调用父类构造
    Singleton() = default;
    ~Singleton() = default;
};

}// namespace pans

#endif //PANS_INCLUDE_PANS_SINGLETON_HPP