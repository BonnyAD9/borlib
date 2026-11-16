#pragma once

#include <cstddef>
#include <type_traits>

#include "counters.hpp"
#include "err.hpp"
#include "owned_slice.hpp"
#include "ref_mut.hpp"
#include "source_pos.hpp"
#include "val_inner.hpp"

namespace bor {

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
    constexpr SliceMut() = default;

    template<IsOwnedSlice<T> S>
    SliceMut(detail::ValInner<S> &val BOR(, detail::SourcePos loc)) noexcept :
        _r(OwnedSlice<S>().data_mut(val.value)),
        _len(OwnedSlice<S>().len(val.value)) BOR(, _cnts(&val.cnts)) {
#if BOR_CHECK
        val.cnts.create_ref_mut(loc.inc());
#endif
    }

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
            end >= len, loc.inc(), "Index out of bounds {}/{}", end, len
        );
        _cnts->create_ref_mut(loc.inc());
#endif
    }

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
        if (_r) {
            _cnts->remove_ref_mut();
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
        return { _r, _len };
    }

    RefMut<T> operator[](std::size_t idx BOR(
        , detail::SourcePos loc = detail::SourcePos::current()
    )) const && noexcept {
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

    SliceMut<T> operator[](
        std::size_t start,
        std::size_t end
            BOR(, detail::SourcePos loc = detail::SourcePos::current())
    ) const && noexcept {
#if BOR_CHECK
        panic_if(
            !_r, loc.inc(), "Cannot subslice uninitialized mutable reference."
        );
        panic_if(
            _borrowed, loc.inc(), "Cannot access borrowed mutable slice."
        );
        panic_if(end < start, loc.inc(), "Invalid range ({}..{})", start, end);
        panic_if(
            end >= _len,
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
        return { r BOR(, _cnts) };
    }

    operator Slice<T>(this SliceMut<T> &&self) {
        return self.into_slice(BOR(detail::SourcePos::caller2()));
    }

    template<typename R, typename F>
    R borrow(
        F f,
        std::size_t idx
            BOR(, detail::SourcePos loc = detail::SourcePos::current())
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
        const auto cnt = _cnts->ref_cnt();
        _cnts->add_ref();
        _borrowed = true;
#endif
        R res = f(Ref(_r + idx BOR(, _cnts)));
#if BOR_CHECK
        _borrowed = false;
        detail::panic_if(
            _cnts->ref_cnt() != cnt,
            loc.inc(),
            "Borrowed slice escaped its lifetime."
        );
#endif
        return res;
    }

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
        const auto cnt = _cnts->ref_cnt();
        _cnts->add_ref();
        _borrowed = true;
#endif
        R res = f(Slice(_r, _len BOR(, _cnts)));
#if BOR_CHECK
        _borrowed = false;
        detail::panic_if(
            _cnts->ref_cnt() != cnt,
            loc.inc(),
            "Borrowed slice escaped its lifetime."
        );
#endif
        return res;
    }

    template<typename R, typename F>
    R borrow(
        F f,
        std::size_t start,
        std::size_t end
            BOR(, detail::SourcePos loc = detail::SourcePos::current())
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
            end >= _len,
            loc.inc(),
            "Mutable slice index out of range {}/{}.",
            end,
            _len
        );
        const auto cnt = _cnts->ref_cnt();
        _cnts->add_ref();
        _borrowed = true;
#endif
        R res = f(Slice(_r + start, end - start BOR(, _cnts)));
#if BOR_CHECK
        _borrowed = false;
        detail::panic_if(
            _cnts->ref_cnt() != cnt,
            loc.inc(),
            "Borrowed slice escaped its lifetime."
        );
#endif
        return res;
    }

    template<typename F>
    void borrow(
        F f,
        std::size_t idx
            BOR(, detail::SourcePos loc = detail::SourcePos::current())
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
        const auto cnt = _cnts->ref_cnt();
        _cnts->add_ref();
        _borrowed = true;
#endif
        f(Ref(_r + idx BOR(, _cnts)));
#if BOR_CHECK
        _borrowed = false;
        detail::panic_if(
            _cnts->ref_cnt() != cnt,
            loc.inc(),
            "Borrowed slice escaped its lifetime."
        );
#endif
    }

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
        const auto cnt = _cnts->ref_cnt();
        _cnts->add_ref();
        _borrowed = true;
#endif
        f(Slice(_r, _len BOR(, _cnts)));
#if BOR_CHECK
        _borrowed = false;
        detail::panic_if(
            _cnts->ref_cnt() != cnt,
            loc.inc(),
            "Borrowed slice escaped its lifetime."
        );
