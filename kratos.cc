// KRATOS: Kapture Run And Test Output Safely	-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// Not For Distribution

// KRATOS is a test executor.  It looks for RUN lines in the provided
// source and executes them, within a restricted pipeline of checking.

// Joust
#include "error.hh"
#include "lexer.hh"
#include "logger.hh"
#include "readBuffer.hh"
#include "scanner.hh"
#include "spawn.hh"
#include "symbols.hh"
#include "token.hh"
// NMS
#include "fatal.hh"
#include "option.hh"
// C++
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
// C
#include <cstring>
// OS
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/fcntl.h>
#include <sys/signalfd.h>
#include <sys/time.h>
#include <sys/wait.h>

using namespace NMS;
using namespace Joust;

// Not (yet) supported (because I don't have a need for it):

// * Offloading to a remote execution system.  Add $wrapper variable or
// something?

// * Copying files to/from remote system.  Add $cpto $cp from
// variables along with RUN-AUX: or similar.

// * Iteration over a set of flags.  Add RUN-ITERATE: along with
// ability to defer some toplevel variable expansion to runtime.
// RUN-REQUIRE inside a loop would continue to the next iteration of
// the loop.

namespace {

#include "kratos-command.inc"
#include "kratos-pipeline.inc"
#include "kratos-command.inc"
#include "kratos-parser.inc"

}

static void Title (FILE *stream)
{
  fprintf (stream, "KRATOS: Kapture Run And Test Output Safely\n");
  fprintf (stream, "Copyright 2020 Nathan Sidwell, nathan@acm.org\n");
}

int main (int argc, char *argv[])
{
  struct Flags 
  {
    std::vector<char const *> prefixes; // Pattern prefixes
    std::vector<char const *> defines;  // Var defines
    char const *include = nullptr;  // file of var defines
    bool help = false;
    bool version = false;
    bool verbose = false;
  } flags;
  auto append = []
    (Option const *option, char const *opt, char const *arg, void *f)
		{
		  if (!arg[0])
		    Fatal ("option '%s' is empty", opt);
		  for (char const *p = arg; *p; p++)
		    {
		      if (*p == '='
			  && option->offset == offsetof (Flags, defines))
			break;
		      if (!std::isalnum (*p))
			Fatal ("option '%s%s%s' is ill-formed with '%c'", opt,
			       option->argform[0] == '+' ? "" : " ",
			       option->argform[0] == '+' ? "" : arg, *p);
		    }
		  option->Flag<std::vector<char const *>> (f).push_back (arg);
		};
  static constexpr Option const options[] =
    {
      {"help", 'h', offsetof (Flags, help), nullptr, nullptr, "Help"},
      {"version", 0, offsetof (Flags, version), nullptr, nullptr, "Version"},
      {"verbose", 'v', offsetof (Flags, verbose), nullptr, nullptr, "Verbose"},
      {nullptr, 'D', offsetof (Flags, defines), append, "+var=val", "Define"},
      {"defines", 'd', offsetof (Flags, include), nullptr,
       "file", "File of defines"},
      {"prefix", 'p', offsetof (Flags, prefixes), append,
       "prefix", "Pattern prefix (repeatable)"},
      {nullptr, 0, 0, nullptr, nullptr, nullptr}
    };
  int argno = options->Process (argc, argv, &flags);
  if (flags.help)
    {
      Title (stdout);
      options->Help (stdout, "testfile");
      return 0;
    }
  if (flags.version)
    {
      Title (stdout);
      BuildNote (stdout);
      return 0;
    }
  if (argno == argc)
    Fatal ("expected test filename");
  char const *testFile = argv[argno++];

  if (!flags.prefixes.size ())
    flags.prefixes.push_back ("RUN");

  Symbols syms;

  // Register defines
  for (auto d : flags.defines)
    syms.Define (std::string_view (d));
  if (flags.include)
    syms.Read (flags.include);
  if (auto *vars = getenv ("JOUST"))
    if (*vars)
      syms.Read (vars);

  std::vector<Pipeline> pipes;
  {
    std::string pathname = syms.Origin (testFile);
    Parser parser (testFile, pipes, syms);

    // Scan the pattern file
    parser.ScanFile (pathname, flags.prefixes);
  }

  if (pipes.empty () || Error::Errors ())
    Fatal ("failed to construct commands '%s'", testFile);

  Logger logger;
  if (flags.verbose)
    {
      logger.Sum () << "Pipelines\n";
      for (unsigned ix = 0; ix != pipes.size (); ix++)
	logger.Sum () << ix << pipes[ix];
    }

  bool skipping = false;
  unsigned limits[PL_HWM + 1];

  for (unsigned ix = PL_HWM + 1; ix--;)
    {
      static char const *const vars[PL_HWM + 1]
	= {"cpulimit", "memlimit", "filelimit", "timelimit"};

      limits[ix] = 0;
      if (auto limit = syms.Get (vars[ix]))
	{
	  char *eptr;
	  unsigned v = strtoul (limit->c_str (), &eptr, 10);
	  if (*eptr)
	    logger.Result (Logger::ERROR)
	      << "limit '" << vars[ix] << "=" << limit << "' invalid";
	  else
	    limits[ix] = v;
	}
    }

  for (auto &pipe : pipes)
    {
      if (!skipping)
	{
	  logger.Log () << '\n';
	  int e = pipe.Execute (logger, pipe.GetKind () != Pipeline::REQUIRE
				? limits : nullptr);
	  if (e && pipe.GetKind () == Pipeline::REQUIRE)
	    skipping = true;
	  if (e == EINTR)
	    break;
	}
      else
	pipe.Result (logger, Logger::UNSUPPORTED);
    }

  return Error::Errors ();
}
