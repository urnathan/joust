// Joust Test Suite			-*- mode:c++ -*-
// Copyright (C) 2020-2023 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#ifndef GAIGE_ERROR_HH

// NMS
#include "nms/srcloc.hh"
// C++
#include <ostream>
#include <utility>

namespace gaige {

class Error
{
private:
  std::ostream *stream;

private:
  static bool errored;

public:
  Error (nms::SrcLoc);
  Error (Error &&src)
    : stream (src.stream)
  {
    src.stream = nullptr;
  }
  ~Error ()
  {
    if (stream)
      *stream << '\n';
  };

private:
  Error &operator= (Error &&) = delete;

public:
  template <typename T>
  Error &operator<< (T &&obj)
  {
    *stream << std::forward<T> (obj);

    return *this;
  }

public:
  static bool Errors ()
  {
    return errored;
  }
};

} // namespace gaige

#define GAIGE_ERROR_HH
#endif
