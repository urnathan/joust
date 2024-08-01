// Joust Test Suite			-*- mode:c++ -*-
// Copyright (C) 2020-2024 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#ifndef GAIGE_ERROR_HH

// NMS
#include "nms/srcloc.hh"
// C++
#include <ostream>
#include <utility>

namespace gaige {

class Error {
private:
  std::ostream *Stream;

private:
  static bool HasErrored;

public:
  Error (nms::SrcLoc);
  Error (Error &&src)
    : Stream(src.Stream) {
    src.Stream = nullptr;
  }
  ~Error () {
    if (Stream)
      *Stream << '\n';
  }

private:
  Error &operator= (Error &&) = delete;

public:
  template <typename T>
  Error &operator<< (T &&obj) {
    *Stream << std::forward<T>(obj);

    return *this;
  }

public:
  static bool hasErrored () { return HasErrored; }
};

} // namespace gaige

#define GAIGE_ERROR_HH
#endif
