#pragma once

#include "owned_slice.hpp"
#include "ref.hpp"
#include "val_inner.hpp"

#if BOR_CHECK
# include "counters.hpp"
# include "err.hpp"
# include "source_pos.hpp"
#endif

namespace bor {

template<typename T> class Slice {
    const T *_r{ nullptr };
    std::size_t _len{ 0 };

#if BOR_CHECK
    detail::Counters *_cnts{ nullptr };
#endif

    constexpr Slice(
        const T *r, std::size_t len BOR(, detail::Counters *cnts)
    ) :
        _r(r), _len(len) BOR(, _cnts(cnts)) { }

public:
    constexpr Slice() = default;

    /// Create slice from the given slicable object.
    ///
    /// Panics if the creating the reference is invalid.
    template<IsOwnedSlice<T> S>
    Slice(detail::ValInner<S> &val BOR(, detail::SourcePos loc)) noexcept :
        _r(OwnedSlice<S>().data(val.value)),
        _len(OwnedSlice<S>().len(val.value)) BOR(, _cnts(&val.cnts)) {
#if BOR_CHECK
        val.cnts.create_ref(loc.inc());
#endif
    }

    /// Create subslice of the given slicable object.
    ///
    /// Panics if creating the reference is invalid or if the range is invalid
    /// for the object.
    template<typename S>
    Slice(
        detail::ValInner<S> &val,
        std::size_t start,
        std::size_t end BOR(, detail::SourcePos loc)
    ) noexcept
        requires(IsOwnedSlice<S, T>)
        :
        _r(OwnedSlice<S>().data(val.value) + start),
        _len(end - start) BOR(, _cnts(&val.cnts)) {
#if BOR_CHECK
        auto len = OwnedSlice<S>().len(val.value);
        detail::panic_if(
            start > end,
            loc.inc(),
            "Range start is bigger than end ({}..{})",
            start,
            end
        );
        detail::panic_if(
            end >= len, loc.inc(), "Index out of bounds {}/{}.", end, len
        );
        _cnts->create_ref(loc.inc());
#endif
    }

    /// Create slice from reference to the given slicable object.
    ///
    /// Panics if reference is uninitialized or creating the slice is invalid.
    template<IsOwnedSlice<T> S>
    Slice(
        const Ref<S> &ref
            BOR(, detail::SourcePos loc = detail::SourcePos::caller())
    ) noexcept :
        _r(OwnedSlice<S>().data(ref.get(BOR(loc.inc())))),
        _len(OwnedSlice<S>().len(ref.get(BOR(loc.inc())))) BOR(
            , _cnts(ref._cnts)
        ) {
#if BOR_CHECK
        _cnts->create_ref(loc.inc());
#endif
    }

    /// Transfer reference to slicable object to slice.
    ///
    /// Panics if reference is uninitialized.
    template<IsOwnedSlice<T> S>
    Slice(
        Ref<S> &&ref BOR(, detail::SourcePos loc = detail::SourcePos::caller())
    ) noexcept :
        _r(OwnedSlice<S>().data(ref.get(BOR(loc.inc())))),
        _len(OwnedSlice<S>().len(ref.get(BOR(loc.inc())))) BOR(
            , _cnts(ref._cnts)
        ) {
#if BOR_CHECK
        ref._r = nullptr;
#endif
    }

#if BOR_CHECK
    /// Move constructor for slice.
    ///
    /// Panics if the slice is uninitialized.
    Slice(
        Slice<T> &&other, detail::SourcePos loc = detail::SourcePos::current()
    ) noexcept :
        _r(other._r), _len(other._len), _cnts(other._cnts) {
        detail::panic_if(!_r, loc.inc(), "Cannot copy uninitialized slice.");
        other._r = nullptr;
    }

    /// Copy constructor for slice.
    ///
    /// Panics if the slice is uninitialized.
    Slice(
        const Slice<T> &other,
        detail::SourcePos loc = detail::SourcePos::current()
    ) noexcept :
        _r(other._r), _len(other._len), _cnts(other._cnts) {
        detail::panic_if(!_r, loc.inc(), "Cannot move uninitialized slice.");
        _cnts->add_ref();
    }

    /// Move assignment operator for slice.
    ///
    /// Panics if the slice is uninitialized.
    Slice<T> &operator=(Slice<T> &&other) noexcept {
        detail::panic_if(
            !other._r,
            detail::SourcePos::caller2(),
            "Cannot move uninitialized reference."
        );
        if (_r) {
            _cnts->remove_ref();
        }
        _r = other._r;
        _len = other._len;
        _cnts = other._cnts;
        other._r = nullptr;
        return *this;
    }

    /// Copy assignment operator for slice.
    ///
    /// Panics if the slice is uninitialized.
    Slice<T> &operator=(const Slice<T> &other) noexcept {
        detail::panic_if(
            !other._r,
            detail::SourcePos::caller2(),
            "Cannot copy uninitialized slice."
        );
        if (this == &other) {
            return *this;
        }
        if (_r) {
            _cnts->remove_ref();
        }
        _r = other._r;
        _len = other._len;
        _cnts = other._cnts;
        _cnts->add_ref();
        return *this;
    }
#else
    Slice(Slice<T> &&) = default;
    Slice(const Slice<T> &) = default;
    Slice &operator=(Slice<T> &&) = default;
    Slice &operator=(const Slice<T> &) = default;
#endif // BOR_CHECK

    /// Check whether the slice is initialized.
    [[nodiscard]]
    constexpr operator bool() const noexcept {
        return _r;
    }

    /// Get the number of items in the slice.
    ///
    /// Panics if the slice is uninitialized.
    [[nodiscard]]
    std::size_t len(
        BOR(detail::SourcePos loc = detail::SourcePos::current())
    ) const {
#if BOR_CHECK
        detail::panic_if(!_r, loc.inc(), "Cannot access uninitialized slice.");
#endif
        return _len;
    }

    /// Convert the slice to span.
    ///
    /// Panics if the slice is uninitialized.
    [[nodiscard]]
    std::span<const T> as_span(
        BOR(detail::SourcePos loc = detail::SourcePos::current())
    ) const noexcept {
#if BOR_CHECK
        detail::panic_if(!_r, loc.inc(), "Cannot access uninitialized slice.");
#endif
        return { _r, _len };
    }

    /// Index into the slice.
    ///
    /// Panics if the slice is uninitialized or if the index is out of range.
    Ref<T> operator[](std::size_t idx BOR(
        , detail::SourcePos loc = detail::SourcePos::current()
    )) const noexcept {
#if BOR_CHECK
        detail::panic_if(!_r, loc.inc(), "Cannot access uninitialized slice.");
        detail::panic_if(
            idx >= _len,
            loc.inc(),
            "Slice out of bounds access {}/{}.",
            idx,
            _len
        );
        _cnts->add_ref();
#endif
        return { _r + idx BOR(, _cnts) };
    }

    /// Get subslice of the slice.
    ///
    /// Panics if the slice is uninitialized or if the range is invalid for the
    /// slice.
    Slice<T> operator[](
        std::size_t start,
        std::size_t end
            BOR(, detail::SourcePos loc = detail::SourcePos::current())
    ) const noexcept {
#if BOR_CHECK
        detail::panic_if(!_r, loc.inc(), "Cannot access uninitialized slice.");
        detail::panic_if(
            start > end, loc.inc(), "Range start is larger than end."
        );
        detail::panic_if(
            end >= _len,
            loc.inc(),
            "Slice out of bounds access {}/{}.",
            end,
            _len
        );
        _cnts->add_ref();
#endif
        return { _r + start, end - start BOR(, _cnts) };
    }

#if BOR_CHECK
    ~Slice() noexcept {
        if (_r) {
            _cnts->remove_ref();
            _r = nullptr;
        }
    }
#else
    ~Slice() = default;
#endif
};

} // namespace bor