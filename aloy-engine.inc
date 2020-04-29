// ALOY: Apply List, Observe Yield		-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// Not For Distribution

class Engine : public Logger
{
public:
  using Parent = Logger;

private:
  enum class FD
  {
    SIGNAL,	// sig_fd
    GEN_OUT,	// gen_out
    GEN_ERR,	// gen_err
    MAKE_IN,	// make_in
    MAKE_OUT,	// make_out
    HWM
  };

private:
  std::deque<Job> jobs;
  std::vector<std::string> command;
  std::string generator;
  std::string used_tokens; // tokens to send back to make
  std::string ready_tokens; // tokens we've got from make
  ReadBuffer gen_buffers[2];
  pid_t gen_pid = 0;
  unsigned gen_state = 0;
  unsigned gen_exit = 0;
  unsigned job_limit = 1;  // static number of jobs we can spawn
  unsigned fixed_jobs = 0; // non-jobserver jobs running
  unsigned make_want = 0;
  int make_in = -1, make_out = -1;
  int sig_fd = -1;
  int poll_fd = -1;

private:
  unsigned retired = 0; // Jobs written out
  unsigned completed = 0;// Jobs completed, but waiting to write
  unsigned running = 0;	// Jobs running
  unsigned pending = 0;	// Jobs waiting to run

private:
  unsigned counts[STATUS_HWM];

public:
  Engine (unsigned limit, std::ostream &sum, std::ostream &log);
  ~Engine ();

public:
  bool IsLive () const
  {
    return gen_state || !jobs.empty ()
      || !used_tokens.empty () || !ready_tokens.empty ();
  }
  void Init (char const *tester, char const *generator,
	     int argc, char const *const argv[]);
  void Fini (std::ostream * = nullptr);
  bool Process (int timeout);
  void Retire ();
  void Fini (Job const &);
  void Spawn ();
  std::string Progress ();

public:
  auto Result (Status status)
  {
    counts[status]++;
    return Parent::Result (status);
  }

private:
  void ReadGen (unsigned);
  void ReadSignal ();
  void StopMake (int);
  void WantMake ();
  void ReadMake ();
  void WriteMake ();
  void QueueMake (int token);

  void Init (Job *);
  void Fini (Job *);

  friend std::ostream &operator<< (std::ostream &, Engine const &);
};

