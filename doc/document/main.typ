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

= Rust borrowing rules

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

The ownership rules are:
- Each value in Rust has an _owner_.
- There can only be one owner at a time.
- When the owner goes out of scope, the value will be dropped.

== Rules of references <ref-rules>

= Implementation in C++

= Showcase
