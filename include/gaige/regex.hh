// Joust Test Suite			-*- mode:c++ -*-
// Copyright (C) 2020-2023 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#ifndef GAIGE_REGEX_HH

// C++
#include <regex>

// regex propagates errors via exceptions, hence this wrapper to catch
// and contain them.
namespace gaige::regex {

enum Result
{
  // YES, NO, FILE-NOT-FOUND
  NOTFOUND,
  FOUND,
  FAILED
};

Result Create (std::regex &, std::string_view const &text, int &) noexcept;
Result Search (std::regex const &, std::string_view const &text,
	       std::cmatch &, int &) noexcept;
char const *Error (int);
void Protect (std::string &, std::string_view const &);
inline std::string
Protect (std::string_view const &src)
{
  std::string dst;
  Protect (dst, src);
  return dst;
}

} // namespace gaige::Regex

#define GAIGE_REGEX_HH
#endif
