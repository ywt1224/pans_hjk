#ifndef PANS_SRC_LOGGER_FORMATTER_H
#define PANS_SRC_LOGGER_FORMATTER_H

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "logger/buffer.h"
#include "logger/buffer_config.h"
#include "logger/log_record.h"

namespace pans::detail
{

using FormattedRecordBuffer = InlineBuffer<FORMATTED_RECORD_INLINE_CAPACITY>;
/*
    这里就是一个大的格式化器，把传入的日志信息结构体，
    按照pattern转化成完成的日志，然后通过输出参数 FormattedRecordBuffer
    传出去，内部就是调用各种类型的 FormatItem 去进行输出
    所以在构造的时候，会根据pattern，创建对应的 FormatItem
    保存在这里，有这个思路就好很多了
*/
class Formatter final
{
public:
    class FormatItem //就是一个父类指针，后面会多态成对应类型的子类 FormatItem 
    {
    public:
        virtual ~FormatItem() = default;//这个虚析构肯定是要的啦，虽然好像子类也没有什么需要额外释放的内存
        virtual void format(const LogRecordView& record,FormattedRecordBuffer& output) const = 0;
    };

    explicit Formatter(std::string_view pattern);
    void format(const LogRecordView& record,FormattedRecordBuffer& output) const;
    
    [[nodiscard]] const std::string& getPattern() const noexcept{return m_pattern;};
private:
    int parse();//提供给构造格式化器的内部使用，返回值代表成功与否
    void addLiteral(std::string& literal);//就是创建字面量对应的 FormatItem （所以这个名字不是特别好）
private:
    std::string m_pattern;//存储外部传入的日志格式
    std::vector<std::unique_ptr<FormatItem>> m_items;//就是解析格式时对应的FormatItem处理器
};
} // namespace pans::detail


#endif //PANS_SRC_LOGGER_FORMATTER_H