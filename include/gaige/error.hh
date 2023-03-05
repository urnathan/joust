// Joust Test Suite			-*- mode:c++ -*-
// Copyright (C) 2020-2023 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#ifndef GAIGE_ERROR_HH

// C++
#include <ostream>
#include <utility>

namespace Gaige
{

class Error
{
private:
  std::ostream *stream;

private:
  static bool errored;

public:
  Error (char const *file, unsigned line);
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

} // namespace Gaige

#define GAIGE_ERROR_HH
#endif
