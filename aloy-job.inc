// Joust/ALOY: Apply List, Observe Yield		-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#if !defined (ALOY_JOB)
#define ALOY_JOB
class Job
{
  std::vector<std::string> command;
  ReadBuffer buffers[2];	// Job's stdout, stderr
  pid_t pid = pid_t (-1);	// Job's PID
  int exit_status = 0;		// Exit status of job
  short make_token = -1;	// Make job-server token
  int state = 0;

public:
  Job (std::string_view const &cmd)
  {
    command.emplace_back (cmd);
  }
  Job (std::string &&cmd)
  {
    command.emplace_back (std::move (cmd));
  }
  Job ()
  {
  }

private:
  // We rely on object reference invariance, so ban copies
  Job (Job const &) = delete;
  Job &operator= (Job const &) = delete;

public:
  std::vector<std::string> &Command ()
  {
    return command;
  }
  auto &Buffer (unsigned ix)
  {
    return buffers[ix];
  }
  auto const &Buffer (unsigned ix) const
  {
    return buffers[ix];
  }

public:
  void Read (Engine &, unsigned subcode, int poll_fd);

  bool Spawn (Engine &, std::vector<std::string> const &preamble,
	      int poll_fd, int token = -1);
  int Reap (int status);
  bool IsPid (pid_t p) const
  {
    return pid == p;
  }
  void Stop (int signal)
  {
    if (pid > 0)
      kill (pid, signal);
  }
  bool Ready () const
  {
    return !state;
  }
  void ReportExit (Engine &) const;

  friend std::ostream &operator<< (std::ostream &, Job const &);
};

#else

void Job::Read (Engine &log, unsigned subcode, int poll_fd)
{
  Assert (subcode < 2 && state > 0);

  if (int done = buffers[subcode].Read ())
    {
      if (done >= 0)
	{
	  static char const *const io[] = {" stdout:", " stderr:"};
	  log.Result (Logger::ERROR)
	    << "failed reading "
	    << *this << io[subcode] << strerror (done);
	}

      int fd = buffers[subcode].Close ();

      epoll_ctl (poll_fd, EPOLL_CTL_DEL, fd, nullptr);
      close (fd);
      state--;
    }
}

// Spawn a job, return true if we managed to spawn it.

bool Job::Spawn (Engine &log, std::vector<std::string> const &preamble,
		 int poll_fd, int token)
{
  Assert (!state && buffers[0].empty () && buffers[0].empty ());

  if (!preamble.size ())
    // No execute mode, we're done
    return false;

  int job_fds[2] {-1, -1};
  int null_fd = -1;

  int err;
  for (unsigned ix = 0; ix != 2; ix++)
    {
      int pipe[2];
      if (MakePipe (pipe) < 0)
	{
	  err = errno;
	  goto fail;
	}
      job_fds[ix] = pipe[1];
      buffers[ix].Open (pipe[0]);

      epoll_event ev;
      ev.events = EPOLLIN;
      ev.data.u64 = reinterpret_cast<uint64_t> (this) | ix;
      while (epoll_ctl (poll_fd, EPOLL_CTL_ADD, pipe[0], &ev) < 0)
	Assert (errno == EINTR);
    }

  null_fd = open ("/dev/null", O_RDONLY | O_CLOEXEC);
  if (null_fd < 0)
    err = errno;
  else
    {
      auto spawn = Joust::Spawn (null_fd, job_fds[0], job_fds[1],
				 command, &preamble);
      pid = std::get<0> (spawn);
      err = std::get<1> (spawn);
    }

 fail:
  if (err)
    log.Result (Logger::ERROR)
      << "failed spawning " << *this << ":" << strerror (err);
  
  if (pid)
    {
      state = 3;

      // Keep the make token
      make_token = token;
    }
  else
    {
      if (null_fd > 0)
	close (null_fd);
      for (unsigned ix = 0; ix != 2; ix++)
	{
	  close (job_fds[ix]);
	  int fd = buffers[ix].Close ();
	  epoll_ctl (poll_fd, EPOLL_CTL_DEL, fd, nullptr);
	  close (fd);
	}
    }

  return pid != 0;
}

// Reap a completed job, returns the make token (or -1)
int Job::Reap (int status)
{
  Assert (pid >= 0 && state);
  exit_status = status;

  pid = pid_t (-1);
  state--;

  int r = make_token;
  make_token = -1;
  return r;
}

void Job::ReportExit (Engine &log) const
{
  if (WIFSIGNALED (exit_status))
    {
      int sig = WTERMSIG (exit_status);
      log.Result (Logger::ERROR)
	<< *this << " terminated with signal " << sig;
    }
  else if (WIFEXITED (exit_status))
    {
      if (int code = WEXITSTATUS (exit_status))
	log.Result (Logger::ERROR)
	  << *this << " exited with status " << code;
    }
  else
    Unreachable ();
}

std::ostream &operator<< (std::ostream &s, Job const &job)
{
  s << job.command[0];

  return s;
}

#endif
