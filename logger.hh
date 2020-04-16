// Joust			-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// Not For Distribution

#pragma once

// C++
#include <iostream>
#include <string_view>

namespace Joust {

class Logger
{
private:
  class Streamer
  {
    Logger *logger;

  public:
    Streamer (Logger *l)
      : logger (l)
    {
    }
    Streamer (Streamer &&s)
      : logger (s.logger)
    {
    }
    ~Streamer ()
    {
      if (logger)
	*logger << '\n';
    }

  private:
    Streamer &operator= (Streamer const &) = delete;

  public:
    template<typename T> Streamer &operator << (T &&obj)
    {
      *logger << std::forward<T> (obj);

      return *this;
    }
  };

public:  
#define LOGGER_STATUSES				\
  LOGGER_STATUS_FROB(PASS),			\
    LOGGER_STATUS_FROB(FAIL),			\
    LOGGER_STATUS_FROB(XPASS),			\
    LOGGER_STATUS_FROB(XFAIL),			\
    LOGGER_STATUS_FROB(ERROR),			\
    LOGGER_STATUS_FROB(UNSUPPORTED)
#define LOGGER_STATUS_FROB(STATUS) STATUS
    enum Status { LOGGER_STATUSES, STATUS_HWM };
#undef LOGGER_STATUS_FROB

public:
  static constexpr std::string_view statuses[STATUS_HWM]
#define LOGGER_STATUS_FROB(STATUS) std::string_view (#STATUS)
    = { LOGGER_STATUSES };
#undef LOGGER_STATUS_FROB

private:
  std::ostream &sum;
  std::ostream &log;
  // FIXME: Move to derived class, or putit in aloy-engine?
  unsigned counts[STATUS_HWM];

public:
  Logger (std::ostream &s, std::ostream &l);
  Logger ()
    : Logger (std::cout, std::cerr)
  {
  }

private:
  Logger (Logger const &) = delete;
  Logger &operator= (Logger &) = delete;

public:
  std::ostream &Sum () const
  {
    return sum;
  }
  std::ostream &Log () const
  {
    return log;
  }
  
public:
  static Status PassFail (bool pass, bool xfail = false)
  {
    return Status ((pass ? PASS : FAIL) + (xfail ? XPASS - PASS : 0));
  }

public:
  bool AddStatus (std::string_view const &);

public:
  Streamer Result (Status status);
  Streamer Result (Status status, char const *file, unsigned line);

public:
  template<typename T> Logger const &operator << (T const &obj) const
  {
    sum << obj;
    log << obj;

    return *this;
  }
  

  friend std::ostream &operator<< (std::ostream &, Logger const &);
};

}
