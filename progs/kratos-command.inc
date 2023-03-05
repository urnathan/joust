// Joust/KRATOS: Kapture Run And Test Output Safely	-*- mode:c++ -*-
// Copyright (C) 2020-2023 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#if !defined (KRATOS_COMMAND)
#define KRATOS_COMMAND
class Command
{
public:
  enum Redirect : char
  {
    R_NORMAL,
    R_FILE,
  };

private:
  std::vector<std::string> words; // parsed line
  NMS::SrcLoc loc;
  int stdin = -1;
  pid_t pid = 0;
  Redirect redirect = R_NORMAL;

public:
  auto Error ()
  { return Gaige::Error (loc); }

public:
  auto const &Loc () const 
  { return loc; }
  void Loc (NMS::SrcLoc l)
  { loc = l; }

public:
  auto RedirectType () const
  { return redirect; }
  void RedirectType (Redirect r)
  {
    Assert (redirect == R_NORMAL);
    redirect = r;
  }

public:
  auto Empty () const
  { return words.empty (); }

public:
  bool Execute (int, int, unsigned const *limits = nullptr);
  void Stop (int sig)
  {
    if (pid > 0)
      kill (pid, sig);
  }

private:
  void Shellify (Symbols const &);

  friend std::ostream &operator<< (std::ostream &, Command const &);

  friend class Pipeline;
  friend class Parser;
};

#else

void
Command::Shellify (Symbols const &syms)
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
      if (auto sh = syms.Get ("SHELL"))
	words.emplace_back (*sh);
      else
	{
	  char const *shell = getenv ("SHELL");
	  if (!shell || !*shell)
	    shell = "zsh";
	  words.emplace_back (shell);
	}
      words.emplace_back ("-c");
      words.emplace_back (std::move (concat));
    }
}

bool
Command::Execute (int fd_out, int fd_err, unsigned const *limits)
{
  auto spawn = Spawn (stdin, fd_out, fd_err, words, nullptr, limits);

  pid = std::get<0> (spawn);
  if (int err = std::get<1> (spawn))
    Error () << "failed to spawn '" << words[0]
	     << "': " << strerror (err);

  return std::get<0> (spawn);
}

std::ostream &
operator<< (std::ostream &s, Command const &cmd)
{
  for (auto witer = cmd.words.begin (); witer != cmd.words.end (); ++witer)
    {
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
