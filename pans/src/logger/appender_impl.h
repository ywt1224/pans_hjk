#ifndef PANS_SRC_LOGGER_APPENDER_IMPL_H
#define PANS_SRC_LOGGER_APPENDER_IMPL_H

#include <atomic>
#include <cstdio>
#include <mutex>
#include <string_view>

#include <pans/logger/appender.h>

namespace pans {

class Appender::Impl //Appender的真正实现类，是两个子类的父类
{
public:
    virtual ~Impl() = default;//这个append函数应该就是对应 提交格式化后的日志到Appender吧，感觉是交给AppenderAccess去用的
    void append(LogLevel::Level level, std::string_view formatted_record) noexcept;
    void setLevel(LogLevel::Level level) noexcept;
    [[nodiscard]] LogLevel::Level getLevel() const noexcept;
    void flush();
    void sync();
    
protected://这里为啥还要把这些接口放到protected关键字下面，这个关键字作用我又忘了
    virtual void writeUnlocked(std::string_view formatted_record) noexcept = 0;
    virtual void flushUnlocked() noexcept = 0;
    virtual void syncUnlocked() noexcept = 0;
    
    std::mutex m_mutex;

private:
    std::atomic<LogLevel::Level> m_level{LogLevel::Level::LOG_LV_DEBUG};
};

namespace detail {

class AppenderAccess final
{//这就是给非Appender用的能访问 Appender 内部实现的权限了，全是静态函数
public:
    [[nodiscard]] static AppenderPtr MakeStdoutAppender();
    [[nodiscard]] static AppenderPtr MakeFileAppender(std::string file_name);
    static void Append(const AppenderPtr& appender, LogLevel::Level level, std::string_view formatted_record) noexcept;    
};

}// namespace detail
}// namespace pans

#endif //PANS_SRC_LOGGER_APPENDER_IMPL_H