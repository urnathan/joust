// Joust Test Suite			-*- mode:c++ -*-
// Copyright (C) 2020-2023 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#ifndef GAIGE_READBUFFER_HH

// C++
#include <vector>

namespace gaige {

class ReadBuffer
  : public std::vector<char>
{
  using Parent = std::vector<char>;

private:
  static constexpr size_t blockSize = 16384;

private:
  int fd = -1;

public:
  // Read from fd, return errno on error, -1 on eof, 0 otherwise
  int Read ();

public:
  bool IsOpen () const
  { return fd >= 0; }

public:
  int GetFD () const
  { return fd; }

  void Open (int f)
  { fd = f; }

  int Close ()
  {
    int f = fd;
    fd = -1;
    return f;
  }
};

} // namespace gaige

#define GAIGE_READBUFFER_HH
#endif
