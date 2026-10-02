#pragma once

#include <atomic>
#include <format>
#include <source_location>

#include "err.hpp"
namespace bor {

class Counters {
    std::atomic_size_t _ref;
    std::atomic_bool _mut;

public:
    Counters() = default;

    template<typename... Args>
    Counters(
        bool cond,
        std::source_location loc,
        std::format_string<Args...> fmt,
        Args &&...args
    ) {
        panic_if(cond, loc, fmt, args...);
    }

    [[nodiscard]]
    static Counters move_out(const Counters &of, std::source_location loc) {
        of.check_move_out(loc);
        return {};
    }

    [[nodiscard]]
    static Counters copy_out(const Counters &of, std::source_location loc) {
        of.check_copy_out(loc);
        return {};
    }

    [[nodiscard]]
    bool any_ref(std::memory_order ord = std::memory_order_acquire) const {
        return _ref.load(ord) != 0;
    }

    [[nodiscard]]
    bool is_mut(std::memory_order ord = std::memory_order_acquire) const {
        return _mut.load(ord);
    }

    void check_move_out(std::source_location loc) const {
        panic_if(any_ref(), loc, "Cannot move out of value that is borrowed.");
    }

    void check_copy_out(std::source_location loc) const {
        panic_if(is_mut(), loc, "Cannot copy value that is mutably borrowed.");
    }

    void check_write(std::source_location loc) const {
        panic_if(is_mut(), loc, "Cannot write to value that is borrowed.");
    }

    void check_read(std::source_location loc) const {
        panic_if(
            any_ref(), loc, "Cannot read value that is mutably borrowed."
        );
    }

    void check_destroy(std::source_location loc) const {
        panic_if(any_ref(), loc, "Cannot destroy value that is borrowed.");
    }

    void create_ref(std::source_location loc) {
        _ref.fetch_add(1, std::memory_order_acquire);
        if (_mut.load(std::memory_order_acquire)) {
            _ref.fetch_sub(1, std::memory_order_release);
            panic(
                loc, "Cannot borrow value that is already borrowed mutably."
            );
        }
    }

    void add_ref() { _ref.fetch_add(1, std::memory_order_acquire); }

    void remove_ref() { _ref.fetch_sub(1, std::memory_order_release); }

    void create_ref_mut(std::source_location loc) {
        panic_if(
            _mut.exchange(true, std::memory_order_acquire),
            loc,
            "Cannot mutably borrow value that is already mutably borrowed."
        );
        panic_if(
            _ref.fetch_add(1) > 0,
            loc,
            "Cannot mutably borrow value that is already borrowed."
        );
    }

    void remove_ref_mut() {
        if (_ref.fetch_sub(1, std::memory_order_release) == 0) {
            _mut.store(false, std::memory_order_release);
        }
    }

    void transfer_mut_to_ref() {
        if (_ref.load() == 1) {
            _mut.store(false, std::memory_order_release);
        }
    }

    std::size_t ref_cnt() { return _ref.load(); }
};

} // namespace bor