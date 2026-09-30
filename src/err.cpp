#include "err.hpp"

#include <mutex>

namespace bor {

static std::mutex exit_mutex;

[[noreturn]]
void panic(const int status) {
    exit_mutex.lock();
    // Exit is guarded by mutex so this is safe.
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    std::exit(status);
}

} // namespace bor