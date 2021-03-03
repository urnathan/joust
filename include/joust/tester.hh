// Joust Test Suite			-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#ifndef JOUST_TESTER_HH

// C++
#include <iostream>
#include <string_view>

#if __GNUC__ >= 10 || __clang_major__ >= 11
#define JOUST_FILE() __builtin_FILE ()
#define JOUST_LINE() __builtin_LINE ()
#elif __has_include (<source_location>)
#include <source_location>
#define JOUST_FILE() source_location::current ().file ()
#define JOUST_LINE() source_location::current ().line ()
#else
#define JOUST_FILE() nullptr
#define JOUST_LINE() 0
#endif

namespace Joust
{

class Tester
{
private:
  class Streamer
  {
    Tester *logger;

  public:
    Streamer
      (Tester *l)
      noexcept
      : logger (l)
    {
    }
    Streamer
      (Streamer &&s)
      noexcept
      : logger (s.logger)
    {
      s.logger = nullptr;
    }
    ~Streamer
      ()
      noexcept
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
      noexcept
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
  std::ostream *sum;
  std::ostream &log;

public:
  Tester
    (std::ostream &s, std::ostream &l)
    noexcept
    : sum (&s), log (l)
  {
  }

  Tester
    (std::ostream &l)
    noexcept
    : sum (nullptr), log (l)
  {
  }

  Tester
    ()
    noexcept;

private:
  Tester
    (Tester const &)
    = delete;
  Tester &operator=
    (Tester const &)
    = delete;

public:
  std::ostream &Sum
    ()
    const
    noexcept
  {
    return sum ? *sum : log;
  }
  std::ostream &Log
    ()
    const
    noexcept
  {
    return log;
  }
  
public:
  void Flush
    ()
    noexcept
  {
    if (sum)
      sum->flush ();
    log.flush ();
  }
  
public:
  static Status PassFail
    (bool pass, bool xfail = false)
    noexcept
  {
    return Status ((pass ? PASS : FAIL) + (xfail ? XPASS - PASS : 0));
  }

protected:
  Status DecodeStatus
    (std::string_view const &)
    noexcept;

public:
  Streamer Result
    (Status status,
     char const *file = JOUST_FILE (), unsigned line = JOUST_LINE ())
    noexcept;
  Streamer Result
    (bool pass, bool xfail = false,
     char const *file = JOUST_FILE (), unsigned line = JOUST_LINE ())
    noexcept
  {
    return Result (PassFail (pass, xfail), file, line);
  }

public:
  template<typename T>
  Tester const &operator<<
    (T const &obj)
    const
    noexcept
  {
    if (sum)
      *sum << obj;
    log << obj;

    return *this;
  }  
};

#undef JOUST_FILE
#undef JOUST_LINE

}

#define JOUST_TESTER_HH
#endif
