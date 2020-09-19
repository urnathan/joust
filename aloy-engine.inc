// Joust/ALOY: Apply List, Observe Yield		-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#if !defined (ALOY_ENGINE)
#define ALOY_ENGINE
class Engine
  : public Logger
{
public:
  using Parent = Logger;

private:
  enum class FD
  {
    MAKE_IN,	// make_in
    MAKE_OUT,	// make_out
    SIGNAL,	// sig_fd
    HWM
  };

private:
  std::deque<Job> jobs;
  std::vector<std::string> command;
  std::string used_tokens; // tokens to send back to make
  std::string ready_tokens; // tokens we've got from make
  Job generator;

private:
  unsigned job_limit = 1;  // static number of jobs we can spawn
  unsigned fixed_jobs = 0; // non-jobserver jobs running
  unsigned make_want = 0;
  int make_in = -1, make_out = -1;

private:
#ifdef USE_EPOLL
  int sig_fd = -1;
  int poll_fd = -1;
#else
  static int const poll_fd = -1;
  sigset_t sigselect;
  struct sigaction *old_actions = nullptr;
#endif

private:
  bool stopping = false;

private:
  unsigned retired = 0; // Jobs written out
  unsigned completed = 0;// Jobs completed, but waiting to write
  unsigned running = 0;	// Jobs running
  unsigned pending = 0;	// Jobs waiting to run

private:
  unsigned counts[STATUS_HWM];

public:
  Engine
    (unsigned limit, std::ostream &sum, std::ostream &log);
  ~Engine
    ();

public:
  bool IsLive
    ()
    const
  {
    return !generator.Ready () || !jobs.empty ()
      || !used_tokens.empty () || !ready_tokens.empty ();
  }
  void Init
    (char const *tester, char const *genner,
     int argc, char const *const argv[]);
  void Fini
    (std::ostream * = nullptr);
  void Stop
    (int sig);
  void Process
    ();
  void Retire
    ();
  void Fini
    (Job const &, bool is_generator = false);
  void Spawn
    ();
  std::string Progress
    ();

public:
  auto Result
    (Status status)
  {
    counts[status]++;
    return Parent::Result (status, nullptr, 0);
  }

private:
  void ReadGenerator
    ();
  void HandleSignal
    (int sig);

private:
  void StopMake
    (int);
  void WantMake
    ();
  void ReadMake
    ();
  void WriteMake
    ();
  void QueueMake
    (int token);

  friend std::ostream &operator<<
    (std::ostream &, Engine const &);
};

#else

static constexpr unsigned char sigs[]
  = {
  SIGHUP, SIGQUIT, SIGPIPE, SIGCHLD, SIGTERM
};

Engine::Engine
  (unsigned limit, std::ostream &sum_, std::ostream &log_)
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

Engine::~Engine
  ()
{
}

std::ostream &operator<<
  (std::ostream &s, Engine const &self)
{
  for (unsigned ix = 0; ix != Logger::STATUS_HWM; ix++)
    if (ix == Logger::PASS || self.counts[ix])
      s << self.statuses[ix] << ' ' << self.counts[ix] << '\n';

  return s;
}

#ifndef USE_EPOLL
// Signals we saw
volatile unsigned sigs_seen;

void NoteSignal
  (int signal)
{
  for (unsigned ix = sizeof (sigs); ix--;)
    if (signal == sigs[ix])
      {
	sigs_seen = sigs_seen | (1 << ix);
	break;
      }
}
#endif

