#import "@preview/charged-ieee:0.1.4": ieee

#let todo(msg) = {
  [#text(fill: red, weight: "bold", size: 12pt)[TODO #msg]]
}

#show: ieee.with(
  title: [Smart references for checking Rust borrowing rules in C++],
  abstract: [
    This paper describes a project implementing _borlib_, a C++ header-only
    library with smart pointers that check ownership rules and rules of
    references used in Rust. Unlike Rust, the smart pointers perform the checks
    at runtime. Violation of the rules will lead to process termination with a
    helpful error message and source location as is shown in examples at the
    end.
  ],
  authors: (
    (
      name: "Bc. Jakub Antonín Štigler",
      // department: [Co-Founder],
      // organization: [Typst GmbH],
      location: [Brno, Czech Republic],
      email: "xstigl00@vutbr.cz"
    ),
  ),
  index-terms: (
    "Borlib", "Rust", "Ownership rules", "Rules of references",
    "Borrowing rules", "C++", "Smart pointers"
  ),
  bibliography: bibliography("refs.yaml"),
  figure-supplement: [Figure],
  paper-size: "a4",
)

#let codeblock(body, caption: none, lineNum:false) = {
  if lineNum {
    show raw.where(block:true): it =>{
      set par(justify: false)
      block(fill: luma(240),inset: 0.3em,radius: 0.3em,
        // grid size: N*2
        grid(
          columns: 2,
          align: left+top,
          column-gutter: 0.5em,
          stroke: (x,y) => if x==0 {( right: (paint:gray, dash:"densely-dashed") )},
          inset: 0.3em,
          ..it.lines.map((line) => (str(line.number), line.body)).flatten()
        )
      )
    }
    figure(body, caption: caption, kind: "code", supplement: "Code")
  }
  else{
    figure(body, caption: caption, kind: "code", supplement: "Code")
  }
}

= Introduction

It is hard to write correct code, and some bugs in code may manifest in special
situations that may occur sparsely. This is why it may be beneficial to
restrict how code can be written to avoid patterns that may lead to undefined
behaviour. One example of a language that takes this approach is Rust. Rust
places restrictions on how the programmer can manage memory in the form of
ownership rules and rules of references. If one can prove that a program
follows these rules, it is possible to prove that certain kinds of undefined
behaviour cannot occur in the program. Rust tries to verify the rules at
compile time. This project describes a C++ header-only library that provides
smart pointers that check these rules at runtime. Checking at runtime is still
beneficial because the problem is usually caught before it could manifest.

#ref(<rs-bor>) describes ownership rules and rules of references as they are in
Rust. #ref(<cpp-bor>) describes how it is possible to implement checking of the
rules in C++ at runtime and describes _borlib_, a new header-only library that
implements checking of the rules in C++ at runtime. #ref(<show>) showcases some
examples where violations of the borrowing rules are caught by borlib.

= Rust borrowing rules <rs-bor>

There are several rules concerning ownership and references in the Rust
programming language. All of them are described in #cite(<therust>) either
as _Ownership Rules_ or _Rules of References_. Together they enforce a memory
model which prevents several kinds of common bugs related to memory management
and memory access. The term _borrowing rules_ will be used to refer to both the
ownership rules and rules of references together.

Unless otherwise specified, this chapter summarizes information from
#cite(<therust>). The ownership rules are described in detail in
#ref(<own-rules>) and the rules of references are described in detail in
#ref(<ref-rules>).

== Ownership rules <own-rules>

The purpose of the ownership rules is primarily memory management. Although it
is most commonly associated with heap memory allocation, it can be any other
kind of resource which the program uses and has to be freed such as file
descriptors on Unix systems or handles on Windows systems.

Some programming languages (e.g. _C_) don't have any kind of automatic memory
management. The memory management is fully in the hands of the programmer. This
gives a lot of power into the hands of the programmer and so well written
programs can be very efficient. The problem is that people are prone to making
errors and incorrect memory management can lead to vulnerabilities in the
program. Examples include _use after free_ and _memory leaks_.

