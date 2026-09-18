#include <array>
#include <cstdarg>
#include <cstdio>
#include <memory>
#include <new>
#include <ostream>
#include <vector>

#include <pans/logger/log.h>
#include <pans/utils/system_utils.h>
#include <pans/utils/thread_utils.h>

#include "logger/buffer.h"
#include "logger/buffer_config.h"
#include "logger/logger_impl.h"
#include "logger/log_record.h"

namespace pans::detail {

struct LogLine::Impl final
{// 你看成员变量就知道这个结构体的作用了
    Impl(Logger& logger, LogLevel::Level level, u32 line, std::string_view file_name)
        : m_logger(logger)//所以构造这个结构体的时候，也就没那个
        , m_level(level)
        , m_line(line)
        , m_fileName(file_name)
        , m_timestamp(std::chrono::system_clock::now())
        , m_elapsed(GetElapsedTime())
        , m_threadId(GetThreadId())
        , m_fiberId(GetFiberId())
        , m_threadName(GetThreadName())
        , m_streamBuffer(m_inlineBuffer)
        , m_stream(&m_streamBuffer)// 这个的构造方式就和我们之前用的是一样的了
    {}

    [[nodiscard]] LogRecordView getRecord() const noexcept
    {
        return {
            m_level,
            m_logger.getName(),
            m_inlineBuffer.view(),
            m_timestamp,
            m_elapsed,
            m_threadId,
            m_fiberId,
            m_threadName,
            m_fileName,
            m_line,
        };
    }

    Logger& m_logger;
    LogLevel::Level m_level;
    u32 m_line = 0;
    std::string_view m_fileName;
    std::chrono::system_clock::time_point m_timestamp;
    std::chrono::steady_clock::duration m_elapsed;
    u64 m_threadId = 0;
    u64 m_fiberId = 0;
    std::string_view m_threadName;
    InlineBuffer<LOG_MESSAGE_INLINE_CAPACITY> m_inlineBuffer;
    SmallStreamBuffer<LOG_MESSAGE_INLINE_CAPACITY> m_streamBuffer;
    std::ostream m_stream;// 为了支持流式输出
};

LogLine::LogLine(Logger& logger, LogLevel::Level level, u32 line, std::string_view file_name)
{
    static_assert(sizeof(Impl) <= LOG_LINE_IMPL_SIZE, "LogLine inline implementation storage is too small");
    static_assert(alignof(Impl) <= alignof(std::max_align_t), "LogLine implementation requires excessive alignment");
    // 所以alignas(std::max_align_t)就是为了能保证 reinterpret_cast<Impl*> 得到的地址满足 Impl 的对齐要求。
    std::construct_at(reinterpret_cast<Impl*>(m_implStorage), logger, level, line, file_name);// 你看果然就是用来构造的，可是为什么要这样呢
}

LogLine::Impl& LogLine::getImpl() noexcept
{//std::launder 用来告诉编译器：“这块内存现在真的是一个 Impl 对象，请按 Impl 的规则来访问”。
    return *std::launder(reinterpret_cast<Impl*>(m_implStorage));
}

LogLine::~LogLine() noexcept
{//析构时，输出所有的日志
    Impl& impl = getImpl();
    LoggerAccess::Submit(impl.m_logger, impl.getRecord());
    std::destroy_at(&impl);
}

std::ostream& LogLine::stream() noexcept
{// LogLine没有 Impl m_impl; 这样的成员。
//所以 m_impl.m_stream 只能通过getImpl() 简洁得到。
    return getImpl().m_stream;
}

void LogPrintf(Logger& logger, LogLevel::Level level, u32 line, std::string_view file_name, const char* format, ...)
{
    LogLine log_line(logger, level, line, file_name);
    if(format == nullptr)
    {//格式为空 返回，然后 log_line 析构，提交带有 "<null-format>" 的日志
        log_line.stream() << "<null-format>";
        return;
    }

    std::array<char, PRINTF_FORMAT_INLINE_CAPACITY> inline_buffer;

    va_list arguments;
    va_list arguments_copy; // 访问可变参数会破坏参数指针偏移量，为了能再次访问可变参数包，必须拷贝一份
    va_start(arguments, format);// 这个是啥意思
    va_copy(arguments_copy, arguments);//那这个是拷贝动作，
    const int required_size = std::vsnprintf(inline_buffer.data(), inline_buffer.size(), format, arguments);
    va_end(arguments);//尝试获取 模板 对应的日志的长度，如果符合inline_buffer的长度，就可以直接输出
    //如果超过了 PRINTF_FORMAT_INLINE_CAPACITY 那这个inline_buffer就不全，需要重新拷贝

    if(required_size < 0)
    {//格式化失败，
        va_end(arguments_copy);
        log_line.stream() << "<format-error>";
        return;
    }
    
    const std::size_t message_size = static_cast<std::size_t>(required_size);
    if(message_size < inline_buffer.size())
    {//符合inline_buffer的长度，就可以直接输出
        va_end(arguments_copy);
        log_line.stream().write(inline_buffer.data(), static_cast<std::streamsize>(message_size));
        return;
    }
    //实际长度超过了 PRINTF_FORMAT_INLINE_CAPACITY 那这个inline_buffer就不全，需要重新拷贝
    std::vector<char> overflow_buffer(message_size + 1);
    const int second_result = std::vsnprintf(overflow_buffer.data(), overflow_buffer.size(), format, arguments_copy);
    va_end(arguments_copy);
    if(second_result < 0)
    {
        log_line.stream() << "<format-error>";
        return;
    }
    
    log_line.stream().write(overflow_buffer.data(), static_cast<std::streamsize>(message_size));
    // 到这里还只是在内存里格式化这条日志, 出了这个函数，log_line对象析构，才把日志从内存提交到logger的appender中去
}

}// namespace pans::detail