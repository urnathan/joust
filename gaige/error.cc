// Joust Test Suite			-*- mode:c++ -*-
// Copyright (C) 2020-2023 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#include "joust/cfg.h"
// Gaige
#include "gaige/error.hh"
// C++
#include <iostream>

namespace gaige {

bool Error::errored;

Error::Error (nms::SrcLoc loc)
  : stream (&std::cerr)
{
  errored = true;
  *stream << loc.File () << ':' << loc.Line () << ": error: ";
}

} // namespace gaige
