#pragma once

#include <source_location>
#include <type_traits>

#include "ref.hpp"
#include "ref_mut.hpp"
#include "val_inner.hpp"

namespace bor {

template<typename T> class Val {
    ValInner<T> _v;

public:
    Val(T value) : _v(value) { }

    Val(Val<T> &&other,
        std::source_location loc = std::source_location::current()) noexcept
        requires(std::is_move_constructible_v<T>)
        : _v(std::move(other._v), loc) { }

    Val(const Val<T> &other,
        std::source_location loc = std::source_location::current()) noexcept
        requires(std::is_copy_constructible_v<T>)
        : _v(other._v, loc) { }

    Val<T> &operator=(Val<T> &&other) noexcept
        requires(std::is_move_assignable_v<T>)
    {
        set(std::move(other));
    }

    Val<T> &operator=(const Val<T> &other) noexcept
        requires(std::is_copy_assignable_v<T>)
    {
        set(other);
    }

    void set(
        Val<T> &&other,
        std::source_location loc = std::source_location::current()
    ) noexcept
        requires(std::is_move_assignable_v<T>)
    {
        _v.set(std::move(other._v), loc);
    }

    void set(
        const Val<T> &other,
        std::source_location loc = std::source_location::current()
    ) noexcept
        requires(std::is_copy_assignable_v<T>)
    {
        _v.set(other._v, loc);
    }

    operator T()
        requires(std::is_copy_constructible_v<T>)
    {
        _v.cnts.check_read(std::source_location::current());
        return _v.value;
    }

    Ref<T> ref(std::source_location loc = std::source_location::current()) {
        return { _v, loc };
    }

    RefMut<T> ref_mut(
        std::source_location loc = std::source_location::current()
    ) {
        return { _v, loc };
    }

    Ref<T> operator&() const { return ref(); }

    RefMut<T> operator&() { return ref_mut(); }

    ~Val() { _v.cnts.check_destroy(std::source_location::current()); }
};

} // namespace bor