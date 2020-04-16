// KRATOS: Kapture Run And Test Output Safely	-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// Not For Distribution

class Command
{
private:
  std::vector<std::string> words;  // parsed line
  char const *file = nullptr;
  unsigned line = 0;
  int stdin = -1;
  pid_t pid = 0;

public:
  Command (char const *f, unsigned l)
    :file (f), line (l)
  {
  }

public:
  auto Error ()
  {
    return Joust::Error (file, line);
  }

public:
  bool Execute (int *fds);

private:
  void Shellify ();

  friend std::ostream &operator<<  (std::ostream &, Command const &);

  friend class Pipeline;
  friend class Parser;
};

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

void Command::Shellify ()
{
  bool active = false;
  size_t len = 0;
  for (auto iter = words.begin (); iter != words.end (); ++iter)
    {
      std::string const &word = *iter;
      len += word.size () + 1;
      if (!active)
	{
	  constexpr char const *meta = R"(|&;[])";
	  for (auto *ptr = meta; *ptr; ptr++)
	    if (word.find (*ptr) != word.npos)
	      {
		active = true;
		break;
	      }
	}
    }

  if (active)
    {
      // We found a shell-active character.  Make a single string
      std::string concat;
      concat.reserve (len);
      
      for (auto iter = words.begin (); iter != words.end (); ++iter)
	{
	  if (iter != words.begin ())
	    concat.push_back (' ');
	  concat += *iter;
	}

      words.clear ();
      char const *shell = getenv ("SHELL");
      if (!shell || !*shell)
	shell = "zsh";
      words.push_back (shell);
      words.push_back ("-c");
      words.push_back (std::move (concat));
    }
}

bool Command::Execute (int *fds)
{
  pid = fork ();

  if (!pid)
    {
      // Child
      unsigned nargs = words.size ();
      auto args = reinterpret_cast<char const **> (alloca (sizeof (char const *)
							   * (nargs + 1)));
      for (unsigned ix = 0; ix != nargs; ix++)
	args[ix] = words[ix].c_str ();
      args[nargs] = nullptr;

      if ((stdin > 2 && dup2 (stdin, 0) < 0)
	  || (fds && ((fds[0] > 2 && dup2 (fds[0], 1) < 0)
		      || (fds[1] > 2 && dup2 (fds[1], 2) < 0)))
	  || execvp (args[0], const_cast<char **> (args)))
	{
	  int err = errno;
	  // FIXME: Use a pipe to tell our parent explicitly
	  Error () << "cannot execute '" << words[0]
		   << "': " << strerror (err);
	}
      exit (2);
      return false;
    }

  // Parent

  // Close stdin
  if (stdin > 2)
    {
      close (stdin);
      stdin = -1;
    }

  if (pid == pid_t (-1))
    {
      int err = errno;
      Error () << "failed to spawn process '" << words[0]
		   << "': " << strerror (err);
      return false;
    }

  return true;
}

std::ostream &operator<< (std::ostream &s, Command const &cmd)
{
  for (auto witer = cmd.words.begin (); witer != cmd.words.end (); ++witer)
    {
      if (witer != cmd.words.begin ())
	s << ' ';

      std::string_view const word = *witer;
      bool quoted = false;
      constexpr char const *meta (R"('"`()[]\$ 	*?|;&<>)");
      for (auto *ptr = meta; *ptr; ptr++)
	if (word.find (*ptr) != word.npos)
	  {
	    quoted = true;
	    break;
	  }

      if (quoted)
	{
	  s << '\'';
	  for (auto citer = word.begin (); citer != word.end ();)
	    {
	      auto quote = std::find (citer, word.end (), '\'');
	      s << std::string_view (citer, quote);
	      citer = quote;
	      if (citer != word.end ())
		{
		  s << "'\\''";
		  ++citer;
		}
	    }
	  s << '\'';
	}
      else
	s << word;
    }

  return s;
}

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
// EZIO: Expect Zero Irregulities Observed	-*- mode:c++ -*-

// cat | aloy ... -jN subcmd options
// MAKEFLAGS --jobserver-auth=R,W

JOUST Journal Of User Scripted Tests
JOUST Journal Of Utterly Stupid Tests


*/
