// Joust/KRATOS: Kapture Run And Test Output Safely	-*- mode:c++ -*-
// Copyright (C) 2020-2024 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

class Pipeline
{
public:
#define PIPELINE_KINDS RUN, SIGNAL, REQUIRE, END
  enum Kind : unsigned char
  {
    NMS_LIST (NMS_IDENT, PIPELINE_KINDS),
    PIPELINE_HWM,
    PIPE_HWM = REQUIRE
  };

public:
  static char const *const kinds[PIPELINE_HWM];

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
  { return kind; }

public:
  void SetIn (std::string &&stdin, bool here)
  {
    src = stdin;
    here_doc = here;
  }

public:
  int Execute (Tester &, unsigned const *);

public:
  void Result (Tester &, Tester::Status);

private:
  friend std::ostream &operator<< (std::ostream &s, Pipeline const &pipe);
  friend class Parser;
};

constinit char const *const Pipeline::kinds[PIPELINE_HWM]
= {NMS_LIST (NMS_STRING, PIPELINE_KINDS)};

static constinit unsigned char const sigs[]
  = {SIGHUP, SIGQUIT, SIGPIPE, SIGCHLD, SIGALRM, SIGTERM};

std::ostream &
operator<< (std::ostream &s, Pipeline const &pipe)
{
  {
    auto &cmd = pipe.commands.front ();

    if (pipe.here_doc)
      {
	for (size_t base = 0, eol; base != pipe.src.size (); base = eol + 1)
	  {
	    eol = pipe.src.find_first_of ('\n', base);
	    Assert (eol != pipe.src.npos);
	    s << cmd.Loc ().File () << ':' << cmd.Loc ().Line ()
	      << " in<<" << std::string_view (&pipe.src[base], eol - base)
	      << '\n';
	  }
      }
    else if (!pipe.src.empty ())
      s << cmd.Loc ().File () << ':' << cmd.Loc ().Line ()
	<< " in< " << pipe.src << '\n';
  
    s << cmd.Loc ().File () << ':' << cmd.Loc ().Line () << " "
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
	  s << filter.Loc ().File () << ':'
	    << filter.Loc ().Line () << outs[ix - 1];
	  s << (filter.RedirectType () == Command::R_FILE ? '>' : '|');
	  if (ix == 2)
	    s << '&';
	  s << filter << '\n';
	}
    }

  return s;
}

#ifndef USE_EPOLL
// Signals we saw
volatile unsigned sigs_seen;

void
NoteSignal (int signal)
{
  for (unsigned ix = sizeof (sigs); ix--;)
    if (signal == sigs[ix])
      {
	sigs_seen = sigs_seen | (1 << ix);
	break;
      }
}
#endif

// This mucks about with signals, so expects to be single threaded

int
Pipeline::Execute (Tester &logger, unsigned const *limits)
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

	if (MakePipe (pipe) < 0)
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
	  if (MakePipe (pipe) < 0)
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

  // Signals to block
  sigset_t sigmask, sigorig;
  sigemptyset (&sigmask);
  for (unsigned ix = sizeof (sigs); ix--;)
    sigaddset (&sigmask, sigs[ix]);
#ifdef USE_EPOLL
  while (sigprocmask (SIG_BLOCK, &sigmask, &sigorig) < 0)
    Assert (errno == EINTR);

  int poll_fd = epoll_create1 (EPOLL_CLOEXEC);
  int sig_fd = signalfd (-1, &sigmask, SFD_NONBLOCK | SFD_CLOEXEC);
  Assert (poll_fd >= 0 && sig_fd >= 0);

  {
    epoll_event ev;
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
  }
#else
  while (sigprocmask (SIG_UNBLOCK, &sigmask, &sigorig) < 0)
    Assert (errno == EINTR);
  sigset_t sigselect;
  while (sigprocmask (SIG_BLOCK, &sigmask, &sigselect) < 0)
    Assert (errno == EINTR);

  // Install signal handlers
  struct sigaction old_actions[sizeof (sigs)];
  for (unsigned ix = sizeof (sigs); ix--;)
    {
      struct sigaction action;
      action.sa_handler = NoteSignal;
      sigfillset (&action.sa_mask);
      action.sa_flags = 0;
      sigaction (sigs[ix], &action, &old_actions[ix]);
    }
#endif

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
	    if (MakePipe (pipe[0]) < 0
		|| MakePipe (pipe[1]) < 0)
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
	      unsigned strix = (ix - 1) * 2 + io;
	      int pipe_fd = pipe[io][0];

	      streams[strix].Open (pipe_fd);
	      num_streams++;
#ifdef USE_EPOLL
	      epoll_event ev;
	      ev.events = EPOLLIN;
	      ev.data.u64 = strix | 4;
	      if (epoll_ctl (poll_fd, EPOLL_CTL_ADD, pipe_fd, &ev) < 0)
		Unreachable ();
#endif
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
      unsigned seen_sig = 0;
      int count;
#ifdef USE_EPOLL
      constexpr int max_events = 20;
      epoll_event events[max_events];

      count = epoll_wait (poll_fd, events, max_events, -1);
