#ifndef PANS_SRC_LOGGER_BUFFER_H
#define PANS_SRC_LOGGER_BUFFER_H

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <streambuf>
#include <string_view>
#include <vector>

namespace pans::detail {
// 自定义一个日志缓冲区，针对高频、小数据量的日志场景，在栈上分配日志缓冲区，遇超长日志，再在堆上分配内存
template <std::size_t INLINE_CAPACITY>//栈和堆的分配在哪里呢？
class InlineBuffer
{
public:
    void append(const char* data,std::size_t size)//传入需要拷入的数据源和大小
    {
        if (size == 0)//要再好一点，可以判断负值和打印错误信息之类的
        {
            return;
        }
        
        if(m_overflow.empty() && m_size + size <= INLINE_CAPACITY)
        {
            std::memcpy(m_inline.data() + m_size, data, size);
            m_size += size;
            return;      
        }
        //能够进入后续的逻辑就代表打破了之前的其中一个条件
        //比如第一次超过预定大小，这就要把原有的m_inline的数据拷贝过来
        //后续再拷贝时，就直接把新数据拷贝到m_overflow
        if(m_overflow.empty())//这个应该就是解决1第一次超过预设大小的时候吧
        {
            const std::size_t required_capacity = m_size + size;
            m_overflow.reserve(std::max(INLINE_CAPACITY * 2, required_capacity));
            m_overflow.insert(m_overflow.end(), m_inline.data(), m_inline.data() + m_size);
        }        

        m_overflow.insert(m_overflow.end(), data, data + size);
        m_size = m_overflow.size();
    }

    void append(std::string_view value)
    {
        append(value.data(),value.size());
    }

    void append(char value)
    {
        append(&value,1);
    }

    [[nodiscard]] const char* data() const noexcept
    {
        return m_overflow.empty() ? m_inline.data() : m_overflow.data();
    }

    [[nodiscard]] std::size_t size() const noexcept
    {
        return m_size;
    }

    [[nodiscard]] std::string_view view() const noexcept
    {
        return {data(), size()};
    }
    
private:
    std::array<char, INLINE_CAPACITY> m_inline{};//小数据量日志
    std::vector<char> m_overflow;//用于存储
    std::size_t m_size = 0;// 当前缓冲区大小
};

//这里为什么是设置成成员，而不直接用InlineBuffer去继承streambuf？然后都实现这些接口捏？
//这应该就是两个独立的类，我们都希望能用到这两个类，而不是只用SmallStreamBuffer
// 继承std::streambuf是为了能够能够继续使用流式输入接口，兼容ostream生态
//这个类的作用，现在还看的不是很明白，其实也就是看一下重载的两个函数有什么作用
template <std::size_t INLINE_CAPACITY>
class SmallStreamBuffer final : public std::streambuf
{
public:
    explicit SmallStreamBuffer(InlineBuffer<INLINE_CAPACITY>& buffer) noexcept
        : m_buffer(buffer)
    {
    }
protected://坏了，这个关键字的作用都忘了
    std::streamsize xsputn(const char* data, std::streamsize size) override
    {//这个函数就是重载往里写数据，嗯...应该就是取消ostringstream的只有堆上分配？
        if(size <= 0)
        {
            return 0;
        }

        m_buffer.append(data, static_cast<std::size_t>(size));
        return size;
    }

    int_type overflow(int_type character) override
    {//这个函数的逻辑能看个大概，就是不知道有啥用,还有就是 traits_type 这一类到底有啥作用做个拓展视野吧
        // overflow，除了可以接受一般字符，还可能会接收流终止符EOF
        // 如果我们遇到了EOF, 把它转成非EOF值向上报告写入成功，实际上不向Buffer里写入任何值。
        if(traits_type::eq_int_type(character,traits_type::eof()))
        {
            return traits_type::not_eof(character);
        }
        m_buffer.append(traits_type::to_char_type(character));
        return character;
    }


private:
    InlineBuffer<INLINE_CAPACITY>& m_buffer;
};


} // namespace pans::detail

#endif // PANS_SRC_LOGGER_BUFFER_H