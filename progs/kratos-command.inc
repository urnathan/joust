// Joust/KRATOS: Kapture Run And Test Output Safely	-*- mode:c++ -*-
// Copyright (C) 2020-2024 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#if !defined (KRATOS_COMMAND)
#define KRATOS_COMMAND
class Command
{
public:
  enum Redirects : char
  {
    R_NORMAL,
    R_FILE,
  };

private:
  std::vector<std::string> Words; // parsed line
  SrcLoc Loc;
  int Stdin = -1;
  pid_t Pid = 0;
  Redirects Redirect = R_NORMAL;

public:
  auto error ()
  { return gaige::Error (Loc); }

public:
  auto const &loc () const 
  { return Loc; }
  void loc (SrcLoc l)
  { Loc = l; }

public:
  auto redirect () const
  { return Redirect; }
  void redirect (Redirects r)
  {
    assert (Redirect == R_NORMAL);
    Redirect = r;
  }

public:
  auto empty () const
  { return Words.empty (); }

public:
  bool execute (int, int, unsigned const *limits = nullptr);
  void stop (int sig)
  {
    if (Pid > 0)
      kill (Pid, sig);
  }

private:
  void shellify (Symbols const &);

  friend std::ostream &operator<< (std::ostream &, Command const &);

  friend class Pipeline;
  friend class Parser;
};

#else

void Command::shellify (Symbols const &syms)
{
  bool active = false;
  size_t len = 0;
  for (auto const &word: Words)
    {
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

      for (auto const &word : Words)
	{
	  if (!concat.empty())
	    concat.push_back (' ');
	  concat += word;
	}

      Words.clear ();
      if (auto sh = syms.value ("SHELL"))
	Words.emplace_back (*sh);
      else
	{
	  char const *shell = getenv ("SHELL");
	  if (!shell || !*shell)
	    shell = "zsh";
	  Words.emplace_back (shell);
	}
      Words.emplace_back ("-c");
      Words.emplace_back (std::move (concat));
    }
}

bool Command::execute (int fd_out, int fd_err, unsigned const *limits)
{
  auto [p, err] = spawn (Stdin, fd_out, fd_err, Words, nullptr, limits);

  Pid = p;
  if (err)
    error () << "failed to spawn '" << Words[0]
	     << "': " << strerror (err);

  return bool (Pid);
}

std::ostream &operator<< (std::ostream &s, Command const &cmd)
{
  for (auto const &word : cmd.Words)
    {
      s << ' ';

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
