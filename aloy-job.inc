// ALOY: Apply List, Observe Yield		-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// Not For Distribution

class Job
{
  std::string command;
  std::vector<char> text[2];	// text from job
  int fds[2] = {-1, -1};	// FDs from job
  pid_t pid = pid_t (-1);	// Job's PID
  int exit_status = 0;		// Exit status of job
  short make_token = -1;	// Make job-server token
  int state = 0;

public:
  Job (std::string_view const &cmd)
    : command (cmd)
  {
    printf ("Creating job '%s'\n", command.c_str ());
  }

private:
  // We rely on object reference invariance, so ban copies
  Job (Job const &) = delete;
  Job &operator= (Job const &) = delete;

public:
  void Read (unsigned subcode, int poll_fd);
  bool Spawn (std::vector<std::string> const &cmd, int token = -1);
  int Reap (int status);
  auto *Fds () const
  {
    return state ? fds : nullptr;
  }
  bool IsPid (pid_t p)
  {
    return pid == p;
  }
  bool MaybeFini ();

  friend std::ostream &operator<< (std::ostream &, Job const &);
};

void Job::Read (unsigned subcode, int poll_fd)
{
  Assert (subcode < 2 && fds[subcode] >= 0 && state > 0);

  // Read
  size_t size = text[subcode].size ();
  size_t hwm = text[subcode].capacity ();
  if (hwm - size < 10)
    hwm += 1024;
  text[subcode].resize (hwm);
  ssize_t count = read (fds[subcode], &text[subcode][size], hwm - size);
  text[subcode].resize (size + (count >= 0 ? count : 0));

  if (count < 0 && errno != EINTR)
    {
      // FIXME: Log ERROR
      count = 0;
    }

  if (!count)
    {
      epoll_ctl (poll_fd, EPOLL_CTL_DEL, fds[subcode], nullptr);
      close (fds[subcode]);
      fds[subcode] = -1;
      state--;
    }
}

bool Job::Spawn (std::vector<std::string> const &cmd, int token)
{
  Assert (!state && text[0].empty () && text[1].empty ());

  printf ("spawning\n");

  if (!cmd.size ())
    // No execute mode, we're done
    return false;

  int job_fds[2];

  for (unsigned ix = 0; ix != 2; ix++)
    {
      int pipe[2];
      if (pipe2 (pipe, O_CLOEXEC) < 0)
	; // FIXME: Log error
      fds[ix] = pipe[0];
      job_fds[ix] = pipe[1];
    }

  pid = fork ();
  if (!pid)
    {
      // Child
      unsigned nargs = cmd.size () + 1;
      auto args = reinterpret_cast<char const **> (alloca (sizeof (char const *)
							   * (nargs + 1)));
      unsigned ix = 0;
      for (auto iter = cmd.begin (); iter != cmd.end (); ++iter)
	args[ix++] = iter->c_str ();
      args[ix++] = command.c_str ();
      args[ix] = nullptr;

      if (dup2 (job_fds[0], 1) < 0
	  || dup2 (job_fds[1], 2) < 0
	  || execvp (args[0], const_cast<char **> (args)))
	{
	  int err = errno;
	  // FIXME: Use a pipe to tell our parent explicitly
	}
      exit (2);
    }

  close (job_fds[0]);
  close (job_fds[1]);

  state = 3;

  if (pid != pid_t (-1))
    {
      // Keep the make token
      make_token = token;
      return true;
    }

  // FIXME: Failed
  state--;
  return false;
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

bool Job::MaybeFini ()
{
  if (state)
    return false;

  // Write out text

  return true;
}

std::ostream &operator<< (std::ostream &s, Job const &job)
{
  for (auto iter = job.command.begin (); iter != job.command.end (); ++iter)
    {
      if (iter != job.command.begin ())
	s << ' ';
      s << *iter;
    }

  return s;
}
