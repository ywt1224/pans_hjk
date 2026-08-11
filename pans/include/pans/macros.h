#ifndef PANS_INCLUDE_PANS_MACROS_H //这个就是一般定义为相对位置+文件名
#define PANS_INCLUDE_PANS_MACROS_H

#include <cassert>
#include <cstdint>
#include <iostream>

//这里是定义宏函数，为什么要是这种写法和普通函数作用域的不同？
//o,我知道了，我们定义宏函数的用法之前就是写成一行，
//#define 后面就是跟着对应的宏函数 后面就是对应的函数处理，这应该是为了支持多行？
#define PANS_ASSERT(x) \
    if(!(x)) [[unlikely]]\
    { \
        std::cerr << __FILE__ << ":"\
                  << __LINE__ <<" ASSERT  FAILED: "\
                  << #x\
                  << "\nStacktrace: to do \n"; \
        assert(x);\
    }

#define PANS_ASSERT2(x,w) \
    if(!(x))\
    {\
        std::cerr << __FILE__ << ":"\
                  << __LINE__ << " Assert "\
                  << #x << " failed. ["   \
                  << w  << "].\nStacktrace: to do \n";    \
        assert(x);\
    }

//为什么，这里要是循环呢，是do我能理解？
#define ASSERT_RETVAL(x,val) \
    do{\
        if(x)[[likely]] {break;}\
        PANS_ASSERT(x);\
        return val;\
    }while (0)
    
#define ASSERT_RETVAL2(x,val,info)\
    do{\
        if(x)[[likely]] {break;}\
        PANS_ASSERT2(x,info);\
        return val;\
    }while(0)

#define ASSERT_RETNONE(x)\
    do{\
        if(x)[[likely]] {break;}\
        PANS_ASSERT(x);\
        return;\
    }while(0)

#define ASSERT_RETNONE2(x,info)\
    do{\
        if(x)[[likely]] {break;}\
        PANS_ASSERT2(x,info);\
        return;\
    }while(0)

#define ASSERT_NOEFFECT(x)\
    do{\
        if(x)[[likely]] {break;}\
        PANS_ASSERT(x);\
    }while(0)

#define ASSERT_NOEFFECT2(x,info)\
    do{\
        if(x)[[likely]] {break;}\
        PANS_ASSERT2(x,info);\
    }while(0)

#define ASSERT_CONTINUE(x)\
    if(!(x)) [[unlikely]]\
    {\
        PANS_ASSERT(x);\
        continue;\
    }else{}

#define ASSERT_CONTINUE2(x,info)\
    if(!(x)) [[unlikely]]\
    {\
        PANS_ASSERT2(x,info);\
        continue;\
    }

#define ASSERT_BREAK(x)\
    if(!(x)) [[unlikely]]{\
        PANS_ASSERT(x);\
        break;\
    }else{}

#define ASSERT_BREAK2(x, info)\
    if(!(x)) [[unlikely]]{\
        PANS_ASSERT2(x, info);\
        break;\
    }else{}

//宏变量后面的这是个啥
#define INVALID64 (~0ULL)
#define INVALID32 0xFFFFFFFF
#define INVALID16 0xFFFF
#define INVALID8   0xFF

#define MAX_U8    0xFF
#define MAX_U16   0xFFFF
#define MAX_U32   0xFFFFFFFF
#define MAX_U64   (~0ULL)

//xia面这些就来自 <cstdint> 这个头文件
using u8 = uint8_t;
using s8 = int8_t;
using u16 = uint16_t;
using s16 = int16_t;
using u32 = uint32_t;
using s32 = int32_t;
using u64 = uint64_t;
using s64 = int64_t;

#endif
