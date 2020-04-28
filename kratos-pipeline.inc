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
  bool Execute (Logger &, unsigned const *);

public:
  void Result (Logger &, Logger::Status);

private:
  friend std::ostream &operator<< (std::ostream &s, Pipeline const &pipe);
  friend class Parser;
};

std::ostream &operator<< (std::ostream &s, Pipeline const &pipe)
{
  auto iter = pipe.commands.begin ();

  if (!pipe.srcsink[0].empty ())
    s << iter->File () << ':' << iter->Line ()
      << " in< " << pipe.srcsink[0] << '\n';
  
  s << iter->File () << ':' << iter->Line () << " "
    << Pipeline::kinds[pipe.kind] << ':';
  if (pipe.expected_exit)
    s << pipe.expected_exit;
  s << ("! " + !pipe.expect_inverted);
  s << *iter << '\n';

  unsigned fd = 0;
  static char const *const outs[] = {" out", " err"};
  if (!pipe.srcsink[1].empty ())
    s << iter->File () << ':' << iter->Line ()
      << outs[fd++] << "> " << pipe.srcsink[1] << '\n';

  while (++iter != pipe.commands.end ())
    s << iter->File () << ':' << iter->Line ()
      << outs[fd++] << "| " << *iter << '\n';

  return s;
}

bool Pipeline::Execute (Logger &logger, unsigned const *limits)
{
  Assert (commands.size () >= 1 && commands.size () <= 3);

  std::cerr << *this;

  int fds[2] {1, 2}; // sinks of cmd[0]
  {
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
	// stdout is to a file
	fds[0] = open (srcsink[1].c_str (),
		       O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
		       S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP
		       | S_IROTH | S_IWOTH);
	if (fds[0] < 0)
	  {
	    int err = errno;
	    cmd.Error () << "cannot write '" << srcsink[1] << "': "
			 << strerror (err);
	    fds[0] = 1;
	  }
      }

    // Pipes between commands.  The final command (if any) gets stderr,
    // the one before that (if any) gets stdout
    for (unsigned ix = 1; commands.size () - ix; ix++)
      {
	// pipe ends: 0-read from, 1-write to
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
  }

  int poll_fd = epoll_create1 (EPOLL_CLOEXEC);
  Assert (poll_fd >= 0);
  epoll_event ev;

  // We must buffer the 4 streams from the 2 filters, otherwise the
  // output can be randomly intermixed, and that'll be terribly
  // confusing at best.
  ReadBuffer streams[4];
  unsigned num_streams = 0;
  unsigned subtasks = 0;
  {
    auto iter = commands.begin ();
    if (iter->Execute (fds[0], fds[1], limits))
      subtasks++;
    while (++iter != commands.end ())
      {
	// pipe ends: 0-read from, 1-write to
	int pipe[2][2] = {{-1,-1}, {-1,-1}};
	if (pipe2 (pipe[0], O_CLOEXEC) < 0
	    || pipe2 (pipe[1], O_CLOEXEC) < 0)
	  {
	    int err = errno;
	    iter->Error () << "cannot create pipe: " << strerror (err);
	  }
	if (iter->Execute (pipe[0][1], pipe[1][1]))
	  subtasks++;
	for (unsigned ix = 0; ix != 2; ix++)
	  {
	    close (pipe[ix][1]);
	    streams[num_streams++].Open (pipe[ix][0]);
	    ev.events = EPOLLIN;
	    ev.data.u64 = num_streams;
	    if (epoll_ctl (poll_fd, EPOLL_CTL_ADD, pipe[ix][0], &ev) < 0)
	      Unreachable ();
	  }
      }
  }

  // Signal fd and block sigchild 
  sigset_t sigmask, sigold;
  sigemptyset (&sigmask);
  static constexpr unsigned char sigs[] =
    {SIGHUP, SIGQUIT, SIGPIPE, SIGCHLD, 0 };
  for (unsigned ix = 0; sigs[ix]; ix++)
    sigaddset (&sigmask, sigs[ix]);
  int sig_fd = signalfd (-1, &sigmask, SFD_NONBLOCK | SFD_CLOEXEC);
  Assert (sig_fd >= 0);
  if (sigprocmask (SIG_BLOCK, &sigmask, &sigold) < 0)
    Unreachable ();
  ev.events = EPOLLIN;
  ev.data.u64 = 0;
  if (epoll_ctl (poll_fd, EPOLL_CTL_ADD, sig_fd, &ev) < 0)
    Unreachable ();

  // Wait for completion
  bool signalled = false;
  int exit_code = -1;
  while (subtasks || num_streams)
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
	if (uint32_t cookie = events[ix].data.u32)
	  {
	    unsigned stream = cookie - 1;
	    Assert (stream < 4 && num_streams);
	    if (int done = streams[stream].Read ())
	      {
		if (done >= 0)
		  {
		    static char const *const io[] = {" stdout:", " stderr:"};
		    auto &cmd = commands[1 + stream / 2];
		    cmd.Error ()
		      << "failed reading "
		      << cmd.words.front () << io[stream & 1]
		      << strerror (done);
		    Result (logger, Logger::ERROR);
		  }
		int fd = streams[stream].Close ();

		epoll_ctl (poll_fd, EPOLL_CTL_DEL, fd, nullptr);
		close (fd);
		num_streams--;
	      }
	  }
	else
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
			    if (iter == commands.begin ()
				&& (kind != REQUIRE || !is_sig))
			      {
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
		{
		  // FIXME: Terminate application, release tokens to make
		  Unreachable ();
		}
		break;
		
	      case SIGPIPE:
	      default:
		Unreachable ();
	      }
	  }
    }

  close (poll_fd);
  close (sig_fd);
  sigprocmask (SIG_SETMASK, &sigold, nullptr);

  auto iter = commands.begin ();
  for (unsigned ix = 0; ix != 4; ix++)
    {
      if (!(ix & 1))
	++iter;
      if (size_t size = streams[ix].size ())
	{
	  std::ostream *s = &std::cout;
	  if (ix & 1)
	    {
	      s = &std::cerr;
	      *s << "#ERR " << iter->File () << ':' << iter->Line ()
		 << *iter << '\n';
	    }
	  *s << std::string_view (streams[ix].data (), size);
	}
    }

  bool pass = (exit_code >= 0
	       && signalled == (kind == SIGNAL)
	       && (exit_code == expected_exit) == !expect_inverted);
  if (exit_code >= 0 && kind != REQUIRE)
    {
      if (!pass)
	logger.Log () << commands.front ().words[0] << " exited with "
		      << (signalled ? "signal " : "code ") << exit_code << '\n';
      Result (logger, Logger::PassFail (pass, xfailed));
    }

  return pass;
}

void Pipeline::Result (Logger &logger, Logger::Status status)
{
  auto const &cmd = commands.front ();

  logger.Result (status, cmd.file, cmd.line)
    << kinds[kind] << ' ' << cmd.words.front ();
}
