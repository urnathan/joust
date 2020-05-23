// KRATOS: Kapture Run And Test Output Safely	-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// Not For Distribution

class Parser : public Scanner
{
  using Parent = Scanner;

private:
  enum State : unsigned char
  {
    IDLE,       // not started
    CONTINUED,  // ended in \.
    PIPED,	// ended in | 
  };

private:
  std::vector<Pipeline> &pipes;
  Symbols &syms;
  State state = IDLE;

public:
  Parser (char const *file_, std::vector<Pipeline> &p, Symbols &s)
    : Parent (file_), pipes (p), syms (s)
  {
  }

public:
  void Unlexed (Lexer const &lexer, char const *ctx);

protected:
  virtual bool ProcessLine (std::string_view const &variant,
			    std::string_view const &pattern) override;

private:
  bool RecursivelyExpand (std::vector<std::string> &, Lexer &,
			  bool braced, std::string &word,
			  std::vector<std::string const *> &stack);
  bool RecursivelyExpand (std::vector<std::string> &,
			  std::string_view const &val, bool braced,
			  std::string &word,
			  std::vector<std::string const *> &stack);
};

void Parser::Unlexed (Lexer const &lexer, char const *ctx)
{
  Error ()
    << "failed to lex " << ctx
    << " after '" << lexer.Before ()
    << "' at '" << lexer.After () << "'";
}

// [<from] [>to] cmd [| cmd [| cmd] ]]
bool Parser::ProcessLine (std::string_view const &variant,
			  std::string_view const &pattern) 
{
  unsigned kind = 0;
  if (variant.size ())
    {
      if (state != IDLE)
	{
	  Error () << "command variant only permitted at start of pipeline"
		   << " '" << variant << '\'';
	  state = IDLE;
	}

      for (; kind != Pipeline::PIPELINE_HWM; ++kind)
	if (variant == Pipeline::kinds[kind])
	  goto found;

      return Parent::ProcessLine (variant, pattern);
    found:;
      if (kind == Pipeline::END)
	return true;
    }

  Lexer lexer (pattern);

  do
    {
      enum Redirecting
	{
	  R_REDIRECTABLE = -1,
	  R_NONE,
	  R_IN,
	  R_OUT
	};

      Redirecting redirecting = R_NONE;

      if (state != CONTINUED)
	{
	  unsigned exit = 0;
	  bool inverted = false;

	  if (lexer.Peek () == '!')
	    {
	      if (state != IDLE)
		Error ()
		  << "status negation only permitted at start of pipeline";
	      inverted = true;
	      lexer.Advance ();
	    }

	  if (kind == Pipeline::RUN
	      || kind == Pipeline::SIGNAL)
	    {
	      if (state == IDLE)
		redirecting = R_REDIRECTABLE;
	      if (lexer.Integer ())
		{
		  if (state != IDLE)
		    Error () << "exit status only permitted at"
		      " start of pipeline";
		  exit = lexer.GetToken ()->GetInteger ();
		}
	      else if (kind == Pipeline::SIGNAL)
		{
		  if (lexer.Identifier ())
		    {
		      static std::tuple<unsigned, char const *> sigmap[]
			= {
#define SIGNAL(NAME) {SIG##NAME, #NAME}
			SIGNAL (INT), SIGNAL (ILL), SIGNAL (ABRT),
			SIGNAL (FPE), SIGNAL (SEGV), SIGNAL (TERM),
			SIGNAL (HUP), SIGNAL (QUIT), SIGNAL (TRAP),
			SIGNAL (KILL), SIGNAL (BUS), SIGNAL (SYS),
			SIGNAL (PIPE), SIGNAL (ALRM),SIGNAL (URG),
			SIGNAL (STOP), SIGNAL (TSTP), SIGNAL (CONT),
			SIGNAL (CHLD), SIGNAL (TTIN), SIGNAL (TTOU),
			SIGNAL (POLL), SIGNAL (XCPU), SIGNAL (XFSZ),
			SIGNAL (VTALRM), SIGNAL (PROF), SIGNAL (USR1),
			SIGNAL (USR2), SIGNAL (WINCH), SIGNAL (IO),
			SIGNAL (IOT), SIGNAL (CLD), {0, nullptr}
#undef SIGNAL
		      };

		      std::string const &signal
			= lexer.GetToken ()->GetString ();

		      for (auto *ptr = sigmap; std::get<1> (*ptr); ptr++)
			if (signal == std::get<1> (*ptr))
			  {
			    exit = std::get<0> (*ptr);
			    break;
			  }
		      if (!exit)
			Error () << "unknown signal '" << signal << '\'';
		    }
		  else
		    // By default we expect to get any signal!
		    inverted = true;
		}
	    }

	  if (state == IDLE)
	    pipes.emplace_back (Pipeline::Kind (kind), inverted, exit);

	  auto &last = pipes.back ();
	  last.commands.emplace_back (GetFile (), GetLine ());
	  if (last.commands.size () + !last.srcsink[1].empty () > 3)
	    Error () << "too many commands in pipeline";
	}

      state = IDLE;
      Command &cmd = pipes.back ().commands.back ();

      std::string word;
      unsigned depth = 0;
      bool escape = false;
      for (;;)
	{
	  char c = lexer.Peek ();

	  if (redirecting < R_NONE)
	    {
	      if (c == '<' || c == '>')
		{
		  redirecting = c == '<' ? R_IN : R_OUT;
		  goto advance;
		}
	      if (c == ' ' || c == '\t')
		goto advance;
	      redirecting = R_NONE;
	    }

	  if (escape && c)
	    {
	      escape = false;
	    append:
	      word.push_back (c);
	    advance:
	      lexer.Advance ();
	    }
	  else if (c == '\\')
	    {
	      escape = true;
	      goto append;
	    }
	  else if (c == '{')
	    {
	      if (word.empty ())
		redirecting = R_NONE;
	      if (depth++)
		goto append;
	      goto advance;
	    }
	  else if (c == '}')
	    {
	      if (!depth)
		Unlexed (lexer, "unexpected '}'");
	      else if (--depth)
		goto append;
	      goto advance;
	    }
	  else if (c == '$')
	    {
	      std::vector<std::string const *> stack;
	      RecursivelyExpand (cmd.words, lexer, depth, word, stack);
	    }
	  else if (!c || c == ' ' || c == '\t')
	    {
	      if (c && depth)
		goto append;
	      
	      if (word.size ())
		{
		  if (redirecting)
		    {
		      Assert (redirecting > R_NONE);

		      auto &srcsink = pipes.back ().srcsink[redirecting - R_IN];

		      if (cmd.words.size ())
			{
			  Error () << "redirection expanded to multiple words";
			  redirecting = R_NONE;
			}
		      else if (!srcsink.empty ())
			{
			  Error () << "redirection already specified";
			  redirecting = R_REDIRECTABLE;
			  goto clear_word;
			}
		      else
			{
			  srcsink = word;
			  redirecting = R_REDIRECTABLE;
			  goto clear_word;
			}
		    }

		
		  if (kind != Pipeline::REQUIRE)
		    {
		      if (!c && word == "\\")
			{
			  state = CONTINUED;
			  break;
			}
		      else if (word == "|")
			{
			  state = PIPED;
			  break;
			}
		      else
			goto append_word;
		    }
		  else
		    {
		    append_word:
		      cmd.words.emplace_back (std::move (word));
		    clear_word:
		      word.clear ();
		    }
		}

	      if (!c)
		break;
	      goto advance;
	    }
	  else
	    goto append;
	}

      if (kind == Pipeline::REQUIRE)
	cmd.Shellify ();

      if (cmd.words.empty ())
	cmd.Error () << "command is empty";

      if (depth)
	Unlexed (lexer, "unexpected end of line");
    }
  while (lexer.Peek ());

  return false;
}

