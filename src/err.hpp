#pragma once

#include <format>
#include <iostream>
#include <print>
#include <source_location>

namespace bor {

/// Exits with the given status code.
[[noreturn]]
void panic(int status = EXIT_FAILURE) noexcept;

/// Prints the given source location and error message and exits with erroneus
/// status code.
template<typename... Args>
[[noreturn]]
void panic(
    std::source_location loc, std::format_string<Args...> fmt, Args &&...args
) noexcept {
    try {
        std::print(
            std::cerr,
            "{}:{}:{}: \x1b[91mborrow violation:\x1b[0m ",
            loc.file_name(),
            loc.line(),
            loc.column()
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
    bool cond,
    std::source_location loc,
    std::format_string<Args...> fmt,
    Args &&...args
) noexcept {
    if (cond) {
        panic(loc, fmt, args...);
    }
}

} // namespace bor