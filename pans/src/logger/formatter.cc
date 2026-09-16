#include "logger/formatter.h"
#include <array>
#include <charconv>
#include <ctime>
#include <unordered_map>

#include <pans/macros.h>

/*
    这部分的实现思路，完全就可以按照具体的处理流程来嘛
    就是格式化器的构造，然后内部实现解析 pattern，
    初始化对应的这个 FormatItem 
    然后就是格式化器的执行 format （这里其实就是调用拥有的FormatItem 
                                去处理对应的日志结构体里对应的字段）
*/

namespace pans::detail
{
//默认的时间格式，在时间格式的 FormatItem 里会用到
constexpr std::string_view DEFAULT_DATE_FORMAT = "%Y-%m-%d %H:%M:%S";

Formatter::Formatter(std::string_view pattern)
    :m_pattern(pattern)//存储外部传入的日志格式，然后解析，创建对应的子日志格式处理器
{
    if(parse() != 0)
    {
        throw std::invalid_argument("invalid logger format pattern");
    }
}

void Formatter::format(const LogRecordView& record, FormattedRecordBuffer& output) const
{
    for(const auto& item : m_items)
    {
        item->format(record, output);
    }
}

template <typename T>//这个主要就是添加数字到缓冲区的时候用，像线程ID，行号，协程ID这些
void AppendInteger(FormattedRecordBuffer& output, T value)
{//这个to_chars 就是把数字转成字符串，返回类型是 std::to_chars_result ，只代表转换的结果
    std::array<char, 24> buffer{};//这个函数前两个参数，就是输出字符串的缓冲区区域，左闭右开
    const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value); // 基本上不会失败，转失败了也没关系
    ASSERT_RETNONE2(result.ec == std::errc(), "trans " << value << " to chars failed.");
    output.append(buffer.data(), static_cast<std::size_t>(result.ptr - buffer.data()));
}

class LiteralFormatItem final : public Formatter::FormatItem
{//这个构造函数设计的是真好，就是外部不管传左值还是右值
public://传右值就是触发 std::string 移动构造，然后再拿移动构造 让成员变量接管
    explicit LiteralFormatItem(std::string value)
        : m_value(std::move(value))
    {}
    void format(const LogRecordView&, FormattedRecordBuffer& output) const override
    {
        output.append(m_value);
    }
private:
    std::string m_value;
};

class MessageFormatItem final : public Formatter::FormatItem
{
public:
    void format(const LogRecordView& record, FormattedRecordBuffer& output) const override
    {
        output.append(record.m_message);
    }
};

class LevelFormatItem final : public Formatter::FormatItem
{
public:
    void format(const LogRecordView& record, FormattedRecordBuffer& output) const override
    {
        output.append(LogLevel::ToString(record.m_level));
    }
};

class ElapsedFormatItem final : public Formatter::FormatItem
{
public:
    void format(const LogRecordView& record, FormattedRecordBuffer& output) const override
    {
        AppendInteger(output, std::chrono::duration_cast<std::chrono::milliseconds>(record.m_elapsed).count());
    }
};

class LoggerNameFormatItem final : public Formatter::FormatItem
{
public:
    void format(const LogRecordView& record, FormattedRecordBuffer& output) const override
    {
        output.append(record.m_loggerName);
    }
};

class ThreadIdFormatItem final : public Formatter::FormatItem
{
public:
    void format(const LogRecordView& record, FormattedRecordBuffer& output) const override
    {
        AppendInteger(output, record.m_threadId);
    }
};

class NewLineFormatItem final : public Formatter::FormatItem
{
public:
    void format(const LogRecordView&, FormattedRecordBuffer& output) const override
    {
        output.append('\n');
    }
};

//这个是处理 %d{...} 对应指令的 FormatItem 
//作用：处理 pattern 里的 %d 或 %d{...}，
//把 record.m_timestamp 格式化成人类可读的时间字符串，比如 2026-09-15 12:34:56。
class DateTimeFormatItem final : public Formatter::FormatItem
{
public:
    explicit DateTimeFormatItem(std::string_view format)
        : m_format(format.empty() ? DEFAULT_DATE_FORMAT : format)
    {}

