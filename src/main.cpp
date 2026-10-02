#include <exception>
#include <iostream>
#include <print>

#include "ref.hpp"
#include "val.hpp"

namespace bor {

static void start();
static void tryout();

} // namespace bor

int main() {
    try {
        bor::start();
    } catch (std::exception &ex) {
        try {
            std::println(std::cerr, "{}", ex.what());
            // NOLINTNEXTLINE(bugprone-empty-catch)
        } catch (...) {
        }
    } catch (...) {
        try {
            std::println(std::cerr, "An unknown exception occured.");
            // NOLINTNEXTLINE(bugprone-empty-catch)
        } catch (...) {
        }
    }
}

namespace bor {

static void start() {
    tryout();
}

static void test_dangling() {
    Ref<int> ref;

    {
        Val<int> n = 5;
        ref = &n;
        std::println("Value goes out of scope.");
        // borrow violation: Cannot destroy value that is borrowed.
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

static void test_borrow_escape() {
    Val<int> n = 5;
    auto mref = n.ref_mut();
    auto ref = mref.borrow<Ref<int>>([](auto r) {
        std::println("Borrowed the value {}", r);
        return r;
        // borrow violation: Borrowed reference escaped its lifetime.
    });

    std::println("Reading borrowed reference.");
    std::println("{}", ref);
}

static void test_borrow_infiltrate() {
    Val<int> n = 5;
    auto mref = n.ref_mut();
    mref.borrow([&](const auto &) {
        std::println("Accessing original reference from borrow context.");
        std::println("{}", mref); // borrow violation: Cannot access borrowed
                                  // mutable reference.
    });
}

static void tryout() {
    test_borrow_escape();
    test_borrow_infiltrate();
    test_read_mut();
    test_multi_borrow();
    test_dangling();
}

} // namespace bor
