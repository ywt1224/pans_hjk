#ifndef PANS_INCLUDE_PANS_LOGGER_APPENDER_H
#define PANS_INCLUDE_PANS_LOGGER_APPENDER_H

#include <memory>
#include <string>

#include <pans/export.h>
#include <pans/logger/log_level.h>

namespace pans {

//用来代理我们的内部权限，严格控制访问我们的Appender私有实现的哪些功能
namespace detail {//为什么要采取友元的方式呢？嗯...有没有什么其他方式，对比有哪些好处
class AppenderAccess;
}

class PANS_API Appender final
{
public:
    //声明一个内部实现类, 类外定义
    class Impl;//这个就是具体的实现，外部所有都转发到这里完成
    ~Appender() = default;
    //禁用所有的构造和赋值重载
    Appender(const Appender&) = delete;
    Appender& operator=(const Appender&) = delete;
    Appender(Appender&&) = delete;
    Appender& operator=(Appender&&) = delete; 
    
    void setLevel(LogLevel::Level level) noexcept;
    [[nodiscard]] LogLevel::Level getLevel() const noexcept;

    void flush();
    void sync();

private://构造函数私有不允许外部调用构造，只能通过AppenderAccess使用
    explicit Appender(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> m_impl;
    friend class detail::AppenderAccess;
};

using AppenderPtr = std::shared_ptr<Appender>;
//这两就是提供的对外接口，能够用来创建对应的Appender（输出终端
[[nodiscard]] PANS_API AppenderPtr MakeStdoutAppender();
[[nodiscard]] PANS_API AppenderPtr MakeFileAppender(std::string file_name);

}

#endif //PANS_INCLUDE_PANS_LOGGER_APPENDER_H