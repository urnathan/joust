// Joust			-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// Not For Distribution

#pragma once

// C++
#include <ostream>
#include <string>
#include <string_view>

namespace Joust {

class Tester 
{
public:  
#define TESTER_STATUSES				\
  TESTER_STATUS_FROB(PASS),			\
    TESTER_STATUS_FROB(FAIL),			\
    TESTER_STATUS_FROB(XPASS),			\
    TESTER_STATUS_FROB(XFAIL),			\
    TESTER_STATUS_FROB(ERROR),			\
    TESTER_STATUS_FROB(UNSUPPORTED)
#define TESTER_STATUS_FROB(STATUS) STATUS
    enum Status { TESTER_STATUSES, STATUS_HWM };
#undef TESTER_STATUS_FROB

public:
  static constexpr std::string_view statuses[STATUS_HWM]
#define TESTER_STATUS_FROB(STATUS) std::string_view (#STATUS)
    = { TESTER_STATUSES };
#undef TESTER_STATUS_FROB

private:
  unsigned counts[STATUS_HWM];

public:
  Tester ()
  {
    for (unsigned ix = STATUS_HWM; ix--;)
      counts[ix] = 0;
  }

public:
  static Status PassFail (bool pass, bool xfail = false)
  {
    return Status ((pass ? PASS : FAIL) + (xfail ? XPASS - PASS : 0));
  }

public:
  bool AddStatus (std::string_view const &);

public:
  static void Result (Status status,
		      char const *file, unsigned line,
		      char const *kind, std::string const &message);

  friend std::ostream &operator<< (std::ostream &, Tester const &);
};

}
