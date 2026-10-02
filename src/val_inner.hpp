#pragma once

#include "counters.hpp"
namespace bor {

template<typename T> struct ValInner {
    Counters cnts;
    T value;

    ValInner(ValInner<T> &&) =
        delete ("Use constructor with source location.");
    ValInner(const ValInner<T> &) =
        delete ("Use constructor with source location.");
    ValInner<T> &operator=(ValInner<T> &&) = delete ("Use `set`.");
    ValInner<T> &operator=(const ValInner<T> &) = delete ("Use `set`.");

    ValInner() = default;
    ValInner(T value) : value(value) { }

    ValInner(ValInner<T> &&other, std::source_location loc) noexcept
        requires(std::is_move_constructible_v<T>)
        :
        cnts(Counters::move_out(other.cnts, loc)),
        value(std::move(other.value)) { }

    ValInner(const ValInner<T> &other, std::source_location loc) noexcept
        requires(std::is_copy_constructible_v<T>)
        : cnts(Counters::copy_out(other.cnts, loc)), value(other.value) { }

    void set(ValInner<T> &&other, std::source_location loc) noexcept
        requires(std::is_move_assignable_v<T>)
    {
        other.cnts.check_move_out(loc);
        cnts.check_write(loc);
        value = std::move(other.value);
    }

    void set(const ValInner<T> &other, std::source_location loc) noexcept
        requires(std::is_copy_assignable_v<T>)
    {
        other.cnts.check_copy_out(loc);
        cnts.check_write(loc);
        value = other.value;
    }

    ~ValInner() { cnts.check_destroy(std::source_location::current()); }
};

} // namespace bor