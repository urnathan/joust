// NMS Test Suite			-*- mode:c++ -*-
// Copyright (C) 2020-2021 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#pragma once

// C
#include <cstdio>

namespace NMS
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
  void (*helper)
    (Option const *, FILE *, char const *pfx);

public:
  template<typename T>
  T &Flag
    (void *f)
    const
    noexcept
  {
    return *reinterpret_cast<T *> (reinterpret_cast<char *> (f) + offset);
  }
  int Process
    (int argc, char **argv, void *flags)
    const;
  void Help
    (FILE *stream, char const *)
    const;

public:
  static void False
    (Option const *opt, char const *, char const *, void *flags)
    noexcept
  {
    opt->Flag<bool> (flags) = false;
  }
};

}