    /**
     * 这个的设计思路，就是同一秒内的日志
     * 按照对应格式的精度，完全没有必要重新再去获取一次值
     * 因为就算获取一次，也是一样的，所以完全可以直接复用上一次
     * 同一秒的值，这就是这里的优化思路
     * 
     * 一点点小问题
     * 缓存是 thread_local，它是类共享的，线程中的多个对象是复用它的
     * 跨 DateTimeFormatItem 实例共享。
     * 如果同一线程里有多个不同 m_format 
     * 的 DateTimeFormatItem，
     * 同一秒内第二个会拿到第一个的缓存，输出串台。
     * 
     */
    void format(const LogRecordView& record, FormattedRecordBuffer& output) const override
    {
        // Warning: 同一个进程中的datetime format要一致，否则会串
        static thread_local time_t last_second = 0;//这几个线程级的变量就是为了配合localtime_r的线程安全使用
        static thread_local char cached_date_time[20] = {'\0'};    
        // 返回从 epoch（1970-01-01 00:00:00 UTC）到该时刻的 duration。
        const auto duration = record.m_timestamp.time_since_epoch();
        //转化成秒级，再转成 time_t 给后面的 localtime_r 用，就是现在只有秒，没有年月日这些东西
        const time_t current_second = static_cast<time_t>(std::chrono::duration_cast<std::chrono::seconds>(duration).count());
        //下面就是开始转化成对应格式，把秒数转成对应的年月日这些
        if(current_second != last_second)
        {
            std::tm buffer{};//输出参数，结构体含年、月、日、时、分、秒等字段。
#if defined(_WIN32)//所以这里只能转成结构体，还需要转成字符串
            const errno_t result = localtime_s(&buffer, &current_second);
            ASSERT_RETNONE2(result == 0, "failed to convert log time to local time");
#else
             /**
             * @brief localtime_r是线程安全的，但是消耗也大：
             * 1. 全局有锁可能排队
             * 2. 需要进行复杂的时区计算(尤其是有过夏令时变更历史的时区)
             * 3. 如果系统没有加载时区信息或者要求响应时区变更, 每次调用这个接口还要去发起磁盘IO，读系统文件/etc/localtime
             */
            const std::tm* result = localtime_r(&current_second, &buffer);
            ASSERT_RETNONE2(result != nullptr, "failed to convert log time to local time");
#endif
            const std::size_t size = std::strftime(cached_date_time, sizeof(cached_date_time), m_format.c_str(), &buffer);
            ASSERT_RETNONE2(size != 0, "failed to format log time");
            last_second = current_second;
        } 

        output.append(cached_date_time);
    }
private:
    std::string m_format; // 内部需要拷贝一份，保证使用时有效，保证 item 生命周期内有效。
};

//这个就是获取微妙数，就是处理  %d{%Y-%m-%d %H:%M:%S}.%u 这里就是用来处理 %u 的
//小数点后的数字，就已经是微秒单位了
/**
 * 思路其实挺简单的，就是把总时间转成微妙级别，
 * 然后获取秒内微秒（一秒内走了多少微秒，不足一秒的部分）
 * 所以肯定有个取模的步骤
 */
class MicrosecondsFormatItem final : public Formatter::FormatItem
{
public:
    void format(const LogRecordView& record, FormattedRecordBuffer& output) const override
    {
        const auto total_microseconds = std::chrono::duration_cast<std::chrono::microseconds>(record.m_timestamp.time_since_epoch()).count(); 
        s64 microseconds = total_microseconds % 1'000'000;
        if(microseconds < 0)//为什么是一百万捏，因为秒和微秒的换算就是这个
        {//最终得到的就是一秒内走过的微秒数，不足一秒的部分
            microseconds += 1'000'000;
        }
        //然后这里得到的是微秒数嘛，并且要配合秒的小数点去使用
        //那肯定要补足六位啊，比如5us，就是0.000005s嘛，
        //所以肯定要不足六位才能输出这个 微妙的格式化器到最后的缓冲区，这样才不会错
        std::array<char, MICROSECONDS_WIDTH> digits{};
        auto remaining = static_cast<u32>(microseconds);
        for(std::size_t index = digits.size(); index-- > 0;)
        {
            digits[index] = static_cast<char>('0' + remaining % 10);
            remaining /= 10;
        }
        output.append(digits.data(), digits.size());  
    }
private:
    static constexpr std::size_t MICROSECONDS_WIDTH = 6;   
};

class FileNameFormatItem final : public Formatter::FormatItem
{
public:
    void format(const LogRecordView& record, FormattedRecordBuffer& output) const override
    {
        output.append(record.m_fileName);
    }
};

class LineFormatItem final : public Formatter::FormatItem
{
public:
    void format(const LogRecordView& record, FormattedRecordBuffer& output) const override
    {
        AppendInteger(output, record.m_line);
    }
};

class TabFormatItem final : public Formatter::FormatItem
{
public:
    void format(const LogRecordView&, FormattedRecordBuffer& output) const override
    {
        output.append('\t');
    }
};

class FiberIdFormatItem final : public Formatter::FormatItem
{
public:
    void format(const LogRecordView& record, FormattedRecordBuffer& output) const override
    {
        AppendInteger(output, record.m_fiberId);
    }
};

class ThreadNameFormatItem final : public Formatter::FormatItem
{
public:
    void format(const LogRecordView& record, FormattedRecordBuffer& output) const override
    {
        output.append(record.m_threadName);
    }
};

// 定义创建 FormatItem 的函数指针，主要就是给后面的查表用
//还有就是不用这种查表的写法，怎么写，就是对应笔记的第二篇子文档
//然后就知道这样写的好处与规范了
using FormatItemFactory = std::unique_ptr<Formatter::FormatItem> (*)(std::string_view);

template <typename Item>
[[nodiscard]] std::unique_ptr<Formatter::FormatItem>
CreateSimpleFormatItem(std::string_view)
{
    return std::make_unique<Item>();
}

