// Joust Test Suite			-*- mode:c++ -*-
// Copyright (C) 2020-2024 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#ifndef GAIGE_REGEX_HH

// C++
#include <regex>

// regex propagates errors via exceptions, hence this wrapper to catch
// and contain them.
namespace gaige::regex {

enum Results {
  // YES, NO, FILE-NOT-FOUND
  NOTFOUND,
  FOUND,
  FAILED
};

Results create (std::regex &, std::string_view const &text, int &) noexcept;
Results search (std::regex const &, std::string_view const &text,
                std::cmatch &, int &) noexcept;
char const *error (int) noexcept;
void protect (std::string &, std::string_view const &);
inline std::string protect (std::string_view const &src) {
  std::string dst;
  protect(dst, src);
  return dst;
}

} // namespace gaige::regex

#define GAIGE_REGEX_HH
#endif