#endif
    }

    template<typename F>
    void borrow(
        F f,
        std::size_t start,
        std::size_t end
            BOR(, detail::SourcePos loc = detail::SourcePos::current())
    )
        requires(std::is_invocable_r_v<F, Slice<T>>)
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
            end >= _len,
            loc.inc(),
            "Mutable slice index out of range {}/{}.",
            end,
            _len
        );
        const auto cnt = _cnts->ref_cnt();
        _cnts->add_ref();
        _borrowed = true;
#endif
        f(Slice(_r + start, end - start BOR(, _cnts)));
#if BOR_CHECK
        _borrowed = false;
        detail::panic_if(
            _cnts->ref_cnt() != cnt,
            loc.inc(),
            "Borrowed slice escaped its lifetime."
        );
#endif
    }

    template<typename R, typename F>
    R borrow_mut(
        F f,
        std::size_t idx
            BOR(, detail::SourcePos loc = detail::SourcePos::current())
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
        const auto cnt = _cnts->ref_cnt();
        _cnts->add_ref();
        _borrowed = true;
#endif
        R res = f(RefMut(_r + idx BOR(, _cnts)));
#if BOR_CHECK
        _borrowed = false;
        detail::panic_if(
            _cnts->ref_cnt() != cnt,
            loc.inc(),
            "Borrowed slice escaped its lifetime."
        );
#endif
        return res;
    }

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
        const auto cnt = _cnts->ref_cnt();
        _cnts->add_ref();
        _borrowed = true;
#endif
        R res = f(SliceMut(_r, _len BOR(, _cnts)));
#if BOR_CHECK
        _borrowed = false;
        detail::panic_if(
            _cnts->ref_cnt() != cnt,
            loc.inc(),
            "Borrowed slice escaped its lifetime."
        );
#endif
        return res;
    }

    template<typename R, typename F>
    R borrow_mut(
        F f,
        std::size_t start,
        std::size_t end
            BOR(, detail::SourcePos loc = detail::SourcePos::current())
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
            end >= _len,
            loc.inc(),
            "Mutable slice index out of range {}/{}.",
            end,
            _len
        );
        const auto cnt = _cnts->ref_cnt();
        _cnts->add_ref();
        _borrowed = true;
#endif
        R res = f(SliceMut(_r + start, end - start BOR(, _cnts)));
#if BOR_CHECK
        _borrowed = false;
        detail::panic_if(
            _cnts->ref_cnt() != cnt,
            loc.inc(),
            "Borrowed slice escaped its lifetime."
        );
#endif
        return res;
    }

    template<typename F>
    void borrow_mut(
        F f,
        std::size_t idx
            BOR(, detail::SourcePos loc = detail::SourcePos::current())
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
        const auto cnt = _cnts->ref_cnt();
        _cnts->add_ref();
        _borrowed = true;
#endif
        f(RefMut(_r + idx BOR(, _cnts)));
#if BOR_CHECK
        _borrowed = false;
        detail::panic_if(
            _cnts->ref_cnt() != cnt,
            loc.inc(),
            "Borrowed slice escaped its lifetime."
        );
#endif
    }

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
        const auto cnt = _cnts->ref_cnt();
        _cnts->add_ref();
        _borrowed = true;
#endif
        f(SliceMut(_r, _len BOR(, _cnts)));
#if BOR_CHECK
        _borrowed = false;
        detail::panic_if(
            _cnts->ref_cnt() != cnt,
            loc.inc(),
            "Borrowed slice escaped its lifetime."
        );
#endif
    }

    template<typename F>
    void borrow_mut(
        F f,
        std::size_t start,
        std::size_t end
            BOR(, detail::SourcePos loc = detail::SourcePos::current())
    )
        requires(std::is_invocable_r_v<F, SliceMut<T>>)
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
            end >= _len,
            loc.inc(),
            "Mutable slice index out of range {}/{}.",
            end,
            _len
        );
        const auto cnt = _cnts->ref_cnt();
        _cnts->add_ref();
        _borrowed = true;
#endif
        f(SliceMut(_r + start, end - start BOR(, _cnts)));
#if BOR_CHECK
        _borrowed = false;
        detail::panic_if(
            _cnts->ref_cnt() != cnt,
            loc.inc(),
            "Borrowed slice escaped its lifetime."
        );
#endif
    }

#if BOR_CHECK
    ~SliceMut() {
        if (_r) {
            _cnts->remove_ref_mut();
            _r = nullptr;
        }
    }
#else
    ~SliceMut() = default;
#endif
};

} // namespace bor