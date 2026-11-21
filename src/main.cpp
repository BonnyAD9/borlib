#include <cassert>
#include <exception>
#include <format>
#include <iostream>
#include <print>
#include <thread>
#include <vector>

#include "ref.hpp"
#include "slice_mut.hpp"
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
    // NOLINTNEXTLINE(readability-simplify-boolean-expr)
    if (true) {
        test_panic();
    }
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

static void test_race() {
    Val<int> v = 5;
    const auto foo = [](RefMut<int> mr) { *mr = 7; };
    std::thread t{ foo, v.ref_mut() };
    std::println("value: {}", v.get());
    t.join();
}

static void test_split() {
    Val<std::vector<int>> vec = std::vector{ 3, 1, 4 };
    auto [s1, s2] = vec.slice_mut().split({ 0, 2 }, { 1, 3 });
}

static void test_race2() {
    Val<int> v = 5;
    auto r1 = v.ref_mut();
    auto r2 = v.ref_mut();

    const auto foo = [](auto mr, int v) { *mr = v; };
    std::thread t1{ foo, std::move(r1), 7 };
    std::thread t2{ foo, std::move(r2), 9 };

    t1.join();
    t2.join();
}

static void test_panic() {
    // Only the first test is always tested, because the code panics and other
    // tests won't run.

    test_race2();
    test_split();
    test_borrow_escape();
    test_race();
    test_dangling();
    test_slice_mut();
    test_slice();
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

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
static void test_full_ref() {
    {
        const Ref<int> r;
        assert(!r);
    }

    Val<int> val = 5;
    Ref<int> r1 = &val;
    Ref<int> r2{ r1 };

    assert(*r1 == 5);
    assert(*r2 == 5);

    r1 = r2;
    r2 = std::move(r1);

    // NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
    assert(!r1);
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

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
static void test_full_ref_mut() {
    {
        const RefMut<int> r;
        assert(!r);
    }

    Val<int> val = 5;

    {
        auto r = val.ref_mut();
        *r = 7;
    }

    assert(val == 7);

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
        auto r = vec.index_mut(6);
        assert(*r == 2);
        *r = 11;
    }

    assert(*vec[6] == 11);

    {
        auto r1 = val.ref_mut();
        RefMut<int> r2{ std::move(r1) };

        assert(*r2 == 7);
        *r2 = 13;

        r1 = std::move(r2);

        // NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
        assert(!r2);
        assert(r1.get() == 13);

        r1.get() = 17;

        assert(*r1.operator->() == 17);

        const auto r3 = r1.into_ref();

        // NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
        assert(!r2);
        assert(*r3 == 17);
    }

    assert(val == 17);

    {
        const Ref<int> r = val.ref_mut();
        assert(*r == 17);
    }

    {
        auto r = val.ref_mut();

        // NOLINTNEXTLINE(performance-unnecessary-value-param)
        auto res = r.borrow<int>([](auto br) {
            // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
            const Ref<int> br2 = br;
            return *br * 2;
        });

        assert(res == 34);

        // NOLINTNEXTLINE(performance-unnecessary-value-param)
        r.borrow([](auto br) { assert(*br == 17); });

        res = r.borrow_mut<int>([](auto br) {
            const auto rv = *br * 2;
            *br = 19;
            return rv;
        });

        assert(res == 34);
        assert(*r == 19);

        r.borrow_mut([](auto br) { *br = 23; });

        assert(*r == 23);
        assert(std::format("{}", r) == "23");
    }

    assert(val == 23);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
static void test_full_slice() {
    {
        const Slice<int> s;
        assert(!s);
    }

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

    auto r = vec.ref();
    Slice<int> s1{ r };
    Slice<int> s2{ std::move(r) };
    const Slice<int> s3{ std::move(s1) };
    const Slice<int> s4{ s2 };

    // NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
    assert(!s1);
    assert(s2);
    assert(s3);
    assert(s4);
    assert(*s2[0] == 3);
    assert(*s3[1] == 1);
    assert(*s4[2] == 4);

    s1 = std::move(s2);

    assert(s1);
    // NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
    assert(!s2);
    assert(*s1[3] == 1);

    s2 = s3;

    assert(s2);
    assert(s3);
    assert(*s2[4] == 5);
    assert(s1.len() == 11);

    const auto span = s1.as_span();

    assert(span.size() == 11);
    assert(span[5] == 9);

    s2 = s1[5, 9];

    assert(*s2[1] == 2);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
static void test_full_slice_mut() {
    {
        const SliceMut<int> s;
        assert(!s);
    }

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
        SliceMut<int> s1{ vec.ref_mut() };

        assert(s1.len() == 11);
        assert(s1[0] == 3);

        s1[0] = 2;
        SliceMut<int> s2{ std::move(s1) };

        // NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
        assert(!s1);
        assert(s2[1] == 1);

        s2[1] = 3;
        s1 = std::move(s2);

        // NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
        assert(!s2);
        assert(s1[2] == 4);

        s1[2] = 5;

        const auto s = s1.as_span();

        assert(s.size() == 11);
        assert(s[3] == 1);

        s[3] = 7;

        auto r = std::move(s1)[4];

        // NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
        assert(!s1);
        assert(*r == 5);

        *r = 11;
    }

    assert(*vec[0] == 2);
    assert(*vec[1] == 3);
    assert(*vec[2] == 5);
    assert(*vec[3] == 7);
    assert(*vec[4] == 11);

    {
        auto s1 = vec.slice_mut();
        auto s2 = std::move(s1)[4, 10];

        // NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
        assert(!s1);
        assert(s2.len() == 6);
        assert(s2[1] == 9);

        s2[1] = 13;

        const auto slic = s2.into_slice();

        assert(slic.len() == 6);
        assert(*slic[2] == 2);
    }

    assert(*vec[5] == 13);

    {
        auto s = vec.slice_mut();

        // NOLINTNEXTLINE(performance-unnecessary-value-param)
        auto res = s.borrow<int>(7, [](auto r) { return *r; });

        assert(res == 6);

        // NOLINTNEXTLINE(performance-unnecessary-value-param)
        res = s.borrow<int>([](auto bs) { return *bs[8]; });

        assert(res == 5);

        // NOLINTNEXTLINE(performance-unnecessary-value-param)
        res = s.borrow<int>(8, 11, [](auto bs) {
            assert(bs.len() == 3);
            return *bs[1];
        });

        assert(res == 3);

        // NOLINTNEXTLINE(performance-unnecessary-value-param)
        s.borrow(10, [](auto r) { assert(*r == 5); });

        // NOLINTNEXTLINE(performance-unnecessary-value-param)
        s.borrow([](auto bs) { assert(*bs[0] == 2); });

        // NOLINTNEXTLINE(performance-unnecessary-value-param)
        s.borrow(1, 7, [](auto bs) { assert(*bs[0] == 3); });

        res = s.borrow_mut<int>(2, [](auto bs) {
            const auto res = *bs * 2;
            *bs = 17;
            return res;
        });

        assert(res == 10);

        res = s.borrow_mut<int>([](auto bs) {
            const auto res = bs[3] * 2;
            bs[3] = 19;
            return res;
        });

        assert(res == 14);

        res = s.borrow_mut<int>(2, 9, [](auto bs) {
            const auto res = bs[2] * 2;
            bs[2] = 23;
            return res;
        });

        assert(res == 22);

        s.borrow_mut(5, [](auto r) { *r = 29; });

        assert(s[5] == 29);

        s.borrow_mut([](auto r) { r[6] = 31; });

        assert(s[6] == 31);

        s.borrow_mut(7, 8, [](auto r) {
            assert(r.len() == 1);
            r[0] = 37;
        });

        assert(s[7] == 37);
    }

    assert(*vec[2] == 17);
    assert(*vec[3] == 19);
    assert(*vec[4] == 23);
    assert(*vec[5] == 29);
    assert(*vec[6] == 31);
    assert(*vec[7] == 37);

    {
        auto [s1, s2] = vec.slice_mut().split({ 2, 9 }, { 9, 11 });

        assert(s1.len() == 7);
        assert(s1[6] == 5);
        assert(s2.len() == 2);
        assert(s2[0] == 3);

        s1[6] = 41;
        s2[0] = 43;
    }

    assert(*vec[8] == 41);
    assert(*vec[9] == 43);
}

static void test_no_panic() {
    test_full_val();
    test_full_ref();
    test_full_ref_mut();
    test_full_slice();
    test_full_slice_mut();
}

} // namespace bor
