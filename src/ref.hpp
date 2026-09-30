#pragma once

#include "val_inner.hpp"

namespace bor {

template<typename T>
class Ref {
    const ValInner<T> *_r;

public:
    // TODO: constructors
    
    const T &get() const {
        return *_r->value;
    }

    const T &operator*() const {
        return get();
    }
};
    
}