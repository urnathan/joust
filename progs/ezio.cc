// Joust/EZIO: Expect Zero Irregularities Observed	-*- mode:c++ -*-
// Copyright (C) 2020-2024 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

// EZIO is a pattern matcher.  It looks for CHECK lines in the
// provided source and then applies those to the text provided in
// stdin.

#include "joust/cfg.h"
// NMS
#include "nms/fatal.hh"
#include "nms/macros.hh"
#include "nms/option.hh"
// Gaige
#include "gaige/error.hh"
#include "gaige/lexer.hh"
#include "gaige/regex.hh"
#include "gaige/scanner.hh"
#include "gaige/symbols.hh"
#include "gaige/token.hh"
// Joust
#include "joust/tester.hh"
// C++
#include <algorithm>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <string_view>
#include <unordered_map>
// C
#include <stddef.h>
// OS
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace nms;
using namespace joust;
using namespace gaige;

namespace {
class Engine;
// clang-format off
#include "ezio-parser.inc"
#include "ezio-pattern.inc"
#include "ezio-engine.inc"
#include "ezio-parser.inc"
// clang-format on
} // namespace

static void title (FILE *stream) {
  fprintf(stream, "EZIO: Expect Zero Irregularities Observed\n");
  fprintf(stream, "Copyright 2020-2024 Nathan Sidwell, nathan@acm.org\n");
}

int main (int argc, char *argv[]) {
#include "joust/project-ident.inc"
  nms::setBuildInfo(JOUST_PROJECT_IDENTS);
  nms::installSignalHandlers();

  struct Flags {
    bool help = false;
    bool version = false;
    bool verbose = false;
    std::vector<char const *> prefixes; // Pattern prefixes
    std::vector<char const *> defines;  // Var defines
    char const *include = nullptr;      // File of var defines
    char const *in = "";
    char const *out = "";
    char const *dir = nullptr;
  } flags;
  static constinit nms::Option const options[] = {
      {"help", 'h', OPTION_FLDFN(Flags, help), "Help"},
      {"version", 0, OPTION_FLDFN(Flags, version), "Version"},
      {"verbose", 'v', OPTION_FLDFN(Flags, verbose), "Verbose"},
      {"dir", 'C', nms::Option::F_IsConcatenated, OPTION_FLDFN(Flags, dir),
       "DIR:Set directory"},
      {nullptr, 'D', OPTION_FLDFN(Flags, defines), "VAR=VAL:Define"},
      {"defines", 'd', OPTION_FLDFN(Flags, include), "FILE:File of defines"},
      {"in", 'i', OPTION_FLDFN(Flags, in), "FILE:Input"},
      {"out", 'o', OPTION_FLDFN(Flags, out), "FILE:Output"},
      {"prefix", 'p', OPTION_FLDFN(Flags, prefixes), "PREFIX:Pattern prefix"},
      {}};
  int argno = options->parseArgs(argc, argv, &flags);
  if (flags.help) {
    title(stdout);
    options->printUsage(stdout, "pattern-files+");
    return 0;
  }
  if (flags.version) {
    title(stdout);
    printBuildNote(stdout);
    return 0;
  }
  if (flags.dir)
    if (chdir(flags.dir) < 0)
      fatalExit("cannot chdir '%s': %m", flags.dir);

  if (!flags.prefixes.size())
    flags.prefixes.push_back("CHECK");

  Symbols syms;

  // Register defines
  syms.value("EOF", "${}EOF");
  for (auto d : flags.defines)
    syms.define(std::string_view(d));
  if (flags.include)
    syms.readFile(flags.include);
  if (auto *vars = getenv("JOUST"))
    if (*vars)
      syms.readFile(vars);

  std::ofstream sum, log;
  if (!flags.out[flags.out[0] == '-'])
    flags.out = nullptr;
  else {
    std::string out(flags.out);
    size_t len = out.size();
    out.append(".sum");
    sum.open(out);
    if (!sum.is_open())
      fatalExit("cannot write '%s': %m", out.c_str());
    out.erase(len).append(".log");
    log.open(out);
    if (!log.is_open())
      fatalExit("cannot write '%s': %m", out.c_str());
  }

  Engine engine(syms, flags.out ? sum : std::cout,
                flags.out ? log : std::cerr);

  while (argno != argc) {
    char const *patternFile = argv[argno++];
    std::string pathname = syms.setOriginValues(patternFile);
    Parser parser(patternFile, engine);

    // Scan the pattern file
    parser.scanFile(pathname, flags.prefixes);
    if (Error::hasErrored())
      fatalExit("failed to construct patterns '%s'", patternFile);
  }

  engine.initialize();

  if (flags.verbose)
    engine.log() << engine << '\n';

  engine.process(flags.in);

  engine.finalize();

  sum.close();
  log.close();

  return Error::hasErrored();
}
