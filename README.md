# Borlib

Smart references for checking Rust borrowing rules in C++.

## Contents of the archive
- `xstigl00-borlib-documentation.pdf`: The project documentation.
- `xstigl00-borlib-presentation.pdf`: Slides for the project.
- `presentation.mp4`: Video of the presentation.
- `src`: The source code.
    - The library is in all of the hpp files.
    - `main.cpp` contains tests and examples.
- `Makefile`: ease of use for cmake.
- `CMakeLists.txt`: Build description for the tests and examples. At the start
  there are 2 features that can be disabled or enabled (OFF/ON, enabled by
  default):
    - `BOR_CHECK`: Whether checking is enabled or not.
    - `BOR_STACKTRACE`: Whether stacktrace is enabled or not. Enabling this
      will sometimes produce better source code locations, but it may not be
      supported by some implementations of the standard library.