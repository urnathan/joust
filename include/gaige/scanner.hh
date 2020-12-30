// Joust Test Suite			-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#pragma once

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
  char const *file;
  unsigned line = 0;
  
public:
  Scanner
    (char const *file_)
    : file (file_)
  {
  }

public:
  char const *GetFile
    ()
    const 
  {
    return file;
  }
  unsigned GetLine
    ()
    const
  {
    return line;
  }

public:
  auto Error
    ()
  {
    return Gaige::Error (file, line);
  }

public:
  bool ScanFile
    (std::string const &, std::vector<char const *> const &prefixes);

protected:
  virtual bool ProcessLine
    (std::string_view const &variant, std::string_view const &line);
};

}
