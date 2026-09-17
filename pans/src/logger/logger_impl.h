#ifndef PANS_SRC_LOGGER_LOGGER_IMPL_H
#define PANS_SRC_LOGGER_LOGGER_IMPL_H

#include <atomic>
#include <memory>
#include <shared_mutex>
#include <string>
#include <vector>

#include <pans/logger/logger.h>

#include "logger/formatter.h"
#include "logger/log_record.h"

namespace pans {

class Logger::Impl
{
public:
    explicit Impl(std::string name);
    [[nodiscard]] bool shouldLog(LogLevel::Level level) const noexcept;
    // submit函数也是给Access用的
    void submit(const detail::LogRecordView& record) noexcept;
    void setLevel(LogLevel::Level level) noexcept;
    [[nodiscard]] LogLevel::Level getLevel() const noexcept;
    [[nodiscard]] std::string_view getName() const noexcept;
   void setFormatter(std::shared_ptr<const detail::Formatter> formatter);
    [[nodiscard]] std::shared_ptr<const detail::Formatter> getFormatter() const noexcept;
    void addAppender(AppenderPtr appender);
    void removeAppender(const AppenderPtr& appender);
    void clearAppenders();    

    void flush();
    void sync();

    void setRoot(const LoggerPtr& root) noexcept;//给access用的
private:
    std::string m_name;
    std::atomic<LogLevel::Level> m_level{LogLevel::Level::LOG_LV_DEBUG};

    mutable std::shared_mutex m_mutex;// 这些成员变量倒还是能想到，就是这个shared_mutex用来保护啥的呢
    std::shared_ptr<const detail::Formatter> m_formatter;
    std::vector<AppenderPtr> m_appenders;

    LoggerPtr m_root;// 这里还额外准备一个 root 的 LoggerPtr 的意义是啥  
};

namespace detail {
/**
 * LoggerAccess本质上是一个内部权限访问的桥梁，
 * 用于在不扩大Logger公共API的情况下，访问其私有实现
 */
class LoggerAccess final//用来提供给非 Logger 去调用 Logger的一些内部实现
{
public://这个接口的设计方式和 appender 都是一样的 所有参数都是外部传入，这也符合 Access 只是要对应的权限的设计理念
    static void Submit(Logger& logger, const LogRecordView& record) noexcept;
    static void SetRoot(Logger& logger, const LoggerPtr& root) noexcept;
};

} // namespace detail
} // namespace pans 

#endif //PANS_SRC_LOGGER_LOGGER_IMPL_H