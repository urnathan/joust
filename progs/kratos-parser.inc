// Joust/KRATOS: Kapture Run And Test Output Safely	-*- mode:c++ -*-
// Copyright (C) 2020-2024 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

class Parser : public Scanner
{
  using Parent = Scanner;

private:
  enum States : signed char
  {
    IDLE,		// not started
    REDIRECT_IN,	// read from file
    REDIRECT_HERE,	// here document
    REDIRECT_OUT,	// write stdout to file
    ACTIVE_LWM,		// writing command
  };

private:
  std::vector<Pipeline> &Pipes;
  Symbols &Syms;
  std::string Src;
  States State = IDLE;
  bool IsHereDoc = false;

public:
  Parser (char const *file, std::vector<Pipeline> &p, Symbols &s)
    : Parent (file), Pipes (p), Syms (s)
  {
  }

public:
  void lexError (Lexer const &lexer, char const *ctx);

protected:
  virtual bool processLine (std::string_view const &variant,
			    std::string_view const &pattern) override;

private:
  bool recursivelyExpand (std::vector<std::string> *, Lexer &,
			  bool braced, std::string &word,
			  std::vector<std::string const *> &stack);
  bool recursivelyExpand (std::vector<std::string> *,
			  std::string_view const &val, bool braced,
			  std::string &word,
			  std::vector<std::string const *> &stack);
};

void Parser::lexError (Lexer const &lexer, char const *ctx)
{
  error ()
    << "failed to lex " << ctx
    << " after '" << lexer.before ()
    << "' at '" << lexer.after () << "'";
}

// [<from] [>to] cmd [| cmd [| cmd] ]]
bool Parser::processLine (std::string_view const &variant,
			  std::string_view const &pattern) 
{
  unsigned kind = 0;
  if (variant.size ())
    {
      if (State >= ACTIVE_LWM)
	{
	  error () << "command variant only permitted at start of pipeline"
		   << " '" << variant << '\'';
	  State = IDLE;
	}

      for (; kind != Pipeline::PIPELINE_HWM; ++kind)
	if (variant == Pipeline::KindNames[kind])
	  goto found;

      return Parent::processLine (variant, pattern);
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
      char c = lexer.peekChar ();

      if (word.empty () && !depth && !escape && State != REDIRECT_HERE)
	{
	  // Lex separator
	  States next_state = IDLE;
	  bool inverted = false;
	  int exit = 0;

	  if (c == '<' && State == IDLE)
	    {
	      next_state = REDIRECT_IN;
	      lexer.advanceChar ();
	      if (lexer.peekChar () == '<')
		{
		  next_state = REDIRECT_HERE;
		  lexer.advanceChar ();
		}
	    }
	  else if (c == '>')
	    {
	      next_state = REDIRECT_OUT;
	      lexer.advanceChar ();
	    }
	  else if (c == '|')
	    {
	      next_state = States (ACTIVE_LWM + 1);
	      lexer.advanceChar ();
	      if (lexer.peekChar () == '&')
		{
		  next_state = States (ACTIVE_LWM + 2);
		  lexer.advanceChar ();
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
		  lexer.advanceChar ();
		}

	      if (lexer.isInteger ())
		{
		  exit = lexer.getToken ()->integer ();
		  lexed = true;
		}
	      else if (kind == Pipeline::SIGNAL)
		{
		  if (lexer.isIdentifier ())
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
			= lexer.getToken ()->string ();

		      for (auto *ptr = sigmap; std::get<1> (*ptr); ptr++)
			if (signal == std::get<1> (*ptr))
			  {
			    exit = std::get<0> (*ptr);
			    break;
			  }

		      if (!exit)
			error () << "unknown signal '" << signal << '\'';
		      lexed = true;
		    }
		}
	      if (lexed)
		next_state = ACTIVE_LWM;
	    }

	  bol = false;

	  if (next_state != IDLE)
	    {
	      switch (State)
		{
		case IDLE:
		  break;

		case REDIRECT_IN:
		  if (Src.empty ())
		    error () << "no source redirection specified";
		  break;

		default:
		  assert (State >= ACTIVE_LWM);
		  if (Pipes.back ().Commands[State - ACTIVE_LWM].empty ())
		    error () << "no "
			     << (State == ACTIVE_LWM ? "command" : "filter")
			     << " specified";
		  break;

		  break;
		}

	      int next_cmd_index = -1;

	      switch (next_state)
		{
		case REDIRECT_HERE:
		  if (IsHereDoc)
		    break;
		  IsHereDoc = true;
		  [[fallthrough]];

		case REDIRECT_IN:
		  if (!Src.empty ())
		    error () << "input redirection already specified";
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
		  if (Pipes.empty ())
		    error () << "no command to filter";
		  else if (Pipes.back ().Commands.size () > 1
			   && !Pipes.back ().Commands[next_cmd_index].empty ())
		    {
		      error () << (next_cmd_index == 1 ? "stdout" : "stderr")
			       << "filter already specified";
		      Pipes.back ().Commands[next_cmd_index].Words.clear ();
		    }
		  break;
		}

	      if (next_state == ACTIVE_LWM
		  || (next_cmd_index > 0 && Pipes.empty ()))
		{
		  Pipes.emplace_back (Pipeline::Kinds (kind), inverted, exit);
		  Pipes.back ().src (std::move (Src), IsHereDoc);
		  Src.clear ();
		  IsHereDoc = false;
		}

	      if (next_cmd_index > 0 && Pipes.back ().Commands.size () == 1)
		{
		  // Add empty out & err cmds
		  Pipes.back ().Commands.emplace_back ();
		  Pipes.back ().Commands.emplace_back ();
		}

	      State = next_state;
	      if (next_state == REDIRECT_OUT)
		{
		  State = States (ACTIVE_LWM + 1);
		  Pipes.back ().Commands[1].redirect (Command::R_FILE);
		}
	      continue;
	    }

	  if (c == '\\' && !lexer.peekChar (1) && State != REDIRECT_HERE)
	    {
	      // Continued line
	      lexer.advanceChar ();
	      break;
	    }
	}

      bol = false;

      if (!c || ((c == ' ' || c == '\t')
		 && !depth && !escape
		 && State != REDIRECT_HERE))
	{
	  // end of word
	  if (word.size () || State == REDIRECT_HERE)
	    {
	      if (State == REDIRECT_IN)
		{
		  if (!Src.empty ())
		    error () << "extra after input redirection";
		  Src = std::move (word);
		}
	      else if (State == REDIRECT_HERE)
		{
		  Src.append (std::move (word));
		  Src.push_back ('\n');
		}
	      else
		{
		  assert (State >= ACTIVE_LWM);
		  Pipes.back ().Commands[State - ACTIVE_LWM].Words.emplace_back
		    (std::move (word));
		}
	      word.clear ();
	    }

	  if (!c)
	    {
	      // End of line
	      if (depth
		  || (State == REDIRECT_IN && Src.empty ())
		  || (State >= ACTIVE_LWM
		      && Pipes.back ().Commands[State - ACTIVE_LWM].empty ())
		  || State == IDLE)
		error () << "unexpected end of line";

	      if (State == ACTIVE_LWM)
		{
		  auto &last = Pipes.back ();

		  if (last.kind () == Pipeline::REQUIRE)
		    {
		      if (!last.Src.empty ())
			{
			  error () << "command cannot have redirected input";
			  last.Src.clear ();
			}

		      assert (last.Commands.size () == 1);
		      last.Commands[0].shellify (Syms);
		      last.Commands.emplace_back ();
		      last.Commands.emplace_back ();
		    }
		}
	      State = IDLE;

	      break;
	    }
	  goto advance;
	}

      if (State == IDLE)
	{
	  // Commit to a state.
	  Pipes.emplace_back (Pipeline::Kinds (kind), false, 0);
	  Pipes.back ().src (std::move (Src), IsHereDoc);
	  Src.clear ();
	  IsHereDoc = false;
	  State = ACTIVE_LWM;
	}

      if (State >= ACTIVE_LWM
	  && word.empty ()
	  && Pipes.back ().Commands[State - ACTIVE_LWM].empty ())
	Pipes.back ().Commands[State - ACTIVE_LWM].loc (loc ());
      
      if (escape)
	{
	  escape = false;
	append:
	  word.push_back (c);
	advance:
	  lexer.advanceChar ();
	}
      else if (c == '\\')
	{
	  if (!depth && State == REDIRECT_HERE)
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
	    lexError (lexer, "unexpected '}'");
	  else if (--depth)
	    goto append;
	  goto advance;
	}
      else if (c == '$')
	{
	  std::vector<std::string const *> stack;
	  std::vector<std::string> *accum = nullptr;
	  if (State >= ACTIVE_LWM)
	    {
	      auto &cmd = Pipes.back ().Commands[State - ACTIVE_LWM];
	      if (cmd.redirect () != Command::R_FILE)
		accum = &cmd.Words;
	    }
	  recursivelyExpand (accum, lexer, depth, word, stack);
	}
      else
	goto append;
    }

  return false;
}

