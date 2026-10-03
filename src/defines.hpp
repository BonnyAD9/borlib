#pragma once

#include <version>

#ifndef BOR_STACKTRACE
# ifdef __cpp_lib_stacktrace
#  if __cpp_lib_stacktrace == 202011L
#   define BOR_STACKTRACE 1
#  else
#   define BOR_STACKTRACE 0
#  endif
# else
#  define BOR_STACKTRACE 0
# endif
#endif

#ifndef BOR_CHECK
# ifdef NDEBUG
#  define BOR_CHECK 0
# else
#  define BOR_CHECK 1
# endif
#endif

#if BOR_CHECK
/// If borrow checking is enabled, this will expand to its contents, otherwise
/// it will expand to nothing.
# define BOR(...) __VA_ARGS__
#else
/// If borrow checking is enabled, this will expand to its contents, otherwise
/// it will expand to nothing.
# define BOR(...)
#endif