// Joust Test Suite			-*- mode:c++ -*-
// Copyright (C) 2020-2023 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#ifndef GAIGE_SCANNER_HH

// Gaige
#include "gaige/error.hh"
// C++
#include <string_view>
#include <vector>

namespace Gaige
{

class Scanner
{
private:
  NMS::SrcLoc loc;
  
public:
  Scanner (char const *file_)
    : loc (file_)
  {}

public:
  auto const &Loc () const
  { return loc; }

public:
  auto Error ()
  { return Gaige::Error (loc); }

public:
  bool ScanFile (std::string const &, std::vector<char const *> const &prefixes);

protected:
  virtual bool ProcessLine (std::string_view const &variant,
			    std::string_view const &line);
};

} // namespace Gaige

#define GAIGE_SCANNER_HH
#endif
