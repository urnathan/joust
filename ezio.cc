// EZIO: Expect Zero Irregulities Observed	-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// Not For Distribution

// EZIO is a pattern matcher.  It looks for CHECK lines in the
// provided source and then applies those to the text provided in
// stdin.

// Joust
#include "error.hh"
#include "lexer.hh"
#include "logger.hh"
#include "regex.hh"
#include "scanner.hh"
#include "symbols.hh"
#include "token.hh"
// Utils
#include "fatal.hh"
#include "option.hh"
// C++
#include <algorithm>
#include <iostream>
#include <regex>
#include <string>
#include <string_view>
#include <unordered_map>
// OS
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

using namespace Utils;
using namespace Joust;

namespace {

class Engine;
#include "ezio-parser.inc"
#include "ezio-symtab.inc"
#include "ezio-pattern.inc"
#include "ezio-engine.inc"
#include "ezio-parser.inc"
}

static void Title (FILE *stream)
{
  fprintf (stream, "EZIO: Expect Zero Irregularities Observed\n");
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
      {"include", 'i', offsetof (Flags, include), nullptr,
       "file", "File of defines"},
      {"prefix", 'p', offsetof (Flags, prefixes), append,
       "prefix", "Pattern prefix (repeatable)"},
      {nullptr, 0, 0, nullptr, nullptr, nullptr}
    };
  int argno = options->Process (argc, argv, &flags);
  if (flags.help)
    {
      Title (stdout);
      options->Help (stdout, "pattern-file [test-file]");
      return 0;
    }
  if (flags.version)
    {
      Title (stdout);
      BuildNote (stdout);
      return 0;
    }

  if (argno == argc)
    Fatal ("pattern filename missing");
  char const *patternFile = argv[argno++];
  char const *outputFile = "-";
  if (argno != argc)
    outputFile = argv[argno++];
  if (argno != argc)
    Fatal ("unexpected argument after '%s'", outputFile);

  if (!flags.prefixes.size ())
    flags.prefixes.push_back ("CHECK");

  Symtab syms;

  // Register defines
  syms.text.Set ("EOF", "");
  for (auto d : flags.defines)
    syms.text.Define (std::string_view (d));
  if (flags.include)
    syms.text.Read (flags.include);
  std::string pathname = syms.text.SetPaths (patternFile);
  Engine engine (syms);
  {
    Parser parser (patternFile, engine);

    // Scan the pattern file
    parser.ScanFile (pathname.c_str (), flags.prefixes);
    engine.Initialize (parser);
  }

  if (Error::Errors ())
    Fatal ("failed to construct patterns '%s'", patternFile);

  if (flags.verbose)
    {
      // FIXME: dump the patterns
    }

  engine.Process (outputFile);

  engine.Finalize ();

  return Error::Errors ();
}
