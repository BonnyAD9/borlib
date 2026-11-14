#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <type_traits>
#include <vector>

namespace bor {

/// A trait struct for objects which own a slice of memory.
template<typename T> struct OwnedSlice {
    /// Items stored inside the owned slice.
    using Item = struct { };

    /// Create the trait object.
    OwnedSlice() { static_assert(false, "Not specialized."); }

    /// Get the length of the slice (in items).
    std::size_t len(const T &) { static_assert(false, "Not specalized."); }

    /// Get the data pointer to the slice.
    const Item *data(const T &) { static_assert(false, "Not specialized."); }

    /// Get mutable data pointer to the slice.
    Item *data_mut(T &) { static_assert(false, "Not specialized."); }
};

template<typename T> struct OwnedSlice<std::vector<T>> {
    using Item = T;

    OwnedSlice() = default;

    std::size_t len(const std::vector<T> &self) { return self.size(); }

    const Item *data(const std::vector<T> &self) { return self.data(); }

    Item *data_mut(std::vector<T> &self) { return self.data(); }
};

template<typename T, std::size_t N> struct OwnedSlice<std::array<T, N>> {
    using Item = T;

    OwnedSlice() = default;

    std::size_t len(const std::array<T, N> &) { return N; }

    const Item *data(const std::array<T, N> &self) { return self.data(); }

    Item *data_mut(std::array<T, N> &self) { return self.data(); }
};

template<> struct OwnedSlice<std::string> {
    using Item = char;

    OwnedSlice() = default;

    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    std::size_t len(const std::string &self) { return self.size(); }

    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    const Item *data(const std::string &self) { return self.data(); }

    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    Item *data_mut(std::string &self) { return self.data(); }
};

/// Concept for objects that specialize the trait object OwnedSlice with the
/// given item type.
template<typename T, typename I = OwnedSlice<T>::Item>
concept IsOwnedSlice = std::is_default_constructible_v<OwnedSlice<T>> &&
                       std::is_same_v<I, typename OwnedSlice<T>::Item>;

} // namespace bor