Another approach is to automate the memory management with a garbage collector
(e.g. in _C\#_). In most cases the programmer doesn't have to think about when
is the correct time to free an allocated resource. This in most cases avoids
the related bugs such as memory leaks or use after free, but the resulting
program is not as efficient because garbage collectors have to periodically
check the memory used by the program and free any unreachable memory. There are
many algorithms for garbage collecting but none of them can minimize processor
time, paused time of the program and memory usage at the same time.

The ownership rules place restrictions on how memory is organized within the
program so that it is possible to correctly determine when the memory should be
freed at compile time without it being specified by the programmer. This has
the advantage of not being required to manually specify when memory has to be
freed and at the same time not paying the cost of having a garbage collector.

The ownership rules are:
- Each value in Rust has an _owner_.
- There can only be one owner at a time.
- When the owner goes out of scope, the value will be dropped.

In Rust terminology _dropping_ a value refers to freeing the resources that the
value owns (e.g. allocated memory). An owner is another value that is
responsible for the owned value. When owner is dropped, all of its owned values
are also dropped. On the top level the owner is the program stack. It is not
possible to create something as an owner cycle.

At first it may seem that restricting memory organisation within a program
may result in suboptimal solutions because some memory layouts may not be
expressible under these restrictions. In reality the rules restrict the memory
organisation to organisation that is actually commonly used when memory is
managed manually. For cases where the rules are too restrictive, Rust has the
option of writing code within `unsafe` blocks which allow breaking the rules
within the scope of the `unsafe` block. This allows for implementing correct
data types that internally don't align with the ownership rules. The types
themselves don't require the usage of the `unsafe` block, because they are
correct from the outside. At first glance, unsafe blocks may appear to
undermine Rust's safety guarantees. In practice, however, they are rarely
required, and their use cleary identifies code that deserves additional
attention. One example of type that circumvents the rules is `Arc` which allows
shared ownership and is implemented with reference counting.

Rust allows compliance with the ownership rules by providing the `Drop` trait
which has a similar job to that of destructors in other languages: it allows
specifying code that should run when a value of a given type is dropped. The
rules are also enforced by allowing the allocation of unowned resources only in
`unsafe` blocks. Even though Rust has the ownership rules, leaking a resource
is not considered an undefined behaviour, so there it is possible to leak
memory without the use of `unsafe` block. However, the API that allows this is
always very explicit about this behaviour because most of the time the only
purpose of such APIs is to leak the memory (e.g. the function `Box::leak`).

== Rules of references <ref-rules>

Ownership rules by themselves mainly prevent memory leaks. For the rest of the
guarantees that Rust makes about safe code, rules of references are required.
Complying with the rules of references guarantees that any memory access is
valid and without data races.

The rules of references are:
- At any given time, you can have either one mutable reference or any number of
  immutable references.
- References must always be valid.

The first rule prevents data races, as there can only be one mutable reference
to the same value at any given time. The second rule guarantees valid memory
access through references. The second rule disallows for example null
references, use after free and out of bounds access.

In comparison to the ownership rules, rules of references are much more
limiting and they are much harder to enforce at compile time. Rust enforces
them with the use of _lifetimes_. Lifetime is a compile time information that
is associated with references. In many ways it behaves similarly to a generic
parameter (as in compile time specialization) but it doesn't specialize the
items with which it is associated. It is used only to mark the origin of the
memory to which the item refers so that it is possible to verify that the
program doesn't break any of the rules of references.

In some cases, these rules are too restrictive for some use cases especially
when using multiple threads. This is why Rust provides _raw pointers_ which
don't have to follow the rules. In order to make code that circumvents the
rules more explicit, Rust allows the use of raw pointers only within an
`unsafe` block. In order to maintain its safety, `unsafe` blocks are only used
to implement internal details of types or an API that is safe to use, even
though it internally breaks the rules of references to achieve optimal
implementation. An example is the type `Mutex<T>` which wraps a generic type
`T`. `Mutex` allows one to obtain a mutable reference to the internal type `T`
even if you have an immutable reference to `Mutex` itself. Internally mutex
uses a lock to ensure that the immutable access is exclusive and so it doesn't
break the rules from the outside even if the internal implementation requires
to convert immutable reference to mutable reference.

It is very difficult to properly verify that the program follows the rules at
compile time without being too restrictive. Rust itself is at this time
considered _unsound_ because the compiler can be tricked to accept programs
that break the rules. The situations where this happens are most of the time
very unusual and it is unlikely that someone would accidentally write code that
would break the rules without the compiler noticing. It is also continually
being worked on and with time the compiler is capable of rejecting more and
more of unsound code that would previously be accepted.

= Implementation in C++ <cpp-bor>

Everything in this project uses C++ 23, the latest approved C++ standard. As
discussed in #ref(<rs-bor>), proving that a program adheres to the borrowing
rules implies that certain kinds of undefined behaviour cannot happen. The Rust
compiler attempts to verify that the rules are satisfied at compile time.
Something like that wouldn't be achievable in C++ without modifying the
compiler but C++ provides enough features to implement data types that will
check the borrowing rules at runtime. This project presents a header-only C++
library called _borlib_ that implements smart pointers which check the
borrowing rules at runtime.

The main implemented types are:
- `Val<T>`: wrapper for owned value of type `T`.
- `Ref<T>`: smart pointer wrapping pointer to immutable value of type `T` owned
  by `Val<T>`.
- `RefMut<T>`: smart pointer wrapping pointer to mutable value of type `T`
  owned by `Val<T>`.
- `Slice<T>`: smart pointer wrapping immutable continuous memory region with
  elements of type `T` owned by `Val<S>` where `S` is some kind of container
  for that memory region (e.g. `std::vector` or `std::array`).
- `SliceMut<T>`: smart pointer wrapping mutable continuous memory region with
  elements of type `T` owned by `Val<S>` similarly to `Slice<T>`.

All operations on the implemented types verify that the operation doesn't
violate any of the borrowing rules and if the operation would violate any of
the rules the function `panic` is called which will print an error message and
the source location where the operation was invoked and terminate the process.

The advantage of performing runtime checks is that violations are detected
before they could cause undefined behaviour. There are already tools that can
detect incorrect memory accesses (such as address sanitizers or valgrind) but
they will catch the issue only if the memory access is truly invalid. If, for
example, some memory gets freed, but there is still a pointer pointing to that
address and some next allocation gets placed at the same address, access
through the original pointer will not be regarded as invalid. But when checking
the borrowing rules, the issue gets caught already when the memory is freed
while there still exists a pointer to that memory. Another problem is that
these tools are unable to detect data races. When the borrowing rules are
enforced, data races cannot occur because it is impossible to create a mutable
pointer to memory while that memory can be accessed elswhere. Consequently,
issues will be detected before the data race occurs, namely when the
possibility of such a data race is created.

This chapter contains details about the implementation. #ref(<cpp-own>) and
#ref(<cpp-ref>) discuss how checking of the borrowing rules and rules of
references respectively can be achieved in C++. #ref(<val>), #ref(<ref>),
#ref(<ref-mut>), #ref(<slice>) and #ref(<slice-mut>) describe the structure and
API of the borlib types `Val<T>`, `Ref<T>`, `RefMut<T>`, `Slice<T>` and
`SliceMut<T>` respectively. #ref(<err-rep>) describes how borlib retrieves
locations of violations of borrowing rules in source code. #ref(<overhead>)
describes the overhead of borlib.

== Ownership rules <cpp-own>

C++ already provides tools to partially follow the ownership rules. With the
help of destructors, one can write any code that will run when a type goes out
of scope similarly to the `Drop` trait in Rust. But C++ in no way enforces that
each value has exactly one owner. The main problem is with pointers. Pointers
can refer to either an owned value on stack (e.g. pointer in the `std::vector`
implementation) or to a borrowed value (e.g. when they are used as iterators)
and sometimes it is not clear which part of the code should be responsible for
freeing the pointer. This can lead to various issues such as use after free and
double free. C++ provides references which are clearly not owning and they can
be and are in many cases used instead of pointers. But in many cases, pointers
still have to be used because references are immutable, once a reference is
created the address that it stores cannot be changed. References also cannot
refer to references; it is not possible to create reference to reference to a
value (the syntax `T &&` in C++ doesn't declare double reference but an
rvalue). Since C++ 11 the standard library contains smart pointers (e.g
`std::unique_ptr`) which have clear ownership semantics and modern code should
use them when possible. However, C++ doesn't enforce anything and it is
possible to have an owning reference or multiple instances of `std::unique_ptr`
with the same address (although the latter will lead to undefined behaviour).

There is no need to reinvent the wheel and so borlib doesn't provide any
wrapper for an owning pointer, all the smart pointers in borlib (`Ref`,
`RefMut`, `Slice` and `SliceMut`) are not owning. Borlib can be used with the
standard library so the owning pointer to data of type `T` can be achieved with
`std::unique_ptr<Val<T>>`. It is already common practice in C++ to create types
in a way that they follow the ownership rules, so borlib doesn't provide many
additional advantages regarding the compliance with ownership rules.

== Rules of references <cpp-ref>

In this section the term _reference_ will refer to the smart pointers in
borlib. Terms _mutable reference_ and _immutable reference_ will mean that the
data that the reference points to is mutable or immutable data and not the
pointer itself is mutable or immutable.

The rules of references are verified through the class `Counters` which is
created for each instance of `Val<T>`. The class contains two atomic variables:
- `std::atomic_size_t _ref`: Reference counter that tracks the number of
  references to the value.
- `std::atomic_bool _mut`: Flag that indicates whether any of the references
  have mutable access to the value.
The class `Counters` allows modification of the variables and basic checks
through an API that names the semantic actions that can be done with the value
or its references. Access to the value through `Val<T>` can be checked with the
methods `check_move_out`, `check_copy_out`, `check_write`, `check_read` and
`check_destroy`. These methods don't alter the atomic variables. Moving the
value, writing to it or destroying it is valid only if there are no references.
Copying or reading the value is allowed if there is no mutable reference.
Operations on references which will actually modify the counters are achieved
with the methods `create_ref`, `create_ref_mut`, `add_ref`, `remove_ref` and
`transfer_mut_to_ref`. Creating an immutable reference is valid only if there
is no mutable reference and the operation will increment the field `_ref`.
Creating a mutable reference is valid only if there are no other references and
the operation will also set the field `_mut` to `true` while incrementing
`_ref`. Operations `add_ref` and `remove_ref` only increase or decrease the
reference counter. When removing the last reference, `_mut` is also set to
`false`. The operation `transfer_mut_to_ref` is used when mutable reference is
converted to immutable reference. It will set the variable `_mut` to `false` if
this is the only reference.

It may seem that `_ref` must always be `1` if `_mut` is `true`. This is not the
case when splitting mutable slices which is more closely described in
#ref(<slice-mut>).

References to single value are represented with two classes `Ref<T>` and
`RefMut<T>` to differentiate between mutable and immutable reference. They each
contain a pointer to the data of type `T` and a pointer to the structure
`Counters`. It might be tempting to store only one pointer to `Val<T>` since it
contains both the data and counters, but that wouldn't allow representing
situations where the reference refers only to a portion of the data (e.g.
single item from vector). `RefMut<T>` additionally contains a flag `_borrowed`
that is used in reborrowing which will be discussed later. `Ref<T>` is
described in #ref(<ref>) and `RefMut<T>` is described in #ref(<ref-mut>).

References to multiple values stored continuously in memory are represented by
classes `Slice<T>` and `SliceMut<T>`. They contain the same fields as `Ref<T>`
and `RefMut<T>` respectively and additionally have a field storing the number
of items inside the referred memory region. `Slice<T>` is described in
#ref(<slice>) and `SliceMut<T>` is described in #ref(<slice-mut>).

== `Val<T>` <val>

The class `Val<T>` has the following simplified structure:
```cpp
template<typename T> class Val {
  Counters cnts;
  T value;
public:
  Val(T);
  const T &get();
  T &get_mut();
  Ref<T> ref();
  RefMut<T> ref_mut();
  Slice<T::Item> slice()
    requires(IsOwnedSlice<T>);
  SliceMut<T::Item> slice_mut()
    requires(IsOwnedSlice<T>);
};
```

It contains the counters and the value itself. It allows arbitrary access to
the inner value, but it checks the borrowing rules on each access. It also
allows getting mutable and immutable access to the value through references. If
the value `T` is some type that owns multiple items in continuous memory,
`Val<T>` allows retrieving the continuous memory region as `Slice` or
`SliceMut`. All the methods check if the operation violates the borrowing
rules.

`Val<T>` can be copied or moved if the type `T` supports it. All these
operations also check if it violates the borrowing rules.

== `Ref<T>` <ref>

The class `Ref<T>` has the following simplified structure:
```cpp
template<typename T> class Ref {
  Counters *_cnts;
  const T *_r;
public:
  Ref();
  const T &get();
};
```

`Ref<T>` represents an immutable reference to a value of type `T`. The
reference allows immutable access to the value and cloning the reference
itself. It is possible to create uninitialized reference (represented by
`_r == nullptr`) and all operations on the reference check if the reference is
valid.

== `RefMut<T>` <ref-mut>

The class `RefMut<T>` has the following simplified structure:
```cpp
template<typename T> class RefMut {
  Counters *_cnts;
  bool _borrowed;
  const T *_r;
public:
  RefMut();
  RefMut(const RefMut<T> &) = delete;
  RefMut<T> &operator=(const RefMut<T> &)
    = delete;
  T &get();
  Ref<T> into_ref();
  template<typename F> void borrow(F f);
  template<typename F>
  void borrow_mut(F f);
};
```

Mutable reference cannot be copied (this is why the copy constructor and
operator is deleted), but it allows mutable access to the underlying data. It
can also be converted to an immutable reference. The methods `borrow` and
`borrow_mut` are used for reborrowing. Reborrowing is when a mutable reference
is borrowed to be used by someone else for some duration without losing
ownership of the original reference. For the duration of the borrowing, the
original reference cannot be used. The flag is used to indicate that the
reference is borrowed and cannot be used. This is necessary when calling a
function that takes the mutable reference as an argument.

Reborrowing requires checks both when it begins and when it ends. For this
reason, it is implemented through the higher order methods `borrow` and
`borrow_mut` where the type `F` represents a function which will take the
reborrowed reference as an argument. Reborrowing is implemented by creating a
new instance of `Counters` which is associated with the borrowed reference.
After the method finishes, the number of references in the created counters
must be 0. If not, the borrowed reference escaped its scope and this will lead
to violation of the borrowing rules.

== `Slice<T>` <slice>

The simplified structure of the class `Slice<T>` is the following:

```cpp
template<typename T> class Slice {
  Counters *_cnts;
  const T *_r;
  std::size_t _len;
public:
  Slice();
  std::size_t len();
  std::span<const T> as_span();
  Ref<T> operator[](std::size_t idx);
  Slice<T> operator[](
    std::size_t start, std::size_t end
  );
};
```

Slice is very similar to `Ref<T>`. The difference is that it immutably borrows
continuous slice of memory instead of just one item. It provides information
about the length of the slice with the method `len` and to its data through
the method `as_span`. The indexing operators are used to get a reference to
portion of the slice. Either to get a single item (with the first `operator[]`)
or to get a subslice (with the second `operator[]`).

== `SliceMut<T>` <slice-mut>

Structure of the class `SliceMut<T>` can be simplified as follows:

```cpp
template<typename T> class SliceMut {
  Counters *_cnts;
  bool _borrowed;
  T *_r;
  std::size_t _len;
public:
  SliceMut();
  SliceMut(const SliceMut &) = delete;
  SliceMut &operator=(const SliceMut &)
    = delete;
  std::size_t len();
  std::span<T> as_span();
  RefMut<T> operator[](std::size_t i) &&;
  SliceMut<T> operator[](
    std::size_t start, std::size_t end
  ) &&;
  Slice<T> into_slice();
  std::pair<SliceMut<T>> split(
    Range a, Range b
  ) &&;
  template<typename F>
  void borrow(Index idx, F f);
  template<typename F>
  void borrow_mut(Index idx, F f);
}
```

`SliceMut` is to `Slice` what `RefMut` is to `Ref`. It refers to mutable
continuous slice of memory. Similarly to `RefMut`, the copy constructor and
operator is deleted. It provides information about its length with the method
`len` and access to its contents with the method `as_span`. The borrowed range
of data can be changed with the indexing operators (the `&&` at the end of the
declaration requires that the method is called with rvalue, so the old slice
will become invalid). It can also be changed to an immutable slice with the
method `into_slice`. The reborrowing methods `borrow` and `borrow_mut` are
similar to the reborrowing methods in `RefMut`, but there are more overloads
for the placeholder type `Index`. It can be either nothing so the whole slice
is reborrowed, single `std::size_t` where only one item is reborrowed or two
`std::size_t`s to reborrow only portion of the slice.

Interesting is the method `split`. The type signature has been simplified to
avoid long types. The correct spelling of the return type is
`std::pair<SliceMut<T>, SliceMut<T>>` and the type `Range` stands for
`std::pair<std::size_t, std::size_t>`. The method splits the mutable slice into
two disjoint mutable slices. This split is what will make the reference counter
in the counters differ from 1 even when `_mut` is `true`.

== Error reporting <err-rep>

When the borrowing rules are violated, the program is terminated and prints the
source code location where the violation occurred. The helpful source location
of the violation is always location of the caller of any of the methods in
borlib. The simplest way of getting this information is with the use of
`std::source_location` which can get location of the caller at compile time.
The limitation of this approach is that it relies on additional default
argument and therefore can only be used with methods whose signatures permit
them. Namely, the limitation is a problem for overloaded operators which
usually have fixed signature (e.g. `operator=`) or destructors. In this case
the source location cannot be simply obtained at compile time and has to be
obtained at runtime. This can be achieved with the standard C++ _stacktrace_
API. The problem is that stacktrace is not widely supported by implementations
of the standard library and so it would be best to make its use optional. To
simplify this, borlib contains wrapper `SourcePos` which wraps both
`std::source_location` and stacktrace. When using `SourcePos::current()`, it
behaves exactly as `std::source_location`. When using `SourcePos::caller()` it
will track the relative position of the caller's stack frame so it can be later
used to retrieve the proper source location. If stacktrace is disabled,
`SourcePos::caller()` will fall back to using `std::source_location`.
Whether stacktrace is enabled is controlled at compile time with the macro
`BOR_STACKTRACE`.

== Overhead of checking borrowing rules <overhead>

Checking of the borrowing rules can add significant overhead. `Val<T>` takes up
more space than just `T`, the references `Ref<T>` and `RefMut<T>` take more
space than just `T *` and slices take more space than just `std::span<T>`. The
checks performed on each operation are also very expensive compared to the
performed operation. This is why borlib adds the option to disable all of the
checking to remove the overhead. When disabled, the size of the types is the
same as their standard counterparts and all checks are disabled. The only
overhead of the references and slices is that when they are moved or destroyed
they will set the underlying pointer to `nullptr` so that the behaviour of the
public API doesn't change. Whether the checks are enabled is controlled at
compile time with the macro `BOR_CHECK`. The idea is that code can be written
with borlib, tested with enabled checks and if performance is critical, the
checks can be disabled for release builds.

= Showcase and examples <show>

This chapter contains examples where borlib catches borrow violations. Each
example contains code and the output. The examples are all run with checks
enabled and stacktrace enabled.

== Examples

The format of the error messages is: _file_`:`_line_`:`_column_`:`
_error message_.

=== Dangling pointer
In this example a value is created in an inner scope and reference to it is
assigned to variable that outlives that scope. This would result in a dangling
pointer.
#codeblock(lineNum: true)[
  ```cpp
  int main() {
    Ref<int> ref;
    {
      Val<int> n = 5;
      ref = &n;
      std::println("pos1");
    }
    std::println("pos2");
    std::println("pos3: {}", ref);
  }
  ```
]
*Output:*
```
pos1
main.cpp:7:0: borrow violation: Cannot destroy value that is borrowed.
```

