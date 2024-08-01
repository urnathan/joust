// Joust Test Suite			-*- mode:c++ -*-
// Copyright (C) 2020-2024 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#ifndef JOUST_TESTER_HH

// NMS
#include "nms/macros.hh"
#include "nms/srcloc.hh"
// C++
#include <iostream>
#include <string_view>

namespace joust {

class Tester {
private:
  class Streamer {
    Tester *Logger;

  public:
    Streamer (Tester *l) noexcept
      : Logger(l) {}
    Streamer (Streamer &&s) noexcept
      : Logger(s.Logger) {
      s.Logger = nullptr;
    }
    ~Streamer () noexcept {
      if (Logger)
        *Logger << '\n';
    }

  private:
    Streamer &operator= (Streamer &&) = delete;

  public:
    template <typename T>
    Streamer &operator<< (T &&obj) noexcept {
      *Logger << std::forward<T>(obj);

      return *this;
    }
  };

public:
#define JOUST_STATUSES PASS, FAIL, XPASS, XFAIL, ERROR, UNSUPPORTED, MSG
  enum Statuses {
    NMS_LIST(NMS_IDENT, JOUST_STATUSES),
    STATUS_HWM,
    STATUS_REPORT = MSG
  };

public:
  static std::string_view const StatusNames[STATUS_HWM];

private:
  std::ostream *Sum;
  std::ostream &Log;

public:
  Tester (std::ostream &s, std::ostream &l) noexcept
    : Sum(&s), Log(l) {}

  Tester (std::ostream &l) noexcept
    : Sum(nullptr), Log(l) {}

  Tester () noexcept;

private:
  Tester (Tester const &) = delete;
  Tester &operator= (Tester const &) = delete;

public:
  std::ostream &sum () const noexcept { return Sum ? *Sum : Log; }

  std::ostream &log () const noexcept { return Log; }

public:
  void flush () noexcept {
    if (Sum)
      Sum->flush();
    Log.flush();
  }

public:
  static Statuses passFail (bool pass, bool xfail = false) noexcept {
    return Statuses((pass ? PASS : FAIL) + (xfail ? XPASS - PASS : 0));
  }

protected:
  Statuses decodeStatus (std::string_view const &) noexcept;

public:
  Streamer result (Statuses status, char const *filename) noexcept {
    return result(status, nms::SrcLoc(filename));
  }
  Streamer result (Statuses status,
                   nms::SrcLoc = nms::SrcLoc::here()) noexcept;
  Streamer result (bool pass, bool xfail = false,
                   nms::SrcLoc loc = nms::SrcLoc::here()) noexcept {
    return result(passFail(pass, xfail), loc);
  }
  Streamer message (nms::SrcLoc loc = nms::SrcLoc::here()) noexcept {
    return result(MSG, loc);
  }

public:
  template <typename T>
  Tester const &operator<< (T const &obj) const noexcept {
    if (Sum)
      *Sum << obj;
    Log << obj;

    return *this;
  }
};

} // namespace joust

#define JOUST_TESTER_HH
#endif
