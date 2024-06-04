// Joust/KRATOS: Kapture Run And Test Output Safely	-*- mode:c++ -*-
// Copyright (C) 2020-2024 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

// KRATOS is a test executor.  It looks for RUN lines in the provided
// source and executes them, within a restricted pipeline of checking.

#include "joust/cfg.h"
// NMS
#include "nms/fatal.hh"
#include "nms/macros.hh"
#include "nms/option.hh"
// Gaige
#include "gaige/error.hh"
#include "gaige/lexer.hh"
#include "gaige/readBuffer.hh"
#include "gaige/scanner.hh"
#include "gaige/spawn.hh"
#include "gaige/symbols.hh"
#include "gaige/token.hh"
// Joust
#include "joust/tester.hh"
// C++
#include <algorithm>
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
#ifdef USE_EPOLL
#include <sys/epoll.h>
#include <sys/signalfd.h>
#else
#include <sys/select.h>
#endif
#include <sys/fcntl.h>
#include <sys/time.h>
#include <sys/wait.h>

using namespace nms;
using namespace joust;
using namespace gaige;

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

} // namespace

static void Title (FILE *stream)
{
  fprintf (stream, "KRATOS: Kapture Run And Test Output Safely\n");
  fprintf (stream, "Copyright 2020-2024 Nathan Sidwell, nathan@acm.org\n");
}

int main (int argc, char *argv[])
{
#include "joust/project-ident.inc"
  nms::setBuildInfo (JOUST_PROJECT_IDENTS);
  nms::installSignalHandlers ();

  struct Flags
  {
    bool help = false;
    bool version = false;
    bool verbose = false;
    std::vector<char const *> prefixes; // Pattern prefixes
    std::vector<char const *> defines;  // Var defines
    char const *include = nullptr;      // file of var defines
    char const *out = "";
    char const *dir = nullptr;
  } flags;
  static constinit nms::Option const options[]
    = {{"help", 'h', OPTION_FLDFN (Flags, help), "Help"},
       {"version", 0, OPTION_FLDFN (Flags, version), "Version"},
       {"verbose", 'v', OPTION_FLDFN (Flags, verbose), "Verbose"},
       {"dir", 'C', OPTION_FLDFN (Flags, dir), "DIR:Set directory"},
       {nullptr, 'D', nms::Option::F_IsConcatenated,
	OPTION_FLDFN (Flags, defines), "VAR=VAL:Define"},
       {"defines", 'd', OPTION_FLDFN (Flags, include), "FILE:File of defines"},
       {"out", 'o', OPTION_FLDFN (Flags, out), "FILE:Output"},
       {"prefix", 'p', OPTION_FLDFN (Flags, prefixes), "PREFIX:Pattern prefix"},
       {}};
  int argno = options->parseArgs (argc, argv, &flags);
  if (flags.help)
    {
      Title (stdout);
      options->printUsage (stdout, "testfile");
      return 0;
    }
  if (flags.version)
    {
      Title (stdout);
      printBuildNote (stdout);
      return 0;
    }
  if (flags.dir)
    if (chdir (flags.dir) < 0)
      fatalExit ("?cannot chdir '%s': %m", flags.dir);

  if (argno == argc)
    fatalExit ("?expected test filename");
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
  bool ended = false;
  {
    std::string pathname = syms.Origin (testFile);
    Parser parser (testFile, pipes, syms);

    // Scan the pattern file
    ended = parser.ScanFile (pathname, flags.prefixes);
  }

  if ((!ended && pipes.empty ()) || Error::Errors ())
    fatalExit ("?failed to construct commands '%s'", testFile);

  std::ofstream sum, log;
  if (!flags.out[flags.out[0] == '-'])
    flags.out = nullptr;
  else
    {
      std::string out (flags.out);
      size_t len = out.size ();
      out.append (".sum");
      sum.open (out);
      if (!sum.is_open ())
	fatalExit ("cannot write '%s': %m", out.c_str ());
      out.erase (len).append (".log");
      log.open (out);
      if (!log.is_open ())
	fatalExit ("cannot write '%s': %m", out.c_str ());
    }

  Tester logger (flags.out ? sum : std::cout, flags.out ? log : std::cerr);
  if (flags.verbose)
    {
      logger.Sum () << "Pipelines\n";
      for (unsigned ix = 0; ix != pipes.size (); ix++)
	logger.Sum () << ix << pipes[ix];
    }

  unsigned limits[PL_HWM + 1];

  for (unsigned ix = PL_HWM + 1; ix--;)
    {
      static char const *const vars[PL_HWM + 1]
	= {"cpulimit", "memlimit", "filelimit", "timelimit"};

      // Default to 1 minute or 1 GB
      limits[ix] = ix == PL_CPU || ix == PL_HWM ? 60 : 1;
      if (auto limit = syms.Get (vars[ix]))
	{
	  Lexer lexer (*limit);

	  if (!lexer.Integer () || lexer.Peek ())
	    logger.Result (Tester::ERROR, testFile)
	      << "limit '" << vars[ix] << "=" << *limit << "' invalid";
	  else
	    limits[ix] = lexer.GetToken ()->GetInteger ();
	}
    }

  if (pipes.empty ())
    logger.Result (Tester::PASS, nms::SrcLoc (testFile)) << "No tests to test";

  bool skipping = false;
  for (auto &pipe : pipes)
    {
      if (!skipping)
	{
	  logger.Log () << '\n';
	  int e = pipe.Execute (logger, pipe.GetKind () < Pipeline::PIPE_HWM
				? limits : nullptr);
	  if (e == EINTR)
	    break;

	  if (e && pipe.GetKind () == Pipeline::REQUIRE)
	    skipping = true;
	}
      else if (pipe.GetKind () != Pipeline::REQUIRE)
	{
	  pipe.Result (logger, Tester::UNSUPPORTED);
	  skipping = false;
	}
    }

  return Error::Errors ();
}
