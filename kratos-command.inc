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
  auto File () const
  {
    return file;
  }
  auto Line () const
  {
    return line;
  }

public:
  bool Execute (int, int);

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

bool Command::Execute (int fd_out, int fd_err)
{
  pid_t p = Spawn (stdin, fd_out, fd_err, &words, nullptr);

  if (stdin > 2)
    {
      close (stdin);
      stdin = -1;
    }
  if (fd_out > 2)
    close (fd_out);
  if (fd_err > 2)
    close (fd_err);

  if (p < 0)
    {
      Error () << "failed to spawn process '" << words[0]
	       << "': " << strerror (int (-p));
      return false;
    }

  pid = p;
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
