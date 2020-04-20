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
	   std::vector<std::string> const &words)
{
  int pipe_fds[2];

  // pipe ends: 0-read from, 1-write to
  if (pipe2 (pipe_fds, O_CLOEXEC) < 0)
    return -errno;

  // Fork it!
  pid_t pid = fork ();

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

      if ((fd_in <= 2 || dup2 (fd_in, 0) >= 0)
	  && (fd_out <= 2 || dup2 (fd_out, 1) >= 0)
	  && (fd_err <= 2 || dup2 (fd_err, 2) >= 0))
	execvp (args[0], const_cast<char **> (args));

      // Something failed.  write errno to pipe;
      write (pipe_fds[1], &errno, sizeof (errno));
      close (pipe_fds[1]);
      exit (2);
      Unreachable ();
    }

  // Parent
  close (pipe_fds[1]);

  decltype(errno) err = errno;
  bool failed = false;
  if (pid < 0)
    failed = true;
  else if (read (pipe_fds[0], &err, sizeof (errno)) == sizeof (errno))
    failed = true;
  close (pipe_fds[0]);

  return failed ? pid_t (-err) : pid;
}

}
