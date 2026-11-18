#pragma once

#include <cstddef>
#include <type_traits>
#include <utility>

#include "counters.hpp"
#include "err.hpp"
#include "owned_slice.hpp"
#include "ref_mut.hpp"
#include "source_pos.hpp"
#include "val_inner.hpp"

namespace bor {

/// A mutable slice of data.
template<typename T> class SliceMut {
    T *_r{ nullptr };
    std::size_t _len{ 0 };

#if BOR_CHECK
    detail::Counters *_cnts{ nullptr };
    bool _borrowed{ false };
#endif

    constexpr SliceMut(T *r, std::size_t len BOR(, detail::Counters *cnts)) :
        _r(r), _len(len) BOR(, _cnts(cnts)) { }

public:
    /// Create uninitialized slice of data.
    constexpr SliceMut() = default;

    /// Create mutable slice from the given slicable object.
    ///
    /// Panics if creating a mutable reference is invalid.
    template<IsOwnedSlice<T> S>
    SliceMut(detail::ValInner<S> &val BOR(, detail::SourcePos loc)) noexcept :
        _r(OwnedSlice<S>().data_mut(val.value)),
        _len(OwnedSlice<S>().len(val.value)) BOR(, _cnts(&val.cnts)) {
#if BOR_CHECK
        val.cnts.create_ref_mut(loc.inc());
#endif
    }

    /// Create subslice from the given slicable object.
    ///
    /// Panics if the range is invalid or if creating a mutable reference is
    /// invalid.
    template<typename S>
    SliceMut(
        detail::ValInner<S> &val,
        std::size_t start,
        std::size_t end BOR(, detail::SourcePos loc)
    ) noexcept
        requires(IsOwnedSlice<S, T>)
        :
        _r(OwnedSlice<S>().data_mut(val.value) + start),
        _len(end - start) BOR(, _cnts(&val.cnts)) {
#if BOR_CHECK
        auto len = OwnedSlice<S>().len(val.value);
        detail::panic_if(
            start > end,
            loc.inc(),
            "Range start is bigger than end ({}..{}).",
            start,
            end
        );
        detail::panic_if(
            end > len, loc.inc(), "Index out of bounds {}/{}", end, len
        );
        _cnts->create_ref_mut(loc.inc());
#endif
    }

    /// Convert the mutable reference to slicable object to a slice.
    ///
    /// Panics if the reference is invalid or borrowed.
    template<IsOwnedSlice<T> S>
    SliceMut(
        RefMut<S> &&ref
            BOR(, detail::SourcePos loc = detail::SourcePos::caller())
    ) noexcept :
        _r(OwnedSlice<S>().data_mut(*ref._r)),
        _len(OwnedSlice<S>().len(*ref._r)) BOR(, _cnts(ref._cnts)) {
#if BOR_CHECK
        panic_if(
            !_r,
            loc.inc(),
            "Cannot create slice from uninitialized mutable reference."
        );
        panic_if(
            ref._borrowed, loc.inc(), "Cannot move borrowed mutable reference."
        );
#endif
        ref._r = nullptr;
    }

    /// Move the slice.
    ///
    /// Panics if the other slice is invalid or borrowed.
    SliceMut(
        SliceMut<T> &&other
            BOR(, detail::SourcePos loc = detail::SourcePos::current())
    ) noexcept :
        _r(other._r), _len(other._len) BOR(, _cnts(other._cnts)) {
#if BOR_CHECK
        panic_if(
            !_r, loc.inc(), "Cannot move from uninitialized mutable slice."
        );
        panic_if(
            other._borrowed, loc.inc(), "Cannot move borrowed mutable slice."
        );
#endif
        other._r = nullptr;
    }

    SliceMut(const SliceMut<T> &) = delete (
        "Mutable slice cannot be copied. Consider using SliceMut::borrow_mut."
    );

    /// Move teh slice.
    ///
    /// Panics if the other slice is invalid or borrowed.
    SliceMut &operator=(SliceMut<T> &&other) noexcept {
#if BOR_CHECK
        panic_if(
            !other._r,
            detail::SourcePos::caller2(),
            "Cannot move uninitialized mutable slice."
        );
        panic_if(
            other._borrowed,
            detail::SourcePos::caller2(),
            "Cannot move borrowed mutable slice."
        );
        detail::panic_if(
            _borrowed,
            detail::SourcePos::caller2(),
            "Cannot overwrite borrowed reference."
        );
        if (_r) {
            _cnts->remove_ref();
        }
        _borrowed = false;
        _cnts = other._cnts;
#endif
        _r = other._r;
        other._r = nullptr;
        return *this;
    }

    SliceMut &operator=(const SliceMut<T> &) = delete (
        "Mutable slice cannot be copied. Consider using SliceMut::borrow_mut."
    );

    /// Check whether the slice is initialized.
    [[nodiscard]]
    constexpr operator bool() const noexcept {
        return _r;
    }

    /// Get the length of the slice.
    ///
    /// Panics if this slice is invalid or borrowed.
    [[nodiscard]]
    std::size_t len(
        BOR(detail::SourcePos loc = detail::SourcePos::current())
    ) const {
#if BOR_CHECK
        panic_if(!_r, loc.inc(), "Cannot access uninitialized mutable slice.");
        panic_if(
            _borrowed, loc.inc(), "Cannot access borrowed mutable slice."
        );
#endif
        return _len;
    }

    /// Get span of the data inside the slice.
    [[nodiscard]]
    std::span<T> as_span(
        BOR(detail::SourcePos loc = detail::SourcePos::current())
    ) const noexcept {
#if BOR_CHECK
        panic_if(!_r, loc.inc(), "Cannot access uninitialized mutable slice.");
        panic_if(
            _borrowed, loc.inc(), "Cannot access borrowed mutable slice."
        );
#endif
        return std::span(_r, _len);
    }

    /// Convert this slice to a reference to object at the given index.
    ///
    /// Panics if the slice is unintialized or borrowed or if the index is out
    /// of bounds.
    RefMut<T> operator[](std::size_t idx BOR(
        , detail::SourcePos loc = detail::SourcePos::current()
    )) && noexcept {
#if BOR_CHECK
        panic_if(!_r, loc.inc(), "Cannot access uninitialized mutable slice.");
        panic_if(
            _borrowed, loc.inc(), "Cannot access borrowed mutable slice."
        );
        panic_if(
            idx >= _len,
            loc.inc(),
            "Slice out of bounds access {}/{}",
            idx,
            _len
        );
#endif
        auto r = _r;
        _r = nullptr;
        return { r + idx BOR(, _cnts) };
    }

    /// Index into the slice.
    ///
    /// Panics if the slice is unintialized or borrowed or if the index is out
    /// of bounds.
    T &operator[](std::size_t idx BOR(
        , detail::SourcePos loc = detail::SourcePos::current()
    )) const & noexcept {
#if BOR_CHECK
        panic_if(!_r, loc.inc(), "Cannot access uninitialized mutable slice.");
        panic_if(
            _borrowed, loc.inc(), "Cannot access borrowed mutable slice."
        );
        panic_if(
            idx >= _len,
            loc.inc(),
            "Slice out of bounds access {}/{}",
            idx,
            _len
        );
#endif
        return _r[idx];
    }

    /// Convert the mutable slice into a subslice.
    ///
    /// Panics if the slice is unintialized or borrowed or if the range is
    /// invalid.
    SliceMut<T> operator[](
        std::size_t start,
        std::size_t end
            BOR(, detail::SourcePos loc = detail::SourcePos::current())
    ) && noexcept {
#if BOR_CHECK
        panic_if(
            !_r, loc.inc(), "Cannot subslice uninitialized mutable reference."
        );
        panic_if(
            _borrowed, loc.inc(), "Cannot access borrowed mutable slice."
        );
        panic_if(end < start, loc.inc(), "Invalid range ({}..{})", start, end);
        panic_if(
            end > _len,
            loc.inc(),
            "Invalid index out of bounds {}/{}.",
            end,
            _len
        );
#endif
        auto r = _r;
        _r = nullptr;
        return { r + start, end - start BOR(, _cnts) };
    }

    /// Convert the mutable slice into immutable slice.
    ///
    /// Panics if the slice is uninitialized or borrowed.
    Slice<T> into_slice(
        BOR(detail::SourcePos loc = detail::SourcePos::current())
    ) {
#if BOR_CHECK
        detail::panic_if(
            !_r, loc.inc(), "Cannot transfer uninitialized mutable slice."
        );
        detail::panic_if(
            _borrowed, loc.inc(), "Cannot access borrowed mutable slice."
        );
        _cnts->transfer_mut_to_ref();
#endif
        auto r = _r;
        _r = nullptr;
        return { r, _len BOR(, _cnts) };
    }

    /// Convert the mutable slice into immutable slice.
    ///
    /// Panics if the slice is uninitialized or borrowed.
    operator Slice<T>(this SliceMut<T> &&self) {
        return self.into_slice(BOR(detail::SourcePos::caller2()));
    }

    /// Split the mutable slice into two disjunct mutable subslices.
    ///
    /// Panics if the ranges are invalid or if they overlap.
    std::pair<SliceMut<T>, SliceMut<T>> split(
        std::pair<std::size_t, std::size_t> a,
        std::pair<std::size_t, std::size_t> b
            BOR(, detail::SourcePos loc = detail::SourcePos::current())
    ) && noexcept {
#if BOR_CHECK
        detail::panic_if(
            a.first > a.second, loc.inc(), "First range is invalid."
        );
        detail::panic_if(
            b.first > b.second, loc.inc(), "Second range is invalid."
        );
        detail::panic_if(
            (a.first < b.first && a.second > b.first) ||
                (b.first < a.first && b.second > a.first),
            loc.inc(),
            "The ranges overlap."
        );
        detail::panic_if(
            a.second > _len,
            loc.inc(),
            "First range is out of bounds {}/{}",
            a.second,
            _len
        );
        detail::panic_if(
            b.second > _len,
            loc.inc(),
            "Second range is out of bounds {}/{}",
            b.second,
            _len
        );
        _cnts->add_ref();
#endif
        auto r = _r;
        _r = nullptr;
        return {
            { r + a.first, a.second - a.first BOR(, _cnts) },
            { r + b.first, b.second - b.first BOR(, _cnts) },
        };
    }

    /// Immutably borrow value at the given index.
    ///
    /// Panics if the slice is uninitialized or borrowed or if the index is out
    /// of range or if the borrowed value escapes the closure.
    template<typename R, typename F>
    R borrow(
        std::size_t idx,
        F f BOR(, detail::SourcePos loc = detail::SourcePos::current())
    )
        requires(
            std::is_invocable_r_v<R, F, Ref<T>> &&
            std::is_move_constructible_v<R>
        )
    {
#if BOR_CHECK
        detail::panic_if(
            !_r, loc.inc(), "Cannot borrow uninitialized mutable slice."
        );
        detail::panic_if(
            _borrowed, loc.inc(), "Cannot borrow borrowed mutable slice."
        );
        detail::panic_if(
            idx >= _len,
            loc.inc(),
            "Mutable slice index out of range {}/{}.",
            idx,
            _len
        );
        auto cnts = detail::Counters::borrowed();
        _borrowed = true;
#endif
        R res = f(Ref(_r + idx BOR(, &cnts)));
#if BOR_CHECK
        _borrowed = false;
        detail::panic_if(
            cnts.ref_cnt() != 0,
            loc.inc(),
            "Borrowed slice escaped its lifetime."
        );
#endif
        return res;
    }

    /// Immutably borrow the slice.
    ///
    /// Panics if the slice is uninitialized or borrowed or if the borrowed
    /// value escapes the closure.
    template<typename R, typename F>
    R borrow(F f BOR(, detail::SourcePos loc = detail::SourcePos::current()))
        requires(
            std::is_invocable_r_v<R, F, Slice<T>> &&
            std::is_move_constructible_v<R>
        )
    {
#if BOR_CHECK
        detail::panic_if(
            !_r, loc.inc(), "Cannot borrow uninitialized mutable slice."
        );
        detail::panic_if(
            _borrowed, loc.inc(), "Cannot borrow borrowed mutable slice."
        );
        auto cnts = detail::Counters::borrowed();
        _borrowed = true;
#endif
        R res = f(Slice(_r, _len BOR(, &cnts)));
#if BOR_CHECK
        _borrowed = false;
        detail::panic_if(
            cnts.ref_cnt() != 0,
            loc.inc(),
            "Borrowed slice escaped its lifetime."
        );
#endif
        return res;
    }

    /// Immutably borrow subslice.
    ///
    /// Panics if the slice is uninitialized or borrowed or if the range is
    /// invalid or if the borrowed value escapes the closure.
    template<typename R, typename F>
    R borrow(
        std::size_t start,
        std::size_t end,
        F f BOR(, detail::SourcePos loc = detail::SourcePos::current())
    )
        requires(
            std::is_invocable_r_v<R, F, Slice<T>> &&
            std::is_move_constructible_v<R>
        )
    {
#if BOR_CHECK
        detail::panic_if(
            !_r, loc.inc(), "Cannot borrow uninitialized mutable slice."
        );
        detail::panic_if(
            _borrowed, loc.inc(), "Cannot borrow borrowed mutable slice."
        );
        detail::panic_if(
            end < start,
            loc.inc(),
            "Invalid range end is smaller than start ({}..{})",
            start,
            end
        );
        detail::panic_if(
            end > _len,
            loc.inc(),
            "Mutable slice index out of range {}/{}.",
            end,
            _len
        );
        auto cnts = detail::Counters::borrowed();
        _borrowed = true;
#endif
        R res = f(Slice(_r + start, end - start BOR(, &cnts)));
#if BOR_CHECK
        _borrowed = false;
        detail::panic_if(
            cnts.ref_cnt() != 0,
            loc.inc(),
            "Borrowed slice escaped its lifetime."
        );
#endif
        return res;
    }

    /// Immutably borrow value at the given index.
    ///
    /// Panics if the slice is uninitialized or borrowed or if the index is out
    /// of range or if the borrowed value escapes the closure.
    template<typename F>
    void borrow(
        std::size_t idx,
        F f BOR(, detail::SourcePos loc = detail::SourcePos::current())
    )
        requires(std::is_invocable_v<F, Ref<T>>)
    {
#if BOR_CHECK
        detail::panic_if(
            !_r, loc.inc(), "Cannot borrow uninitialized mutable slice."
        );
        detail::panic_if(
            _borrowed, loc.inc(), "Cannot borrow borrowed mutable slice."
        );
        detail::panic_if(
            idx >= _len,
            loc.inc(),
            "Mutable slice index out of range {}/{}.",
            idx,
            _len
        );
        auto cnts = detail::Counters::borrowed();
        _borrowed = true;
#endif
        f(Ref(_r + idx BOR(, &cnts)));
#if BOR_CHECK
        _borrowed = false;
        detail::panic_if(
            cnts.ref_cnt() != 0,
            loc.inc(),
            "Borrowed slice escaped its lifetime."
        );
#endif
    }

    /// Immutably borrow the slice.
    ///
    /// Panics if the slice is uninitialized or borrowed or if the borrowed
    /// value escapes the closure.
    template<typename F>
    void borrow(
        F f BOR(, detail::SourcePos loc = detail::SourcePos::current())
    )
        requires(std::is_invocable_v<F, Slice<T>>)
    {
#if BOR_CHECK
        detail::panic_if(
            !_r, loc.inc(), "Cannot borrow uninitialized mutable slice."
        );
        detail::panic_if(
            _borrowed, loc.inc(), "Cannot borrow borrowed mutable slice."
        );
        auto cnts = detail::Counters::borrowed();
        _borrowed = true;
#endif
        f(Slice(_r, _len BOR(, &cnts)));
#if BOR_CHECK
        _borrowed = false;
        detail::panic_if(
            cnts.ref_cnt() != 0,
            loc.inc(),
            "Borrowed slice escaped its lifetime."
        );
#endif
    }

    /// Immutably orrow subslice.
    ///
    /// Panics if the slice is uninitialized or borrowed or if the range is
    /// invalid or if the borrowed value escapes the closure.
    template<typename F>
    void borrow(
        std::size_t start,
        std::size_t end,
        F f BOR(, detail::SourcePos loc = detail::SourcePos::current())
    )
        requires(std::is_invocable_v<F, Slice<T>>)
    {
#if BOR_CHECK
        detail::panic_if(
            !_r, loc.inc(), "Cannot borrow uninitialized mutable slice."
        );
        detail::panic_if(
            _borrowed, loc.inc(), "Cannot borrow borrowed mutable slice."
        );
        detail::panic_if(
            end < start,
            loc.inc(),
            "Invalid range end is smaller than start ({}..{})",
            start,
            end
        );
        detail::panic_if(
            end > _len,
            loc.inc(),
            "Mutable slice index out of range {}/{}.",
            end,
            _len
        );
        auto cnts = detail::Counters::borrowed();
        _borrowed = true;
#endif
        f(Slice(_r + start, end - start BOR(, &cnts)));
#if BOR_CHECK
        _borrowed = false;
        detail::panic_if(
            cnts.ref_cnt() != 0,
            loc.inc(),
            "Borrowed slice escaped its lifetime."
        );
#endif
    }

    /// Borrow value at the given index.
    ///
    /// Panics if the slice is uninitialized or borrowed or if the index is out
    /// of range or if the borrowed value escapes the closure.
    template<typename R, typename F>
    R borrow_mut(
        std::size_t idx,
        F f BOR(, detail::SourcePos loc = detail::SourcePos::current())
    )
        requires(
            std::is_invocable_r_v<R, F, RefMut<T>> &&
            std::is_move_constructible_v<R>
        )
    {
#if BOR_CHECK
        detail::panic_if(
            !_r, loc.inc(), "Cannot borrow uninitialized mutable slice."
        );
        detail::panic_if(
            _borrowed, loc.inc(), "Cannot borrow borrowed mutable slice."
        );
        detail::panic_if(
            idx >= _len,
            loc.inc(),
            "Mutable slice index out of range {}/{}.",
            idx,
            _len
        );
        auto cnts = detail::Counters::borrowed_mut();
        _borrowed = true;
#endif
        R res = f(RefMut(_r + idx BOR(, &cnts)));
#if BOR_CHECK
        _borrowed = false;
        detail::panic_if(
            cnts.ref_cnt() != 0,
            loc.inc(),
            "Borrowed slice escaped its lifetime."
        );
#endif
        return res;
    }

    /// Borrow the slice.
    ///
    /// Panics if the slice is uninitialized or borrowed or if the borrowed
    /// value escapes the closure.
    template<typename R, typename F>
    R borrow_mut(
        F f BOR(, detail::SourcePos loc = detail::SourcePos::current())
    )
        requires(
            std::is_invocable_r_v<R, F, SliceMut<T>> &&
            std::is_move_constructible_v<R>
        )
    {
#if BOR_CHECK
        detail::panic_if(
            !_r, loc.inc(), "Cannot borrow uninitialized mutable slice."
        );
        detail::panic_if(
            _borrowed, loc.inc(), "Cannot borrow borrowed mutable slice."
        );
        auto cnts = detail::Counters::borrowed_mut();
        _borrowed = true;
#endif
        R res = f(SliceMut(_r, _len BOR(, &cnts)));
#if BOR_CHECK
        _borrowed = false;
        detail::panic_if(
            cnts.ref_cnt() != 0,
            loc.inc(),
            "Borrowed slice escaped its lifetime."
        );
#endif
        return res;
    }

    /// Borrow subslice.
    ///
    /// Panics if the slice is uninitialized or borrowed or if the range is
    /// invalid or if the borrowed value escapes the closure.
    template<typename R, typename F>
    R borrow_mut(
        std::size_t start,
        std::size_t end,
        F f BOR(, detail::SourcePos loc = detail::SourcePos::current())
    )
        requires(
            std::is_invocable_r_v<R, F, SliceMut<T>> &&
            std::is_move_constructible_v<R>
        )
    {
#if BOR_CHECK
        detail::panic_if(
            !_r, loc.inc(), "Cannot borrow uninitialized mutable slice."
        );
        detail::panic_if(
            _borrowed, loc.inc(), "Cannot borrow borrowed mutable slice."
        );
        detail::panic_if(
            end < start,
            loc.inc(),
            "Invalid range end is smaller than start ({}..{})",
            start,
            end
        );
        detail::panic_if(
            end > _len,
            loc.inc(),
            "Mutable slice index out of range {}/{}.",
            end,
            _len
        );
        auto cnts = detail::Counters::borrowed_mut();
        _borrowed = true;
#endif
        R res = f(SliceMut(_r + start, end - start BOR(, &cnts)));
#if BOR_CHECK
        _borrowed = false;
        detail::panic_if(
            cnts.ref_cnt() != 0,
            loc.inc(),
            "Borrowed slice escaped its lifetime."
        );
#endif
        return res;
    }

    /// Borrow value at the given index.
    ///
    /// Panics if the slice is uninitialized or borrowed or if the index is out
    /// of range or if the borrowed value escapes the closure.
    template<typename F>
    void borrow_mut(
        std::size_t idx,
        F f BOR(, detail::SourcePos loc = detail::SourcePos::current())
    )
        requires(std::is_invocable_v<F, RefMut<T>>)
    {
#if BOR_CHECK
        detail::panic_if(
            !_r, loc.inc(), "Cannot borrow uninitialized mutable slice."
        );
        detail::panic_if(
            _borrowed, loc.inc(), "Cannot borrow borrowed mutable slice."
        );
        detail::panic_if(
            idx >= _len,
            loc.inc(),
            "Mutable slice index out of range {}/{}.",
            idx,
            _len
        );
        auto cnts = detail::Counters::borrowed_mut();
        _borrowed = true;
#endif
        f(RefMut(_r + idx BOR(, &cnts)));
#if BOR_CHECK
        _borrowed = false;
        detail::panic_if(
            cnts.ref_cnt() != 0,
            loc.inc(),
            "Borrowed slice escaped its lifetime."
        );
#endif
    }

    /// Borrow the slice.
    ///
    /// Panics if the slice is uninitialized or borrowed or if the borrowed
    /// value escapes the closure.
    template<typename F>
    void borrow_mut(
        F f BOR(, detail::SourcePos loc = detail::SourcePos::current())
    )
        requires(std::is_invocable_v<F, SliceMut<T>>)
    {
#if BOR_CHECK
        detail::panic_if(
            !_r, loc.inc(), "Cannot borrow uninitialized mutable slice."
        );
        detail::panic_if(
            _borrowed, loc.inc(), "Cannot borrow borrowed mutable slice."
        );
        auto cnts = detail::Counters::borrowed_mut();
        _borrowed = true;
#endif
        f(SliceMut(_r, _len BOR(, &cnts)));
#if BOR_CHECK
        _borrowed = false;
        detail::panic_if(
            cnts.ref_cnt() != 0,
            loc.inc(),
            "Borrowed slice escaped its lifetime."
        );
#endif
    }

    /// Borrow subslice.
    ///
    /// Panics if the slice is uninitialized or borrowed or if the range is
    /// invalid or if the borrowed value escapes the closure.
    template<typename F>
    void borrow_mut(
        std::size_t start,
        std::size_t end,
        F f BOR(, detail::SourcePos loc = detail::SourcePos::current())
    )
        requires(std::is_invocable_v<F, SliceMut<T>>)
    {
#if BOR_CHECK
        detail::panic_if(
            !_r, loc.inc(), "Cannot borrow uninitialized mutable slice."
        );
        detail::panic_if(
            _borrowed, loc.inc(), "Cannot borrow borrowed mutable slice."
        );
        detail::panic_if(
            end < start,
            loc.inc(),
            "Invalid range end is smaller than start ({}..{})",
            start,
            end
        );
        detail::panic_if(
            end > _len,
            loc.inc(),
            "Mutable slice index out of range {}/{}.",
            end,
            _len
        );
        auto cnts = detail::Counters::borrowed_mut();
        _borrowed = true;
#endif
        f(SliceMut(_r + start, end - start BOR(, &cnts)));
#if BOR_CHECK
        _borrowed = false;
        detail::panic_if(
            cnts.ref_cnt() != 0,
            loc.inc(),
            "Borrowed slice escaped its lifetime."
        );
#endif
    }

#if BOR_CHECK
    ~SliceMut() {
        if (_r) {
            _cnts->remove_ref();
            _r = nullptr;
        }
    }
#else
    ~SliceMut() = default;
#endif
};

} // namespace bor