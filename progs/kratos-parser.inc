// Joust/KRATOS: Kapture Run And Test Output Safely	-*- mode:c++ -*-
// Copyright (C) 2020-2024 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

class Parser : public Scanner
{
  using Parent = Scanner;

private:
  enum State : signed char
  {
    IDLE,		// not started
    REDIRECT_IN,	// read from file
    REDIRECT_HERE,	// here document
    REDIRECT_OUT,	// write stdout to file
    ACTIVE_LWM,		// writing command
  };

private:
  std::vector<Pipeline> &pipes;
  Symbols &syms;
  std::string src;
  State state = IDLE;
  bool here = false;

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
  bool RecursivelyExpand (std::vector<std::string> *, Lexer &,
			  bool braced, std::string &word,
			  std::vector<std::string const *> &stack);
  bool RecursivelyExpand (std::vector<std::string> *,
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
      if (state >= ACTIVE_LWM)
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

  std::string word;
  unsigned depth = 0;
  bool bol = true;
  bool escape = false;
  for (;;)
    {
      char c = lexer.Peek ();

      if (word.empty () && !depth && !escape && state != REDIRECT_HERE)
	{
	  // Lex separator
	  State next_state = IDLE;
	  bool inverted = false;
	  int exit = 0;

	  if (c == '<' && state == IDLE)
	    {
	      next_state = REDIRECT_IN;
	      lexer.Advance ();
	      if (lexer.Peek () == '<')
		{
		  next_state = REDIRECT_HERE;
		  lexer.Advance ();
		}
	    }
	  else if (c == '>')
	    {
	      next_state = REDIRECT_OUT;
	      lexer.Advance ();
	    }
	  else if (c == '|')
	    {
	      next_state = State (ACTIVE_LWM + 1);
	      lexer.Advance ();
	      if (lexer.Peek () == '&')
		{
		  next_state = State (ACTIVE_LWM + 2);
		  lexer.Advance ();
		}
	    }
	  else if (bol)
	    {
	      // Look for !exit-code
	      bool lexed = false;
	      if (c == '!')
		{
		  inverted = true;
		  lexed = true;
		  lexer.Advance ();
		}

	      if (lexer.Integer ())
		{
		  exit = lexer.GetToken ()->GetInteger ();
		  lexed = true;
		}
	      else if (kind == Pipeline::SIGNAL)
		{
		  if (lexer.Identifier ())
		    {
		      static std::tuple<unsigned, char const *> sigmap[]
#define SIGNAL(NAME) {SIG##NAME, #NAME}
			= {SIGNAL (INT), SIGNAL (ILL), SIGNAL (ABRT),
			   SIGNAL (FPE), SIGNAL (SEGV), SIGNAL (TERM),
			   SIGNAL (HUP), SIGNAL (QUIT), SIGNAL (TRAP),
			   SIGNAL (KILL), SIGNAL (BUS), SIGNAL (SYS),
			   SIGNAL (PIPE), SIGNAL (ALRM),SIGNAL (URG),
			   SIGNAL (STOP), SIGNAL (TSTP), SIGNAL (CONT),
			   SIGNAL (CHLD), SIGNAL (TTIN), SIGNAL (TTOU),
			   SIGNAL (XCPU), SIGNAL (XFSZ), SIGNAL (VTALRM),
			   SIGNAL (PROF), SIGNAL (USR1), SIGNAL (USR2),
			   SIGNAL (WINCH), SIGNAL (IO), SIGNAL (IOT),
			   {0, nullptr}};
#undef SIGNAL

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
		      lexed = true;
		    }
		}
	      if (lexed)
		next_state = ACTIVE_LWM;
	    }

	  bol = false;

	  if (next_state != IDLE)
	    {
	      switch (state)
		{
		case IDLE:
		  break;

		case REDIRECT_IN:
		  if (src.empty ())
		    Error () << "no source redirection specified";
		  break;

		default:
		  assert (state >= ACTIVE_LWM);
		  if (pipes.back ().commands[state - ACTIVE_LWM].Empty ())
		    Error () << "no "
			     << (state == ACTIVE_LWM ? "command" : "filter")
			     << " specified";
		  break;

		  break;
		}

	      int next_cmd_index = -1;

	      switch (next_state)
		{
		case REDIRECT_HERE:
		  if (here)
		    break;
		  here = true;
		  [[fallthrough]];

		case REDIRECT_IN:
		  if (!src.empty ())
		    Error () << "input redirection already specified";
		  break;

		case ACTIVE_LWM:
		  next_cmd_index = 0;
		  break;

		default:
		  assert (next_state > ACTIVE_LWM);
		  [[fallthrough]];
		case REDIRECT_OUT:
		  next_cmd_index = (next_state == REDIRECT_OUT
				    ? 1 : next_state - ACTIVE_LWM);
		  if (pipes.empty ())
		    Error () << "no command to filter";
		  else if (pipes.back ().commands.size () > 1
			   && !pipes.back ().commands[next_cmd_index].Empty ())
		    {
		      Error () << (next_cmd_index == 1 ? "stdout" : "stderr")
			       << "filter already specified";
		      pipes.back ().commands[next_cmd_index].words.clear ();
		    }
		  break;
		}

	      if (next_state == ACTIVE_LWM
		  || (next_cmd_index > 0 && pipes.empty ()))
		{
		  pipes.emplace_back (Pipeline::Kind (kind), inverted, exit);
		  auto &last = pipes.back ();
		  last.SetIn (std::move (src), here);
		  src.clear ();
		  here = false;
		}

	      if (next_cmd_index > 0 && pipes.back ().commands.size () == 1)
		{
		  // Add empty out & err cmds
		  pipes.back ().commands.emplace_back ();
		  pipes.back ().commands.emplace_back ();
		}

	      state = next_state;
	      if (next_state == REDIRECT_OUT)
		{
		  state = State (ACTIVE_LWM + 1);
		  pipes.back ().commands[1].RedirectType (Command::R_FILE);
		}
	      continue;
	    }

	  if (c == '\\' && !lexer.Peek (1) && state != REDIRECT_HERE)
	    {
	      // Continued line
	      lexer.Advance ();
	      break;
	    }
	}

      bol = false;

      if (!c || ((c == ' ' || c == '\t')
		 && !depth && !escape
		 && state != REDIRECT_HERE))
	{
	  // end of word
	  if (word.size () || state == REDIRECT_HERE)
	    {
	      if (state == REDIRECT_IN)
		{
		  if (!src.empty ())
		    Error () << "extra after input redirection";
		  src = std::move (word);
		}
	      else if (state == REDIRECT_HERE)
		{
		  src.append (std::move (word));
		  src.push_back ('\n');
		}
	      else
		{
		  assert (state >= ACTIVE_LWM);
		  pipes.back ().commands[state - ACTIVE_LWM].words.emplace_back
		    (std::move (word));
		}
	      word.clear ();
	    }

	  if (!c)
	    {
	      // End of line
	      if (depth
		  || (state == REDIRECT_IN && src.empty ())
		  || (state >= ACTIVE_LWM
		      && pipes.back ().commands[state - ACTIVE_LWM].Empty ())
		  || state == IDLE)
		Error () << "unexpected end of line";

	      if (state == ACTIVE_LWM)
		{
		  auto &last = pipes.back ();

		  if (last.GetKind () == Pipeline::REQUIRE)
		    {
		      if (!last.src.empty ())
			{
			  Error () << "command cannot have redirected input";
			  last.src.clear ();
			}

		      assert (last.commands.size () == 1);
		      last.commands[0].Shellify (syms);
		      last.commands.emplace_back ();
		      last.commands.emplace_back ();
		    }
		}
	      state = IDLE;

	      break;
	    }
	  goto advance;
	}

      if (state == IDLE)
	{
	  // Commit to a state.
	  pipes.emplace_back (Pipeline::Kind (kind), false, 0);
	  auto &last = pipes.back ();
	  last.SetIn (std::move (src), here);
	  src.clear ();
	  here = false;
	  state = ACTIVE_LWM;
	}

      if (state >= ACTIVE_LWM
	  && word.empty ()
	  && pipes.back ().commands[state - ACTIVE_LWM].Empty ())
	pipes.back ().commands[state - ACTIVE_LWM].Loc (Loc ());
      
      if (escape)
	{
	  escape = false;
	append:
	  word.push_back (c);
	advance:
	  lexer.Advance ();
	}
      else if (c == '\\')
	{
	  if (!depth && state == REDIRECT_HERE)
	    goto append;
	  escape = true;
	  goto advance;
	}
      else if (c == '{')
	{
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
	  std::vector<std::string> *accum = nullptr;
	  if (state >= ACTIVE_LWM)
	    {
	      auto &cmd = pipes.back ().commands[state - ACTIVE_LWM];
	      if (cmd.RedirectType () != Command::R_FILE)
		accum = &cmd.words;
	    }
	  RecursivelyExpand (accum, lexer, depth, word, stack);
	}
      else
	goto append;
    }

  return false;
}

bool Parser::RecursivelyExpand (std::vector<std::string> *words, Lexer &lexer,
				bool quoted, std::string &word,
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

bool Parser::RecursivelyExpand (std::vector<std::string> *words,
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
	  if (quoted || !words)
	    goto append;
	  if (word.size ())
	    {
	      words->emplace_back (std::move (word));
	      word.clear ();
	    }
	  goto advance;
	}
      else
	goto append;
    }

  return ok;
}
