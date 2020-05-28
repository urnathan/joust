// Joust Test Suite			-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#pragma once

// C
#include <cstdio>

namespace Joust {

class Option 
{
public:
  char const *sname;
  char cname;
  unsigned offset;
  void (*process) (Option const *, char const *opt,
		   char const *arg, void *flags);
  char const *argform;
  char const *help;

public:
  template<typename T>
  T &Flag (void *f) const
  {
    return *reinterpret_cast<T *> (reinterpret_cast<char *> (f) + offset);
  }
  int Process (int argc, char **argv, void *flags) const;
  void Help (FILE *stream, char const *) const;
};

}
