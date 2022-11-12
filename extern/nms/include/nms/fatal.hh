// NMS Test Suite			-*- mode:c++ -*-
// Copyright (C) 2019-2022 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#ifndef NMS_FATAL_HH

// C++
#include <source_location>
// C
#include <cstdio>

namespace NMS
{

[[noreturn]]
void HCF (char const *msg, char const *optional = nullptr
#if NMS_CHECKING
	  , std::source_location = std::source_location::current ()
#endif
	  ) noexcept;

#if NMS_CHECKING
[[noreturn]]
void AssertFailed (char const *msg = nullptr, std::source_location loc
		   = std::source_location::current ()) noexcept;

[[noreturn]]
void Unreachable (char const *msg = nullptr, std::source_location loc
		  = std::source_location::current ()) noexcept;

[[noreturn]]
void Unimplemented (char const *msg = nullptr, std::source_location loc
		    = std::source_location::current ()) noexcept;

#define AssertFailed(MSG) NMS::AssertFailed (MSG)
#define Unreachable(MSG) NMS::Unreachable (MSG)
#define Unimplemented(MSG) NMS::Unimplemented (MSG)

#define Assert(EXPR, ...)						\
  (__builtin_expect (bool (EXPR), true) ? void (0) 			\
					: AssertFailed (__VA_ARGS__))

#else

#if __clang__
#define Assert(EXPR, ...) __builtin_assume (EXPR)
#else
// Not asserting, use EXPR in an unevaluated context
#define Assert(EXPR, ...) (void (sizeof (bool (EXPR))))
#endif
#define Unreachable(...) __builtin_unreachable ()
#define Unimplemented(...) __builtin_trap ()
#endif

void BuildNote (FILE *stream) noexcept;

[[noreturn]]
void Fatal (char const *, ...) noexcept;

void SignalHandlers () noexcept;
} // namespace NMS

#define NMS_FATAL_HH
#endif
