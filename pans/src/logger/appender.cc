#include "logger/appender_impl.h"

#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <system_error>
#include <utility>

#include <pans/macros.h>

#if defined(_WIN32)//跨平台咯，就是提供底层的那些sync，flush这些
#include <io.h>
#else
#include <unistd.h>
#endif

namespace pans {

// 这个构造函数是私有的，外部用不了，所以这里就只是给 AppenderAccess 用的 
Appender::Appender(std::unique_ptr<Impl> impl) noexcept
    : m_impl(std::move(impl))
{}

void Appender::setLevel(LogLevel::Level level) noexcept
{
    m_impl->setLevel(level);
}

LogLevel::Level Appender::getLevel() const noexcept
{
    return m_impl->getLevel();
}

void Appender::flush()
{
    m_impl->flush();
}

void Appender::sync()
{
    m_impl->sync();
}
//将格式化好的日志，提交到对应的 Appender，这个应该也是给 AppenderAccess 用的
void Appender::Impl::append(LogLevel::Level level, std::string_view formatted_record) noexcept
{

    if(static_cast<u8>(level) < static_cast<u8>(getLevel()))
    {
        return;
    }
    //符合日志级别，就可以输出，这里就是会直接执行对应子类重载的函数
    // std::mutex::lock()函数，在极端情况下会抛出std::system_error异常, lock_guard的构造函数也不是noexcept的
    try
    {//这里为啥要加锁呢，加锁是为了保护啥？
        std::lock_guard<std::mutex> lock(m_mutex);
        writeUnlocked(formatted_record);
        if(level == LogLevel::Level::LOG_LV_FATAL)
        {
            flushUnlocked();
        }
    }
    catch(...)
    {
        constexpr std::string_view message = "pans logger: appender operation failed\n";
        std::fwrite(message.data(),1,message.size(),stderr);
    }
}
//这两个改变原子变量和读取原子变量的方式，就没啥好说的，主要就是这个内存序得看看
void Appender::Impl::setLevel(LogLevel::Level level) noexcept
{
    m_level.store(level, std::memory_order_release);
}

LogLevel::Level Appender::Impl::getLevel() const noexcept
{
    return m_level.load(std::memory_order_acquire);
}

// flush接口，把数据从程序的内存刷到操作系统缓冲区
void Appender::Impl::flush()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    flushUnlocked();
}

// sync接口，把数据从操作系统的缓冲区刷到IO缓冲区
void Appender::Impl::sync()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    syncUnlocked();
}

//下面就是两个子类的重载实现
class StdoutAppenderImpl final : public Appender::Impl
{//这个终端的就还好，挺简单，直接就是调用 std::cout 的，然后实现那三个受保护的接口
protected://这三个接口后面还额可以再看看吧具体实现原理
    void writeUnlocked(std::string_view formatted_record) noexcept override
    {
        std::cout.write(formatted_record.data(), static_cast<std::streamsize>(formatted_record.size()));
    }

    void flushUnlocked() noexcept override
    {
        std::cout.flush();
    }

    //终端就没有落磁盘的要求，所以也还是flush的调用
    void syncUnlocked() noexcept override
    {
        flushUnlocked();
    }
};

class FileAppenderImpl final : public Appender::Impl
{//这个就稍微多一点，因为传进来的是文件名，所以还需要fopen，然后保存 FILE* 指针
public:
    explicit FileAppenderImpl(const std::string& file_name)
    {
        if(file_name.empty())
        {
            throw std::invalid_argument("logger file name cannot be empty");
        }
        m_file = std::fopen(file_name.c_str(),"ab");
        if(m_file == nullptr)
        {
            throw std::system_error(errno, std::generic_category(), "failed to open logger file: " + file_name);
        }                
    }

    ~FileAppenderImpl() override
    {
        if(m_file != nullptr)
        {
            std::fflush(m_file);
            std::fclose(m_file);
        }
    }

protected:
    void writeUnlocked(std::string_view formatted_record) noexcept override
    {
        std::fwrite(formatted_record.data(), 1, formatted_record.size(), m_file);
    }

    void flushUnlocked() noexcept override
    {
        std::fflush(m_file);
    }

    void syncUnlocked() noexcept override
    {
        flushUnlocked();
#if defined(_WIN32)
        ::_commit(::_fileno(m_file));
#else
        ::fdatasync(::fileno(m_file));
#endif
    }

private:
    std::FILE* m_file = nullptr;
};

AppenderPtr detail::AppenderAccess::MakeStdoutAppender()
{
    return AppenderPtr(new Appender(std::make_unique<StdoutAppenderImpl>()));
}

AppenderPtr detail::AppenderAccess::MakeFileAppender(std::string file_name)
{
    return AppenderPtr(new Appender(std::make_unique<FileAppenderImpl>(file_name)));
}

void detail::AppenderAccess::Append(const AppenderPtr& appender, LogLevel::Level level, std::string_view formatted_record) noexcept
{//看跟我想的一样，就是impl的append 就是给AppenderAccess用的
    if(appender != nullptr)
    {
        appender->m_impl->append(level, formatted_record);
    }
}

AppenderPtr MakeStdoutAppender()
{
    return detail::AppenderAccess::MakeStdoutAppender();
}

AppenderPtr MakeFileAppender(std::string file_name)
{
    return detail::AppenderAccess::MakeFileAppender(std::move(file_name));
}

}// namespace pans 