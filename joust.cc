// Joust Test Suite			-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

// Joust
#include "joust.hh"
// C
#include <cstdlib>

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
    {
      if (char const *srcdir = getenv ("srcbuilddir"))
	{
	  // Strip off a leading $srcdir from the filename -- if it
	  // came from __FILE__ it'll have that in it, which is
	  // unnecessary
	  for (size_t pos = 0; ; pos++)
	    {
	      if (char c = srcdir[pos])
		{
		  if (file[pos] != c)
		    break;
		}
	      else
		{
		  if (file[pos] == '/')
		    file += pos + 1;
		  break;
		}
	    }
	}

      result << file << ':' << line << ':';
    }

  return result;
}

}
