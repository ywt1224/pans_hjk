#include "logger/logger_impl.h"

#include <algorithm>
#include <cstdio>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <utility>

#include <pans/macros.h>

#include "logger/appender_impl.h"
#include "logger/buffer.h"

namespace pans {
// 默认的格式，用来创建格式化器
constexpr std::string_view DEFAULT_LOG_PATTERN = "%d{%Y-%m-%d %H:%M:%S}.%u%Tthread=%t%Tfiber=%F%T[%p]%T%f:%l%T%m%n";

Logger::Logger(std::string name)
    : m_impl(std::make_unique<Impl>(std::move(name)))
{}

Logger::~Logger() = default;

bool Logger::shouldLog(LogLevel::Level level) const noexcept
{
    return m_impl->shouldLog(level);
}

void Logger::setLevel(LogLevel::Level level) noexcept
{
    m_impl->setLevel(level);
}

LogLevel::Level Logger::getLevel() const noexcept
{
    return m_impl->getLevel();
}

std::string_view Logger::getName() const noexcept
{
    return m_impl->getName();
}

void Logger::setFormatter(std::string_view pattern)
{//你看Logger对外提供的接口，只需要传入 pattern 这里肯定要转一次的，然后impl要做的就简单了
    auto formatter = std::make_shared<const detail::Formatter>(pattern);
    m_impl->setFormatter(std::move(formatter));
}

std::string Logger::getFormatterPattern() const
{
    const auto formatter = m_impl->getFormatter();
    return formatter == nullptr ? std::string() : formatter->getPattern();
}

void Logger::addAppender(AppenderPtr appender)
{
    m_impl->addAppender(std::move(appender));
}

void Logger::removeAppender(const AppenderPtr& appender)
{
    m_impl->removeAppender(appender);
}

void Logger::clearAppenders()
{
    m_impl->clearAppenders();
}

void Logger::flush()
{
    m_impl->flush();
}

void Logger::sync()
{
    m_impl->sync();
}

Logger::Impl::Impl(std::string name)
    : m_name(std::move(name))
{// 就是两件事，给名字，然后构造一个默认的 formatter ，内部就是根据pattern 依次构造 formatter_item
    if(m_name.empty())
    {
        throw std::invalid_argument("logger name cannot be empty");
    }
    m_formatter = std::make_shared<const detail::Formatter>(DEFAULT_LOG_PATTERN);
}

bool Logger::Impl::shouldLog(LogLevel::Level level) const noexcept
{// 有两个判断条件，一个肯定就是日志级别，另一个就是 Appenders 要非空
    if(static_cast<u8>(level) < static_cast<u8>(getLevel()))
    {
        return false;
    }
    {// 这里还用了个 RAII 机制
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        if(!m_appenders.empty())
        {
            return true;
        } 
    }// 最后的兜底，尝试用 root 的 LoggerPtr 去判断
    return m_root != nullptr && m_root->shouldLog(level);
}

/**
 * 这个实现思路也简单嘛，就是把 LogRecordView 格式化成对应的字符串
 * 然后让所有的appender去提交（这个过程就是由 AppenderAccess::Append 去实现了
 * 
 * 然后就是一些前置检查，如果使用主日志器，最终还是走的上面的流程
 */
void Logger::Impl::submit(const detail::LogRecordView& record) noexcept
{//这个也是提供给内部接口使用的
    if(static_cast<u8>(record.m_level) < static_cast<u8>(getLevel()))
    {
        return;
    }
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    if(m_appenders.empty())
    {//没有 appenders 列表，就使用默认的 root 的 LoggerPtr 
        lock.unlock();
        if(m_root != nullptr)
        {
            m_root->m_impl->submit(record);
        }
        return;
    } 
    // InlineBuffer，它遇到长日志的时候，可能会使用堆内存
    // vector开辟堆内存的时候，会有极小概率抛bad_alloc异常，这里不能让异常逃逸出去
    try
    {
        detail::FormattedRecordBuffer formatted_record;
        m_formatter->format(record, formatted_record);
        for(const AppenderPtr& appender : m_appenders)
        {
            detail::AppenderAccess::Append(appender, record.m_level, formatted_record.view());
        }
    }
    // 如果try失败，我们往标准错误里打一条提示
    catch(...)
    {
        constexpr std::string_view message = "pans logger: record formatting failed\n";
        std::fwrite(message.data(), 1, message.size(), stderr);
    }
}

void Logger::Impl::setLevel(LogLevel::Level level) noexcept
{
    m_level.store(level, std::memory_order_release);
}

LogLevel::Level Logger::Impl::getLevel() const noexcept
{
    return m_level.load(std::memory_order_acquire);
}

std::string_view Logger::Impl::getName() const noexcept
{
    return m_name;
}

void Logger::Impl::setFormatter(std::shared_ptr<const detail::Formatter> formatter)
{//这个是对应 Logger::setFormatter 已经构造好对应的智能指针传入了
    ASSERT_RETNONE2(formatter != nullptr, "logger formatter cannot be null");
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_formatter = std::move(formatter);
}

std::shared_ptr<const detail::Formatter> Logger::Impl::getFormatter() const noexcept
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_formatter;
}

