// EZIO: Expect Zero Irregularities Observed	-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// Not For Distribution

#if !defined (EZIO_PARSER)
#define EZIO_PARSER 1

class Parser : public Scanner
{
  using Parent = Scanner;

private:
  bool matchSpace = false;
  bool matchLine = false;
  bool xfail = false;
  Engine &e;

public:
  Parser (char const *file_, Engine &e_)
    : Parent (file_), e (e_)
  {
  }

public:
  bool GetMatchSpace () const
  {
    return matchSpace;
  }
  bool GetMatchLine () const
  {
    return matchLine;
  }
  bool GetXfail () const
  {
    return xfail;
  }

public:
  void Unparsed (char const *ctx, Token const *);
  void Unlexed (Lexer const &lexer, char const *ctx);
  void completed ();

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
	Assert (token->GetString () == "!");
	if (!on)
	  goto unexpected;
	on = false;
	break;

      case Token::IDENTIFIER:
	if (token->GetString () == "matchspace")
	  matchSpace = on;
	else if (token->GetString () == "matchline")
	  matchLine = on;
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
      
      if (auto *p
	  = Pattern::Parse (this, Pattern::Kind (code), pattern))
	e.Add (p);

      xfail = false;
    }

  return false;
}

#endif
