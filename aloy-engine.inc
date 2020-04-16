// ALOY: Apply List, Observe Yield		-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// Not For Distribution

class Engine
{
  enum class FD
  {
    SIGNAL,	// sig_fd
    INPUT,	// in_fd
    MAKE_IN,	// make_in
    MAKE_OUT,	// make_out
    HWM
  };

private:
  std::deque<Job> jobs;
  std::vector<std::string> command;
  std::string used_tokens;
  std::vector<char> input_buffer;
  Logger logger;
  int in_fd = -1;
  unsigned job_limit = 1;
  int make_in = -1, make_out = -1;
  int sig_fd = -1;
  int poll_fd = -1;

private:
  unsigned retired = 0; // Jobs written out
  unsigned completed = 0;// Jobs completed, but waiting to write
  unsigned running = 0;	// Jobs running
  unsigned pending = 0;	// Jobs waiting to run

public:
  Engine (unsigned argc, char const *const argv[], unsigned limit,
	  int in, std::ostream &sum, std::ostream &log);
  ~Engine ();

public:
  bool IsLive () const
  {
    return in_fd >= 0 || !jobs.empty ();
  }
  void Init ();
  void Fini (std::ostream * = nullptr);
  bool Process (int timeout);
  void Retire ();
  void Fini (Job const &);
  void Spawn ();
  std::string Progress ();

private:
  void ReadInput ();
  void ReadSignal ();
  void StopMake ();
  void ReadMake ();
  void WriteMake ();

  void Init (Job *);
  void Fini (Job *);

  friend std::ostream &operator<< (std::ostream &, Engine const &);
};

Engine::Engine (unsigned argc, char const *const argv[], unsigned limit,
		int in, std::ostream &s, std::ostream &l)
  : logger (s, l), in_fd (in), job_limit (limit)
{
  command.reserve (argc);
  for (unsigned ix = 0; ix != argc; ix++)
    command.emplace_back (argv[ix]);

  // Get jobserver FDs
  if (job_limit)
    ;
  else if (char const *makeflags = getenv ("MAKEFLAGS"))
    {
      // --jobserver-auth=RN,WN
      std::string_view mflags (makeflags);
      constexpr std::string_view jsa = "--jobserver-auth=";
      bool first = true;

      while (!mflags.empty ())
	{
	  auto not_space = mflags.find_first_not_of (' ');
	  if (not_space != mflags.npos)
	    mflags.remove_prefix (not_space);
	  if (mflags.starts_with (jsa))
	    {
	      mflags.remove_prefix (jsa.size ());
	      Lexer lexer (mflags);
	      struct stat stat_buf;

	      if (!lexer.Identifier ()
		  || lexer.Peek () != ','
		  || !lexer.Identifier ()
		  || (lexer.Peek () && lexer.Peek () != ' ')
		  || (make_in = lexer.GetToken ()->GetInteger (),
		      make_out = lexer.GetToken ()->GetInteger (),
		      fstat (make_in, &stat_buf) < 0
		      || (stat_buf.st_mode & S_IFMT) != S_IFIFO
		      || fstat (make_out, &stat_buf) < 0
		      || (stat_buf.st_mode & S_IFMT) != S_IFIFO))
		{
		  /* Not available after all.  */
		  std::cerr << "MAKEFLAGS" << '=' << makeflags
			    << (" specifies unusable jobserver"
				" (prefix command with '+'?)\n");
		  make_in = make_out = -1;
		}
	      break;
	    }
	  else
	    {
	      char c;
	      do
		{
		  c = mflags[0];
		  mflags.remove_prefix (1);
		  if (first && c == 'n')
		    // Non-execution
		    command.clear ();
		}
	      while (c != ' ' && !mflags.empty ());
	      first = false;
	    }
	}

      // We can always spawn one job without asking the jobserver
      job_limit = 1;
    }
  else
    job_limit = 1;
}

Engine::~Engine ()
{
  close (in_fd);
}

std::ostream &operator<< (std::ostream &s, Engine const &self)
{
  for (auto iter = self.command.begin (); iter != self.command.end (); ++iter)
    {
      if (iter != self.command.begin ())
	s << ' ';
      s << *iter;
    }
  return s;
}