void Engine::Init
  (char const *tester, char const *genner, int argc, char const *const argv[])
{
  auto now = time (nullptr);
  *this << "Test run: " << ctime (&now) << "\n";

  // Initialize command vector and gen vector
  command.emplace_back (tester);
  bool dashed = false;
  for (int ix = 0; ix != argc; ix++)
    {
      std::string_view arg (argv[ix]);
      if (!dashed && arg == "--")
	{
	  command.reserve (ix + 1);
	  for (auto iter = generator.Command ().begin ();
	       iter != generator.Command ().end (); ++iter)
	    command.emplace_back (std::move (*iter));
	  generator.Command ().clear ();
	  dashed = true;
	}
      else
	generator.Command ().emplace_back (arg);
    }

  // Signal fd and block sigchild 
  sigset_t sigmask;
  sigemptyset (&sigmask);
  for (unsigned ix = sizeof (sigs); ix--;)
    sigaddset (&sigmask, sigs[ix]);
#ifdef USE_EPOLL
  while (sigprocmask (SIG_BLOCK, &sigmask, nullptr) < 0)
    Assert (errno == EINTR);

  sig_fd = signalfd (-1, &sigmask, SFD_NONBLOCK | SFD_CLOEXEC);
  Assert (sig_fd >= 0);

  // Epoll fd
  poll_fd = epoll_create1 (EPOLL_CLOEXEC);
  Assert (poll_fd >= 0);

  epoll_event ev;
  ev.events = EPOLLIN;
  ev.data.u64 = unsigned (FD::SIGNAL);
  while (epoll_ctl (poll_fd, EPOLL_CTL_ADD, sig_fd, &ev) < 0)
    Assert (errno == EINTR || errno == EAGAIN);
#else
  while (sigprocmask (SIG_UNBLOCK, &sigmask, nullptr) < 0)
    Assert (errno == EINTR);
  while (sigprocmask (SIG_BLOCK, &sigmask, &sigselect) < 0)
    Assert (errno == EINTR);
  old_actions = new struct sigaction[sizeof (sigs)];
  // Install signal handlers
  for (unsigned ix = sizeof (sigs); ix--;)
    {
      struct sigaction action;
      action.sa_handler = NoteSignal;
      sigfillset (&action.sa_mask);
      action.sa_flags = 0;
      sigaction (sigs[ix], &action, &old_actions[ix]);
    }
#endif

  if (genner)
    {
      std::vector<std::string> g {genner};
      
      if (generator.Spawn (*this, g, poll_fd))
	fixed_jobs++;
    }
  else
    {
      // Create pending job queue
      for (auto iter = generator.Command ().begin ();
	   iter != generator.Command ().end (); ++iter)
	{
	  jobs.emplace_back (std::move (*iter));
	  pending++;
	}
      generator.Command ().clear ();
    }
}

void Engine::Fini
  (std::ostream *summary)
{
  Fini (generator, true);

  if (summary)
    *summary << "# Summary of " << retired << " test programs \n" << *this;
  if (retired)
    Sum () << '\n';
  *this << "# Summary of " << retired << " test programs \n" << *this;

  Flush ();

#ifdef USE_EPOLL
  close (poll_fd);
  poll_fd = -1;
  close (sig_fd);
  sig_fd = -1;
#else
  // Restore signal handlers
  for (unsigned ix = sizeof (sigs); ix--;)
    sigaction (sigs[ix], &old_actions[ix], nullptr);
  delete[] old_actions;
  old_actions = nullptr;
#endif
}