template <typename Item>//这个对应有配置的指令去构造 FormatItem
[[nodiscard]] std::unique_ptr<Formatter::FormatItem>
CreateConfiguredFormatItem(std::string_view format)
{
    return std::make_unique<Item>(format);
}

[[nodiscard]] const std::unordered_map<char, FormatItemFactory>& GetFormatItemFactories()
{
    static const std::unordered_map<char, FormatItemFactory> FORMAT_ITEM_FACTORIES{
        {'m', &CreateSimpleFormatItem<MessageFormatItem>},          // 日志内容
        {'p', &CreateSimpleFormatItem<LevelFormatItem>},            // LogLevel
        {'r', &CreateSimpleFormatItem<ElapsedFormatItem>},          // 起服已过时间
        {'c', &CreateSimpleFormatItem<LoggerNameFormatItem>},       // 日志器名称
        {'t', &CreateSimpleFormatItem<ThreadIdFormatItem>},         // 线程id
        {'n', &CreateSimpleFormatItem<NewLineFormatItem>},          // 换行
        {'d', &CreateConfiguredFormatItem<DateTimeFormatItem>},     // datetime
        {'u', &CreateSimpleFormatItem<MicrosecondsFormatItem>},     // 毫秒数
        {'f', &CreateSimpleFormatItem<FileNameFormatItem>},         // 文件名
        {'l', &CreateSimpleFormatItem<LineFormatItem>},             // 行号
        {'T', &CreateSimpleFormatItem<TabFormatItem>},              // 制表符
        {'F', &CreateSimpleFormatItem<FiberIdFormatItem>},          // 协程ID
        {'N', &CreateSimpleFormatItem<ThreadNameFormatItem>},       // 大N线程名
    };
    return FORMAT_ITEM_FACTORIES;
}

[[nodiscard]] std::unique_ptr<Formatter::FormatItem> CreateFormatItem(char directive, std::string_view format)
{//不加这种查表的写法，就直接返回对应的 std::unique_ptr<Formatter::FormatItem>就是
    //这里这样写也是为了更规范和更美观
    const auto& factories = GetFormatItemFactories();
    const auto it = factories.find(directive);
    ASSERT_RETVAL2(it != factories.end(), nullptr, "unknown logger format directive: " << directive);
    return it->second(format);
}

void Formatter::addLiteral(std::string& literal)
{
    if(literal.empty())
    {
        return;
    }
    m_items.push_back(std::make_unique<LiteralFormatItem>(std::move(literal)));
    literal.clear();
}

//这个更清晰的写法，可看笔记，主要就是 const char directive = m_pattern[++index];
//这一行不自增的化，就是 m_pattern[1+index]; 的写法，后面就要注意索引的更新
// %d{%Y-%m-%d %H:%M:%S}.%u%Tthread=%t%Tfiber=%F%T[%p]%T%f:%l%T%m%n
int Formatter::parse()
{
    std::string literal;
    for(std::size_t index = 0; index < m_pattern.size(); ++index)
    {
        if(m_pattern[index] != '%')
        {
            // 指令+配置会被下面的逻辑完整解析完，不走下面的逻辑，就只能是字面量
            literal.push_back(m_pattern[index]);
            // 有这个continue就说明，下面的逻辑也不需要被包裹
            continue;
        }

        // 走到这里说明遇到 '%'，这个分支会把指令或者转义都处理完
        ASSERT_RETVAL2(index + 1 < m_pattern.size(), -1, "logger format pattern ends with an incomplete directive");

        // 如果不是 m_pattern[++index];的写法，后面的写法就很冗余
        const char directive = m_pattern[++index];

        if(directive == '%')
        {
            // 这个分支就代表走的是转义 % 的逻辑
            literal.push_back('%');
            continue;
        }

        // 走出这个逻辑就代表真正开始有指令了，我们就要开始处理指令
        // 但是处理之前先要把累积的字面量输出（也就是构造对应字面量的FormatItem）
        addLiteral(literal);

        // 指令后面跟着配置（可能有
        std::string_view item_format;
        if(index + 1 < m_pattern.size() && m_pattern[index + 1] == '{')
        {
            const std::size_t closing_brace = m_pattern.find('}', index + 2);
            ASSERT_RETVAL2(closing_brace != std::string::npos && closing_brace != (index + 2), -2, "missing a closing brace or empty");

            // 这个就是获取可能的配置了
            item_format = std::string_view(m_pattern).substr(index + 2, closing_brace - index - 2);

            // 下一次就从配置开始（原版 index 指向 directive，这里跳到 closing_brace，下一轮 ++index 会从 '}' 后继续）
            index = closing_brace;
        }

        m_items.push_back(CreateFormatItem(directive, item_format));
    }
    
    // 最后的字面量，也要构造，这种情况就是末尾是字面量
    addLiteral(literal);
    
    // 检查是否有效
    for(const auto& item : m_items)
    {
        ASSERT_RETVAL2(item != nullptr, -3, "logger formatter contains a null format item");
    }

    return 0;
}

}// namespace pans::detail