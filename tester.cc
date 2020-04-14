// Joust			-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// Not For Distribution

// Joust
#include "tester.hh"
// Utils
#include "fatal.hh"
// C++
#include <iostream>

namespace Joust {

bool Tester::AddStatus (std::string_view const &line)
{
  for (unsigned ix = STATUS_HWM; ix--;)
    if (line.size () > statuses[ix].size ()
	&& line.starts_with (statuses[ix])
	&& line[statuses[ix].size ()] == ':')
      {
	counts[ix]++;
	return true;
      }

  return false;
}

void Tester::Result (Status status, char const *file, unsigned line,
		     char const *kind, std::string const &msg)
{
  std::cout << statuses[status] << ": "
	    << file << ':' << line << ':'
	    << kind << ' ' << msg << '\n';
}

std::ostream &operator<< (std::ostream &s, Tester const &self)
{
  for (unsigned ix = 0; ix != Tester::STATUS_HWM; ix++)
    if (ix == Tester::PASS || self.counts[ix])
      s << self.statuses[ix] << ' ' << self.counts[ix] << '\n';

  return s;
}

}
