#pragma once

#include <format>

#include "defines.hpp"
#include "owned_slice.hpp"
#include "val_inner.hpp"

#if BOR_CHECK
# include "err.hpp"
#include "source_pos.hpp"
#include "counters.hpp"
#endif

namespace bor {

template<typename T> class RefMut;
template<typename T> class Slice;
template<typename T> class Val;

/// Safe reference.
///
/// This reference checks that it is valid and that noone can modify the
/// referenced value while the reference exists.
template<typename T> class Ref {
    friend RefMut<T>;
    friend Slice<T>;
    friend Val<T>;

    const T *_r{ nullptr };
#if BOR_CHECK
    detail::Counters *_cnts{ nullptr };
#endif

    constexpr Ref(detail::ValInner<T> *val) noexcept :
        _r(&val->value) BOR(, _cnts(&val->cnts)) { }

    constexpr Ref(const T *r BOR(, detail::Counters *cnts)) :
        _r(r) BOR(, _cnts(cnts)) { }

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
        _r(&val.value) BOR(, _cnts(&val.cnts)) {
#if BOR_CHECK
        val.cnts.create_ref(loc.inc());
#endif
    }

    /// Get reference to an item owned by the given slicable type at the given
    /// index.
    /// 
    /// Panics if creating the reference is not valid or if the index is out of
    /// bounds.
    template<typename S>
    Ref(detail::ValInner<S> &val,
        std::size_t idx BOR(, detail::SourcePos loc)) noexcept
        requires(IsOwnedSlice<S, T>)
        : _r(OwnedSlice<S>().data(val.value) + idx) BOR(, _cnts(&val.cnts)) {
#if BOR_CHECK
        auto len = OwnedSlice<S>().len(val.value);
        detail::panic_if(
            idx >= len, loc.inc(), "Index out of bounds {}/{}.", idx, len
        );
        _cnts->create_ref(loc.inc());
#endif
    }

#if BOR_CHECK
    /// Move constructor of reference. The other reference will be made
    /// uninitialized.
    ///
    /// Panics if the other reference is uninitialized.
    Ref(Ref<T> &&other,
        detail::SourcePos loc = detail::SourcePos::current()) noexcept :
        _r(other._r), _cnts(other._cnts) {
        panic_if(!_r, loc.inc(), "Cannot move uninitialized reference.");
        other._r = nullptr;
    }

    /// Copy constructor of reference.
    ///
    /// Panics if the other reference is uninitialized.
    Ref(const Ref<T> &other,
        detail::SourcePos loc = detail::SourcePos::current()) noexcept :
        _r(other._r), _cnts(other._cnts) {
        panic_if(!_r, loc.inc(), "Cannot copy uninitialized reference.");
        _cnts->add_ref();
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
            _cnts->remove_ref();
        }
        _r = other._r;
        _cnts = other._cnts;
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
            _cnts->remove_ref();
        }
        _r = other._r;
        _cnts = other._cnts;
        _cnts->add_ref();
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
        return *_r;
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

    /// Index into the object in this slice.
    /// 
    /// Panics if the reference is invalid or if the index is out of range.
    Ref<typename OwnedSlice<T>::Item> operator[](std::size_t idx BOR(
        , detail::SourcePos loc = detail::SourcePos::current()
    )) const noexcept
        requires(IsOwnedSlice<T>)
    {
#if BOR_CHECK
        panic_if(
            !_r, loc.inc(), "Cannot index uninitialized reference as slice."
        );
        auto len = OwnedSlice<T>().len(*_r);
        panic_if(idx >= len, "Index out of range {}/{}", idx, len);
        _cnts->add_ref();
#endif
        return { OwnedSlice<T>().data(*_r) + idx BOR(, _cnts) };
    }

#if BOR_CHECK
    /// Destroy the reference.
    ~Ref() noexcept {
        if (_r) {
            _cnts->remove_ref();
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