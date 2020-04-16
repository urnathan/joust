// Joust			-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// Not For Distribution

// Joust
#include "logger.hh"
// NMS
#include "fatal.hh"
// C++
#include <iostream>

namespace Joust {

Logger::Logger (std::ostream &s, std::ostream &l)
  : sum (s), log (l)
{
  for (unsigned ix = STATUS_HWM; ix--;)
    counts[ix] = 0;
}

bool Logger::AddStatus (std::string_view const &line)
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

Logger::Streamer Logger::Result (Status status)
{
  counts[status]++;

  Streamer result (this);

  result << statuses[status] << ": ";

  return result;
}

Logger::Streamer Logger::Result (Status status, char const *file, unsigned line)
{
  auto result = Result (status);

  result << file << ':' << line << ':';

  return result;
}

std::ostream &operator<< (std::ostream &s, Logger const &self)
{
  for (unsigned ix = 0; ix != Logger::STATUS_HWM; ix++)
    if (ix == Logger::PASS || self.counts[ix])
      s << self.statuses[ix] << ' ' << self.counts[ix] << '\n';

  return s;
}

}
