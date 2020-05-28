// NMS		-*- mode:c++ -*-
// Copyright (C) 2019-2020 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#pragma once
#define NMS_LOC_BUILTINS (__GNUC__ >= 10)
// C++
#if !NMS_LOC_BUILTINS
#include <source_location>
#endif
// C
#include <cstdio>

namespace Joust {

class Location
{
protected:
  char const *file;
  unsigned line;

public:
  constexpr Location (char const *file_
#if NMS_LOC_BUILTINS
		      = __builtin_FILE ()
#endif
		      , unsigned line_
#if NMS_LOC_BUILTINS
		      = __builtin_LINE ()
#endif
		      )
    :file (file_), line (line_)
  {
  }

#if !NMS_LOC_BUILTINS
  constexpr Location (source_location loc == source_location::current ())
    :file (loc.file ()), line (loc.line ())
  {
  }
#endif

public:
  constexpr char const *File () const
  {
    return file;
  }
  constexpr unsigned Line () const
  {
    return line;
  }
};

extern char const *progname;

void Progname (char const *argv0);

void HCF [[noreturn]]
(
 char const *msg
#if NMS_CHECKING
 , Location const = Location ()
#endif
 ) noexcept;

#if NMS_CHECKING
void AssertFailed [[noreturn]] (Location loc = Location ());
void Unreachable [[noreturn]] (Location loc = Location ());

#define Assert(EXPR, ...)						\
  (__builtin_expect (bool (EXPR __VA_OPT__ (, __VA_ARGS__)), true)	\
   ? (void)0 : AssertFailed ())
#else
#define Assert(EXPR, ...)					\
  ((void)sizeof (bool (EXPR __VA_OPT__ (, __VA_ARGS__))), (void)0)
inline void Unreachable ()
{
  __builtin_unreachable ();
}
#endif

void BuildNote (FILE *stream) noexcept;
void Fatal [[noreturn]] (char const *, ...) noexcept;
void SignalHandlers () noexcept;
}
