// Joust Test Suite			-*- mode:c++ -*-
// Copyright (C) 2020-2024 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#include "joust/cfg.h"
// NMS
#include "nms/fatal.hh"
// Gaige
#include "gaige/spawn.hh"
// OS
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>

using namespace nms;
using namespace gaige;

std::tuple<pid_t, int> gaige::spawn (int fd_in, int fd_out, int fd_err,
				     std::vector<std::string> const &command,
				     std::vector<std::string> const *wrapper,
				     unsigned const *limits)
{
  std::tuple<pid_t, int> res {0, 0};
  auto &[pid, err] = res;
  int pipe_fds[2];

  // pipe ends: 0-read from, 1-write to
  if (makePipe (pipe_fds) < 0)
    err = errno;
  else
    {
      // Fork it!
      pid = fork ();

      if (!pid)
	{
	  // Child
	  close (pipe_fds[0]);

	  // Count the arguments
	  unsigned nargs = command.size ();
	  if (wrapper)
	    nargs += wrapper->size ();

	  // Assemble them
	  auto args = reinterpret_cast<char const **>
	    (alloca (sizeof (char const *) * (nargs + 1)));
	  unsigned ix = 0;
	  if (wrapper)
	    for (auto iter = wrapper->begin (); iter != wrapper->end (); ++iter)
	      args[ix++] = iter->c_str ();
	  for (auto iter = command.begin (); iter != command.end (); ++iter)
	    args[ix++] = iter->c_str ();
	  args[ix] = nullptr;

	  if ((fd_in == 0 || dup2 (fd_in, 0) >= 0)
	      && (fd_out == 1 || dup2 (fd_out, 1) >= 0)
	      && (fd_err == 2 || dup2 (fd_err, 2) >= 0))
	    {
	      if (limits)
		{
		  // If limit setting fails, do not exec
		  for (unsigned jx = PL_HWM; jx--;)
		    if (limits[jx])
		      {
			rlim_t v = limits[jx];
			if (jx != PL_CPU)
			  v *= 1024 * 1024 * 1024;

			struct rlimit limit;
			limit.rlim_cur = limit.rlim_max = v;
			static int const inits[PL_HWM]
			  = {RLIMIT_CPU, RLIMIT_DATA, RLIMIT_FSIZE};
			if (setrlimit (inits[jx], &limit) < 0)
			  goto failed;
		      }
		}

	      execvp (args[0], const_cast<char **> (args));
	    }

	failed:
	  // Something failed.  write errno to pipe;
	  write (pipe_fds[1], &errno, sizeof (errno));
	  close (pipe_fds[1]);
	  // Do not run atexit
	  _exit (0);
	  unreachable ();
	}

      err = errno;

      // Parent
      close (pipe_fds[1]);

      if (pid < 0)
	pid = 0;
      else if (!read (pipe_fds[0], &err, sizeof (err)))
	err = 0;

      close (pipe_fds[0]);
    }

  if (fd_in != 0)
    close (fd_in);
  if (fd_out != 1)
    close (fd_out);
  if (fd_err != 2)
    close (fd_err);

  return res;
}

#ifndef HAVE_PIPE2
int gaige::makePipe (int pipes[2])
{
  if (pipe (pipes) < 0)
    // Failed to make the pipes
    return -1;

  // Initial F_GETFD value is zero
  if (!fcntl (pipes[0], F_SETFD, FD_CLOEXEC)
      && !fcntl (pipes[1], F_SETFD, FD_CLOEXEC))
    // Succeeded in fcntling the ends
    return 0;

  int err = errno;
  close (pipes[0]);
  close (pipes[1]);
  errno = err;
  return -1;
}
#endif
