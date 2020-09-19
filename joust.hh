// Joust Test Suite			-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#pragma once

// C++
#include <iostream>
#include <string_view>

#if __GNUC__ >= 10
#define JOUST_FILE() __builtin_FILE ()
#define JOUST_LINE() __builtin_LINE ()
#else
#define JOUST_FILE() nullptr
#define JOUST_LINE() 0
#endif

namespace Joust
{

class Logger
{
private:
  class Streamer
  {
    Logger *logger;

  public:
    Streamer
      (Logger *l)
      : logger (l)
    {
    }
    Streamer
      (Streamer &&s)
      : logger (s.logger)
    {
      s.logger = nullptr;
    }
    ~Streamer
      ()
    {
      if (logger)
	*logger << '\n';
    }

  private:
    Streamer &operator=
      (Streamer &&)
      = delete;

  public:
    template<typename T>
    Streamer &operator<<
      (T &&obj)
    {
      *logger << std::forward<T> (obj);

      return *this;
    }
  };

public:  
#define JOUST_STATUSES				\
  JOUST_STATUS_FROB(PASS),			\
    JOUST_STATUS_FROB(FAIL),			\
    JOUST_STATUS_FROB(XPASS),			\
    JOUST_STATUS_FROB(XFAIL),			\
    JOUST_STATUS_FROB(ERROR),			\
    JOUST_STATUS_FROB(UNSUPPORTED)
#define JOUST_STATUS_FROB(STATUS) STATUS
    enum Status
    {
      JOUST_STATUSES,
      STATUS_HWM
    };
#undef JOUST_STATUS_FROB

public:
  static constexpr std::string_view statuses[STATUS_HWM]
#define JOUST_STATUS_FROB(STATUS) std::string_view (#STATUS)
    = {
    JOUST_STATUSES
  };
#undef JOUST_STATUS_FROB
#undef JOUST_STATUSES

private:
  std::ostream &sum;
  std::ostream &log;

public:
  Logger
    (std::ostream &s, std::ostream &l)
    : sum (s), log (l)
  {
  }

  Logger
    ()
    : Logger (std::cout, std::cerr)
  {
  }

private:
  Logger
    (Logger const &)
    = delete;
  Logger &operator=
    (Logger const &)
    = delete;

public:
  std::ostream &Sum
    ()
    const
  {
    return sum;
  }
  std::ostream &Log
    ()
    const
  {
    return log;
  }

public:
  void Flush
    ()
  {
    sum.flush ();
    log.flush ();
  }
  
public:
  static Status PassFail
    (bool pass, bool xfail = false)
  {
    return Status ((pass ? PASS : FAIL) + (xfail ? XPASS - PASS : 0));
  }

protected:
  Status DecodeStatus
    (std::string_view const &);

public:
  Streamer Result
    (Status status,
     char const *file = JOUST_FILE (), unsigned line = JOUST_LINE ());

public:
  template<typename T>
  Logger const &operator<<
    (T const &obj)
    const
  {
    sum << obj;
    log << obj;

    return *this;
  }  
};

#undef JOUST_FILE
#undef JOUST_LINE

}