Engine::Engine (unsigned limit,
		std::ostream &sum_, std::ostream &log_)
  : Parent (sum_, log_), job_limit (limit)
{
  for (unsigned ix = STATUS_HWM; ix--;)
    counts[ix] = 0;

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
	  if (not_space == mflags.npos)
	    not_space = mflags.size ();
	  auto space = mflags.find_first_of (' ', not_space);
	  if (space == mflags.npos)
	    space = mflags.size ();

	  auto option = mflags.substr (not_space, space - not_space);
	  mflags.remove_prefix (space);

	  if (option == "--")
	    break;
	  else if (option.starts_with (jsa))
	    {
	      option.remove_prefix (jsa.size ());
	      Lexer lexer (option);
	      struct stat stat_buf;

	      if (!lexer.Integer ()
		  || lexer.PeekAdvance () != ','
		  || !lexer.Integer ()
		  || lexer.Peek ()
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
	  else if (first)
	    {
	      if (option.find_first_of ('n') != option.npos)
		// Non-execution
		command.clear ();
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
}

std::ostream &operator<< (std::ostream &s, Engine const &self)
{
  for (unsigned ix = 0; ix != Logger::STATUS_HWM; ix++)
    if (ix == Logger::PASS || self.counts[ix])
      s << self.statuses[ix] << ' ' << self.counts[ix] << '\n';

  return s;
}

void Engine::Init (char const *tester, char const *generate,
		   int argc, char const *const argv[])
{
  auto now = time (nullptr);
  *this << "Test run: " << ctime (&now) << "\n";

  // Initialize command vector and gen vector
  std::vector<std::string> gen;
  gen.reserve (argc + 1);
  if (generate)
    {
      generator = generate;
      gen.emplace_back (generator);
    }
  command.emplace_back (tester);
  bool dashed = false;
  for (int ix = 0; ix != argc; ix++)
    {
      std::string_view arg (argv[ix]);
      if (!dashed && arg == "--")
	{
	  command.reserve (ix + 1);
	  for (auto iter = gen.begin () + !generator.empty ();
	       iter != gen.end (); ++iter)
	    command.emplace_back (std::move (*iter));
	  gen.resize (!generator.empty ());
	  dashed = true;
	}
      else
	gen.emplace_back (arg);
    }

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
  while (epoll_ctl (poll_fd, EPOLL_CTL_ADD, sig_fd, &ev) < 0)
    Assert (errno == EINTR);

  if (!generator.empty ())
    {
      // Spawn generator process
      int gen_fds[2] = {-1, -1};
      int null_fd = -1;

      int err;
      for (unsigned ix = 0; ix != 2; ix++)
	{
	  int pipe[2];
	  if (pipe2 (pipe, O_CLOEXEC) < 0)
	    {
	      err = errno;
	      goto fail;
	    }
	  gen_fds[ix] = pipe[1];
	  gen_buffers[ix].Open (pipe[0]);

	  ev.data.u64 = unsigned (FD::GEN_OUT) + ix;
	  while (epoll_ctl (poll_fd, EPOLL_CTL_ADD, pipe[0], &ev) < 0)
	    Assert (errno == EINTR);
	}

      null_fd = open ("/dev/null", O_RDONLY | O_CLOEXEC);
      if (null_fd < 0)
	err = errno;
      else
	{
	  auto spawn = Joust::Spawn (null_fd, gen_fds[0], gen_fds[1], gen);
	  gen_pid = std::get<0> (spawn);
	  err = std::get<1> (spawn);
	}

    fail:
      if (err)
	Result (Logger::ERROR)
	  << "failed spawning generator " << generator << ":" << strerror (err);

      gen_state = 2;
      if (gen_pid)
	{
	  fixed_jobs++;
	  gen_state++;
	}
      else
	{
	  if (null_fd > 0)
	    close (null_fd);
	  for (unsigned ix = 0; ix != 2; ix++)
	    {
	      close (gen_fds[ix]);
	      int fd = gen_buffers[ix].Close ();
	      epoll_ctl (poll_fd, EPOLL_CTL_DEL, fd, nullptr);
	      close (fd);
	    }
	}
    }
  else
    // Create pending job queue
    for (auto iter = gen.begin (); iter != gen.end (); ++iter)
      jobs.emplace_back (std::move (*iter));
}

void Engine::Fini (std::ostream *summary)
{
  if (!generator.empty ()
      && (!gen_buffers[1].empty ()
	  || WIFSIGNALED (gen_exit)
	  || (WIFEXITED (gen_exit) && WEXITSTATUS (gen_exit))))
    {
      Log () << '\n';
      Log () << "# Generator Errors";
      Log () << std::string_view (gen_buffers[1].data (),
				  gen_buffers[1].size ());

      if (WIFSIGNALED (gen_exit))
	{
	  int sig = WTERMSIG (gen_exit);
	  Result (Logger::ERROR) << generator
				 << " terminated with signal " << sig;
	}
      if (WIFEXITED (gen_exit))
	if (int code = WEXITSTATUS (gen_exit))
	  Result (Logger::ERROR) << generator
				 << " exited with status " << code;
    }

  if (summary)
    *summary << "# Summary of " << retired << " tests \n" << *this;
  Sum () << '\n';
  *this << "# Summary of " << retired << " tests \n" << *this;

  Flush ();

  close (poll_fd);
  poll_fd = -1;
  close (sig_fd);
  sig_fd = -1;

  // FIXME: Restore sigmask
}

void Engine::ReadGen (unsigned stream)
{
  // Read
  bool out_live = true;
  if (int done = gen_buffers[stream].Read ())
    {
      if (done >= 0)
	{
	  static char const *const io[] = {" stdout:", " stderr:"};
	  Result (Logger::ERROR)
	    << "failed reading generator "
	    << generator << io[stream] << strerror (done);
	}

      int fd = gen_buffers[stream].Close ();

      epoll_ctl (poll_fd, EPOLL_CTL_DEL, fd, nullptr);
      close (fd);
      gen_state--;
      out_live = false;
    }

  if (!stream)
    {
      // Lex
      unsigned added = 0;
      auto first = gen_buffers[0].begin ();
      auto last = gen_buffers[0].end ();
      while (first != last)
	{
	  auto eol = std::find (first, last, '\n');
	  if (eol == last && out_live)
	    break;

	  std::string_view line (first, eol);
	  first = eol + out_live;

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
      if (first != gen_buffers[0].begin ())
	gen_buffers[0].erase (gen_buffers[0].begin (), first);

      WantMake ();
    }
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
		Assert (!running && gen_pid == -1);
		break;
	      }

	    if (WIFEXITED (status) || WIFSIGNALED (status))
	      {
		if (child == gen_pid)
		  {
		    gen_state--;
		    gen_pid = -1;
		    fixed_jobs--;

		    gen_exit = status;
		  }
		else
		  {
		    auto iter = jobs.begin ();
		    for (unsigned limit = completed + running; limit--; ++iter)
		      if (iter->IsPid (child))
			{
			  int token = iter->Reap (status);
			  if (token != -1)
			    QueueMake (token);
			  else
			    fixed_jobs--;

			  completed++;
			  running--;
			  break;
			}
		  }
	      }
	    if (!running && gen_pid == -1)
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

void Engine::WantMake ()
{
  if (make_in >= 0)
    {
      unsigned want = pending;
      unsigned ready = ready_tokens.size () + (job_limit - fixed_jobs);
      if (want < ready)
	want = 0;
      else
	want -= ready;

      if (!want)
	{
	  if (make_want)
	    while (epoll_ctl (poll_fd, EPOLL_CTL_DEL, make_in, nullptr) < 0)
	      Assert (errno == EINTR);
	}
      else
	{
	  if (!make_want)
	    {
	      epoll_event ev;
	      ev.events = EPOLLIN;
	      ev.data.u64 = unsigned (FD::MAKE_IN);
	      while (epoll_ctl (poll_fd, EPOLL_CTL_ADD, make_in, &ev) < 0)
		Assert (errno == EINTR);
	    }
	}
      make_want = want;
    }
}

void Engine::StopMake (int err)
{
  Assert (make_out >= 0 && make_in >= 0);
  epoll_ctl (poll_fd, EPOLL_CTL_DEL, make_out, nullptr);
  epoll_ctl (poll_fd, EPOLL_CTL_DEL, make_in, nullptr);
  make_out = make_in = -1;
  used_tokens.clear ();
  Result (Logger::ERROR) << " terminating jobserver:" << strerror (err);
}

void Engine::ReadMake ()
{
  unsigned const block_size = 16;
  char buffer[block_size];
  
  unsigned want = make_want < block_size ? make_want : block_size;
  ssize_t got = read (make_in, buffer, want);
  if (got < 0)
    {
      if (errno != EINTR)
	StopMake (errno);
    }
  else
    ready_tokens.append (buffer, got);

  WantMake ();
}

// Write the token back to jobcontrol to
// allow other processes a chance.
void Engine::QueueMake (int token)
{
  if (used_tokens.empty ())
    {
      epoll_event ev;
      ev.events = EPOLLOUT;
      ev.data.u64 = unsigned (FD::MAKE_OUT);
      while (epoll_ctl (poll_fd, EPOLL_CTL_ADD, make_out, &ev) < 0)
	Assert (errno == EINTR);
    }
  used_tokens.push_back (char (token));
}

// Able to write something to the make pipe.  Do it.

void Engine::WriteMake ()
{
  size_t tokens = used_tokens.size ();
  ssize_t wrote = write (make_out, used_tokens.data (), used_tokens.size ());
  if (wrote < 0)
    {
      if (errno != EINTR)
	StopMake (errno);
      wrote = 0;
    }

  if (size_t (wrote) == used_tokens.size ())
    {
      used_tokens.clear ();
      while (epoll_ctl (poll_fd, EPOLL_CTL_DEL, make_out, nullptr) < 0)
	Assert (errno == EINTR);
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
	  case unsigned (FD::GEN_OUT):
	  case unsigned (FD::GEN_ERR):
	    ReadGen (unsigned (cookie - unsigned (FD::GEN_OUT)));
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
	    {
	      Job *job = reinterpret_cast<Job *> (cookie ^ (cookie & 7));

	      job->Read (*this, cookie & 7, poll_fd);
	    }
	    break;
	  }
    }
  while (count == max_events);

  return !count;
}

void Engine::Fini (Job const &job)
{
  Log () << "# Test:" << retired << " " << job << '\n';
  Log () << "ALOY:";
  for (auto iter = command.begin (); iter != command.end (); ++iter)
    Log () << *iter << ' ';
  Log () << job << '\n';

  auto &log_text = job.Buffer (1);
  Log () << std::string_view (log_text.data (), log_text.size ());

  unsigned bad_count = 0;
  std::string_view bad_line;
  auto &sum_text = job.Buffer (0);
  for (auto sol = sum_text.data (), end = sol + sum_text.size (); sol != end;)
    {
      auto eol = std::find (sol, end, '\n');
      auto line = std::string_view (sol, eol);

      Status st = DecodeStatus (line);
      if (st == STATUS_HWM)
	{
	  bad_count++;
	  if (!bad_line.size ())
	    bad_line = line;
	}
      else
	counts[st]++;
      Sum () << line << '\n';
      sol = eol + (eol != end);
    }
  if (bad_count)
    Result (Logger::ERROR) << job << ": unexpected summary line '"
			   << bad_line << '\'';

  int exit = job.Exit ();
  if (WIFSIGNALED (exit))
    {
      int sig = WTERMSIG (exit);
      Result (Logger::ERROR) << job << " terminated with signal " << sig;
    }
  else if (WIFEXITED (exit))
    {
      if (int code = WEXITSTATUS (exit))
	Result (Logger::ERROR) << job << " exited with status " << code;
    }
  else
    Unreachable ();

  Log () << '\n';
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
  for (auto iter = jobs.begin () + (completed + running); pending; ++iter)
    {
      int token = -1;
      if (job_limit > fixed_jobs)
	;
      else if (ready_tokens.size ())
	{
	  token = ready_tokens.back ();
	  ready_tokens.pop_back ();
	}
      else
	break;

      if (iter->Spawn (*this, command, poll_fd, token))
	{
	  running++;
	  if (token < 0)
	    fixed_jobs++;
	}
      else
	{
	  completed++;
	  if (token >= 0)
	    QueueMake (token);
	}
      pending--;
    }

  while (!ready_tokens.empty ())
    {
      QueueMake (ready_tokens.back ());
      ready_tokens.pop_back ();
    }

  WantMake ();
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
  else if (gen_state)
    progress << " ...";
  return progress.str ();
}
