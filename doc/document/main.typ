#import "@preview/charged-ieee:0.1.4": ieee

#let todo(msg) = {
  [#text(fill: red, weight: "bold", size: 12pt)[TODO #msg]]
}

#show: ieee.with(
  title: [Smart references for checking Rust borrowing rules in C++],
  abstract: [
    #todo[Abstract]
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
    [#todo[Keywords]],
    "Scientific writing", "Typesetting", "Document creation", "Syntax"
  ),
  bibliography: bibliography("refs.yaml"),
  figure-supplement: [Figure],
  paper-size: "a4",
)

= Introduction

= Rust borrowing rules <rs-bor>

There are several rules concerning ownership and references in the Rust
programming language. All of them are described in #cite(<therust>) either
as _Ownership Rules_ or _Rules of References_. Together they enforce memory
model which prevents several kinds of common bugs related to memory management
and memory access. The term _borrowing rules_ will be used to refer to both the
ownership rules and rules of references toghether.

Unless otherwise specified, this chapter summarizes information from
#cite(<therust>). The ownership rules are described in detail in
#ref(<own-rules>) and the rules of references are described in detail in
#ref(<ref-rules>).

== Ownership rules <own-rules>

The purpose of the ownership rules is primarily memory management. Although it
is most commonly asociated with heap memory allocation, it can be any other
kind of resource which the program uses and has to be freed such as file
descriptors on Unix systems or handles on Windows systems.

Some programming languages (e.g. _C_) don't have any kind of automatic memory
management. The memory management is fully in the hands of the programmer. This
gives a lot of power into the hands of the programmer and so well written
programs can be very efficent. The problem is that people are prone to making
errors and incorrect memory management can lead to voulnerabilities in the
program. Examples include _use after free_ and _memory leaks_.

Another approach isto automate the memory management with garbage collector
(e.g. in _C\#_). In most cases the programmer doesn't have to think about when
is the correct time to free allocated resource. This in most cases avoids the
related bugs such as memory leaks or use after free, but the resulting program
is not as efficient because garbage collectors have to periodically check the
memory used by the program and free any unreachable memory. There are many
algorithms for garbage collecting but none of them can minimaze processor time,
paused time of the program and memory usage at the same time.

The owneship rules place restrictions on how memory is organized within the
program so that it is possible to correctly determie when the memory should be
freed at compile time without it being specified by the programmer. This has
the advantage of not being required to manually specify when memory has to be
freed and at the same time not paying the cost of having a garbage collector.

The ownership rules are:
- Each value in Rust has an _owner_.
- There can only be one owner at a time.
- When the owner goes out of scope, the value will be dropped.

In Rust terminology _dropping_ a value refers to freeing the resources that the
value owns (e.g. allocated memory). Owner is another value that is responsible
for the owned value. When owner is dropped, all of its owned values are also
dropped. On the top level the owner is the program stack. It is not possible to
create something as an owner cycle.

At first it may seem that that restricting memory organization within the
program may result in unoptimal programs because the optimal solution to the
problem may not be representable within the restrictions. In reality the rules
restrict the memory organization to organization that is actually commonly used
when memory is managed manually. For cases where the rules are too restrictive,
Rust has the option of writing code within `unsafe` blocks which allow breaking
the rules within the scope of the `unsafe` block. This allows for
implementing correct data types that internally don't align with the ownership
rules. The types themself don't require the usage of the `unsafe` block,
because they are correct from the outside. It may seem that `unsafe` blocks
break the safety guarantees but in most cases they are not required and when
they are used it is clear which code should be analyzed more carefuly. One
example of type that circumvents the rules is `Arc` which allows shared
ownership and is implemented with reference counting.

Rust allows compliance with the ownership rules by providing the `Drop` trait
which has simmilar job to destructors in other languages: it allows specifying
code that should run when a value of given type is dropped. The rules are also
enforced by allowing the allocation of unowned resources only in `unsafe`
blocks. Even though Rust has the ownership rules, leaking a resource is not
considered an undefined behaviour, so there it is possible to leak memory
without the use of `unsafe` block. However, the api that allows this is always
very explicit about this beahviour because most of the time the only purpose of
such api is to leak the memory (e.g. the function `Box::leak`).

== Rules of references <ref-rules>

Ownership rules by themself mainly prevent memory leaks. For the rest of the
guarantess that Rust makes about safe code, rules of references are required.
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
is asociated with references. In many ways it behaves similarly to a generic
parameters (as in compile time specialization) but it doesn't specialize the
items with which it is asociated. It is used only to mark the origin of the
memory to which the item refers so that it is possible to verify that the
program doesn't break any of the rules of references.

In some cases, these rules are too restricitve for some usecaseses especially
when using multiple threads. This is why rust provides _raw pointers_ which
don't have to follow the rules. In order to make code that circumvents the
rules more explicit, Rust allows the use of raw pointers only within an
`unsafe` block. In order to maintain its safety, `unsafe` blocks are only used
to implement internal details of types or api that is safe to use, even though
it internally breaks the rules of references to achieve optimal implementation.
An example is the type `Mutex<T>` which wraps a generic type `T`. `Mutex`
allows to get mutable reference to the internal type `T` even if you have
immutable reference to `Mutex` itself. Internally mutex uses a lock to ensure
that the immutable access is exclusive and so it doesn't break the rules from
the outside even if the internal implementation requires to convert immutable
reference to mutable reference.

It is very difficult to properly verify that the program follows the rules at
compile time without being too restrictive. Rust itself is at this time
considered _unsound_ because the compiler can be tricked to accept programs
that break the rules. The situations where this happens are most of the time
very unusual and it is unlikely that someone would accidentally write code that
would breaks the rules without the compiler noticing. It is also continually
being worked on and with time the complier is capable of rejecting more and
more of unsound code that would previously be accepted.

= Implementation in C++


As discussed in #ref(<rs-bor>), prooving that a program adheres to the
borrwing rules implies that certain kinds of undefined behaviour cannot happen.
The rust compiler tries to verify that the rules are fulfilled ad compile time.
Something like that wouldn't be achievable in C++ without modifying the
compiler but C++ provides enough features to implement data types that will
check the borrowing rules at runtime. This project presents header only C++
library called _borlib_ that implements smart pointers which check the
borrowing rules at runtime.

The main implemented types are:
- `Val<T>`: wrapper for owned value of type `T`.
- `Ref<T>`: smart pointer wrapping immutable pointer to value of type `T` owned
  by `Val<T>`.
- `RefMut<T>`: smart pointer wrapping mutable pointer to value of type `T`
  owned by `Val<T>`.
- `Slice<T>`: smart pointer wrapping immutable continuous memory region with
  elements of type `T` owned by `Val<S>` where `S` is some kind of container
  for that memory region (e.g. `std::vector` or `std::array`).
- `SliceMut<T>`: smart pointer wrapping mutable continuous memory region with
  elements of type `T` owned by `Val<S>` simmilarily to `Slice<T>`.

= Showcase