void Engine::Init ()
{
  // Signal fd and block sigchild 
  sigset_t sigmask;
  sigemptyset (&sigmask);
  static constexpr unsigned char sigs[] =
    {SIGHUP, SIGQUIT, SIGPIPE, SIGCHLD, 0 };
  for (unsigned ix = 0; sigs[ix]; ix++)
    sigaddset (&sigmask, sigs[ix]);
  sig_fd = signalfd (-1, &sigmask, SFD_NONBLOCK | SFD_CLOEXEC);
  Assert (sig_fd >= 0);
  if (sigprocmask (SIG_BLOCK, &sigmask, nullptr) < 0)
    Unreachable ();

  // Epoll fd
  poll_fd = epoll_create1 (EPOLL_CLOEXEC);
  Assert (poll_fd >= 0);

  epoll_event ev;
  ev.events = EPOLLIN;
  ev.data.u64 = unsigned (FD::SIGNAL);
  if (epoll_ctl (poll_fd, EPOLL_CTL_ADD, sig_fd, &ev) < 0)
    Unreachable ();
  ev.data.u64 = unsigned (FD::INPUT);
  if (epoll_ctl (poll_fd, EPOLL_CTL_ADD, in_fd, &ev) < 0)
    Unreachable ();

  auto now = time (nullptr);
  logger << "Test run: " << ctime (&now);
  logger << "\n";
}

void Engine::Fini (std::ostream *summary)
{
  if (summary)
    *summary << "# Summary of " << retired << " tests \n" << logger;
  logger.Sum () << '\n';
  logger << "# Summary of " << retired << " tests \n" << logger;

  logger.Sum ().flush ();
  logger.Log ().flush ();

  close (poll_fd);
  poll_fd = -1;
  close (sig_fd);
  sig_fd = -1;
}

void Engine::ReadInput ()
{
  Assert (in_fd >= 0);

  // Read
  size_t size = input_buffer.size ();
  size_t hwm = input_buffer.capacity ();
  if (hwm - size < 10)
    hwm += 1024;
  input_buffer.resize (hwm);
  ssize_t count = read (in_fd, &input_buffer[size], hwm - size);
  input_buffer.resize (size + (count >= 0 ? count : 0));
  if (count < 0 && errno != EINTR)
    {
      // FIXME: Log ERROR
      count = 0;
    }

  if (!count)
    {
      epoll_ctl (poll_fd, EPOLL_CTL_DEL, in_fd, nullptr);
      close (in_fd);
      in_fd = -1;
    }

  // Lex
  unsigned added = 0;
  auto first = input_buffer.begin ();
  auto last = input_buffer.end ();
  while (first != last)
    {
      auto eol = std::find (first, last, '\n');
      if (eol == last && in_fd >= 0)
	break;

      std::string_view line (first, eol);
      first = eol + (in_fd >= 0);

      bool sol = true;
      for (size_t pos = 0;; sol = false)
	{
	  pos = line.find_first_not_of (' ', pos);
	  if (pos == line.npos)
	    break;
	  if (sol && line[pos] == '#')
	    break;

	  size_t end = line.find_first_of (' ', pos);
	  if (end == line.npos)
	    end = line.size ();

	  jobs.emplace_back (line.substr (pos, end - pos));
	  pending++;
	  pos = end;
	}
    }

  // Adjust
  if (first != input_buffer.begin ())
    input_buffer.erase (input_buffer.begin (), first);
}

