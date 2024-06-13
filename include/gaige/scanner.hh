// Joust Test Suite			-*- mode:c++ -*-
// Copyright (C) 2020-2024 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#ifndef GAIGE_SCANNER_HH

// Gaige
#include "gaige/error.hh"
// C++
#include <string_view>
#include <vector>

namespace gaige {

class Scanner
{
private:
  nms::SrcLoc Loc;
  
public:
  Scanner (char const *file_)
    : Loc (file_)
  {}

public:
  auto const &loc () const
  { return Loc; }

public:
  auto error ()
  { return Error (Loc); }

public:
  bool scanFile (std::string const &, std::vector<char const *> const &prefixes);

protected:
  virtual bool processLine (std::string_view const &variant,
			    std::string_view const &line);
};

} // namespace gaige

#define GAIGE_SCANNER_HH
#endif
