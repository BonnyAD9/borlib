#pragma once

#include <type_traits>
#include <utility>

#include "defines.hpp"

#if BOR_CHECK
# include "counters.hpp"
#endif

namespace bor {

/// Inner representation of the value.
template<typename T> struct ValInner {
#if BOR_CHECK
    Counters cnts;
#endif
    T value;

#if BOR_CHECK
    ValInner(ValInner<T> &&) =
        delete ("Use constructor with source location.");
    ValInner(const ValInner<T> &) =
        delete ("Use constructor with source location.");
    ValInner<T> &operator=(ValInner<T> &&) = delete ("Use `set`.");
    ValInner<T> &operator=(const ValInner<T> &) = delete ("Use `set`.");
#else
    ValInner(ValInner<T> &&) = default;
    ValInner(const ValInner<T> &) = default;
    ValInner<T> &operator=(ValInner<T> &&) = default;
    ValInner<T> &operator=(const ValInner<T> &) = default;
#endif

    /// Move the value.
    ///
    /// Panics if there is any reference to the value.
    ValInner(ValInner<T> &&other BOR(, SourcePos loc)) noexcept
        requires(std::is_move_constructible_v<T>)
        :
        BOR(
            cnts(Counters::move_out(other.cnts, loc.inc())),
        ) value(std::move(other.value)) { }

    /// Copy the value.
    ///
    /// Panics if there is any mutable reference to the value.
    ValInner(const ValInner<T> &other BOR(, SourcePos loc)) noexcept
        requires(std::is_copy_constructible_v<T>)
        :
        BOR(
            cnts(Counters::copy_out(other.cnts, loc.inc())),
        ) value(other.value) { }

    /// Emplace the value.
    template<typename... Args>
    constexpr ValInner(Args... args)
        requires(std::is_constructible_v<T, Args...>)
        : value(std::forward<Args>(args)...) { }

    /// Move the value.
    ///
    /// Panics if there is any reference to this or other.
    void set(ValInner<T> &&other BOR(, SourcePos loc)) noexcept
        requires(std::is_move_assignable_v<T>)
    {
#if BOR_CHECK
        other.cnts.check_move_out(loc.inc());
        cnts.check_write(loc.inc());
#endif
        value = std::move(other.value);
    }

    /// Copy the value.
    ///
    /// Panics if there is any reference to this or mutable reference to other.
    void set(const ValInner<T> &other BOR(, SourcePos loc)) noexcept
        requires(std::is_copy_assignable_v<T>)
    {
#if BOR_CHECK
        other.cnts.check_copy_out(loc.inc());
        cnts.check_write(loc.inc());
#endif
        value = other.value;
    }
};

} // namespace bor