#include <pans/logger/log_level.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <string>

namespace pans {
std::string_view 
LogLevel::ToString(Level level) noexcept
{
        switch (level)
        {
#define XX(name) case Level::LOG_LV_##name: return #name;//这种宏定义取消重复写法的思路，倒是可以学一下，挺好看的
        XX(DEBUG)
        XX(INFO)
        XX(WARN)
        XX(ERROR)
        XX(FATAL)
        XX(OFF)  
#undef XX  
        }
        return "UNKNOWN";
}

LogLevel::Level 
LogLevel::FromString(std::string_view value) noexcept
{
    //那这个就是默认只会传类似 DEBUG 之类的string_view
    std::array<char,6> normalized{};
    std::transform(value.begin(),value.end(),normalized.begin(),
    [](unsigned char character){
        return static_cast<char>(std::toupper(character));
    });
    const std::string_view upper_value(normalized.data(), value.size());
/*
    switch 的条件表达式只能是整型、枚举型或可隐式转换为整型的类型。
    字符串（std::string、
    std::string_view、const char*）不能作为 switch 的条件。
*/
#define XX(name) if(upper_value == #name) return Level::LOG_LV_##name;
    XX(DEBUG)
    XX(INFO)
    XX(WARN)
    XX(ERROR)
    XX(FATAL)
    XX(OFF)
#undef XX
    return Level::LOG_LV_OFF;
}
}// namespace pans 