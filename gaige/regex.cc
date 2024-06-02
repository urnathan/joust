// Joust Test Suite			-*- mode:c++ -*-
// Copyright (C) 2020-2024 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#include "joust/cfg.h"
// NMS
#include "nms/fatal.hh"
// Gaige
#include "gaige/regex.hh"

using namespace nms;
using namespace gaige::regex;

// Escape characters in STRING that are significant to regex
// extended POSIX, Base Definitions and Headers, Section 9.4

void
gaige::regex::Protect (std::string &dst, std::string_view const &src)
{
  // ) needs escaping, } must not be escaped, ] seems either way
  constexpr std::string_view meta (R"([{()*+?.\^$|)");

  dst.reserve (dst.size () + src.size ());

  for (auto c : src)
    {
      if (meta.find (c) != meta.npos)
	dst.push_back ('\\');
      dst.push_back (c);
    }
}

Result
gaige::regex::Create (std::regex &regex, std::string_view const &text,
		      int &error) noexcept
{
  try
    {
      std::regex r (text.begin (), text.end (),
		    std::regex::extended | std::regex::optimize);
      regex = std::move (r);
      return FOUND;
    }
  catch (std::regex_error const &e)
    {
      error = e.code ();
      return FAILED;
    }
}

Result
gaige::regex::Search (std::regex const &regex, std::string_view const &text,
		      std::cmatch &match, int &error) noexcept
{
  try
    {
      std::cmatch m;
      bool found = std::regex_search (text.begin (), text.end (), m, regex);
      match = std::move (m);
      return found ? FOUND : NOTFOUND;
    }
  catch (std::regex_error const &e)
    {
      error = e.code ();
      return FAILED;
    }
}

char const *
gaige::regex::Error (int error)
{
  using namespace std::regex_constants;
  switch (error)
    {
    case error_collate:	return "invalid collating element name";
    case error_ctype:	return "invalid character class name";
    case error_escape:	return "invalid or trailing escape";
    case error_backref:	return "invalid back reference";
    case error_brack:	return "mismatched []";
    case error_paren:	return "mismatched ()";
    case error_brace:	return "mismatched {}";
    case error_badbrace:return "invalid range in {}";
    case error_range:	return "invalid character range";
    case error_space:	return "insufficient memory";
    case error_badrepeat:return "invalid preceder for *?+{";
    case error_complexity:return "too complex";
    case error_stack:	return "insufficient memory";
    default:
      Unreachable ();
    }
}
