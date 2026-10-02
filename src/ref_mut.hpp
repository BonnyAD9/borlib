#pragma once

#include <source_location>
#include <type_traits>

#include "ref.hpp"
#include "val_inner.hpp"

namespace bor {

/// Mutable safe reference.
///
/// This reference checks that noone else can access the referenced value.
template<typename T> class RefMut {
    ValInner<T> *_r{ nullptr };
    bool _borrowed = false;

    constexpr RefMut(ValInner<T> *val) noexcept : _r(val) { }

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
    RefMut(ValInner<T> &val, std::source_location loc) noexcept : _r(&val) {
        val.cnts.create_ref_mut(loc);
    }

    /// Move constructor for mutable referece. The other reference is made
    /// uninitialized.
    ///
    /// Panics if the other reference is uninitialized or currently borrowed.
    RefMut(
        RefMut<T> &&other,
        std::source_location loc = std::source_location::current()
    ) noexcept :
        _r(other._r) {
        panic_if(!_r, loc, "Cannot move uninitialized mutable reference.");
        panic_if(
            other._borrowed, loc, "Cannot move borroed mutable reference."
        );
        other._r = nullptr;
    }

    RefMut(const RefMut<T> &) = delete ("Mutable reference cannot be copied. "
                                        "Consider using RefMut::borrow_mut.");

    /// Move assignment operator for the mutable reference. This makes the
    /// other reference uninitialized.
    ///
    /// Panics if the other reference is uninitialized or currently borrowed.
    RefMut &operator=(RefMut<T> &&other) noexcept {
        panic_if(
            !other._r,
            std::source_location::current(),
            "Cannot move uninitialized mutable reference."
        );
        panic_if(
            other._borrowed,
            std::source_location::current(),
            "Cannot move borroed mutable reference."
        );
        if (_r) {
            _r->cnts.remove_ref_mut();
        }
        _r = other._r;
        _borrowed = false;
        other._r = nullptr;
    }

    RefMut<T> &operator=(const RefMut<T> &) =
        delete ("Mutable reference cannot be copied. Consider using "
                "RefMut::borrow_mut.");

    /// Get access to the underlaying value.
    ///
    /// Panics if the reference is uninitialized or currently borrowed.
    [[nodiscard]]
    T &get(std::source_location loc = std::source_location::current()) const {
        panic_if(!_r, loc, "Cannot access uninitialized mutable reference.");
        panic_if(_borrowed, loc, "Cannot access borrowed mutable reference.");
        return _r->value;
    }

    /// Get access to the underlaying value.
    ///
    /// Panics if the reference is uninitialized or currently borrowed.
    T &operator*() const { return get(); }

    /// Access member of the underlaying value.
    ///
    /// Panics if the reference is uninitialized or currently borrowed.
    T *operator->() const { return &get(); }

    /// Transform this reference into immutable reference.
    ///
    /// Panics if the reference is uninitialized or currently borrowed.
    Ref<T> into_ref(
        std::source_location loc = std::source_location::current()
    ) {
        panic_if(!_r, loc, "Cannot transfer uninitialized mutable reference.");
        panic_if(_borrowed, loc, "Cannot access borrowed mutable reference.");
        _r->cnts.transfer_mut_to_ref();
        auto r = _r;
        _r = nullptr;
        return { r };
    }

    /// Implicit conversion of rvalue of this reference to immutable reference.
    ///
    /// Panics if the reference is uninitialized or currently borrowed.
    operator Ref<T>(this RefMut<T> &&self) { return self.into_ref(); }

    /// Borrow the reference for the duration of the given closure.
    ///
    /// Panics if the reference is uninitialized, currently borrowed or if the
    /// new borrowed reference outlives execution of the closure.
    template<typename R, typename F>
    R borrow(F f, std::source_location loc = std::source_location::current())
        requires(
            std::is_invocable_r_v<R, F, Ref<T>> &&
            std::is_move_constructible_v<R>
        )
    {
        panic_if(_borrowed, loc, "Cannot borrow borrowed mutable reference.");
        panic_if(!_r, loc, "Cannot borrow uninitialized mutable reference.");
        auto cnt = _r->cnts.ref_cnt();
        _r->cnts.add_ref();
        _borrowed = true;
        R res = f(Ref(_r));
        _borrowed = false;
        panic_if(
            _r->cnts.ref_cnt() != cnt,
            loc,
            "Borrowed reference escaped its lifetime."
        );
        return res;
    }

    /// Borrow the reference for the duration of the given closure.
    ///
    /// Panics if the reference is uninitialized, currently borrowed or if the
    /// new borrowed reference outlives execution of the closure.
    template<typename F>
    void borrow(
        F f, std::source_location loc = std::source_location::current()
    )
        requires(std::is_invocable_v<F, Ref<T>>)
    {
        panic_if(_borrowed, loc, "Cannot borrow borrowed mutable reference.");
        panic_if(!_r, loc, "Cannot borrow uninitialized mutable reference.");
        auto cnt = _r->cnts.ref_cnt();
        _r->cnts.add_ref();
        _borrowed = true;
        f(Ref(_r));
        _borrowed = false;
        panic_if(
            _r->cnts.ref_cnt() != cnt,
            loc,
            "Borrowed reference escaped its lifetime."
        );
    }

    /// Mutably borrow the reference for the duration of the given closure.
    ///
    /// Panics if the reference is uninitialized, currently borrowed or if the
    /// new borrowed reference outlives execution of the closure.
    template<typename R, typename F>
    void borrow_mut(
        F f, std::source_location loc = std::source_location::current()
    )
        requires(
            std::is_invocable_r_v<R, F, RefMut<T>> &&
            std::is_move_constructible_v<R>
        )
    {
        panic_if(
            !_r, loc, "Cannot mutably borrow uninitialized mutable reference."
        );
        panic_if(_borrowed, loc, "Cannot borrow borrowed mutable reference.");
        auto cnt = _r->cnts.ref_cnt();
        _r->cnts.add_ref();
        _borrowed = true;
        R r = f(RefMut(_r));
        _borrowed = false;
        panic_if(
            _r->cnts.ref_cnt() != cnt,
            loc,
            "Borrowed mutable reference escaped its lifetime."
        );
        return r;
    }

    /// Mutably borrow the reference for the duration of the given closure.
    ///
    /// Panics if the reference is uninitialized, currently borrowed or if the
    /// new borrowed reference outlives execution of the closure.
    template<typename F>
    void borrow_mut(
        F f, std::source_location loc = std::source_location::current()
    )
        requires(std::is_invocable_v<F, RefMut<T>>)
    {
        panic_if(
            !_r, loc, "Cannot mutably borrow uninitialized mutable reference."
        );
        panic_if(_borrowed, loc, "Cannot borrow borrowed mutable reference.");
        auto cnt = _r->cnts.ref_cnt();
        _r->cnts.add_ref();
        _borrowed = true;
        f(RefMut(_r));
        _borrowed = false;
        panic_if(
            _r->cnts.ref_cnt() != cnt,
            loc,
            "Borrowed mutable reference escaped its lifetime."
        );
    }

    /// Destroy the mutable reference.
    ~RefMut() {
        if (_r) {
            _r->cnts.remove_ref_mut();
            _r = nullptr;
        }
    }
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