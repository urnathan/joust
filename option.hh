// Joust Test Suite			-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#pragma once

// C
#include <cstdio>

namespace Joust
{

class Option 
{
public:
  char const *sname;	// --string name
  char cname;		// -charname
  unsigned offset;	// offset in struct
  void (*process)
    (Option const *, char const *opt, char const *arg, void *flags);
  char const *argform;
  char const *help;

public:
  template<typename T>
  T &Flag
    (void *f) const
  {
    return *reinterpret_cast<T *> (reinterpret_cast<char *> (f) + offset);
  }
  int Process
    (int argc, char **argv, void *flags) const;
  void Help
    (FILE *stream, char const *) const;

public:
  static void False
    (Option const *opt, char const *, char const *, void *flags)
  {
    opt->Flag<bool> (flags) = false;
  }
};

}
