#pragma once

#include <source_location>
#include <type_traits>

#include "ref.hpp"
#include "val_inner.hpp"

namespace bor {

template<typename T> class RefMut {
    ValInner<T> *_r{ nullptr };

    RefMut(ValInner<T> *val) : _r(val) { }

public:
    RefMut() = default;

    RefMut(ValInner<T> &val, std::source_location loc) : _r(&val) {
        val.cnts.create_ref_mut(loc);
    }

    RefMut(
        RefMut<T> &&other,
        std::source_location loc = std::source_location::current()
    ) noexcept :
        _r(other._r) {
        panic_if(!_r, loc, "Cannot move uninitialized mutable reference.");
        other._r = nullptr;
    }

    RefMut(const RefMut<T> &) = delete ("Mutable reference cannot be copied. "
                                        "Consider using RefMut::borrow_mut.");

    RefMut &operator=(RefMut<T> &&other) noexcept {
        panic_if(
            !other._r,
            std::source_location::current(),
            "Cannot move uninitialized mutable reference."
        );
        if (_r) {
            _r->cnts.remove_ref_mut();
        }
        _r = other._r;
        other._r = nullptr;
    }

    RefMut<T> &operator=(const RefMut<T> &) =
        delete ("Mutable reference cannot be copied. Consider using "
                "RefMut::borrow_mut.");

    T &get(std::source_location loc = std::source_location::current()) const {
        panic_if(!_r, loc, "Cannot access uninitialized mutable reference.");
        return _r->value;
    }

    T &operator*() const { return get(); }

    T *operator->() const { return &get(); }

    Ref<T> into_ref(
        std::source_location loc = std::source_location::current()
    ) {
        panic_if(!_r, loc, "Cannot transfer uninitialized mutable reference.");
        _r->cnts.transfer_mut_to_ref();
        auto r = _r;
        _r = nullptr;
        return { r };
    }

    operator Ref<T>(this RefMut<T> &&self) { return self.into_ref(); }

    template<typename F>
    void borrow(
        F f, std::source_location loc = std::source_location::current()
    )
        requires(std::is_invocable_v<F, Ref<T>>)
    {
        panic_if(!_r, loc, "Cannot borrow uninitialized mutable reference.");
        auto cnt = _r->cnts.ref_cnt();
        _r->cnts.add_ref();
        f(Ref(_r));
        panic_if(
            _r->cnts.ref_cnt() != cnt,
            loc,
            "Borrowed reference escaped its lifetime."
        );
    }

    template<typename F>
    void borrow_mut(
        F f, std::source_location loc = std::source_location::current()
    )
        requires(std::is_invocable_v<F, RefMut<T>>)
    {
        panic_if(
            !_r, loc, "Cannot mutably borrow uninitialized mutable reference."
        );
        auto cnt = _r->cnts.ref_cnt();
        _r->cnts.add_ref();
        f(RefMut(_r));
        panic_if(
            _r->cnts.ref_cnt() != cnt,
            loc,
            "Borrowed mutable reference escaped its lifetime."
        );
    }

    ~RefMut() {
        if (_r) {
            _r->cnts.remove_ref_mut();
            _r = nullptr;
        }
    }
};

} // namespace bor

namespace std {

template<typename T> struct formatter<bor::RefMut<T>> {
    constexpr auto parse(format_parse_context &ctx) { return _fmt.parse(ctx); }

    auto format(const bor::RefMut<T> &v, format_context &ctx) const {
        _fmt.format(*v, ctx);
    }

private:
    formatter<T> _fmt;
};

} // namespace std