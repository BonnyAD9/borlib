#pragma once

#include <cstddef>
#include <optional>
#include <source_location>
#include <stacktrace>
#include <string>
#include <variant>

namespace bor {

/// An extension of `std::source_location` which is capable of using the
/// stacktrace.
class SourcePos {
    struct Pos {
        std::string file;
        std::size_t line;
        std::optional<std::size_t> col;
    };

    std::variant<std::size_t, std::source_location, Pos> _pos;

    Pos &get_pos() {
        switch (_pos.index()) {
        case 0: {
            auto idx = std::get<std::size_t>(_pos);
            auto trace = std::stacktrace::current(idx + 1, 1);
            auto frame = trace.at(0);
            _pos = Pos{
                .file = frame.source_file(),
                .line = frame.source_line(),
                .col = std::nullopt,
            };
            break;
        }
        case 1: {
            auto loc = std::get<std::source_location>(_pos);
            _pos = Pos{
                .file = loc.file_name(),
                .line = loc.line(),
                .col = loc.column(),
            };
            break;
        }
        default:
            break;
        }

        return std::get<Pos>(_pos);
    }

    SourcePos(std::source_location loc) : _pos(loc) { }

    SourcePos(std::size_t depth) : _pos(depth) { }

public:
    /// Get the current source location. This is determined at compile time.
    static SourcePos current(
        std::source_location loc = std::source_location::current()
    ) {
        return loc;
    }

    /// Get the source location of the caller.
    /// 
    /// This will save the callers stack trace depth to be retrieved later.
    static SourcePos caller() { return 2; }

    /// Get the source location of the caller and call inc.
    /// 
    /// This will save the callers stack trace depth to be retrieved later.
    static SourcePos caller2() { return 3; }

    /// Increase the depth on the stack. This is necesary to call when passing
    /// the SourcePos deeper to function so that the source location is
    /// properly retrieved from the stack trace.
    SourcePos inc() {
        auto res = *this;
        auto v = std::get_if<std::size_t>(&res._pos);
        if (v) {
            ++*v;
        }
        return res;
    }

    /// Get the file name.
    std::string_view file() { return get_pos().file; }

    /// Get the line number.
    std::size_t line() { return get_pos().line; }

    /// Get the column. This may be 0 if the column is unknown.
    std::size_t col() { return get_pos().col.value_or(0); }
};

} // namespace bor