=== Race condition
In this example a value is created and a mutable reference to it is passed to
another thread which modifies it. The main thread tries to read the value while
the other thread might modify the value. This could result in a race condition,
but borlib terminates the program with error before the value is read.
#codeblock(lineNum: true)[
  ```cpp
  int main() {
    Val<int> v = 5;
    auto foo = [](RefMut<int> mr) {
        *mr = 7;
    };
    std::thread t(foo, v.ref_mut());
    std::println(
      "value: {}", v.get()
    );
    t.join();
  }
  ```
]
*Output:*
```
main.cpp:8:20: borrow violation: Cannot read value that is mutably borrowed.
```

=== Escape of reborrowed reference
In this example a mutable reference is created and then reborrowed. The
reborrowed reference is then assigned to a value that will outlive the
scope of reborrowing. This would result in having both mutable and immutable
reference to the same value which would violate the borrowing rules.
#codeblock(lineNum: true)[
  ```cpp
  int main() {
    Val<int> v = 5;
    Ref<int> ref;
    RefMut<int> mref = v.ref_mut();
    mref.borrow([](Ref<int> r) {
        ref = r;
    });
    std::println("mut: {}", mref);
    std::println("ref: {}", ref);
  }
  ```
]
*Output:*
```
main.cpp:5:8: borrow violation: Borrowed reference escaped its lifetime.
```

