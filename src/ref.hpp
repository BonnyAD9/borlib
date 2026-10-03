#pragma once

#include <format>

#include "defines.hpp"
#include "val_inner.hpp"

#if BOR_CHECK
# include "err.hpp"
#endif

namespace bor {

template<typename T> class RefMut;

/// Safe reference.
///
/// This reference checks that it is valid and that noone can modify the
/// referenced value while the reference exists.
template<typename T> class Ref {
    friend RefMut<T>;

    detail::ValInner<T> *_r{ nullptr };

    constexpr Ref(detail::ValInner<T> *val) noexcept : _r(val) { }

public:
    /// Create uninitialized reference.
    ///
    /// Any operation other that assigning value or destroying it will panic.
    constexpr Ref() = default;

    /// Internal method for constructing reference from innner value
    /// representation. Checks that creating the reference is valid and updates
    /// the counters.
    ///
    /// Panics if creating the reference is not valid.
    Ref(detail::ValInner<T> &val BOR(, detail::SourcePos loc)) noexcept :
        _r(&val) {
#if BOR_CHECK
        val.cnts.create_ref(loc.inc());
#endif
    }

#if BOR_CHECK
    /// Move constructor of reference. The other reference will be made
    /// uninitialized.
    ///
    /// Panics if the other reference is uninitialized.
    Ref(Ref<T> &&other,
        detail::SourcePos loc = detail::SourcePos::current()) noexcept :
        _r(other._r) {
        panic_if(!_r, loc.inc(), "Cannot move uninitialized reference.");
        other._r = nullptr;
    }

    /// Copy constructor of reference.
    ///
    /// Panics if the other reference is uninitialized.
    Ref(const Ref<T> &other,
        detail::SourcePos loc = detail::SourcePos::current()) noexcept :
        _r(other._r) {
        panic_if(!_r, loc.inc(), "Cannot copy uninitialized reference.");
        _r->cnts.add_ref();
    }

    /// Move assignment operator of reference. The other reference will be made
    /// uninitialized.
    ///
    /// Panics if the other reference is uninitialized.
    Ref<T> &operator=(Ref<T> &&other) noexcept {
        panic_if(
            !other._r,
            detail::SourcePos::caller2(),
            "Cannot move uninitialized reference."
        );
        if (_r) {
            _r->cnts.remove_ref();
        }
        _r = other._r;
        other._r = nullptr;
        return *this;
    }

    /// Copy assignment operator for reference.
    ///
    /// Panics if the other reference is uninitialized.
    Ref<T> &operator=(const Ref<T> &other) noexcept {
        panic_if(
            !other._r,
            detail::SourcePos::caller2(),
            "Cannot copy uninitialized reference."
        );
        if (this == &other) {
            return *this;
        }
        if (_r) {
            _r->cnts.remove_ref();
        }
        _r->cnts.add_ref();
        _r = other._r;
        return *this;
    }
#else
    Ref(Ref<T> &&) = default;
    Ref(const Ref<T> &) = default;
    Ref<T> &operator=(Ref<T> &&) = default;
    Ref<T> &operator=(const Ref<T> &) = default;
#endif // BOR_CHECK

    /// Get the referenced value.
    ///
    /// Panics if the reference is uninitialized.
    [[nodiscard]]
    const T &get(
        BOR(detail::SourcePos loc = detail::SourcePos::current())
    ) const noexcept {
#if BOR_CHECK
        panic_if(!_r, loc.inc(), "Cannot access uninitialized reference.");
#endif
        return _r->value;
    }

    /// Get the referenced value.
    ///
    /// Panics if the reference is uninitialized.
    const T &operator*() const noexcept {
        return get(BOR(detail::SourcePos::caller2()));
    }

    /// Access member of the referenced value.
    ///
    /// Panics if the reference is uninitialized.
    const T *operator->() const noexcept {
        return &get(BOR(detail::SourcePos::caller2()));
    }

#if BOR_CHECK
    /// Destroy the reference.
    ~Ref() noexcept {
        if (_r) {
            _r->cnts.remove_ref();
            _r = nullptr;
        }
    }
#else
    ~Ref() = default;
#endif
};

} // namespace bor

// NOLINTNEXTLINE(bugprone-std-namespace-modification)
template<typename T> struct std::formatter<bor::Ref<T>> {
    constexpr auto parse(format_parse_context &ctx) { return _fmt.parse(ctx); }

    auto format(const bor::Ref<T> &v, format_context &ctx) const {
        return _fmt.format(*v, ctx);
    }

private:
    formatter<T> _fmt;
};