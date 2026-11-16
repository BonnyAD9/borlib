#pragma once

#include <type_traits>

#include "defines.hpp"
#include "ref.hpp"
#include "val_inner.hpp"

#if BOR_CHECK
# include "counters.hpp"
#endif

namespace bor {

template<typename T> class SliceMut;

/// Mutable safe reference.
///
/// This reference checks that noone else can access the referenced value.
template<typename T> class RefMut {
    friend SliceMut<T>;

    T *_r{ nullptr };
#if BOR_CHECK
    detail::Counters *_cnts{ nullptr };
    bool _borrowed = false;
#endif

    constexpr RefMut(detail::ValInner<T> *val) noexcept :
        _r(val) BOR(, _cnts(&val->cnts)) { }

    constexpr RefMut(T *r BOR(, detail::Counters *cnts)) :
        _r(r) BOR(, _cnts(cnts)) { }

public:
    /// Constructs uninitialized reference.
    ///
    /// Any operation other than assigning to this reference or destroying it
    /// will result in panic.
    constexpr RefMut() = default;

    /// Inner constructor for the mutable reference. Checks that constructing
    /// the mutable reference is valid and modifies the counters accordingly.
    ///
    /// Panics if creating the mutable reference is invalid.
    RefMut(detail::ValInner<T> &val BOR(, detail::SourcePos loc)) noexcept :
        _r(&val.value) BOR(, _cnts(&val.cnts)) {
#if BOR_CHECK
        val.cnts.create_ref_mut(loc.inc());
#endif
    }

    /// Inner constructor for the mutable reference from slicable object at the
    /// given index. Checks that the index is within bounds and that
    /// constructing the mutable reference is valid and modifies the counters
    /// accordingly.
    ///
    /// Panics if the index is out of bounds or if creating the mutable
    /// reference is invalid.
    template<typename S>
    RefMut(
        detail::ValInner<S> &val, std::size_t idx BOR(, detail::SourcePos loc)
    ) noexcept
        requires(IsOwnedSlice<S, T>)
        :
        _r(OwnedSlice<S>().data_mut(val.value) + idx) BOR(, _cnts(&val.cnts)) {
#if BOR_CHECK
        auto len = OwnedSlice<S>().len(val.value);
        detail::panic_if(
            idx >= len, loc.inc(), "Index out of bounds {}/{}", idx, len
        );
        _cnts->create_ref_mut(loc.inc());
#endif
    }

    /// Move constructor for mutable referece. The other reference is made
    /// uninitialized.
    ///
    /// Panics if the other reference is uninitialized or currently borrowed.
    RefMut(
        RefMut<T> &&other
            BOR(, detail::SourcePos loc = detail::SourcePos::current())
    ) noexcept :
        _r(other._r) BOR(, _cnts(other._cnts)) {
#if BOR_CHECK
        detail::panic_if(
            !_r, loc.inc(), "Cannot move uninitialized mutable reference."
        );
        detail::panic_if(
            other._borrowed,
            loc.inc(),
            "Cannot move borroed mutable reference."
        );
#endif
        other._r = nullptr;
    }

    RefMut(const RefMut<T> &) = delete ("Mutable reference cannot be copied. "
                                        "Consider using RefMut::borrow_mut.");

    /// Move assignment operator for the mutable reference. This makes the
    /// other reference uninitialized.
    ///
    /// Panics if the other reference is uninitialized or currently borrowed.
    RefMut &operator=(RefMut<T> &&other) noexcept {
#if BOR_CHECK
        detail::panic_if(
            !other._r,
            detail::SourcePos::caller2(),
            "Cannot move uninitialized mutable reference."
        );
        detail::panic_if(
            other._borrowed,
            detail::SourcePos::caller2(),
            "Cannot move borroed mutable reference."
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

    RefMut<T> &operator=(const RefMut<T> &) =
        delete ("Mutable reference cannot be copied. Consider using "
                "RefMut::borrow_mut.");

    /// Get access to the underlaying value.
    ///
    /// Panics if the reference is uninitialized or currently borrowed.
    [[nodiscard]]
    T &get(BOR(detail::SourcePos loc = detail::SourcePos::current())) const {
#if BOR_CHECK
        detail::panic_if(
            !_r, loc.inc(), "Cannot access uninitialized mutable reference."
        );
        detail::panic_if(
            _borrowed, loc.inc(), "Cannot access borrowed mutable reference."
        );
#endif
        return *_r;
    }

    /// Get access to the underlaying value.
    ///
    /// Panics if the reference is uninitialized or currently borrowed.
    T &operator*() const { return get(BOR(detail::SourcePos::caller2())); }

    /// Access member of the underlaying value.
    ///
    /// Panics if the reference is uninitialized or currently borrowed.
    T *operator->() const { return &get(BOR(detail::SourcePos::caller2())); }

    /// Transform this reference into immutable reference.
    ///
    /// Panics if the reference is uninitialized or currently borrowed.
    Ref<T> into_ref(
        BOR(detail::SourcePos loc = detail::SourcePos::current())
    ) {
#if BOR_CHECK
        detail::panic_if(
            !_r, loc.inc(), "Cannot transfer uninitialized mutable reference."
        );
        detail::panic_if(
            _borrowed, loc.inc(), "Cannot access borrowed mutable reference."
        );
        _cnts->transfer_mut_to_ref();
#endif
        auto r = _r;
        _r = nullptr;
        return { r BOR(, _cnts) };
    }

    /// Implicit conversion of rvalue of this reference to immutable reference.
    ///
    /// Panics if the reference is uninitialized or currently borrowed.
    operator Ref<T>(this RefMut<T> &&self) {
        return self.into_ref(BOR(detail::SourcePos::caller2()));
    }

    /// Borrow the reference for the duration of the given closure.
    ///
    /// Panics if the reference is uninitialized, currently borrowed or if the
    /// new borrowed reference outlives execution of the closure.
    template<typename R, typename F>
    R borrow(F f BOR(, detail::SourcePos loc = detail::SourcePos::current()))
        requires(
            std::is_invocable_r_v<R, F, Ref<T>> &&
            std::is_move_constructible_v<R>
        )
    {
#if BOR_CHECK
        detail::panic_if(
            !_r, loc.inc(), "Cannot borrow uninitialized mutable reference."
        );
        detail::panic_if(
            _borrowed, loc.inc(), "Cannot borrow borrowed mutable reference."
        );
        const auto cnt = _cnts->ref_cnt();
        _cnts->add_ref();
        _borrowed = true;
#endif
        R res = f(Ref(_r BOR(, _cnts)));
#if BOR_CHECK
        _borrowed = false;
        detail::panic_if(
            _cnts->ref_cnt() != cnt,
            loc.inc(),
            "Borrowed reference escaped its lifetime."
        );
#endif
        return res;
    }

    /// Borrow the reference for the duration of the given closure.
    ///
    /// Panics if the reference is uninitialized, currently borrowed or if the
    /// new borrowed reference outlives execution of the closure.
    template<typename F>
    void borrow(
        F f BOR(, detail::SourcePos loc = detail::SourcePos::current())
    )
        requires(std::is_invocable_v<F, Ref<T>>)
    {
#if BOR_CHECK
        panic_if(
            _borrowed, loc.inc(), "Cannot borrow borrowed mutable reference."
        );
        panic_if(
            !_r, loc.inc(), "Cannot borrow uninitialized mutable reference."
        );
        const auto cnt = _cnts->ref_cnt();
        _cnts->add_ref();
        _borrowed = true;
#endif
        f(Ref(_r BOR(, _cnts)));
#if BOR_CHECK
        _borrowed = false;
        panic_if(
            _cnts->ref_cnt() != cnt,
            loc.inc(),
            "Borrowed reference escaped its lifetime."
        );
#endif
    }

    /// Mutably borrow the reference for the duration of the given closure.
    ///
    /// Panics if the reference is uninitialized, currently borrowed or if the
    /// new borrowed reference outlives execution of the closure.
    template<typename R, typename F>
    R borrow_mut(
        F f BOR(, detail::SourcePos loc = detail::SourcePos::current())
    )
        requires(
            std::is_invocable_r_v<R, F, RefMut<T>> &&
            std::is_move_constructible_v<R>
        )
    {
#if BOR_CHECK
        panic_if(
            !_r,
            loc.inc(),
            "Cannot mutably borrow uninitialized mutable reference."
        );
        panic_if(
            _borrowed, loc.inc(), "Cannot borrow borrowed mutable reference."
        );
        const auto cnt = _cnts->ref_cnt();
        _cnts->add_ref();
        _borrowed = true;
#endif
        R r = f(RefMut(_r BOR(, _cnts)));
#if BOR_CHECK
        _borrowed = false;
        panic_if(
            _cnts->ref_cnt() != cnt,
            loc.inc(),
            "Borrowed mutable reference escaped its lifetime."
        );
#endif
        return r;
    }

    /// Mutably borrow the reference for the duration of the given closure.
    ///
    /// Panics if the reference is uninitialized, currently borrowed or if the
    /// new borrowed reference outlives execution of the closure.
    template<typename F>
    void borrow_mut(
        F f BOR(, detail::SourcePos loc = detail::SourcePos::current())
    )
        requires(std::is_invocable_v<F, RefMut<T>>)
    {
#if BOR_CHECK
        panic_if(
            !_r,
            loc.inc(),
            "Cannot mutably borrow uninitialized mutable reference."
        );
        panic_if(
            _borrowed, loc.inc(), "Cannot borrow borrowed mutable reference."
        );
        const auto cnt = _cnts->ref_cnt();
        _cnts->add_ref();
        _borrowed = true;
#endif
        f(RefMut(_r BOR(, _cnts)));
#if BOR_CHECK
        _borrowed = false;
        panic_if(
            _cnts->ref_cnt() != cnt,
            loc.inc(),
            "Borrowed mutable reference escaped its lifetime."
        );
#endif
    }

#if BOR_CHECK
    /// Destroy the mutable reference.
    ~RefMut() {
        if (_r) {
            _cnts->remove_ref_mut();
            _r = nullptr;
        }
    }
#else
    ~RefMut() = default;
#endif
};

} // namespace bor

// NOLINTNEXTLINE(bugprone-std-namespace-modification)
template<typename T> struct std::formatter<bor::RefMut<T>> {
    constexpr auto parse(format_parse_context &ctx) { return _fmt.parse(ctx); }

    auto format(const bor::RefMut<T> &v, format_context &ctx) const {
        return _fmt.format(*v, ctx);
    }

private:
    formatter<T> _fmt;
};