void Engine::ReadGenerator
  ()
{
  // Lex
  auto &buffer = generator.Buffer (0);
  unsigned added = 0;
  auto first = buffer.begin ();
  auto last = buffer.end ();
  while (first != last)
    {
      auto eol = std::find (first, last, '\n');
      if (eol == last && buffer.IsOpen ())
	break;

      std::string_view line (first, eol);
      first = eol + buffer.IsOpen ();

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
  if (first != buffer.begin ())
    buffer.erase (buffer.begin (), first);

  WantMake ();
}

void Engine::HandleSignal
  (int sig)
{
  switch (sig)
    {
    case SIGCHLD:
      // Get child status.  There could be multiple children to deal with.
      {
	int status;
	while (pid_t child = waitpid (-1, &status, WNOHANG))
	  {
	    if (child == pid_t (-1))
	      {
		Assert (!running && generator.Ready ());
		break;
	      }

	    if (WIFEXITED (status) || WIFSIGNALED (status))
	      {
		if (generator.IsPid (child))
		  {
		    generator.Reap (status);
		    fixed_jobs--;
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
	    if (!running && !generator.Ready ())
	      break;
	  }
      }
      break;

    case SIGHUP:
    case SIGINT:
    case SIGQUIT:
    case SIGTERM:
      Stop (sig);
      break;
      
    case SIGPIPE:
    default:
      Unreachable ();
    }
}

void Engine::Stop
  (int sig)
{
  stopping = true;
  generator.Stop (sig);
  if (pending)
    do
      jobs.pop_back ();
    while (--pending);
  for (auto iter = jobs.begin (); iter != jobs.end (); ++iter)
    iter->Stop (sig);
}

void Engine::WantMake
  ()
{
  if (make_in >= 0)
    {
      unsigned want = pending;
      unsigned ready = ready_tokens.size () + (job_limit - fixed_jobs);
      if (want < ready)
	want = 0;
      else
	want -= ready;

#ifdef USE_EPOLL
      if (!want)
	{
	  if (make_want)
	    while (epoll_ctl (poll_fd, EPOLL_CTL_DEL, make_in, nullptr) < 0)
	      Assert (errno == EINTR || errno == EAGAIN);
	}
      else
	{
	  if (!make_want)
	    {
	      epoll_event ev;
	      ev.events = EPOLLIN;
	      ev.data.u64 = unsigned (FD::MAKE_IN);
	      while (epoll_ctl (poll_fd, EPOLL_CTL_ADD, make_in, &ev) < 0)
		Assert (errno == EINTR || errno == EAGAIN);
	    }
	}
#endif
      make_want = want;
    }
}

void Engine::StopMake
  (int err)
{
  Assert (make_out >= 0 && make_in >= 0);
#ifdef USE_EPOLL
  epoll_ctl (poll_fd, EPOLL_CTL_DEL, make_out, nullptr);
  epoll_ctl (poll_fd, EPOLL_CTL_DEL, make_in, nullptr);
#endif
  make_out = make_in = -1;
  used_tokens.clear ();
  Result (Logger::ERROR) << " terminating jobserver:" << strerror (err);
}

void Engine::ReadMake
  ()
{
  unsigned const block_size = 16;
  char buffer[block_size];
  
  unsigned want = make_want < block_size ? make_want : block_size;
  ssize_t got = read (make_in, buffer, want);
  if (got < 0)
    {
      if (errno != EINTR && errno != EAGAIN)
	StopMake (errno);
    }
  else
    ready_tokens.append (buffer, got);

  WantMake ();
}

// Write the token back to jobcontrol to
// allow other processes a chance.
void Engine::QueueMake
  (int token)
{
#ifdef USE_EPOLL
  if (used_tokens.empty ())
    {
      epoll_event ev;
      ev.events = EPOLLOUT;
      ev.data.u64 = unsigned (FD::MAKE_OUT);
      while (epoll_ctl (poll_fd, EPOLL_CTL_ADD, make_out, &ev) < 0)
	Assert (errno == EINTR || errno == EAGAIN);
    }
#endif
  used_tokens.push_back (char (token));
}

// Able to write something to the make pipe.  Do it.

void Engine::WriteMake
  ()
{
  size_t tokens = used_tokens.size ();
  ssize_t wrote = write (make_out, used_tokens.data (), used_tokens.size ());
  if (wrote < 0)
    {
      if (errno != EINTR && errno != EAGAIN)
	StopMake (errno);
      wrote = 0;
    }

  if (size_t (wrote) == used_tokens.size ())
    {
      used_tokens.clear ();
#ifdef USE_EPOLL
      while (epoll_ctl (poll_fd, EPOLL_CTL_DEL, make_out, nullptr) < 0)
	Assert (errno == EINTR || errno == EAGAIN);
#endif
    }
  else
    used_tokens.erase (0, wrote);
}

void Engine::Process
  ()
{
#ifdef USE_EPOLL
  constexpr int max_events = 20;
  epoll_event events[max_events];

  int count = epoll_wait (poll_fd, events, max_events, -1);
  if (count < 0)
    {
      count = 0;
      Assert (errno == EINTR || errno == EAGAIN);
    }

  for (int ix = 0; ix != count; ix++)
    switch (uint64_t cookie = events[ix].data.u64)
      {
      case unsigned (FD::SIGNAL):
	{
	  signalfd_siginfo sig;

	  ssize_t bytes = read (sig_fd, &sig, sizeof (sig));
	  Assert (bytes == sizeof (sig));
	  HandleSignal (sig.ssi_signo);
	}
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

	  if (!stopping
	      && !(cookie & 7) && job == &generator)
	    ReadGenerator ();
	}
	break;
      }
#else
  fd_set rd_set, wr_set;
  FD_ZERO (&rd_set);
  FD_ZERO (&wr_set);
  int hwm = 0;

  if (make_want)
    {
      FD_SET (make_in, &rd_set);
      if (hwm <= make_in)
	hwm = make_in + 1;
    }
  if (!used_tokens.empty ())
    {
      FD_SET (make_out, &wr_set);
      if (hwm <= make_out)
	hwm = make_out + 1;
    }
  for (unsigned ix = 0; ix != 2; ix++)
    if (generator.Buffer (ix).IsOpen ())
      {
	int fd = generator.Buffer (ix).GetFD ();
	FD_SET (fd, &rd_set);
	if (hwm <= fd)
	  hwm = fd + 1;
      }
  for (auto iter = jobs.begin (); iter != jobs.end (); ++iter)
    for (unsigned ix = 0; ix != 2; ix++)
      if (iter->Buffer (ix).IsOpen ())
	{
	  int fd = iter->Buffer (ix).GetFD ();
	  FD_SET (fd, &rd_set);
	  if (hwm <= fd)
	    hwm = fd + 1;
	}

  int count = pselect (hwm, &rd_set, &wr_set, nullptr, nullptr, &sigselect);
  if (count < 0)
    Assert (errno == EINTR);
  else
    {
      if (make_want && FD_ISSET (make_in, &rd_set))
	ReadMake ();
      if (!used_tokens.empty () && FD_ISSET (make_out, &wr_set))
	WriteMake ();
      for (unsigned ix = 0; ix != 2; ix++)
	if (generator.Buffer (ix).IsOpen ())
	  {
	    int fd = generator.Buffer (ix).GetFD ();
	    if (FD_ISSET (fd, &rd_set))
	      {
		generator.Read (*this, ix, poll_fd);

		if (!stopping && !ix)
		  ReadGenerator ();
	      }
      }
      for (auto iter = jobs.begin (); iter != jobs.end (); ++iter)
	for (unsigned ix = 0; ix != 2; ix++)
	  if (iter->Buffer (ix).IsOpen ())
	    {
	      int fd = iter->Buffer (ix).GetFD ();
	      if (FD_ISSET (fd, &rd_set))
		iter->Read (*this, ix, poll_fd);
	    }
    }

  if (int seen_sig = sigs_seen)
    {
      sigs_seen = 0;
      for (unsigned ix = sizeof (sigs); ix--;)
	if (seen_sig & (1 << ix))
	  HandleSignal (sigs[ix]);
    }
#endif
}

void Engine::Fini
  (Job const &job, bool is_generator)
{
  if (is_generator)
    Log () << "# Test generator: " << job << '\n';
  else
    {
      Log () << "# Test:" << retired << " " << job << '\n';
      Log () << "ALOY:";
      for (auto iter = command.begin (); iter != command.end (); ++iter)
	Log () << *iter << ' ';
      Log () << job << '\n';
    }

  auto &log_text = job.Buffer (1);
  Log () << std::string_view (log_text.data (), log_text.size ());

  if (!is_generator)
    {
      unsigned bad_count = 0;
      std::string_view bad_line;
      auto &sum_text = job.Buffer (0);
      for (auto sol = sum_text.data (), end = sol + sum_text.size ();
	   sol != end;)
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
	{
	  Result (Logger::ERROR) << job << ": unexpected summary line '"
				 << bad_line << '\'';
	  counts[Logger::ERROR]++;
	}
    }

  job.ReportExit (*this);

  Log () << '\n';
}

void Engine::Retire
  ()
{
  while (completed && jobs.front ().Ready ())
    {
      Fini (jobs.front ());
      jobs.pop_front ();

      completed--;
      retired++;
    }
}

void Engine::Spawn
  ()
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

std::string Engine::Progress
  ()
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
  else if (!generator.Ready ())
    progress << " ...";
  return progress.str ();
}
#endif
