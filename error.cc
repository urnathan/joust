// Joust Test Suite			-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

// Joust
#include "error.hh"
// C++
#include <iostream>

namespace Joust {

bool Error::errored;

Error::Error (char const *file, unsigned line)
  : stream (&std::cerr)
  {
    errored = true;
    *stream << file << ':' << line << ": error: ";
  }
}
