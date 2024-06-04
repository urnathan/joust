// Joust Test Suite			-*- mode:c++ -*-
// Copyright (C) 2020-2024 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#include "joust/cfg.h"
// Joust
#include "joust/tester.hh"
// C
#include <cstdlib>
// OS
#include <unistd.h>

using namespace joust;

constinit
std::string_view const Tester::statuses[]
= {NMS_LIST (NMS_STRING, JOUST_STATUSES)};

Tester::Tester () noexcept
  : Tester (std::cerr)
{
  /* If one of the std streams is not a tty, then enable sum stream
     too.  This is pretty much the best we can do.   */
  if (!isatty (1) || !isatty (2))
    sum = &std::cout;
}

Tester::Status Tester::DecodeStatus (std::string_view const &line) noexcept
{
  for (unsigned ix = STATUS_HWM; ix--;)
    if (line.size () > statuses[ix].size ()
	&& line.starts_with (statuses[ix])
	&& line[statuses[ix].size ()] == ':')
      return Status (ix);

  return STATUS_HWM;
}

Tester::Streamer Tester::Result (Status status, nms::SrcLoc loc) noexcept
{
  Streamer result (this);

  result << statuses[status] << ": ";

  if (auto const *file = loc.file ())
    {
      if (char const *srcdir = getenv ("srcdir"))
	{
	  // Strip off a leading $srcdir from the filename -- if it
	  // came from __FILE__ it'll have that in it, which is
	  // unnecessary
	  for (size_t pos = 0;; pos++)
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

      result << file << ':';
      if (auto line = loc.line ())
	result << line << ':';
    }

  return result;
}
