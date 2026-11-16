#include <cassert>
#include <exception>
#include <format>
#include <iostream>
#include <print>
#include <vector>

#include "ref.hpp"
#include "val.hpp"

namespace bor {

static void start();
static void tryout();
static void test_panic();
static void test_no_panic();

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

static void tryout() {
    test_no_panic();
    test_panic();
}

// The following tests verify that the code panics where it is expected.

static void test_dangling() {
    Ref<int> ref;

    {
        Val<int> n = 5;
        ref = &n;
        std::println("Value goes out of scope.");
    } // borrow violation: Cannot destroy value that is borrowed.

    std::println("I will now print ref.");
    std::println("{}", *ref);
}

static void test_multi_borrow() {
    Val<int> n = 5;

    std::println("Creating 3 references.");
    const auto ref1 = &n;
    const auto ref2 = &n;
    auto ref3 = ref1;
    ref3 = ref1;
    std::println("Values are: {} {} {}", *ref1, *ref2, *ref3);

    std::println("Creating mutable reference.");
    auto mref = n.ref_mut(); // borrow violation: Cannot mutably borrow value
                             // that is already borrowed.

    std::println("Mutating reference.");
    *mref = 20;
}

static void test_read_mut() {
    Val<int> n = 5;

    std::println("Mutably borrowing value.");
    auto mref = n.ref_mut();
    *mref = 10;

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

static void test_multi_borrow2() {
    Val<int> n = 5;

    std::println("Creating mutable reference.");
    auto mref = n.ref_mut();
    *mref = 10;

    std::println("Creating immutable reference.");
    auto ref = &n;

    std::println("Accessing value.");
    std::println("{}", ref);
}

static void test_slice() {
    Val<std::vector<int>> vec{ std::vector{ 1, 2, 3, 4, 5 } };

    const auto slice = vec[1, 4];

    std::println("2nd item: {}", slice[1]);

    std::println("4th item: {}", slice[3]); // borrow violation: Sliceout of
                                            // bounds access 3/3
}

static void test_slice_mut() {
    Val<std::vector<int>> vec{ std::vector{ 1, 2, 3, 4, 5 } };

    const auto slice = vec.slice_mut(1, 4);

    slice[1] = 9;

    std::println("2nd item: {}", slice[1]);

    std::println("4th item: {}", vec[3]); // borrow violation: Cannot borrow
                                          // value that is already borrowed
                                          // mutably.
}

static void test_panic() {
    // Only the first test is always tested, because the code panics and other
    // tests won't run.

    test_slice_mut();
    test_slice();
    test_dangling();
    test_borrow_escape();
    test_multi_borrow2();
    test_borrow_infiltrate();
    test_read_mut();
    test_multi_borrow();
}

// The goal of the following tests is to make sure that all template code is
// forced to generate to verify that it compiles, and to test the basic
// functionality where valid use cases don't panic.

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
static void test_full_val() {
    Val<int> v1;
    Val<int> v2{ 5 };
    Val<int> v3{ v2 };
    Val<int> v4{ std::move(v3) };

    assert(v2 == 5);
    assert(v4 == 5);

    v1 = std::move(v2);
    v2 = v1;

    assert(v1 == 5);
    assert(v2 == 5);

    v2.set(7);
    // NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
    v3.set(std::move(v1));
    // NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
    v1.set(v2);

    assert(v1 == 7);
    assert(v2 == 7);
    assert(v3 == 5);

    v4.get_mut() = 11;
    assert(v4.get() == 11);

    {
        const auto r1 = v1.ref();
        assert(*r1 == 7);
    }

    {
        auto r1 = v1.ref_mut();
        assert(*r1 == 7);
        *r1 = 13;
    }

    assert(v1 == 13);

    Val<std::vector<int>> vec{ std::vector{
        3,
        1,
        4,
        1,
        5,
        9,
        2,
        6,
        5,
        3,
        5,
    } };

    {
        const auto s1 = vec.slice();
        assert(s1.len() == 11);
        assert(*s1[8] == 5);

        assert(*vec[4] == 5);

        const auto s2 = vec[4, 8];
        assert(s2.len() == 4);
        assert(*s2[2] == 2);
    }

    {
        const auto s = vec.slice_mut();
        assert(s.len() == 11);
        s[8] = 8;
    }

    assert(*vec[8] == 8);

    {
        auto r = vec.index_mut(4);
        *r = 4;
    }

    assert(*vec[4] == 4);

    {
        const auto s = vec.slice_mut(4, 8);
        assert(s.len() == 4);
        s[2] = 5;
    }

    assert(*vec[6] == 5);
    assert(std::format("{}", v1) == "13");
}

static void test_full_ref() {
    {
        const Ref<int> r;
    }

    Val<int> val = 5;
    Ref<int> r1 = &val;
    Ref<int> r2{ r1 };

    assert(*r1 == 5);
    assert(*r2 == 5);

    r1 = r2;
    r2 = std::move(r1);

    assert(*r2 == 5);
    assert(r2.get() == 5);
    assert(r2.operator->() == &r2.get());

    Val<std::vector<int>> vec{ std::vector{
        3,
        1,
        4,
        1,
        5,
        9,
        2,
        6,
        5,
        3,
        5,
    } };
    const auto vr = vec.ref();

    assert(*vr[5] == 9);
    assert(std::format("{}", r2) == "5");
}

static void test_no_panic() {
    test_full_val();
    test_full_ref();
}

} // namespace bor
