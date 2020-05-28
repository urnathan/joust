// Joust/KRATOS: Kapture Run And Test Output Safely	-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

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
  std::string src;
  Kind kind = RUN;
  unsigned expected_exit : 8 = 0;
  bool expect_inverted : 1 = false;
  bool xfailed : 1 = false;
  bool stopped : 1 = false;
  bool here_doc : 1 = false;

public:
  Pipeline (Kind k, bool inverted, unsigned exit = 0)
    : kind (k), expected_exit (exit), expect_inverted (inverted)
  {
    commands.emplace_back ();
  }

public:
  Kind GetKind () const
  {
    return kind;
  }

public:
  void SetIn (std::string &&stdin, bool here)
  {
    src = stdin;
    here_doc = here;
  }

public:
  int Execute (Logger &, unsigned const *);

public:
  void Result (Logger &, Logger::Status);

private:
  friend std::ostream &operator<< (std::ostream &s, Pipeline const &pipe);
  friend class Parser;
};

std::ostream &operator<< (std::ostream &s, Pipeline const &pipe)
{
  {
    auto &cmd = pipe.commands.front ();

    if (pipe.here_doc)
      {
	for (size_t base = 0, eol; base != pipe.src.size (); base = eol + 1)
	  {
	    eol = pipe.src.find_first_of ('\n', base);
	    Assert (eol != pipe.src.npos);
	    s << cmd.File () << ':' << cmd.Line ()
	      << " in<<" << std::string_view (&pipe.src[base], eol - base)
	      << '\n';
	  }
      }
    else if (!pipe.src.empty ())
      s << cmd.File () << ':' << cmd.Line ()
	<< " in< " << pipe.src << '\n';
  
    s << cmd.File () << ':' << cmd.Line () << " "
      << Pipeline::kinds[pipe.kind] << ':';
    if (pipe.expected_exit)
      s << pipe.expected_exit;
    if (pipe.expect_inverted)
      s << '!';
    s << cmd << '\n';
  }

  static char const *const outs[] = {" out", " err"};
  for (unsigned ix = 1; ix != pipe.commands.size (); ix++)
    {
      auto &filter = pipe.commands[ix];
      if (!filter.Empty ())
	{
	  s << filter.File () << ':' << filter.Line () << outs[ix - 1];
	  s << (filter.RedirectType () == Command::R_FILE ? '>' : '|');
	  if (ix == 2)
	    s << '&';
	  s << filter << '\n';
	}
    }

  return s;
}

