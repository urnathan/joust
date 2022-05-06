// NMS Test Suite			-*- mode:c++ -*-
// Copyright (C) 2019-2022 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#ifndef NMS_FATAL_HH

// C++
#if __GNUC__ >= 10 || __clang_major__ >= 11
#define NMS_LOC_BUILTIN 1
#elif __has_include (<source_location>)
#include <source_location>
#define NMS_LOC_SOURCE 1
#endif
// C
#include <cstdio>

namespace NMS
{

class SrcLoc
{
protected:
  char const *file;
  unsigned line;

public:
  constexpr SrcLoc (char const *file_
#if NMS_LOC_BUILTIN
		    = __builtin_FILE ()
#elif !NMS_LOC_SOURCE
		    = nullptr
#endif
		    , unsigned line_
#if NMS_LOC_BUILTIN
		    = __builtin_LINE ()
#elif !NMS_LOC_SOURCE
		    = 0
#endif
		    ) noexcept
    : file (file_), line (line_)
  {}

#if !NMS_LOC_BUILTIN && NMS_LOC_SOURCE
  constexpr SrcLoc (source_location loc = source_location::current ()) noexcept
    : file (loc.file ()), line (loc.line ())
  {}
#endif

public:
  constexpr char const *File () const noexcept { return file; }
  constexpr unsigned Line () const noexcept { return line; }
};

[[noreturn]]
void HCF (char const *msg, char const *optional = nullptr
#if NMS_CHECKING
	  , SrcLoc const = SrcLoc ()
#if !NMS_LOC_BUILTIN && !NMS_LOC_SOURCE
#define HCF(M, ...) HCF ((M), __VA_ARGS__ + 0, NMS::SrcLoc (__FILE__, __LINE__))
#endif
#endif
	  ) noexcept;

#if NMS_CHECKING
[[noreturn]]
void AssertFailed (char const *msg = nullptr, SrcLoc loc = SrcLoc ()) noexcept;

[[noreturn]]
void Unreachable (char const *msg = nullptr, SrcLoc loc = SrcLoc ()) noexcept;

[[noreturn]]
void Unimplemented (char const *msg = nullptr, SrcLoc loc = SrcLoc ()) noexcept;

#if NMS_LOC_BUILTIN || NMS_LOC_SOURCE
#define AssertFailed(MSG) NMS::AssertFailed (MSG)
#define Unreachable(MSG) NMS::Unreachable (MSG)
#define Unimplemented(MSG) NMS::Unimplemented (MSG)
#else
#define AssertFailed(MSG)						\
  NMS::AssertFailed (MSG + 0, NMS::SrcLoc (__FILE__, __LINE__))
#define Unreachable(MSG)						\
  NMS::Unreachable (MSG + 0, NMS::SrcLoc (__FILE__, __LINE__))
#define Unimplemented(MSG)						\
  NMS::Unimplemented (MSG + 0, NMS::SrcLoc (__FILE__, __LINE__))
#endif

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
#define Unimplemented(...) __builtin_unreachable ()
#endif

void BuildNote (FILE *stream) noexcept;

[[noreturn]]
void Fatal (char const *, ...) noexcept;

void SignalHandlers () noexcept;
} // namespace NMS

#define NMS_FATAL_HH
#endif
