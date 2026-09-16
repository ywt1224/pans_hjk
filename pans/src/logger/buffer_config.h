#ifndef PANS_SRC_LOGGER_BUFFER_CONFIG_H
#define PANS_SRC_LOGGER_BUFFER_CONFIG_H

#include <cstddef>

namespace pans::detail {//相当于pans命名空间里面再套了一层detail

inline constexpr std::size_t LOG_MESSAGE_INLINE_CAPACITY = 128;

inline constexpr std::size_t PRINTF_FORMAT_INLINE_CAPACITY = 128;

inline constexpr std::size_t FORMATTED_RECORD_INLINE_CAPACITY = 256;

}// namespace pans::detail

#endif // PANS_SRC_LOGGER_BUFFER_CONFIG_H