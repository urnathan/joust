// Joust Test Suite			-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

// Joust
#include "joust-logger.hh"

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

Logger::Streamer Logger::Result (Status status, char const *file, unsigned line)
{
  Streamer result (this);

  result << statuses[status] << ": ";

  if (file)
    result << file << ':' << line << ':';

  return result;
}

}
