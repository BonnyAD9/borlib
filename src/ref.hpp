#pragma once

#include <source_location>

#include "err.hpp"
#include "val_inner.hpp"

namespace bor {

template<typename T> class RefMut;

/// Safe reference.
///
/// This reference checks that it is valid and that noone can modify the
/// referenced value while the reference exists.
template<typename T> class Ref {
    friend RefMut<T>;

    ValInner<T> *_r{ nullptr };

    constexpr Ref(ValInner<T> *val) noexcept : _r(val) { }

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
    Ref(ValInner<T> &val, std::source_location loc) noexcept : _r(&val) {
        val.cnts.create_ref(loc);
    }

    /// Move constructor of reference. The other reference will be made
    /// uninitialized.
    ///
    /// Panics if the other reference is uninitialized.
    Ref(Ref<T> &&other,
        std::source_location loc = std::source_location::current()) noexcept :
        _r(other._r) {
        panic_if(!_r, loc, "Cannot move uninitialized reference.");
        other._r = nullptr;
    }

    /// Copy constructor of reference.
    ///
    /// Panics if the other reference is uninitialized.
    Ref(const Ref<T> &other,
        std::source_location loc = std::source_location::current()) noexcept :
        _r(other._r) {
        panic_if(!_r, loc, "Cannot copy uninitialized reference.");
        _r->cnts.add_ref();
    }

    /// Move assignment operator of reference. The other reference will be made
    /// uninitialized.
    ///
    /// Panics if the other reference is uninitialized.
    Ref<T> &operator=(Ref<T> &&other) noexcept {
        panic_if(
            !other._r,
            std::source_location::current(),
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
            std::source_location::current(),
            "Cannot copy uninitialized reference."
        );
        if (this == &other) {
            return *this;
        }
        if (_r) {
            _r->cnts.remove_ref();
        }
        _r = other._r;
        _r->cnts.add_ref();
        return *this;
    }

    /// Get the referenced value.
    ///
    /// Panics if the reference is uninitialized.
    [[nodiscard]]
    const T &get(
        std::source_location loc = std::source_location::current()
    ) const noexcept {
        panic_if(!_r, loc, "Cannot access uninitialized reference.");
        return _r->value;
    }

    /// Get the referenced value.
    ///
    /// Panics if the reference is uninitialized.
    const T &operator*() const noexcept { return get(); }

    /// Access member of the referenced value.
    ///
    /// Panics if the reference is uninitialized.
    const T *operator->() const noexcept { return &get(); }

    /// Destroy the reference.
    ~Ref() noexcept {
        if (_r) {
            _r->cnts.remove_ref();
            _r = nullptr;
        }
    }
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