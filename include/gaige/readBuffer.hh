// Joust Test Suite			-*- mode:c++ -*-
// Copyright (C) 2020-2024 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#ifndef GAIGE_READBUFFER_HH

// C++
#include <vector>

namespace gaige {

class ReadBuffer : public std::vector<char> {
  using Parent = std::vector<char>;

private:
  static constexpr size_t BlockSize = 16384;

private:
  int FD = -1;

public:
  // Read from fd, return errno on error, -1 on eof, 0 otherwise
  int read ();

public:
  bool isOpen () const { return FD >= 0; }

public:
  int fd () const { return FD; }

  void open (int f) { FD = f; }

  int close () {
    int f = FD;
    FD = -1;
    return f;
  }
};

} // namespace gaige

#define GAIGE_READBUFFER_HH
#endif
