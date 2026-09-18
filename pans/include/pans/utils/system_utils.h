#ifndef PANS_INCLUDE_PANS_UTILS_SYSTEM_UTILS_H
#define PANS_INCLUDE_PANS_UTILS_SYSTEM_UTILS_H

#include <chrono>
#include <pans/macros.h>
#include <pans/export.h>

namespace pans{
// 获取起服至此的时间
[[nodiscard]] PANS_API std::chrono::steady_clock::duration GetElapsedTime() noexcept;
// 获取协程ID TODO
[[nodiscard]] PANS_API u64 GetFiberId() noexcept;
// 获取函数调用栈 TODO
[[nodiscard]] PANS_API std::string GetBacktrace(int size = 20, int skip = 1, const std::string& prefix = "");

}//namespace pans 

#endif // PANS_INCLUDE_PANS_UTILS_SYSTEM_UTILS_H