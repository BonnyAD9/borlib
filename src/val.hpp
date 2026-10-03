#pragma once

#include <format>
#include <type_traits>
#include <utility>

#include "defines.hpp"
#include "ref.hpp"
#include "ref_mut.hpp"
#include "val_inner.hpp"

namespace bor {

/// Value wrapper for checking borrowing rules.
///
/// This wrapper checks that the following is always true when using safe
/// references:
/// - There exists at most one mutable reference.
/// - Mutable and immutable reference cannot exist at the same time.
/// - Value to which exists mutable reference cannot accessed.
/// - Value to which exists immutable reference cannot be modified.
/// - Every reference refering to a value refers to a valid value.
template<typename T> class Val {
    detail::ValInner<T> _v;

public:
#if BOR_CHECK
    /// Move constructor of the value.
    ///
    /// Panics if there is any reference to the value.
    Val(Val<T> &&other,
        detail::SourcePos loc = detail::SourcePos::current()) noexcept
        requires(std::is_move_constructible_v<T>)
        : _v(std::move(other._v), loc.inc()) { }

    /// Copy constructor for the value.
    ///
    /// Panics if there is any mutable reference to the value.
    constexpr Val(
        const Val<T> &other,
        detail::SourcePos loc = detail::SourcePos::current()
    ) noexcept
        requires(std::is_copy_constructible_v<T>)
        : _v(other._v, loc.inc()) { }

    /// Move assignment operator for the value.
    ///
    /// Panics if there is any reference to this or the other value.
    Val<T> &operator=(Val<T> &&other) noexcept
        requires(std::is_move_assignable_v<T>)
    {
        set(std::move(other), detail::SourcePos::caller2());
        return *this;
    }

    /// Copy assignment operator for the value.
    ///
    /// Panics if there is any reference to this value or any mutable reference
    /// to the other value.
    Val<T> &operator=(const Val<T> &other) noexcept
        requires(std::is_copy_assignable_v<T>)
    {
        set(other, detail::SourcePos::caller2());
        return *this;
    }
#else  // BOR_CHECK
    Val(Val<T> &&) = default;
    Val(const Val<T> &) = default;
    Val<T> &operator=(Val<T> &&) = default;
    Val<T> &operator=(const Val<T> &) = default;
#endif // BOR_CHECK

    /// Construct the value.
    template<typename... Args>
    constexpr Val(Args... args)
        requires(std::is_constructible_v<detail::ValInner<T>, Args...>)
        : _v(std::forward<Args>(args)...) { }

    /// Assign to this by moving value.
    ///
    /// Panics if there is any reference to this value.
    Val<T> &operator=(T &&other) noexcept
        requires(std::is_move_assignable_v<T>)
    {
        set(std::move(other) BOR(, detail::SourcePos::caller2()));
        return *this;
    }

    /// Assign to this.
    ///
    /// Panics if there is any reference tothis value.
    Val<T> &operator=(const T &other) noexcept
        requires(std::is_copy_assignable_v<T>)
    {
        set(other BOR(, detail::SourcePos::caller2()));
        return *this;
    }

    /// Move the value.
    ///
    /// Panics if there is any reference to this or the other value.
    void set(
        Val<T> &&other
            BOR(, detail::SourcePos loc = detail::SourcePos::current())
    ) noexcept
        requires(std::is_move_assignable_v<T>)
    {
        _v.set(std::move(other._v) BOR(, loc.inc()));
    }

    /// Copy the value.
    ///
    /// Panics if there is any reference to this value or any mutable reference
    /// to the other value.
    void set(
        const Val<T> &other
            BOR(, detail::SourcePos loc = detail::SourcePos::current())
    ) noexcept
        requires(std::is_copy_assignable_v<T>)
    {
        _v.set(other._v BOR(, loc.inc()));
    }

    /// Move the value.
    ///
    /// Panics if there is any reference to this value.
    void set(
        T &&other BOR(, detail::SourcePos loc = detail::SourcePos::current())
    ) noexcept
        requires(std::is_move_assignable_v<T>)
    {
        get(BOR(loc.inc())) = std::move(other);
    }

    /// Copy the value.
    ///
    /// Panics if there is any reference to this value.
    void set(
        const T &other
            BOR(, detail::SourcePos loc = detail::SourcePos::current())
    ) noexcept
        requires(std::is_copy_assignable_v<T>)
    {
        get(BOR(loc.inc())) = other;
    }

    /// Get the value immutably.
    ///
    /// Panics if there is any mutable reference to this value.
    [[nodiscard]]
    const T &get(
        BOR(detail::SourcePos loc = detail::SourcePos::current())
    ) const noexcept {
#if BOR_CHECK
        _v.cnts.check_read(BOR(loc.inc()));
#endif
        return _v.value;
    }

    /// Get the value mutably.
    ///
    /// Panics if there is any reference to the value.
    T &get_mut(
        BOR(detail::SourcePos loc = detail::SourcePos::current())
    ) noexcept {
#if BOR_CHECK
        _v.cnts.check_write(BOR(loc.inc()));
#endif
        return _v.value;
    }

    /// Get the value.
    ///
    /// Panics if there is any mutable reference to the value.
    operator const T &() const noexcept {
        return get(BOR(detail::SourcePos::caller2()));
    }

    /// Get reference to the value.
    ///
    /// Panics if there is any mutable refererence to the value.
    Ref<T> ref(
        BOR(detail::SourcePos loc = detail::SourcePos::current())
    ) noexcept {
        return { _v BOR(, loc.inc()) };
    }

    /// Get mutable reference to the value.
    ///
    /// Panics if there is any reference to the value.
    RefMut<T> ref_mut(
        BOR(detail::SourcePos loc = detail::SourcePos::current())
    ) noexcept {
        return { _v BOR(, loc.inc()) };
    }

    /// Get reference to the value.
    ///
    /// Panics if there is any reference to the value.
    Ref<T> operator&() noexcept {
        return ref(BOR(detail::SourcePos::caller2()));
    }

#if BOR_CHECK
    /// Destroy the value.
    ///
    /// Panics if there is any reference to the value.
    ~Val() { _v.cnts.check_destroy(detail::SourcePos::caller2()); }
#else
    ~Val() = default;
#endif
};

} // namespace bor

// NOLINTNEXTLINE(bugprone-std-namespace-modification)
template<typename T> struct std::formatter<bor::Val<T>> {
    constexpr auto parse(format_parse_context &ctx) { return _fmt.parse(ctx); }

    auto format(const bor::Val<T> &v, format_context &ctx) const {
        return _fmt.format(v.get(), ctx);
    }

private:
    formatter<T> _fmt;
};
