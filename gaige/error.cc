// Joust Test Suite			-*- mode:c++ -*-
// Copyright (C) 2020-2024 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#include "joust/cfg.h"
// Gaige
#include "gaige/error.hh"
// C++
#include <iostream>

using namespace gaige;

bool Error::HasErrored;

Error::Error (nms::SrcLoc loc)
  : Stream(&std::cerr) {
  HasErrored = true;
  *Stream << loc.file() << ':' << loc.line() << ": error: ";
}
