// Joust/ALOY: Apply List, Observe Yield		-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

// ALOY is a parallelized test execution engine.  It reads lines from
// stdin, and then applies those to the command provided to it.  These
// are executed in parallel under MAKEFLAGS control.

// Joust
#include "error.hh"
#include "joust.hh"
#include "lexer.hh"
#include "readBuffer.hh"
#include "spawn.hh"
#include "fatal.hh"
#include "option.hh"
// C++
#include <algorithm>
#include <deque>
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <string_view>
// C
#include <cstring>
// OS
#include <time.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/fcntl.h>
#ifdef USE_EPOLL
#include <sys/epoll.h>
#include <sys/signalfd.h>
#else
#include <sys/select.h>
#endif
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

using namespace Joust;

namespace
{

class Engine;
#include "aloy-job.inc"
#include "aloy-engine.inc"
#include "aloy-job.inc"
#include "aloy-engine.inc"

}

static void Title
  (FILE *stream)
{
  fprintf (stream, "ALOY: Apply List, Observe Yield\n");
  fprintf (stream, "Copyright 2020 Nathan Sidwell, nathan@acm.org\n");
}

int main
  (int argc, char *argv[])
{
  SignalHandlers ();

  struct Flags 
  {
    bool help = false;
    bool version = false;
    bool verbose = false;
    unsigned jobs = 0;
    char const *tester = "kratos";
    char const *gen = nullptr;
    char const *out = "";
    char const *dir = nullptr;
  } flags;
  constexpr auto uint_fn
    = [] (Option const *option, char const *opt, char const *arg, void *f)
      {
	char *eptr;
	option->Flag<unsigned> (f) = strtol (arg, &eptr, 0);
	if (*eptr || eptr == arg)
	  Fatal ("option '%s %s' illformed", opt, arg);
      };
  static constexpr Option const options[] =
    {
      {"help", 'h', offsetof (Flags, help), nullptr, nullptr, "Help"},
      {"version", 0, offsetof (Flags, version), nullptr, nullptr, "Version"},
      {"verbose", 'v', offsetof (Flags, verbose), nullptr, nullptr, "Verbose"},
      {"dir", 'C', offsetof (Flags, dir), nullptr, "directory", "Set directory"},
      {"jobs", 'j', offsetof (Flags, jobs), uint_fn, "+val", "Define"},
      {"out", 'o', offsetof (Flags, out), nullptr, "file", "Output"},
      {"gen", 'g', offsetof (Flags, gen), nullptr, "prog", "Generator"},
      {"tester", 't', offsetof (Flags, tester), nullptr, "prog", "Tester"},
      {nullptr, 0, 0, nullptr, nullptr, nullptr}
    };
  int argno = options->Process (argc, argv, &flags);
  if (flags.help)
    {
      Title (stdout);
      options->Help (stdout, "[generator-options]");
      return 0;
    }
  if (flags.version)
    {
      Title (stdout);
      BuildNote (stdout);
      return 0;
    }
  if (flags.dir)
    if (chdir (flags.dir) < 0)
      Fatal ("cannot chdir '%s': %m", flags.dir);

  // Get the log streams
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
	Fatal ("cannot write '%s': %m", out.c_str ());
      out.erase (len).append (".log");
      log.open (out);
      if (!log.is_open ())
	Fatal ("cannot write '%s': %m", out.c_str ());
    }

  Engine engine (std::min (flags.jobs, 256u),
		 flags.out ? sum : std::cout, flags.out ? log : std::cerr);

  engine.Init (flags.tester, flags.gen, argc - argno, argv + argno);
  bool show_progress = flags.out && isatty (1);;
  size_t progress_size = 0;

  // MainLoop
  while (engine.IsLive ())
    {
      engine.Spawn ();
      engine.Process ();
      engine.Retire ();
      if (show_progress)
	{
	  std::string text = engine.Progress ();
	  auto text_size = text.size ();

	  // Append spaces to rub out previous longer progress
	  if (text_size < progress_size)
	    text.append (progress_size - text_size, ' ');

	  // Append backspaces so the cursor remains at the start of
	  // the progress.
	  text.append (text.size (), '\b');
	  write (1, text.data (), text.size ());
	  progress_size = text_size;
	}
    }

  if (show_progress)
    {
      std::string text;
      text.append (progress_size, ' ');
      text.append (progress_size, '\b');
      write (1, text.data (), text.size ());
    }

  engine.Fini (flags.out ? &std::cout : nullptr);

  sum.close ();
  log.close ();

  return 0;
}