void Engine::ReadSignal ()
{
  signalfd_siginfo sig;

  ssize_t bytes = read (sig_fd, &sig, sizeof (sig));
  Assert (bytes == sizeof (sig));

  switch (sig.ssi_signo)
    {
    case SIGCHLD:
      // Get child status.  There could be multiple children to deal with.
      {
	int status;
	while (pid_t child = waitpid (-1, &status, WNOHANG))
	  {
	    if (child == pid_t (-1))
	      {
		Assert (!running);
		break;
	      }

	    if (WIFEXITED (status) || WIFSIGNALED (status))
	      {
		auto iter = jobs.begin ();
		for (unsigned limit = completed + running; limit--; ++iter)
		  if (iter->IsPid (child))
		    {
		      int token = iter->Reap (status);
		      if (token != -1)
			{
			  // Write the token back to jobcontrol to
			  // allow other processes a chance.
			  Assert (make_out >= 0);
			  if (used_tokens.empty ())
			    {
			      epoll_event ev;
			      ev.events = EPOLLOUT;
			      ev.data.u64 = unsigned (FD::MAKE_OUT);
			      if (epoll_ctl (poll_fd, EPOLL_CTL_ADD,
					     make_out, &ev) < 0)
				Unreachable ();
			    }
			  used_tokens.push_back (char (token));
			}
		      completed++;
		      running--;
		      goto found;
		    }
	      }
	    Unreachable ();
	  found:
	    if (!running)
	      break;
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

void Engine::StopMake ()
{
  Assert (make_out >= 0 && make_in >= 0);
  epoll_ctl (poll_fd, EPOLL_CTL_DEL, make_out, nullptr);
  epoll_ctl (poll_fd, EPOLL_CTL_DEL, make_in, nullptr);
  make_out = make_in = -1;
  used_tokens.clear ();
  // FIXME: Log error
}

void Engine::ReadMake ()
{
}

// Able to write something to the make pipe.  do it.
void Engine::WriteMake ()
{
  size_t tokens = used_tokens.size ();
  ssize_t wrote = write (make_out, used_tokens.data (), used_tokens.size ());
  if (wrote < 0)
    StopMake ();
  else if (size_t (wrote) == used_tokens.size ())
    {
      used_tokens.clear ();
      if (epoll_ctl (poll_fd, EPOLL_CTL_DEL, make_out, nullptr) < 0)
	Unreachable ();
    }
  else
    used_tokens.erase (0, wrote);
}

bool Engine::Process (int timeout)
{
  constexpr int max_events = 20;
  int count;

  do
    {
      epoll_event events[max_events];
      count = epoll_wait (poll_fd, events, max_events, timeout);
      if (count < 0)
	{
	  count = 0;
	  Assert (errno == EINTR);
	}

      for (int ix = 0; ix != count; ix++)
	switch (uint64_t cookie = events[ix].data.u64)
	  {
	  case unsigned (FD::INPUT):
	    ReadInput ();
	    break;

	  case unsigned (FD::SIGNAL):
	    ReadSignal ();
	    break;

	  case unsigned (FD::MAKE_IN):
	    ReadMake ();
	    break;

	  case unsigned (FD::MAKE_OUT):
	    WriteMake ();
	    break;

	  default:
	    reinterpret_cast<Job *> (cookie ^ (cookie & 7))
	      ->Read (cookie & 7, poll_fd);
	    break;
	  }
    }
  while (count == max_events);

  return !count;
}

void Engine::Fini (Job const &job)
{
  logger.Log () << "# Test:" << retired << " " << job << '\n';
  logger.Log () << *this << ' ' << job << '\n';
  auto &log = job.Text (1);
  logger.Log () << std::string_view (log.data (), log.size ());

  unsigned bad_count = 0;
  std::string_view bad_line;
  auto &sum = job.Text (0);
  for (auto sol = sum.data (), end = sol + sum.size (); sol != end;)
    {
      auto eol = std::find (sol, end, '\n');
      auto line = std::string_view (sol, eol);

      if (!logger.AddStatus (line))
	{
	  bad_count++;
	  if (!bad_line.size ())
	    bad_line = line;
	}
      logger.Sum () << line << '\n';
      sol = eol + (eol != end);
    }
  if (bad_count)
    logger.Result (Logger::ERROR) << job << ": unexpected summary line '"
				  << bad_line << '\'';

  int exit = job.Exit ();
  if (WIFSIGNALED (exit))
    {
      int sig = WTERMSIG (exit);
      logger.Result (Logger::ERROR) << job << " terminated with signal " << sig;
    }
  else if (WIFEXITED (exit))
    {
      if (int code = WEXITSTATUS (exit))
	logger.Result (Logger::ERROR) << job << " exited with status " << code;
    }
  else
    Unreachable ();

  logger.Log () << '\n';
}

void Engine::Retire ()
{
  while (completed && jobs.front ().Ready ())
    {
      Fini (jobs.front ());
      jobs.pop_front ();

      completed--;
      retired++;
    }
}

void Engine::Spawn ()
{
  if (pending)
    for (auto iter = jobs.begin () + (completed + running); pending; ++iter)
      {
	if (job_limit > running)
	  {
	    if (iter->Spawn (command))
	      running++;
	    else
	      completed++;
	    if (auto *fds = iter->Fds ())
	      {
		uint64_t data = reinterpret_cast<uint64_t> (&*iter);

		Assert (!(data & 7));
		for (unsigned ix = 2; ix--;)
		  {
		    epoll_event ev;
		    ev.events = EPOLLIN;
		    ev.data.u64 = data | ix;
		    if (epoll_ctl (poll_fd, EPOLL_CTL_ADD, fds[ix], &ev) < 0)
		      Unreachable ();
		  }
	      }
	    pending--;
	  }
	else
	  break;
	// FIXME jobserver token?
      }
}

std::string Engine::Progress ()
{
  unsigned done = retired + completed;
  unsigned total = done + running + pending;
  std::ostringstream progress;
	  
  progress << "[" << retired;
  if (completed)
    progress << "(&" << completed << ')';
  if (running)
    progress << '+' << running;
  progress << '/' << total
	   << "] " << done * 100 / (total + !total) << '%';
  if (!jobs.empty ())
    progress << ' ' << jobs.front ();
  else if (in_fd >= 0)
    progress << " ...";
  return progress.str ();
}
