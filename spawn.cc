// Joust			-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// Not For Distribution

// Joust
#include "spawn.hh"
// NMS
#include "fatal.hh"
// OS
#include <fcntl.h>
#include <unistd.h>

using namespace NMS;

namespace Joust {

int Spawn (int fd_in, int fd_out, int fd_err,
	   std::vector<std::string> const &words, int &err)
{
  pid_t pid = 0;
  int pipe_fds[2];

  // pipe ends: 0-read from, 1-write to
  if (pipe2 (pipe_fds, O_CLOEXEC) < 0)
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
	  unsigned nargs = words.size ();

	  // Assemble them
	  auto args = reinterpret_cast<char const **>
	    (alloca (sizeof (char const *) * (nargs + 1)));
	  unsigned ix = 0;
	  for (auto iter = words.begin (); iter != words.end (); ++iter)
	    args[ix++] = iter->c_str ();
	  args[ix] = nullptr;

	  if ((fd_in == 0 || dup2 (fd_in, 0) >= 0)
	      && (fd_out == 1 || dup2 (fd_out, 1) >= 0)
	      && (fd_err == 2 || dup2 (fd_err, 2) >= 0))
	    execvp (args[0], const_cast<char **> (args));

	  // Something failed.  write errno to pipe;
	  write (pipe_fds[1], &errno, sizeof (errno));
	  close (pipe_fds[1]);
	  exit (0);
	  Unreachable ();
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

  return pid;
}

}
