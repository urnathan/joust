// KRATOS: Kapture Run And Test Output Safely	-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// Not For Distribution

class Pipeline
{
public:
#define PIPELINE_KINDS				\
  PIPELINE_KIND_FROB(RUN),			\
    PIPELINE_KIND_FROB(SIGNAL),			\
    PIPELINE_KIND_FROB(REQUIRE),		\
    PIPELINE_KIND_FROB(END)
#define PIPELINE_KIND_FROB(KIND) KIND
  enum Kind : unsigned char
  {
    PIPELINE_KINDS,
    PIPELINE_HWM,
    PIPE_HWM = REQUIRE
  };
#undef PIPELINE_KIND_FROB

public:
  static constexpr char const *kinds[PIPELINE_HWM]
#define PIPELINE_KIND_FROB(KIND) #KIND
    = { PIPELINE_KINDS };
#undef PIPELINE_KIND_FROB

private:
  std::vector<Command> commands;
  std::string srcsink[2];
  int fds[2] {-1, -1}; // sinks of cmd[0]
  Kind kind = RUN;
  unsigned expected_exit : 8 = 0;
  bool expect_inverted : 1 = false;
  bool xfailed : 1 = false;

public:
  Pipeline (Kind k, bool inverted, unsigned exit = 0)
    : kind (k), expected_exit (exit), expect_inverted (inverted)
  {
  }

public:
  Kind GetKind () const
  {
    return kind;
  }

public:
  bool Execute (Logger &);

public:
  void Result (Logger &, Logger::Status);

private:
  friend std::ostream &operator<< (std::ostream &s, Pipeline const &pipe);
  friend class Parser;
};

std::ostream &operator<< (std::ostream &s, Pipeline const &pipe)
{
  for (unsigned ix = 0; ix != pipe.commands.size (); ix++)
    {
      Command const &cmd = pipe.commands[ix];

      if (ix)
	s << " |\n ";
      s << '.' << ix << ' ';

      s << Pipeline::kinds[ix ? Pipeline::RUN : pipe.kind] << ':';
      if (!ix)
	{
	  if (pipe.expected_exit)
	    s << pipe.expected_exit;
	  s << ("! " + !pipe.expect_inverted);

	  for (unsigned jx = 0; jx != 2; jx++)
	    if (!pipe.srcsink[jx].empty ())
	      s << (jx ? '>' : '<') << pipe.srcsink[jx] << ' ';
	}
      else
	s << " ";

      s << cmd;
    }
  s << "\n";

  return s;
}

bool Pipeline::Execute (Logger &logger)
{
  Assert (commands.size () >= 1 && commands.size () <= 3);

  auto &cmd = commands.front ();

  char const *in = srcsink[0].c_str ();
  if (!*in)
    in = "/dev/null";
  cmd.stdin = open (in, O_RDONLY | O_CLOEXEC);
  if (cmd.stdin < 0)
    {
      int err = errno;
      cmd.Error () << "cannot read '" << in << "': " << strerror (err);
    }

  if (!srcsink[1].empty ())
    {
      fds[0] = open (srcsink[1].c_str (),
		     O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
		     S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
      if (fds[0] < 0)
	{
	  int err = errno;
	  cmd.Error () << "cannot write '" << srcsink[1] << "': "
		       << strerror (err);
	}
    }

  // Pipes between commands.  The final command (if any) gets stderr,
  // the one before that (if any) gets stdout
  for (unsigned ix = 1; commands.size () - ix; ix++)
    {
      // [0] - read from
      // [1] - write to
      int pipe[2];
      if (pipe2 (pipe, O_CLOEXEC) < 0)
	{
	  int err = errno;
	  cmd.Error () << "cannot create pipe: " << strerror (err);
	}
      else
	{
	  fds[2 - ix] = pipe[1];
	  commands[commands.size () - ix].stdin = pipe[0];
	}
    }

  unsigned subtasks = 0;
  for (auto iter = commands.begin (); iter != commands.end (); ++iter)
    {
      bool first = iter == commands.begin ();
      if (iter->Execute (first ? fds : nullptr))
	subtasks++;
    }

  for (unsigned ix = 2; ix--;)
    if (fds[ix] > 2)
      {
	close (fds[ix]);
	fds[ix] = -1;
      }

  // Wait for completion
  bool signalled = false;
  int exit_code = -1;
  while (subtasks)
    {
      int status;
      pid_t pid = wait (&status);
      Assert (pid > 0);
      for (auto iter = commands.begin (); iter != commands.end (); ++iter)
	if (pid == iter->pid)
	  {
	    bool sig = false;
	    int ex = 0;
	    if (WIFEXITED (status))
	      ex = WEXITSTATUS (status);
	    else if (WIFSIGNALED (status))
	      {
		ex = WTERMSIG (status);
		sig = true;
	      }
	    else
	      break;

	    subtasks--;
	    if (iter == commands.begin ()
		&& (kind != REQUIRE || !signalled))
	      {
		signalled = sig;
		exit_code = ex;
	      }
	    else
	      {
		iter->Error ()
		  << '\'' << iter->words[0] << "' exited with "
		  << (sig ? "signal " : "code ") << ex;
		Result (logger, Logger::ERROR);
	      }
	  }
    }

  bool pass = (exit_code >= 0
	       && signalled == (kind == SIGNAL)
	       && (exit_code == expected_exit) == !expect_inverted);
  if (exit_code >= 0 && kind != REQUIRE)
    Result (logger, Logger::PassFail (pass, xfailed));

  return pass;
}

void Pipeline::Result (Logger &logger, Logger::Status status)
{
  auto const &cmd = commands.front ();

  logger.Result (status, cmd.file, cmd.line)
    << kinds[kind] << ' ' << cmd.words.front ();
}

#if 0
// RUN: cmd args \?
// RUN: file> cmd args >file 2>file |
// RUN: next
// RUN: {...} $var ${var}
// RUN:N ....
// RUN-SIGNAL:N/name ...
// RUN-REQUIRE: program args ...
// RUN-END:
// $tmp $test $basesrc $srcpath
#endif
/*

// DRAKE: Dynamic Response And Keyboard Emulation	-*- mode:c++ -*-
// ALOY: Apply List, Observe Yield		-*- mode:c++ -*-
// KRATOS: Kapture Run And Test Output Safely	-*- mode:c++ -*-
// EZIO: Expect Zero Irregularities Observed	-*- mode:c++ -*-

// cat | aloy ... -jN subcmd options
// MAKEFLAGS --jobserver-auth=R,W

JOUST Journal Of User Scripted Tests
JOUST Journal Of Utterly Stupid Tests


*/
