#pragma once

#include <cstdlib>

#include "defines.hpp"

BOR() // Disables warning on unused "defines.hpp"

#if BOR_CHECK
# include <format>
# include <iostream>
# include <print>

# include "source_pos.hpp"
#endif

namespace bor {

/// Exits with the given status code.
[[noreturn]]
void panic(int status = EXIT_FAILURE) noexcept;

#if BOR_CHECK
/// Prints the given source location and error message and exits with erroneus
/// status code.
template<typename... Args>
[[noreturn]]
void panic(
    SourcePos loc, std::format_string<Args...> fmt, Args &&...args
) noexcept {
    try {
        std::print(
            std::cerr,
            "{}:{}:{}: \x1b[91mborrow violation:\x1b[0m ",
            loc.file(),
            loc.line(),
            loc.col()
        );
        std::println(std::cerr, fmt, args...);
        // NOLINTNEXTLINE(bugprone-empty-catch)
    } catch (...) {
    }
    panic();
}

/// If `cond` is false, prints the given source location and error message and
/// exits with erroneus status code.
template<typename... Args>
void panic_if(
    bool cond, SourcePos loc, std::format_string<Args...> fmt, Args &&...args
) noexcept {
    if (cond) {
        panic(loc.inc(), fmt, args...);
    }
}

#endif // BOR_CHECK

} // namespace bor