void Logger::Impl::addAppender(AppenderPtr appender)
{
    ASSERT_RETNONE2(appender != nullptr, "logger appender cannot be null");
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_appenders.push_back(std::move(appender));
}

void Logger::Impl::removeAppender(const AppenderPtr& appender)
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    std::erase(m_appenders, appender);//这个是vector 的erase，就是直接传需要删去的值
}

void Logger::Impl::clearAppenders()
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_appenders.clear();
}

// 这个 Logger::Impl::flush以及sync 都是调用对应的appender的flush和sync
// 所以要看appenders是否为空，为空就直接使用主日志器（主日志器会有一个默认的终端appender
void Logger::Impl::flush()
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    if(m_appenders.empty())
    {
        lock.unlock();
        if(m_root != nullptr)
        {
            m_root->flush();
        }
        return;
    }
    
    for(const AppenderPtr& appender : m_appenders)
    {
        appender->flush();
    }
}

void Logger::Impl::sync()
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    if(m_appenders.empty())
    {
        lock.unlock();
        if(m_root != nullptr)
        {
            m_root->sync();
        }
        return;
    }

    for(const AppenderPtr& appender : m_appenders)
    {
        appender->sync();
    }
}

void Logger::Impl::setRoot(const LoggerPtr& root) noexcept
{
    m_root = root;
}

void detail::LoggerAccess::Submit(Logger& logger, const LogRecordView& record) noexcept
{
    logger.m_impl->submit(record);
}

void detail::LoggerAccess::SetRoot(Logger& logger, const LoggerPtr& root) noexcept
{
    logger.m_impl->setRoot(root);
}

class LoggerManager final
{
public:
    LoggerManager()//这个主日志器初始化的时候，也就会有一个默认的pattern去构造格式化器
        : m_root(std::make_shared<Logger>("root"))
    {//这个就是使用的 Appender 类的对外提供的接口
        m_root->addAppender(MakeStdoutAppender());//你看主的日志器，就会有一个默认的 appender 嘛
        m_loggers.emplace("root", m_root);
    }

    [[nodiscard]] LoggerPtr getRoot() const noexcept
    {
        return m_root;
    }

    [[nodiscard]] LoggerPtr getLogger(std::string_view name)
    {
        ASSERT_RETVAL2(!name.empty(), nullptr, "logger name cannot be empty");
        std::lock_guard<std::mutex> lock(m_mutex);
        const auto iterator = m_loggers.find(std::string(name));
        if(iterator != m_loggers.end())
        {
            return iterator->second;
        }
        //保底机制，如果没有对应名字的话，那就肯定不是root
        //那就自己创建一个，然后设置对应的主日志器为这个manager的RootPtr
        auto logger = std::make_shared<Logger>(std::string(name));
        detail::LoggerAccess::SetRoot(*logger, m_root);
        m_loggers.emplace(logger->getName(), logger);
        return logger;
    }

private:
    mutable std::mutex m_mutex;//
    std::unordered_map<std::string, LoggerPtr> m_loggers;
    LoggerPtr m_root;
};

LoggerManager& GetLoggerManager()
{
    static LoggerManager mgr;
    return mgr;
}

LoggerPtr GetRootLogger()
{
    return GetLoggerManager().getRoot();
}

LoggerPtr GetLogger(std::string_view name)
{
    return GetLoggerManager().getLogger(name);
}

}// namespace pans