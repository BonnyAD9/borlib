#include <print>

#include "ref.hpp"
#include "val.hpp"

namespace bor {

static void tryout();

}

int main() {
    bor::tryout();
}

namespace bor {

static void test_dangling() {
    Ref<int> ref;

    {
        Val<int> n = 5;
        ref = &n;
        std::println("Value goes out of scope.");
        // borrow violation: Cannto destroy value thatis borrowed.
    }

    std::println("I will now print ref.");
    std::println("{}", *ref);
}

static void test_multi_borrow() {
    Val<int> n = 5;

    std::println("Creating 3 references.");
    auto ref1 = &n;
    auto ref2 = &n;
    auto ref3 = ref1;
    ref3 = ref1;
    std::println("Values are: {} {} {}", *ref1, *ref2, *ref3);

    std::println("Creating mutable reference.");
    auto mref = n.ref_mut(); // Cannot mutably borrow value that is already
                             // borrowed.

    std::println("Mutating reference.");
    *mref = 20;
}

static void test_read_mut() {
    Val<int> n = 5;

    std::println("Mutably borrowing value.");
    auto mref = n.ref_mut();

    std::println("Reading value from the original.");
    std::println("{}", n); // borrow violation: Cannot read value that is
                           // mutably borrowed.
}

static void tryout() {
    test_read_mut();
    test_multi_borrow();
    test_dangling();
}

} // namespace bor
