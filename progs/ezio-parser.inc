// Joust/EZIO: Expect Zero Irregularities Observed	-*- mode:c++ -*-
// Copyright (C) 2020-2024 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#if !defined (EZIO_PARSER)
#define EZIO_PARSER 1

class Parser : public Scanner
{
  using Parent = Scanner;

private:
  bool matchSpace = false;
  bool matchSol = false;
  bool matchEol = false;
  bool xfail = false;
  Engine &e;

public:
  Parser (char const *file_, Engine &e_)
    : Parent (file_), e (e_)
  {}

public:
  bool GetMatchSpace () const
  { return matchSpace; }
  bool GetMatchSol () const
  { return matchSol; }
  bool GetMatchEol () const
  { return matchEol; }
  bool GetXfail () const
  { return xfail; }

public:
  void Unparsed (char const *ctx, Token const *);
  void Unlexed (Lexer const &lexer, char const *ctx);

protected:
  virtual bool ProcessLine (std::string_view const &variant,
			    std::string_view const &pattern) override;

private:
  void Options (std::string_view const &);
};

#else

void Parser::Unlexed (Lexer const &lexer, char const *ctx)
{
  Error ()
    << "failed to lex " << ctx
    << " after '" << lexer.Before ()
    << "' at '" << lexer.After () << "'";
}

void Parser::Unparsed (char const *ctx, Token const *token)
{
  Error () << "unexpected '" << token << "' parsing " << ctx;
}

void Parser::Options (std::string_view const &text)
{
  Lexer lexer (text);

  while (char c = lexer.Peek ())
    switch (c)
      {
      case ' ':
      case '\t':
      case ',':
	lexer.Advance ();
	break;

      case '!':
	lexer.Advance ();
	lexer.Append (Token (Token::STRING, std::string ("!")));
	break;

      default:
	if (!lexer.Identifier ())
	  {
	    Unlexed (lexer, "options");
	    return;
	  }
	break;
      }

  bool on = true;

  while (Token const *token = lexer.GetToken ())
    switch (token->GetKind ())
      {
      case Token::STRING:
	assert (token->GetString () == "!");
	if (!on)
	  goto unexpected;
	on = false;
	break;

      case Token::IDENTIFIER:
	if (token->GetString () == "matchSpace")
	  matchSpace = on;
	else if (token->GetString () == "matchLine")
	  matchSol = matchEol = on;
	else if (token->GetString () == "matchSol")
	  matchSol = on;
	else if (token->GetString () == "matchEol")
	  matchEol = on;
	else if (token->GetString () == "xfail")
	  xfail = on;
	else
	  goto unexpected;
	on = true;
	break;

      default:
	{
	unexpected:
	  Unparsed ("options", token);
	}
      }
}

bool Parser::ProcessLine (std::string_view const &variant,
			  std::string_view const &pattern) 
{
  if (variant == "OPTION")
    Options (std::move (pattern));
  else if (variant == "END")
    return true;
  else
    {
      unsigned code = 0;
      if (variant.size ())
	{
	  for (; code != Pattern::UNKNOWN; ++code)
	    if (variant == Pattern::kinds[code])
	      goto found;
	  return Parent::ProcessLine (variant, pattern);
	found:;
	}

      auto pat = pattern;
      // Strip leading and trailing whitespace
      while (!pat.empty ())
	{
	  if (pat.front () == ' ' || pat.front () == '\t')
	    pat.remove_prefix (1);
	  else if (pat.back () == ' ' || pat.back () == '\t')
	    pat.remove_suffix (1);
	  else
	    break;
	}
      if (auto *p = Pattern::Parse (this, Pattern::Kind (code), pat))
	e.Add (p);

      xfail = false;
    }

  return false;
}

#endif
