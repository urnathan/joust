// NMS Utilities			-*- mode:c++ -*-
// Copyright (C) 2019-2023 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#ifndef NMS_SRCLOC_HH

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
  constexpr SrcLoc (char const *file_, unsigned line_ = 0) noexcept
    : file (file_), line (line_)
  {}

public:
  // consteval is not right here, because we want the default args
  // expanded at the point of the *ultimate* invoker of this call.
  constexpr static SrcLoc Here
#if __GNUC__ >= 10 || __clang_major__ >= 11
  (char const *f = __builtin_FILE (), unsigned l = __builtin_LINE ())
#else
  (char const *f = nullptr, unsigned l = 0)
#endif
  { return SrcLoc (f, l); }

public:
  constexpr char const *File () const noexcept
  { return file; }
  constexpr unsigned Line () const noexcept
  { return line; }

public:
  void NextLine () noexcept
  { line += 1; }
};

} // namespace NMS

#define NMS_SRCLOC_HH
#endif
