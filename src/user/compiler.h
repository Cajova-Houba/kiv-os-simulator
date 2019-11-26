#pragma once

#ifdef __GNUC__
// =================
// == GCC a Clang ==
// =================

#define COMPILER_PRINTF_ARGS_CHECK(...) __attribute__((format(printf,__VA_ARGS__)))

#else
// =============
// == MSVC :( ==
// =============

#define COMPILER_PRINTF_ARGS_CHECK(...)

#endif
