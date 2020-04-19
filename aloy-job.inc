// ALOY: Apply List, Observe Yield		-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// Not For Distribution

class Job
{
  std::string command;
  ReadBuffer buffers[2];	// Job's stdout, stderr
  pid_t pid = pid_t (-1);	// Job's PID
  int exit_status = 0;		// Exit status of job
  short make_token = -1;	// Make job-server token
  int state = 0;

public:
  Job (std::string_view const &cmd)
    : command (cmd)
  {
  }

private:
  // We rely on object reference invariance, so ban copies
  Job (Job const &) = delete;
  Job &operator= (Job const &) = delete;

public:
  void Read (unsigned subcode, int poll_fd, Logger &);

  bool Spawn (std::vector<std::string> &cmd, int poll_fd, int token = -1);
  int Reap (int status);
  bool IsPid (pid_t p) const
  {
    return pid == p;
  }
  bool Ready () const
  {
    return !state;
  }
  int Exit () const
  {
    return exit_status;
  }
  auto const &Buffer (unsigned ix) const
  {
    return buffers[ix];
  }

  friend std::ostream &operator<< (std::ostream &, Job const &);
};

void Job::Read (unsigned subcode, int poll_fd, Logger &log)
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

bool Job::Spawn (std::vector<std::string> &cmd, int poll_fd, int token)
{
  Assert (!state && buffers[0].empty () && buffers[0].empty ());

  if (!cmd.size ())
    // No execute mode, we're done
    return false;

  int job_fds[2];

  for (unsigned ix = 0; ix != 2; ix++)
    {
      int pipe[2];
      if (pipe2 (pipe, O_CLOEXEC) < 0)
	; // FIXME: Log error

      job_fds[ix] = pipe[1];
      buffers[ix].Open (pipe[0]);

      epoll_event ev;
      ev.events = EPOLLIN;
      ev.data.u64 = reinterpret_cast<uint64_t> (this) | ix;
      if (epoll_ctl (poll_fd, EPOLL_CTL_ADD, pipe[0], &ev) < 0)
	Unreachable ();
    }

  cmd.push_back (command);
  pid = Joust::Spawn (-1, job_fds[0], job_fds[1], cmd);
  cmd.pop_back ();

  close (job_fds[0]);
  close (job_fds[1]);

  state = 3;

  if (pid < 0)
    {
      // FIXME: Log error
      state--;
      return false;
    }

  // Keep the make token
  make_token = token;
  return true;
}

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

std::ostream &operator<< (std::ostream &s, Job const &job)
{
  s << job.command;

  return s;
}
