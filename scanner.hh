// Joust			-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// Not For Distribution

#pragma once

// Joust
#include "error.hh"
// C++
#include <string_view>
#include <vector>

namespace Joust {

class Scanner
{
private:
  char const *file;
  unsigned line = 0;
  
public:
  Scanner (char const *file_)
    : file (file_)
  {
  }

public:
  char const *GetFile () const 
  {
    return file;
  }
  unsigned GetLine () const
  {
    return line;
  }

public:
  auto Error ()
  {
    return Joust::Error (file, line);
  }

public:
  void ScanFile (std::string const &, std::vector<char const *> const &prefixes);

protected:
  virtual bool ProcessLine (std::string_view const &variant,
			    std::string_view const &line);
};

}
