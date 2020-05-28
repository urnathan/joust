// Joust Test Suite			-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

// Joust
#include "logger.hh"
// NMS
#include "fatal.hh"
// C++
#include <iostream>

namespace Joust {

Logger::Status Logger::DecodeStatus (std::string_view const &line)
{
  for (unsigned ix = STATUS_HWM; ix--;)
    if (line.size () > statuses[ix].size ()
	&& line.starts_with (statuses[ix])
	&& line[statuses[ix].size ()] == ':')
      return Status (ix);

  return STATUS_HWM;
}

Logger::Streamer Logger::Result (Status status)
{
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

}
