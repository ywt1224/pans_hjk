#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>
#include <mutex>
#include <sstream>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <dbghelp.h>

#else
#include <cxxabi.h>
// #include <elfutils/libdwfl.h> //这个应该是引入的第三方库，这里只实现获取起服时间
#include <execinfo.h>
#include <unistd.h>
#endif

#include <pans/utils/system_utils.h>

namespace pans {

std::chrono::steady_clock::duration GetElapsedTime() noexcept
{
    static const auto START_TIME = std::chrono::steady_clock::now();
    return std::chrono::steady_clock::now() - START_TIME;
}

u64 GetFiberId() noexcept
{
    return 0;
}


}// namespace pans