bool Parser::RecursivelyExpand (std::vector<std::string> &words,
				Lexer &lexer, bool quoted, std::string &word,
				std::vector<std::string const *> &stack)
{
  bool ok = true;
  bool braced = lexer.Peek (1) == '{';
  lexer.Advance (1 + braced);
  if (!lexer.Identifier ())
    {
      word.push_back ('$');
      if (!braced)
	return true;

      if (lexer.Peek () == '}')
	{
	  lexer.Advance ();
	  return true;
	}
      else
	{
	  Unlexed (lexer, "expected variable");
	  return false;
	}
    }

  if (braced)
    {
      if (lexer.Peek () == '}')
	lexer.Advance ();
      else
	{
	  Unlexed (lexer, "expected '}'");
	  ok = false;
	}
    }
  std::string const &var = lexer.GetToken ()->GetString ();

  // Not the most efficient recursion detector, but we don't expect
  // this to ever be very deep.
  for (auto iter = stack.begin (); iter != stack.end (); ++iter)
    if (var == **iter)
      {
	Error () << "recursive expansion of '" << var << "'";
	return false;
      }

  stack.push_back (&var);

  if (std::string const *val = syms.Get (var))
    ok = RecursivelyExpand (words, *val, quoted, word, stack);
  else
    {
      Error () << "undefined variable '" << var << "'";
      ok = false;
    }

  stack.pop_back ();

  return ok;
}

bool Parser::RecursivelyExpand (std::vector<std::string> &words,
				std::string_view const &val,
				bool quoted, std::string &word,
				std::vector<std::string const *> &stack)
{
  Lexer lex (val);
  bool escape = false;
  bool ok = true;

  while (char c = lex.Peek ())
    {
      if (escape)
	{
	append:
	  word.push_back (c);
	advance:
	  lex.Advance ();
	}
      else if (c == '\\')
	{
	  escape = true;
	  goto append;
	}
      else if (c == '$')
	{
	  if (!RecursivelyExpand (words, lex, quoted, word, stack))
	    {
	      Error () << "... expanding '" << stack.back () << '\'';
	      ok = false;
	    }
	}
      else if (c == ' ' || c == '\t')
	{
	  if (quoted)
	    goto append;
	  if (word.size ())
	    {
	      words.emplace_back (std::move (word));
	      word.clear ();
	    }
	  goto advance;
	}
      else
	goto append;
    }

  return ok;
}
