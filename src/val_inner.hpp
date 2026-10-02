#pragma once

#include <type_traits>
#include <utility>

#include "counters.hpp"
namespace bor {

/// Inner representation of the value.
template<typename T> struct ValInner {
    Counters cnts;
    T value;

    ValInner(ValInner<T> &&) =
        delete ("Use constructor with source location.");
    ValInner(const ValInner<T> &) =
        delete ("Use constructor with source location.");
    ValInner<T> &operator=(ValInner<T> &&) = delete ("Use `set`.");
    ValInner<T> &operator=(const ValInner<T> &) = delete ("Use `set`.");

    /// Move the value.
    ///
    /// Panics if there is any reference to the value.
    ValInner(ValInner<T> &&other, std::source_location loc) noexcept
        requires(std::is_move_constructible_v<T>)
        :
        cnts(Counters::move_out(other.cnts, loc)),
        value(std::move(other.value)) { }

    /// Copy the value.
    ///
    /// Panics if there is any mutable reference to the value.
    ValInner(const ValInner<T> &other, std::source_location loc) noexcept
        requires(std::is_copy_constructible_v<T>)
        : cnts(Counters::copy_out(other.cnts, loc)), value(other.value) { }

    /// Emplace the value.
    template<typename... Args>
    constexpr ValInner(Args... args)
        requires(std::is_constructible_v<T, Args...>)
        : value(std::forward<Args>(args)...) { }

    /// Move the value.
    ///
    /// Panics if there is any reference to this or other.
    void set(ValInner<T> &&other, std::source_location loc) noexcept
        requires(std::is_move_assignable_v<T>)
    {
        other.cnts.check_move_out(loc);
        cnts.check_write(loc);
        value = std::move(other.value);
    }

    /// Copy the value.
    ///
    /// Panics if there is any reference to this or mutable reference to other.
    void set(const ValInner<T> &other, std::source_location loc) noexcept
        requires(std::is_copy_assignable_v<T>)
    {
        other.cnts.check_copy_out(loc);
        cnts.check_write(loc);
        value = other.value;
    }

    /// Destroy the value.
    ///
    /// Panics if there is any reference to the value.
    ~ValInner() { cnts.check_destroy(std::source_location::current()); }
};

} // namespace bor