#else
      fd_set rd_set;
      fd_set wr_set;
      int hwm = 0;

      FD_ZERO (&rd_set);
      FD_ZERO (&wr_set);

      if (here_fd >= 0)
	{
	  FD_SET (here_fd, &wr_set);
	  hwm = here_fd + 1;
	}

      for (unsigned ix = 4; ix--;)
	if (streams[ix].IsOpen ())
	  {
	    int fd = streams[ix].GetFD ();
	    FD_SET (fd, &rd_set);
	    if (fd >= hwm)
	      hwm = fd + 1;
	  }
      count = pselect (hwm, &rd_set, &wr_set, nullptr, nullptr, &sigselect);
      seen_sig = sigs_seen;
      sigs_seen = 0;
#endif
      if (count < 0)
	{
	  count = 0;
	  Assert (errno == EINTR);
	}

      for (int ix = 0; ix != count; ix++)
	{
	  unsigned cookie;

#ifdef USE_EPOLL
	  cookie = unsigned (events[ix].data.u32);
#else
	  while (hwm--)
	    {
	      if (FD_ISSET (hwm, &wr_set))
		{
		  cookie = 1; // here_doc
		  break;
		}
	      else if (FD_ISSET (hwm, &rd_set))
		{
		  for (unsigned stream = 4; stream--;)
		    if (streams[stream].IsOpen ()
			&& streams[stream].GetFD () == hwm)
		      {
			cookie = stream | 4;
			goto found_fd;
		      }
		  Unreachable ();
		}
	    }
	found_fd:;
	  Assert (hwm >= 0);
#endif
	  switch (cookie)
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
			Result (logger, Tester::ERROR);
		      }
		    int fd = streams[stream].Close ();
#ifdef USE_EPOLL
		    epoll_ctl (poll_fd, EPOLL_CTL_DEL, fd, nullptr);
#endif
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
			commands[0].Error ()
			  << "failed writing here doc at byte "
			  << here_pos << " (" << limit
			  << " remain): " << strerror (err);
			Result (logger, Tester::ERROR);
			here_pos = src.size ();
		      }
		  }
		else
		  here_pos += wrote;
		if (here_pos == src.size ())
		  {
#ifdef USE_EPOLL
		    epoll_ctl (poll_fd, EPOLL_CTL_DEL, here_fd, nullptr);
#endif
		    close (here_fd);
		    here_fd = -1;
		  }
	      }
	      break;

	    case 0:
	      {
		// Signal
#ifdef USE_EPOLL
		signalfd_siginfo sig;
		ssize_t bytes = read (sig_fd, &sig, sizeof (sig));
		Assert (bytes == sizeof (sig));
		seen_sig = sig.ssi_signo;
#endif
		break;
	      }
	      break;
	    }
	}

      if (seen_sig)
	{
	  int signo;
#ifdef USE_EPOLL
	  signo = seen_sig;
#else
	  unsigned ix = sizeof (sigs);

	another_signal:
	  while (!(seen_sig & (1 << --ix)))
	    continue;
	  seen_sig ^= 1 << ix;
	  signo = sigs[ix];
#endif

	  switch (signo)
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
			    goto found_pid;

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
			      Result (logger, Tester::ERROR);
			    }
			  goto found_pid;
			}
		    Unreachable ();
		  found_pid:;
		  }
	      }
	      break;

	    case SIGHUP:
	    case SIGINT:
	    case SIGQUIT:
	    case SIGTERM:
	      stopped = true;
	      commands.front ().Stop (signo);
	      break;

	    case SIGALRM:
	      commands.front ().Stop (SIGTERM);
	      commands.front ().Error ()
		<< "TIMEOUT after " << limits[PL_HWM] << " seconds";
	      Result (logger, Tester::ERROR);
	      break;

	    case SIGPIPE:
	      break;

	    default:
	      Unreachable ();
	    }
#ifndef USE_EPOLL
	  if (seen_sig)
	    goto another_signal;
#endif
	}
    }

#ifdef USE_EPOLL
  close (poll_fd);
  close (sig_fd);
#else
  // Restore signal handlers
  for (unsigned ix = sizeof (sigs); ix--;)
    sigaction (sigs[ix], &old_actions[ix], nullptr);
#endif
  while (sigprocmask (SIG_SETMASK, &sigorig, nullptr) < 0)
    Assert (errno == EINTR);

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
		  logger.Result (Tester::ERROR, cmd.Loc ())
		    << "# Unexpected" << io_streams[ix - 1]
		    << cmd;
		}
	      else if (io)
		{
		  s = &std::cerr;
		  *s << "# Checker " << filt.Loc ().File ()
		     << ':' << filt.Loc ().Line ()
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
    Result (logger, pass ? Tester::PASS : Tester::UNSUPPORTED);
  else
    Result (logger, Tester::PassFail (pass, xfailed));

  if (stopped)
    {
      commands.front ().Error () << " terminated via signal";
      return EINTR;
    }

  return pass ? 0 : EINVAL;
}

void
Pipeline::Result (Tester &logger, Tester::Status status)
{
  auto const &cmd = commands.front ();
  auto l = logger.Result (status, cmd.Loc ());

  l << kinds[kind];
  if (expect_inverted || expected_exit)
    {
      l << ' ';
      if (expect_inverted)
	l << '!';
      if (expected_exit)
	l << int (expected_exit);
    }
  l << ' ' << cmd.words.front ();
}
