// Joust Test Suite			-*- mode:c++ -*-
// Copyright (C) 2020-2023 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#ifndef JOUST_TESTER_HH

// NMS
#include "nms/macros.hh"
#include "nms/srcloc.hh"
// C++
#include <iostream>
#include <string_view>

namespace Joust
{

class Tester
{
private:
  class Streamer
  {
    Tester *logger;

  public:
    Streamer (Tester *l) noexcept
      : logger (l)
    {
    }
    Streamer (Streamer &&s) noexcept
      : logger (s.logger)
    {
      s.logger = nullptr;
    }
    ~Streamer () noexcept
    {
      if (logger)
	*logger << '\n';
    }

  private:
    Streamer &operator= (Streamer &&) = delete;

  public:
    template <typename T>
    Streamer &operator<< (T &&obj) noexcept
    {
      *logger << std::forward<T> (obj);

      return *this;
    }
  };

public:  
#define JOUST_STATUSES PASS, FAIL, XPASS, XFAIL, ERROR, UNSUPPORTED, MSG
  enum Status
  {
    NMS_LIST (NMS_IDENT, JOUST_STATUSES),
    STATUS_HWM,
    STATUS_REPORT = MSG
  };

public:
  static std::string_view const statuses[STATUS_HWM];

private:
  std::ostream *sum;
  std::ostream &log;

public:
  Tester (std::ostream &s, std::ostream &l) noexcept
    : sum (&s), log (l)
  {}

  Tester (std::ostream &l) noexcept
    : sum (nullptr), log (l)
  {}

  Tester () noexcept;

private:
  Tester (Tester const &) = delete;
  Tester &operator= (Tester const &) = delete;

public:
  std::ostream &Sum () const noexcept
  { return sum ? *sum : log; }

  std::ostream &Log () const noexcept
  { return log; }
  
public:
  void Flush () noexcept
  {
    if (sum)
      sum->flush ();
    log.flush ();
  }

public:
  static Status PassFail (bool pass, bool xfail = false) noexcept
  { return Status ((pass ? PASS : FAIL) + (xfail ? XPASS - PASS : 0)); }

protected:
  Status DecodeStatus (std::string_view const &) noexcept;

public:
  Streamer Result (Status status, char const *filename) noexcept
  { return Result (status, NMS::SrcLoc (filename)); }
  Streamer Result (Status status, NMS::SrcLoc = NMS::SrcLoc::Here ()) noexcept;
  Streamer Result (bool pass, bool xfail = false,
		   NMS::SrcLoc loc = NMS::SrcLoc::Here ()) noexcept
  { return Result (PassFail (pass, xfail), loc); }
  Streamer Message (NMS::SrcLoc loc = NMS::SrcLoc::Here ()) noexcept
  { return Result (MSG, loc); }

public:
  template <typename T>
  Tester const &operator<< (T const &obj) const noexcept
  {
    if (sum)
      *sum << obj;
    log << obj;

    return *this;
  }
};

#undef JOUST_FILE
#undef JOUST_LINE

} // namespace Joust

#define JOUST_TESTER_HH
#endif