bool Parser::recursivelyExpand (std::vector<std::string> *words, Lexer &lexer,
				bool quoted, std::string &word,
				std::vector<std::string const *> &stack)
{
  bool ok = true;
  bool braced = lexer.peekChar (1) == '{';
  lexer.advanceChar (1 + braced);
  if (!lexer.isIdentifier ())
    {
      word.push_back ('$');
      if (!braced)
	return true;

      if (lexer.peekChar () == '}')
	{
	  lexer.advanceChar ();
	  return true;
	}
      else
	{
	  lexError (lexer, "expected variable");
	  return false;
	}
    }

  if (braced)
    {
      if (lexer.peekChar () == '}')
	lexer.advanceChar ();
      else
	{
	  lexError (lexer, "expected '}'");
	  ok = false;
	}
    }
  std::string const &var = lexer.getToken ()->string ();

  // Not the most efficient recursion detector, but we don't expect
  // this to ever be very deep.
  for (auto iter = stack.begin (); iter != stack.end (); ++iter)
    if (var == **iter)
      {
	error () << "recursive expansion of '" << var << "'";
	return false;
      }

  stack.push_back (&var);

  if (std::string const *val = Syms.value (var))
    ok = recursivelyExpand (words, *val, quoted, word, stack);
  else
    {
      error () << "undefined variable '" << var << "'";
      ok = false;
    }

  stack.pop_back ();

  return ok;
}

bool Parser::recursivelyExpand (std::vector<std::string> *words,
				std::string_view const &val,
				bool quoted, std::string &word,
				std::vector<std::string const *> &stack)
{
  Lexer lex (val);
  bool escape = false;
  bool ok = true;

  while (char c = lex.peekChar ())
    {
      if (escape)
	{
	append:
	  word.push_back (c);
	advance:
	  lex.advanceChar ();
	}
      else if (c == '\\')
	{
	  escape = true;
	  goto append;
	}
      else if (c == '$')
	{
	  if (!recursivelyExpand (words, lex, quoted, word, stack))
	    {
	      error () << "... expanding '" << stack.back () << '\'';
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