int Pipeline::Execute (Logger &logger, unsigned const *limits)
{
  Assert (commands.size () == 1 || commands.size () == 3);

  std::cerr << *this;

  int here_fd = -1;
  {
    auto &cmd = commands.front ();

    if (cmd.words.empty ())
      {
	cmd.Error () << "no command to execute";
	return EINVAL;
      }

    if (here_doc)
      {
	int pipe[2];

	if (pipe2 (pipe, O_CLOEXEC) < 0)
	  {
	    int err = errno;
	    cmd.Error () << "cannot create pipe: " << strerror (err);
	  }
	else
	  {
	    here_fd = pipe[1];
	    cmd.stdin = pipe[0];
	  }
      }
    else
      {
	char const *in = src.c_str ();
	if (!*in)
	  in = "/dev/null";
	int fd = open (in, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
	  {
	    int err = errno;
	    cmd.Error () << "cannot read '" << in << "': " << strerror (err);
	  }
	else
	  cmd.stdin = fd;
      }
  }

  // Determine sinks of cmd[0]
  int fds[2] {1, 2};
  for (unsigned ix = 1; ix != commands.size (); ix++)
    {
      auto &filt = commands[ix];

      if (filt.RedirectType () == Command::R_FILE)
	{
	  // To a file
	  Assert (filt.words.size () == 1);

	  char const *out = filt.words.front ().c_str ();
	  int fd = open (out, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
			 S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP
			 | S_IROTH | S_IWOTH);
	  if (fd < 0)
	    {
	      int err = errno;
	      filt.Error () << "cannot write '" << out
			    << "': " << strerror (err);
	    }
	  else
	    fds[ix - 1] = fd;
	}
      else
	{
	  // pipe ends: 0-read from, 1-write to
	  int pipe[2];
	  if (pipe2 (pipe, O_CLOEXEC) < 0)
	    {
	      int err = errno;
	      filt.Error () << "cannot create pipe: " << strerror (err);
	      }
	  else
	    {
	      fds[ix - 1] = pipe[1];
	      filt.stdin = pipe[0];
	    }
	}
    }

  int poll_fd = epoll_create1 (EPOLL_CLOEXEC);
  Assert (poll_fd >= 0);
  epoll_event ev;

  // Signal fd and block sigchild 
  sigset_t sigmask, sigold;
  sigemptyset (&sigmask);
  static constexpr unsigned char sigs[] =
    {SIGHUP, SIGQUIT, SIGPIPE, SIGCHLD, SIGALRM, SIGTERM, 0 };
  for (unsigned ix = 0; sigs[ix]; ix++)
    sigaddset (&sigmask, sigs[ix]);
  int sig_fd = signalfd (-1, &sigmask, SFD_NONBLOCK | SFD_CLOEXEC);
  Assert (sig_fd >= 0);
  while (sigprocmask (SIG_BLOCK, &sigmask, &sigold) < 0)
    Assert (errno == EINTR);
  ev.events = EPOLLIN;
  ev.data.u64 = 0;
  while (epoll_ctl (poll_fd, EPOLL_CTL_ADD, sig_fd, &ev) < 0)
    Assert (errno == EINTR);
  if (here_fd >= 0)
    {
      ev.events = EPOLLOUT;
      ev.data.u64 = 1;
      while (epoll_ctl (poll_fd, EPOLL_CTL_ADD, here_fd, &ev) < 0)
	Assert (errno == EINTR);
    }

  // We must buffer the (up to) 4 streams from the 2 filters,
  // otherwise the output can be randomly intermixed, and that'll be
  // terribly confusing at best.
  ReadBuffer streams[4];
  unsigned num_streams = 0;
  unsigned subtasks = 0;
  {
    if (commands.front ().Execute (fds[0], fds[1], limits))
      subtasks++;

    for (unsigned ix = 1; ix != commands.size (); ix++)
      {
	auto &filt = commands[ix];

	// pipe ends: 0-read from, 1-write to
	int pipe[2][2] = {{-1,-1}, {-1,-1}};

	if (filt.stdin < 0)
	  continue;

	if (filt.Empty ())
	  // Expect no output
	  pipe[0][0] = filt.stdin;
	else
	  {
	    if (pipe2 (pipe[0], O_CLOEXEC) < 0
		|| pipe2 (pipe[1], O_CLOEXEC) < 0)
	      {
		int err = errno;
		filt.Error () << "cannot create pipe: " << strerror (err);
		if (pipe[0][0] >= 0)
		  {
		    close (pipe[0][0]);
		    close (pipe[0][1]);
		  }
		pipe[0][0] = pipe[0][1] = -1;
	      }
	    if (filt.Execute (pipe[0][1], pipe[1][1]))
	      subtasks++;
	  }

	for (unsigned io = 0; io != 2; io++)
	  if (pipe[io][0] >= 0)
	    {
	      unsigned strix = ix * 2 - 2 + io;
	      streams[strix].Open (pipe[io][0]);
	      num_streams++;
	      ev.events = EPOLLIN;
	      ev.data.u64 = strix | 4;
	      if (epoll_ctl (poll_fd, EPOLL_CTL_ADD, pipe[io][0], &ev) < 0)
		Unreachable ();
	    }
      }
  }

  // Wait for completion
  itimerval timeout = {{0, 0}, {0, 0}};
  if (limits && limits[PL_HWM])
    {
      timeout.it_value.tv_sec = limits[PL_HWM];
      setitimer (ITIMER_REAL, &timeout, nullptr);
      timeout.it_value.tv_sec = 0;
    }

  static char const *const io_streams[] = {" stdout:", " stderr:"};
  size_t here_pos = 0;
  bool signalled = false;
  int exit_code = -1;
  while (subtasks || num_streams || here_fd >= 0)
    {
      constexpr int max_events = 20;
      epoll_event events[max_events];

      int count = epoll_wait (poll_fd, events, max_events, -1);
      if (count < 0)
	{
	  count = 0;
	  Assert (errno == EINTR);
	}

      for (int ix = 0; ix != count; ix++)
	switch (unsigned cookie = unsigned (events[ix].data.u32))
	  {
	  default:
	    {
	      unsigned stream = cookie & 3;
	      Assert (stream < 4 && num_streams);
	      if (int done = streams[stream].Read ())
		{
		  if (done >= 0)
		    {
		      auto *cmd = &commands[1 + stream / 2];
		      unsigned iostr = stream & 1;
		      if (cmd->words.empty ())
			{
			  cmd = &commands[0];
			  iostr = stream >> 1;
			}

		      cmd->Error () << "failed reading "
				    << cmd->words.front () << io_streams[iostr]
				    << strerror (done);
		      Result (logger, Logger::ERROR);
		    }
		  int fd = streams[stream].Close ();

		  epoll_ctl (poll_fd, EPOLL_CTL_DEL, fd, nullptr);
		  close (fd);
		  num_streams--;
		}
	    }
	    break;

	  case 1:
	    {
	      // Here doc
	      size_t limit = src.size () - here_pos;
	      ssize_t wrote = write (here_fd, src.data () + here_pos, limit);
	      if (wrote < 0)
		{
		  int err = errno;
		  if (err != EINTR && err != EAGAIN)
		    {
		      commands[0].Error () << "failed writing here doc:"
					   << commands[0].words.front ()
					   << strerror (err);
		      Result (logger, Logger::ERROR);
		      here_pos = src.size ();
		    }
		}
	      else
		here_pos += wrote;
	      if (here_pos == src.size ())
		{
		  epoll_ctl (poll_fd, EPOLL_CTL_DEL, here_fd, nullptr);
		  close (here_fd);
		  here_fd = -1;
		}
	    }
	    break;

	  case 0:
	    {
	      // Signal
	      signalfd_siginfo sig;
	      ssize_t bytes = read (sig_fd, &sig, sizeof (sig));
	      Assert (bytes == sizeof (sig));

	      switch (sig.ssi_signo)
		{
		case SIGCHLD:
		  // Get child status.  There could be multiple children
		  // to deal with.
		  {
		    int status;
		    while (pid_t child = waitpid (-1, &status, WNOHANG))
		      {
			if (child == pid_t (-1))
			  {
			    Assert (!subtasks);
			    break;
			  }

			for (auto iter = commands.begin ();
			     iter != commands.end (); ++iter)
			  if (child == iter->pid)
			    {
			      bool is_sig = false;
			      int ex = 0;
			      if (WIFEXITED (status))
				ex = WEXITSTATUS (status);
			      else if (WIFSIGNALED (status))
				{
				  ex = WTERMSIG (status);
				  is_sig = true;
				}
			      else
				goto found;

			      iter->pid = -1;
			      subtasks--;
			      if (iter == commands.begin ())
				{
				  if (limits)
				    setitimer (ITIMER_REAL, &timeout, nullptr);
				  signalled = is_sig;
				  exit_code = ex;
				}
			      else if (is_sig || ex)
				{
				  iter->Error ()
				    << '\'' << iter->words.front ()
				    << "' exited with "
				    << (is_sig ? "signal " : "code ") << ex;
				  Result (logger, Logger::ERROR);
				}
			      goto found;
			    }
			Unreachable ();
		      found:;
		      }
		  }
		  break;
		
		case SIGHUP:
		case SIGINT:
		case SIGQUIT:
		case SIGTERM:
		  stopped = true;
		  commands.front ().Stop (sig.ssi_signo);
		  break;

		case SIGALRM:
		  commands.front ().Stop (SIGTERM);
		  commands.front ().Error ()
		    << "TIMEOUT after " << limits[PL_HWM] << " seconds";
		  Result (logger, Logger::ERROR);
		  break;

		case SIGPIPE:
		default:
		  Unreachable ();
		}
	    }
	    break;
	  }
    }

  close (poll_fd);
  close (sig_fd);
  sigprocmask (SIG_SETMASK, &sigold, nullptr);

  for (unsigned ix = 1; ix != commands.size (); ix++)
    {
      auto &filt = commands[ix];
      if (filt.stdin < 0)
	continue;

      for (unsigned io = 0; io != 2; io++)
	{
	  auto &stream = streams[ix * 2 - 2 + io];
	  if (size_t size = stream.size ())
	    {
	      std::ostream *s = &std::cout;
	      if (filt.Empty ())
		{
		  s = &std::cerr;
		  auto &cmd = commands[0];
		  logger.Result (Logger::ERROR, cmd.File (), cmd.Line ())
		    << "# Unexpected" << io_streams[ix - 1]
		    << cmd;
		}
	      else if (io)
		{
		  s = &std::cerr;
		  *s << "# Checker " << filt.File () << ':' << filt.Line ()
		     << filt << '\n';
		}
	      *s << std::string_view (stream.data (), size);
	    }
	}
    }

  Assert (exit_code >= 0);

  bool pass = (signalled == (kind == SIGNAL)
	       && (exit_code == expected_exit) == !expect_inverted);
  if (!pass && (kind != REQUIRE || signalled))
    logger.Log () << commands.front ().words[0] << " exited with "
		  << (signalled ? "signal " : "code ") << exit_code << '\n';
  if (kind == REQUIRE)
    Result (logger, pass ? Logger::PASS : Logger::UNSUPPORTED);
  else
    Result (logger, Logger::PassFail (pass, xfailed));

  if (stopped)
    {
      commands.front ().Error () << " terminated via signal";
      return EINTR;
    }

  return pass ? 0 : EINVAL;
}

void Pipeline::Result (Logger &logger, Logger::Status status)
{
  auto const &cmd = commands.front ();

  logger.Result (status, cmd.file, cmd.line)
    << kinds[kind] << ' ' << cmd.words.front ();
}