=== Overlapping mutable slice split
In this example a mutable slice is created and then it is split into two
subslices which overlap. This would allow mutable access to the same memory
from two references at the same time which violates the borrowing rules.
#codeblock(lineNum: true)[
  ```cpp
  int main() {
    Val<std::vector<int>> vec
      = std::vector{ 3, 1, 4 };
    auto [s1, s2] = vec.slice_mut()
      .split({0, 2}, {1, 3});
    assert(s1[1] == s2[0]);
  }
  ```
]
*Output:*
```
main.cpp:5:6: borrow violation: The ranges overlap.
```

= Conclusion

Borrowing rules are useful because they prevent several classes of common bugs.
When they are followed each memory access is guaranteed to be valid and all
memory can be safely freed without requiring a garbage collector. In some
cases, they can also be limiting but the majority of the code will benefit from
them.

Properly checking the borrowing rules at compile time can be difficult and even
Rust is at this time not capable of fully checking the rules. It is impossible
to bring the compile-time borrow-checker capabilities to C++ without modifying
the compiler, but it is possible to create smart references that check the
rules at runtime. Checking the rules at runtime is still beneficial because
they can catch the undefined behaviour much sooner and also catch undefined
behaviour that could occur undeterministically (such as data races).

Borlib implements smart references in C++ that check the borrowing rules at
runtime with helpful information about where and why the code violates the
borrowing rules. The checks add overhead to the code and so it is possible to
disable them at compile time for example for release builds. Borlib is
compatible with other C++ code and so it can be used only when it makes sense
or only in code that has high requirements for correctness.
