// KRATOS: Kapture Run And Test Output Safely	-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// Not For Distribution

#if !defined (KRATOS_COMMAND)
#define KRATOS_COMMAND
class Command
{
private:
  std::vector<std::string> words;  // parsed line
  char const *file = nullptr;
  unsigned line = 0;
  int stdin = -1;
  pid_t pid = 0;

public:
  Command (char const *f, unsigned l)
    :file (f), line (l)
  {
  }

public:
  auto Error ()
  {
    return Joust::Error (file, line);
  }

public:
  bool Execute (int *fds);

private:
  void Shellify ();

  friend std::ostream &operator<<  (std::ostream &, Command const &);

  friend class Pipeline;
  friend class Parser;
};

#else

void Command::Shellify ()
{
  bool active = false;
  size_t len = 0;
  for (auto iter = words.begin (); iter != words.end (); ++iter)
    {
      std::string const &word = *iter;
      len += word.size () + 1;
      if (!active)
	{
	  constexpr char const *meta = R"(|&;[])";
	  for (auto *ptr = meta; *ptr; ptr++)
	    if (word.find (*ptr) != word.npos)
	      {
		active = true;
		break;
	      }
	}
    }

  if (active)
    {
      // We found a shell-active character.  Make a single string
      std::string concat;
      concat.reserve (len);
      
      for (auto iter = words.begin (); iter != words.end (); ++iter)
	{
	  if (iter != words.begin ())
	    concat.push_back (' ');
	  concat += *iter;
	}

      words.clear ();
      char const *shell = getenv ("SHELL");
      if (!shell || !*shell)
	shell = "zsh";
      words.push_back (shell);
      words.push_back ("-c");
      words.push_back (std::move (concat));
    }
}

bool Command::Execute (int *fds)
{
  pid = fork ();

  // FIXME: Can we abstract this?
  if (!pid)
    {
      // Child
      unsigned nargs = words.size ();
      auto args = reinterpret_cast<char const **> (alloca (sizeof (char const *)
							   * (nargs + 1)));
      for (unsigned ix = 0; ix != nargs; ix++)
	args[ix] = words[ix].c_str ();
      args[nargs] = nullptr;

      if ((stdin > 2 && dup2 (stdin, 0) < 0)
	  || (fds && ((fds[0] > 2 && dup2 (fds[0], 1) < 0)
		      || (fds[1] > 2 && dup2 (fds[1], 2) < 0)))
	  || execvp (args[0], const_cast<char **> (args)))
	{
	  int err = errno;
	  // FIXME: Use a pipe to tell our parent explicitly
	  Error () << "cannot execute '" << words[0]
		   << "': " << strerror (err);
	}
      exit (2);
      return false;
    }

  // Parent

  // Close stdin
  if (stdin > 2)
    {
      close (stdin);
      stdin = -1;
    }

  if (pid == pid_t (-1))
    {
      int err = errno;
      Error () << "failed to spawn process '" << words[0]
		   << "': " << strerror (err);
      return false;
    }

  return true;
}

std::ostream &operator<< (std::ostream &s, Command const &cmd)
{
  for (auto witer = cmd.words.begin (); witer != cmd.words.end (); ++witer)
    {
      if (witer != cmd.words.begin ())
	s << ' ';

      std::string_view const word = *witer;
      bool quoted = false;
      constexpr char const *meta (R"('"`()[]\$ 	*?|;&<>)");
      for (auto *ptr = meta; *ptr; ptr++)
	if (word.find (*ptr) != word.npos)
	  {
	    quoted = true;
	    break;
	  }

      if (quoted)
	{
	  s << '\'';
	  for (auto citer = word.begin (); citer != word.end ();)
	    {
	      auto quote = std::find (citer, word.end (), '\'');
	      s << std::string_view (citer, quote);
	      citer = quote;
	      if (citer != word.end ())
		{
		  s << "'\\''";
		  ++citer;
		}
	    }
	  s << '\'';
	}
      else
	s << word;
    }

  return s;
}

#endif
