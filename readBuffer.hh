// Joust			-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// Not For Distribution

#pragma once

// Joust
#include "logger.hh"
// C++
#include <vector>

namespace Joust {

class ReadBuffer : public std::vector<char>
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
  void Open (int f)
  {
    fd = f;
  }
  int Close ()
  {
    int f = fd;
    fd = -1;
    return f;
  }
};

}
