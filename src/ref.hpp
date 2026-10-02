#pragma once

#include <source_location>

#include "err.hpp"
#include "val_inner.hpp"

namespace bor {

template<typename T> class Ref {
    ValInner<T> *_r{ nullptr };

public:
    Ref() = default;

    Ref(ValInner<T> &val, std::source_location loc) : _r(&val) {
        val->cnts.create_ref(loc);
    }

    Ref(Ref<T> &&other,
        std::source_location loc = std::source_location::current()) noexcept :
        _r(other._r) {
        panic_if(!_r, loc, "Cannot move uninitialized reference.");
        other._r = nullptr;
    }

    Ref(const Ref<T> &other,
        std::source_location loc = std::source_location::current()) noexcept :
        _r(other._r) {
        panic_if(!_r, loc, "Cannot copy uninitialized reference.");
        _r->cnts.add_ref();
    }

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

    const T &get(
        std::source_location loc = std::source_location::current()
    ) const {
        panic_if(!_r, loc, "Cannot access uninitialized reference.");
        return *_r->value;
    }

    const T &operator*() const { return get(); }

    const T *operator->() const { return &get(); }

    Ref<T> ref(
        std::source_location loc = std::source_location::current()
    ) const {
        return { *this, loc };
    }
};

} // namespace bor