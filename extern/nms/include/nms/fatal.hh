// NMS Test Suite			-*- mode:c++ -*-
// Copyright (C) 2019-2022 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#ifndef NMS_FATAL_HH

// C
#include <cstdio>

namespace NMS
{

class SrcLoc
{
  // Sadly, std::source_location doesn't permit user-constructed
  // arbitrary source locations.  And we want that functionality
  // for signal handling.

protected:
  char const *file = nullptr;
  unsigned line = 0;

public:
  constexpr SrcLoc () noexcept = default;
  constexpr SrcLoc (char const *file_, unsigned line_) noexcept
    : file (file_), line (line_)
  {}

public:
  constexpr char const *File () const noexcept
  { return file; }
  constexpr unsigned Line () const noexcept
  { return line; }
};

#if __GNUC__ >= 10 || __clang_major__ >= 11
#define NMS_LOC_HERE() SrcLoc (__builtin_FILE (), __builtin_LINE ())
#else
#define NMS_LOC_HERE() SrcLoc (nullptr, 0)
#endif

[[noreturn]]
void HCF (char const *msg, char const *optional = nullptr,
	  SrcLoc = NMS_LOC_HERE ()) noexcept;

#if NMS_CHECKING
[[noreturn]]
void AssertFailed (char const *msg = nullptr, SrcLoc = NMS_LOC_HERE ()) noexcept;

[[noreturn]]
void Unreachable (char const *msg = nullptr, SrcLoc = NMS_LOC_HERE ()) noexcept;

[[noreturn]]
void Unimplemented (char const *msg = nullptr,
		    SrcLoc = NMS_LOC_HERE ()) noexcept;

#define Unreachable(MSG) NMS::Unreachable (MSG)
#define Unimplemented(MSG) NMS::Unimplemented (MSG)

#define Assert(EXPR, ...)						\
		    (__builtin_expect (bool (EXPR), true)		\
		     ? void (0)	: NMS::AssertFailed (__VA_ARGS__))

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
#undef NMS_LOC_HERE

void BuildNote (FILE *stream) noexcept;

[[noreturn]]
void Fatal (char const *, ...) noexcept;

// Install default set of handlers (which abort)
void SignalHandlers () noexcept;

// Default (aborting) handler
void SignalHandler (int sig, void * = nullptr, void * = nullptr) noexcept;

} // namespace NMS

#define NMS_FATAL_HH
#endif
