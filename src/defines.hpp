#pragma once

#include <version>

#ifndef BOR_STACKTRACE
# if __cpp_lib_stacktrace == 202011L
#  define BOR_STACKTRACE 1
# else
#  define BOR_STACKTRACE 0
# endif
#endif