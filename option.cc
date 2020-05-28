// NMS		-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

// NMS
#include "fatal.hh"
#include "option.hh"
// C
#include <cstring>

namespace Joust {

void Option::Help (FILE *stream, char const *args) const
{
  enum {indent = 26};

  fprintf (stdout, "Usage: %s [options] %s\n", progname, args);
  for (auto *opt = this; opt->sname || opt->cname; opt++)
    {
      int len = fprintf (stream, "  ");
      if (opt->cname)
	len += fprintf (stream, "-%c", opt->cname);
      if (opt->sname)
	{
	  if (opt->cname)
	    len += fprintf (stream, " ");
	  len += fprintf (stream, "--%s", opt->sname);
	}
      if (opt->argform)
	{
	  bool adjacent = opt->argform[0] == '+';
	  len += fprintf (stream, "%s<%s>",
			  " " + adjacent,
			  opt->argform + adjacent);
	}

      if (len > indent)
	{
	  fprintf (stream, "\n");
	  len = 0;
	}
      while (len <= indent)
	len += fprintf (stream, " ");
      fprintf (stream, "%s\n", opt->help);
    }
}

int Option::Process (int argc, char **argv, void *flags) const
{
  int argno = 0;

  Progname (argv[argno++]);

  while (argno != argc)
    {
      char const *opt = argv[argno];

      if (opt[0] != '-')
	break;
      argno++;

      bool dash = opt[1] == '-';
      for (auto self = this; self->sname || self->cname; self++)
	{
	  bool single = false;
	  if (!dash && self->cname == opt[1])
	    single = true;
	  else if (!(self->sname && !strcmp (opt + 1 + dash, self->sname)))
	    continue;

	  char const *arg = nullptr;
	  if (self->argform)
	    {
	      if (single && self->argform[0] == '+')
		arg = opt + 2;
	      else if (argno == argc)
		Fatal ("option '%s' requires <%s> argument", opt,
		       argform + (argform[0] == '+'));
	      else
		arg = argv[argno++];
	    }
	  else if (single && opt[2])
	    continue;

	  if (self->process)
	    self->process (self, opt, arg, flags);
	  else if (arg)
	    self->Flag<char const *> (flags) = arg;
	  else
	    self->Flag<bool> (flags) = true;

	  goto found;
	}
      Fatal ("unknown option '%s'", opt);
    found:;
    }

  return argno;
}

}
