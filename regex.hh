// Utils		-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// Not For Distribution

#pragma once

// C++
#include <regex>

// regex propagates errors via exceptions, hence this wrapper to catch
// and contain them.
namespace Joust::Regex {

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
inline std::string Protect (std::string const &src)
{
  std::string dst;
  Protect (dst, src);
  return dst;
}

}
