#pragma once

#include "defines.hpp"

BOR() // Disables warning on unused "defines.hpp"

#if BOR_CHECK

# include <atomic>
# include <format>
# include <source_location>

# include "err.hpp"

namespace bor::detail {

/// Internal type for reference counting of borlib references.
class Counters {
    /// Number of total references, including mutable.
    std::atomic_size_t _ref;
    /// True if any mutable reference exists.
    std::atomic_bool _mut;

    constexpr Counters(std::size_t ref, bool mut) : _ref(ref), _mut(mut) { }

public:
    /// Creates empty counters.
    constexpr Counters() = default;

    /// Check the given condition and construct the counters.
    ///
    /// Panics if `cond` is `false`.
    template<typename... Args>
    Counters(
        bool cond,
        std::source_location loc,
        std::format_string<Args...> fmt,
        Args &&...args
    ) noexcept {
        panic_if(cond, loc, fmt, args...);
    }

    /// Checks if moving the value guarded by `of` is valid and returns new
    /// counters.
    ///
    /// Panics if the move out of `of` is not valid.
    [[nodiscard]]
    static Counters move_out(const Counters &of, SourcePos loc) noexcept {
        of.check_move_out(loc.inc());
        return {};
    }

    /// Checks if copying the value guarded by `of` is valid and returns new
    /// counters.
    ///
    /// Panics if the copy of `of` is not valud.
    [[nodiscard]]
    static Counters copy_out(const Counters &of, SourcePos loc) noexcept {
        of.check_copy_out(loc.inc());
        return {};
    }

    [[nodiscard]]
    static Counters borrowed() noexcept {
        return { 1, false };
    }

    [[nodiscard]]
    static Counters borrowed_mut() noexcept {
        return { 1, true };
    }

    /// Checks if there is any reference.
    [[nodiscard]]
    bool any_ref(
        std::memory_order ord = std::memory_order_acquire
    ) const noexcept {
        return _ref.load(ord) != 0;
    }

    /// Checks if there is any mutable reference.
    [[nodiscard]]
    bool is_mut(
        std::memory_order ord = std::memory_order_acquire
    ) const noexcept {
        return _mut.load(ord);
    }

    /// Checks if move of the value is valid.
    ///
    /// Panics if moving the valus is not valid.
    void check_move_out(SourcePos loc) const noexcept {
        panic_if(
            any_ref(), loc.inc(), "Cannot move out of value that is borrowed."
        );
    }

    /// Checks if copy of the value is valid.
    ///
    /// Panics if copying the valus is not valid.
    void check_copy_out(SourcePos loc) const noexcept {
        panic_if(
            is_mut(), loc.inc(), "Cannot copy value that is mutably borrowed."
        );
    }

    /// Checks if writing the value is valid.
    ///
    /// Panics if writing the valus is not valid.
    void check_write(SourcePos loc) const noexcept {
        panic_if(
            is_mut(), loc.inc(), "Cannot write to value that is borrowed."
        );
    }

    /// Checks if reading the value is valid.
    ///
    /// Panics if reading the valus is not valid.
    void check_read(SourcePos loc) const noexcept {
        panic_if(
            any_ref(), loc.inc(), "Cannot read value that is mutably borrowed."
        );
    }

    /// Checks if destroying the value is valid.
    ///
    /// Panics if destroying the value is not valid.
    void check_destroy(SourcePos loc) const noexcept {
        panic_if(
            any_ref(), loc.inc(), "Cannot destroy value that is borrowed."
        );
    }

    /// Checks if creating new reference is valid and updates the counters.
    ///
    /// Panics if creating new reference is not valid.
    void create_ref(SourcePos loc) noexcept {
        _ref.fetch_add(1, std::memory_order_acquire);
        if (_mut.load(std::memory_order_acquire)) {
            _ref.fetch_sub(1, std::memory_order_release);
            panic(
                loc.inc(),
                "Cannot borrow value that is already borrowed mutably."
            );
        }
    }

    /// Increases the reference counter.
    void add_ref() noexcept { _ref.fetch_add(1, std::memory_order_acquire); }

    /// Decrease the reference counter.
    void remove_ref() noexcept {
        if (_ref.fetch_sub(1, std::memory_order_release) == 1) {
            _mut.store(false, std::memory_order_release);
        }
    }

    /// Checks if creating mutable reference is valid and updates the counters.
    ///
    /// Panics if creating new mutable reference is not valid.
    void create_ref_mut(SourcePos loc) noexcept {
        panic_if(
            _mut.exchange(true, std::memory_order_acquire),
            loc.inc(),
            "Cannot mutably borrow value that is already mutably borrowed."
        );
        panic_if(
            _ref.fetch_add(1) > 0,
            loc.inc(),
            "Cannot mutably borrow value that is already borrowed."
        );
    }

    /// Transforms mutable reference to an immutable reference.
    void transfer_mut_to_ref() noexcept {
        if (_ref.load() == 1) {
            _mut.store(false, std::memory_order_release);
        }
    }

    /// Gets the number of references.
    [[nodiscard]]
    std::size_t ref_cnt() const noexcept {
        return _ref.load();
    }
};

} // namespace bor::detail

#endif // BOR_CHECK == 1