// NMS Test Suite			-*- mode:c++ -*-
// Copyright (C) 2019-2020 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#pragma once

// C++
#if __GNUC__ >= 10
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
  constexpr SrcLoc
    (char const *file_
#if NMS_LOC_BUILTIN
     = __builtin_FILE ()
#endif
     , unsigned line_
#if NMS_LOC_BUILTIN
     = __builtin_LINE ()
#endif
     )
    : file (file_), line (line_)
  {}

#if !NMS_LOC_BUILTIN && NMS_LOC_SOURCE
  constexpr SrcLoc
    (source_location loc == source_location::current ())
    : file (loc.file ()), line (loc.line ())
  {}
#endif

public:
  constexpr char const *File
    () const
  {
    return file;
  }
  constexpr unsigned Line
    () const
  {
    return line;
  }
};

extern char const *progname;

void Progname
  (char const *argv0);

void HCF
  [[noreturn]]
  (char const *msg
#if NMS_CHECKING
   , SrcLoc const = SrcLoc ()
#if !NMS_LOC_BUILTIN && !NMS_LOC_SOURCE
#define HCF(M) HCF ((M), NMS::SrcLoc (__FILE__, __LINE__))
#endif
#endif
   )
  noexcept;

#if NMS_CHECKING
void AssertFailed
  [[noreturn]]
  (SrcLoc loc = SrcLoc ());
void Unreachable
  [[noreturn]]
  (SrcLoc loc = SrcLoc ());
void Unimplemented
  [[noreturn]]
  (SrcLoc loc = SrcLoc ());
#define AssertFailed() NMS::AssertFailed ()
#define Unreachable() NMS::Unreachable ()
#define Unimplemented() NMS::Unimplemented ()
#if !NMS_LOC_BUILTIN && !NMS_LOC_SOURCE
#define AssertFailed()					\
  NMS::AssertFailed (NMS::SrcLoc (__FILE__, __LINE__))
#define Unreachable()					\
  NMS::Unreachable (NMS::SrcLoc (__FILE__, __LINE__))
#define Unimplemented()					\
  NMS::Unimplemented (NMS::SrcLoc (__FILE__, __LINE__))
#endif

// Do we have __VA_OPT__, alas no specific feature macro for it :(
// From stack overflow
// https://stackoverflow.com/questions/48045470/portably-detect-va-opt-support
// Relies on having variadic macros, but they're a C++11 thing, so
// we're good
// FIXME: Move to configure time check
#define HAVE_ARG_3(a,b,c,...) c
#define HAVE_VA_OPT_(...) HAVE_ARG_3(__VA_OPT__(,),true,false,)
#define HAVE_VA_OPT HAVE_VA_OPT_(?)

// Oh, for lazily evaluated function parameters
#if HAVE_VA_OPT
// Assert is variadic, so you can write Assert (TPL<A,B>(C)) without
// extraneous parens.  I don't think we need that though.
#define Assert(EXPR, ...)						\
  (__builtin_expect (bool (EXPR __VA_OPT__ (, __VA_ARGS__)), true)	\
   ? (void)0 : AssertFailed ())
#else
// If you don't have the GNU ,##__VA_ARGS__ pasting extension, we'll
// need another fallback
#define Assert(EXPR, ...)						\
  (__builtin_expect (bool (EXPR, ##__VA_ARGS__), true)		\
   ? (void)0 : AssertFailed ())
#endif
#else
// Not asserting, use EXPR in an unevaluated context
#if  HAVE_VA_OPT
#define Assert(EXPR, ...)					\
  ((void)sizeof (bool (EXPR __VA_OPT__ (, __VA_ARGS__))), (void)0)
#else
#define Assert(EXPR, ...)					\
  ((void)sizeof (bool (EXPR, ##__VA_ARGS__)), (void)0)
#endif
#define Unreachable() __builtin_unreachable ()
#define Unimplemented() __builtin_unreachable ()
#endif

void BuildNote
  (FILE *stream)
  noexcept;
void Fatal
  [[noreturn]]
  (char const *, ...)
  noexcept;
void SignalHandlers
  ()
  noexcept;
}
