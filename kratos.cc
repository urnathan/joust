// KRATOS: Kapture Run And Test Output Safely	-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// Not For Distribution

// KRATOS is a test executor.  It looks for RUN lines in the provided
// source and executes them, within a restricted pipeline of checking.

// Joust
#include "error.hh"
#include "lexer.hh"
#include "logger.hh"
#include "scanner.hh"
#include "symbols.hh"
#include "token.hh"
// NMS
#include "fatal.hh"
#include "option.hh"
// C++
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
#include <sys/wait.h>

using namespace NMS;
using namespace Joust;

// FIXME: add RUN-ITERATE: var {val1} {val2} ...
// FIXME: expand variables upon execution
// FIXME: add RUN-FORWARD

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
      {"include", 'i', offsetof (Flags, include), nullptr,
       "file", "File of defines"},
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
  // Scan the pattern file
  if (argno == argc)
    Fatal ("expected test filename");
  char const *testFile = argv[argno++];

  Symbols syms;

  // Register defines
  for (auto d : flags.defines)
    syms.Define (std::string_view (d));
  if (flags.include)
    syms.Read (flags.include);

  std::vector<Pipeline> pipes;
  {
    std::vector<char const *> prefixes {"RUN"};
    std::string pathname = syms.Origin (testFile);
    Parser parser (testFile, pipes, syms);

    // Scan the pattern file
    parser.ScanFile (pathname, prefixes);
  }

  if (pipes.empty () || Error::Errors ())
    Fatal ("failed to construct commands '%s'", testFile);

  if (flags.verbose)
    {
      std::cout << "Pipelines\n";
      for (unsigned ix = 0; ix != pipes.size (); ix++)
	std::cout << ix << pipes[ix];
    }

  bool skipping = false;
  Logger logger;
  for (auto &pipe : pipes)
    {
      if (!skipping)
	{
	  if (!pipe.Execute (logger)
	      && pipe.GetKind () == Pipeline::REQUIRE)
	    {
	      skipping = true;
	      goto unsupported;
	    }
	}
      else
	{
	unsupported:
	  pipe.Result (logger, Logger::UNSUPPORTED);
	}
    }

  return Error::Errors ();
}
