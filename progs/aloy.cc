// Joust/ALOY: Apply List, Observe Yield		-*- mode:c++ -*-
// Copyright (C) 2020-2024 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

// ALOY is a parallelized test execution engine.  It reads lines from
// stdin, and then applies those to the command provided to it.  These
// are executed in parallel under MAKEFLAGS control.

#include "joust/cfg.h"
// NMS
#include "nms/fatal.hh"
#include "nms/option.hh"
// Gaige
#include "gaige/error.hh"
#include "gaige/lexer.hh"
#include "gaige/readBuffer.hh"
#include "gaige/spawn.hh"
// Joust
#include "joust/tester.hh"
// C++
#include <algorithm>
#include <deque>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
// C
#include <cstring>
// OS
#include <signal.h>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <time.h>
#include <unistd.h>
#ifdef USE_EPOLL
#include <sys/epoll.h>
#include <sys/signalfd.h>
#else
#include <sys/select.h>
#endif
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

using namespace nms;
using namespace joust;
using namespace gaige;

namespace {
// clang-format off
class Engine;
#include "aloy-job.inc"
#include "aloy-engine.inc"
#include "aloy-job.inc"
#include "aloy-engine.inc"
// clang-format on
} // namespace

static void title (FILE *stream) {
  fprintf(stream, "ALOY: Apply List, Observe Yield\n");
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
    unsigned jobs = 0;
    char const *tester = "kratos";
    std::vector<std::string> gen;
    char const *out = "";
    char const *dir = nullptr;
  } flags;
  static constinit nms::Option const options[]
      = {{"help", 'h', OPTION_FLDFN(Flags, help), "Help"},
         {"version", 0, OPTION_FLDFN(Flags, version), "Version"},
         {"verbose", 'v', OPTION_FLDFN(Flags, verbose), "Verbose"},
         {"dir", 'C', OPTION_FLDFN(Flags, dir), "DIR:Set directory"},
         {"jobs", 'j', nms::Option::F_IsConcatenated,
          OPTION_FLDFN(Flags, jobs), "N:Concurrency"},
         {"out", 'o', OPTION_FLDFN(Flags, out), "FILE:Output"},
         {"gen", 'g', OPTION_FLDFN(Flags, gen), "PROGRAM:Generator"},
         {"tester", 't', OPTION_FLDFN(Flags, tester), "PROGRAM:Tester"},
         {}};
  int argno = options->parseArgs(argc, argv, &flags);
  if (flags.help) {
    title(stdout);
    options->printUsage(stdout, "[generator-options]");
    return 0;
  }
  if (flags.version) {
    title(stdout);
    printBuildNote(stdout);
    return 0;
  }
  if (flags.dir)
    if (chdir(flags.dir) < 0)
      fatalExit("?cannot chdir '%s': %m", flags.dir);

  // Get the log streams
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

  Engine engine(std::min(flags.jobs, 256u), flags.out ? sum : std::cout,
                flags.out ? log : std::cerr);

  engine.init(flags.tester, flags.gen.empty() ? nullptr : &flags.gen,
              argc - argno, argv + argno);
  bool show_progress = flags.out && isatty(1);
  size_t progress_size = 0;

  // MainLoop
  while (engine.isLive()) {
    engine.spawn();
    engine.process();
    engine.retire(flags.out ? &std::cout : nullptr);
    if (show_progress) {
      std::string text = engine.getProgress();
      auto text_size = text.size();

      // Append spaces to rub out previous longer progress
      if (text_size < progress_size)
        text.append(progress_size - text_size, ' ');

      // Append backspaces so the cursor remains at the start of
      // the progress.
      text.append(text.size(), '\b');
      write(1, text.data(), text.size());
      progress_size = text_size;
    }
  }

  if (show_progress) {
    std::string text;
    text.append(progress_size, ' ');
    text.append(progress_size, '\b');
    write(1, text.data(), text.size());
  }

  engine.fini(flags.out ? &std::cout : nullptr);

  sum.close();
  log.close();

  return 0;
}
