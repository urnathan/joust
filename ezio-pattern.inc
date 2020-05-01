// EZIO: Expect Zero Irregularities Observed	-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// Not For Distribution

class Pattern
{
  class Lexer : public Joust::Lexer
  {
  public:
    using Parent = Joust::Lexer;

  public:
    using Parent::Parent;

  public:
    bool Option ();
    bool Pattern ();

  protected:
    bool PatternRegexp ();
    bool PatternExpr ();
  };
  
public:
#define PATTERN_KINDS							\
  PATTERN_KIND_FROB(MATCH), PATTERN_KIND_FROB(NEXT),			\
    PATTERN_KIND_FROB(BLOCK),						\
    PATTERN_KIND_FROB(NOT),						\
    PATTERN_KIND_FROB(NEVER), PATTERN_KIND_FROB(NONE),			\
    PATTERN_KIND_FROB(UNKNOWN)
#define PATTERN_KIND_FROB(KIND) KIND
  enum Kind : unsigned char
  {
    PATTERN_KINDS,
    PATTERN_HWM,
    CAPTURE_HWM = NOT,
  };
#undef PATTERN_KIND_FROB

public:
  static constexpr char const *kinds[PATTERN_HWM]
#define PATTERN_KIND_FROB(KIND) #KIND
    = { PATTERN_KINDS };
#undef PATTERN_KIND_FROB

private:
  char const *file;
  unsigned line;

private:
  Kind kind;

private:
  bool matchSpace : 1 = false;
  bool matchSol : 1 = false;
  bool matchEol : 1 = false;
  bool is_eof : 1 = false;
  bool is_xfail : 1 = false;
  bool has_error : 1 = false;
  bool has_matched : 1 = false;

private:
  std::string original;
  std::string expansion;
  std::regex regex;

private:
  std::vector<Token> atoms;

protected:
  Pattern (Kind kind_, Parser const *parser, std::string_view const &text)
    : file (parser->GetFile ()), line (parser->GetLine ()),
      kind (kind_), original (text)
  {
    if (parser->GetMatchSol ())
      matchSol = true;
    if (parser->GetMatchEol ())
      matchEol = true;
    if (parser->GetMatchSpace ())
      matchSpace = true;
    if (parser->GetXfail ())
      is_xfail = true;
  }

public:
  ~Pattern () = default;

public:
  Kind GetKind () const
  {
    return kind;
  }

public:
  bool Errored () const
  {
    return has_error;
  }
  bool Matched () const
  {
    return has_matched;
  }

private:
  void Append (Token &&a)
  {
    atoms.push_back (std::move (a));
  }
  unsigned Size () const
  {
    return atoms.size ();
  }

public:
  Joust::Error Error ();

public:
  bool Canonicalize ();

public:
  Regex::Result Expand (Symtab const &);
  Regex::Result Compare (Symtab &, std::string_view const &text, bool eof);

private:
  Regex::Result DoExpand (Symtab const &);

public:
  static Pattern *Parse (Parser *, Kind kind, std::string_view const &);

private:
  bool ParseEscape (Parser *, Lexer &);

private:
  void Result (Logger &, Logger::Status) const;

public:
  void Result (Logger &) const;
  void Matched (Logger &) const;

  friend std::ostream &operator<< (std::ostream &s, Pattern const &p);
};

// PATTERN_REGEXP is
//   white-space: [ \t]+
//   close-braces: }} -> return
//   variable: $ _? :alpha: :alnum:*
//   variable: $ { _? :alpha: :alnum:* }
//   string: $$
//   string: .+
// Final TOKEN_CLOSE has capture count 
bool Pattern::Lexer::PatternRegexp ()
{
  bool escape = false;
  unsigned square = 0;
  bool brace = false;
  int captures = 0;

  while (char c = Peek ())
    switch (c)
      {
      case '[':
	if (!escape && !square && !brace)
	  square = c_ix;
	goto append;

      case ']':
	if (c_ix != square + 1)
	  square = 0;
	goto append;

      case '(':
	if (!escape && !square)
	  captures++;
	goto append;

      case '{':
	if (!escape && !square)
	  brace = true;
	goto append;

      case '}':
	if (brace)
	  brace = false;
	else if (!escape)
	  {
	    Advance ();
	    Append (Token (Token::CLOSE));
	    tokens.back ().SetUser (captures);
	    return true;
	  }
	goto append;

      case '\\':
	if (!escape && !square)
	  {
	    escape = true;
	    goto escape;
	  }
	goto append;

      case '-':
	if (c_ix == square + 1)
	  square++;
	goto append;

      case '$':
	if (escape || square)
	  goto append;
	{
	  char peek = Peek (1);
	  bool braced = peek == '{';
	  if (!(braced || std::isalpha (peek)))
	    // not ${ or $[:alpha:]
	    goto append;

	  Advance (1 + braced);
	  bool ident = Identifier ();
	  if (braced)
	    {
	      if (Peek () != '}')
		return false;
	      if (!ident)
		// ${} emit the $ eat the {}
		goto append;
	      Advance ();
	    }
	  else
	    Assert (ident);
	}
	break;

      case ' ':
      case '\t':
	// Whitespace in an regexp is literal
	[[fallthrough]];

      default:
      append:
	escape = false;
      escape:
	Advance ();
	Append (Token::STRING, c);
	break;
      }

  return false;
}

// PATTERN_EXPR is
//   white-space: [ \t]:
//   close-braces: } -> PATTERN
//   variable: :alpha: :alnum:*
//   integer: :digit: :alnum:*
//   string: .*
bool Pattern::Lexer::PatternExpr ()
{
  while (char c = Peek ())
    switch (c)
      {
      case ' ':
      case '\t':
	// Ignore whitespace in the expression
	Advance ();
	break;

      case '}':
	Advance ();
	Append (Token (Token::CLOSE));
	return true;

      case '0'...'9':
	if (!Integer ())
	  return false;
	break;

      default:
	if (Identifier ())
	  break;

	Advance ();
	Append (Token::STRING, c);
	break;
      }

  return false;
}

// A somewhat contextual lexer
// ATTRIBUTE is
//  ...
// OPTION is
//   white-space: [ \t]+
//  ...
// PATTERN is
//   white-space: [ \t]+
//   open-braces: {{ -> PATTERN_FIRST
//   variable: $ _? :alpha: :alnum:*
//   variable: $ { _? :alpha: :alnum:* }
//   string: $$
//   string: .+

bool Pattern::Lexer::Pattern ()
{
  while (char c = Peek ())
    switch (c)
      {
      case ' ':
      case '\t':
	Advance ();
	Append (Token::WHITESPACE, c);
	break;

      case '{':
	{
	  char peek = Peek (1);

	  bool ident = std::isalpha (peek);
	  if (!(ident || peek == ':' || peek == '%'))
	    goto append;

	  Advance ();
	  Append (Token (Token::OPEN));

	  if (ident)
	    {
	      if (!Identifier ())
		return false;
	      peek = Peek ();
	    }

	  if (peek == ':')
	    {
	      Advance ();
	      Append (Token (Token::COLON));
	      if (!PatternRegexp ())
		return false;
	    }
	  else if (peek == '%')
	    {
	      unsigned begin = c_ix;
	      for (;;)
		{
		  peek = AdvancePeek ();
		  if (std::isalpha (peek))
		    {
		      Advance ();
		      break;
		    }
		  else if (peek == '}' || peek == ':')
		    break;
		}
	      Append (Token (Token::STRING, string.substr (begin,
							   c_ix - begin)));
	      if (peek == ':')
		{
		  Advance ();
		  Append (Token (Token::COLON));
		  if (!PatternExpr ())
		    return false;
		}
	      else
		return false;
	    }
	  else
	    return false;
	}
	break;

      case '$':
	{
	  char peek = Peek (1);

	  bool braced = peek == '{';
	  if (!(braced || std::isalpha (peek)))
	    // not ${ or $[:alpha:]
	    goto append;

	  Advance (1 + braced);
	  bool ident = Identifier ();
	  if (braced)
	    {
	      if (Peek () != '}')
		return false;
	      if (!ident)
		// ${} emit the $ eat the {}
		goto append;
	      Advance ();
	    }
	  else
	    Assert (ident);
	}
	break;

      default:
      append:
	Advance ();
	Append (Token::STRING, c);
	break;
      }

  return true;
}

Error Pattern::Error ()
{
  has_error = true;
  return Joust::Error (file, line);
}

std::ostream &operator<< (std::ostream &s, Pattern const &p)
{
  return s << p.file << ':' << p.line << ":"
	   << p.kinds[p.GetKind ()] << ", " << p.atoms << '\n';
}

// match-escape: IDENTIFIER-opt COLON regexp-expand CLOSE
//             | IDENTIFIER STRING="%.*" CLOSE
//             |            STRING="%.*" COLON expr-expand CLOSE
// regexp-expand: regexp-expand-opt STRING
//              | regexp-expand-opt IDENTIFIER
// expr-expand: expr-primary
//            | expr-expand + expr-primary
//            | expr-expand - expr-primary
// expr-primary: IDENTIFIER | INTEGER

bool Pattern::ParseEscape (Parser *parser, Lexer &lexer)
{
  Token ident;

  Token *next = lexer.GetToken ();
  bool have_ident = next && next->GetKind () == Token::IDENTIFIER;
  if (have_ident)
    {
      if (GetKind () >= CAPTURE_HWM)
	Error () << "capture not permitted in " << kinds[GetKind ()];
      ident = std::move (*next);
      next = lexer.GetToken ();
    }

  if (!next)
    {
    failed:
      parser->Unparsed ("pattern {{escape}}", next);
      return false;
    }

  unsigned format = 0;
  if (next->GetKind () == Token::STRING
      && next->GetString ()[0] == '%')
    {
      auto fmt = next->GetString ();
      format = fmt.back ();
      if (fmt.size () != 2 || !(format == 'u'))
	{
	  Error () << "unrecognized value capture '" << fmt << '\'';
	  format = 'u';
	}
      next = lexer.GetToken ();
      if (!next)
	goto failed;
    }

  Token escape (format ? Token::VALUE_CAPTURE : Token::REGEX_CAPTURE);

  if (!format || have_ident)
    escape.GetCapture ().push_back (std::move (ident));
  if (format)
    escape.GetCapture ().push_back (Token (Token::FORMAT, format));

  if (!(format && have_ident))
    {
      bool op = false;
      bool first = true;

      for (;; first = false, next = lexer.GetToken ())
	{
	  if (!next)
	    goto failed;
	  if (first && next->GetKind () == Token::COLON)
	    ;
	  else if (first && (format || have_ident))
	    goto failed;
	  else if (op)
	    {
	      if (next->GetKind () != Token::STRING)
		break;
	      auto op_string = next->GetString ();
	      if (op_string.size () != 1)
		break;
	      else if (op_string[0] == '+')
		;
	      else if (op_string[0] == '-')
		escape.GetCapture ().push_back (Token (Token::MINUS));
	      else 
		break;
	      op = false;
	    }
	  else
	    {
	      if (format
		  && next->GetKind () == Token::IDENTIFIER
		  && next->GetString () == "LINE")
		escape.GetCapture ().push_back (Token (Token::INTEGER, line));
	      else if (next->GetKind () == Token::IDENTIFIER
		       || (format ? next->GetKind () == Token::INTEGER
			   : (next->GetKind () == Token::STRING
			      || next->GetKind () == Token::WHITESPACE)))
		escape.GetCapture ().push_back (std::move (*next));
	      else if (format)
		goto failed;
	      else
		break;
	      if (format)
		op = true;
	    }
	}
    }

  if (next->GetKind () != Token::CLOSE)
    goto failed;

  escape.SetUser (next->GetUser ());
  Append (std::move (escape));

  return true;
}

// Parse the match. Its grammar isL
// match: match-opt match-word
// match-word: STRING | IDENTIFIER | OPEN match-escape

Pattern *Pattern::Parse (Parser *parser, Kind kind, std::string_view const &line)
{
  auto p = new Pattern (kind, parser, line);
  if (kind == UNKNOWN)
    {
    failed:
      p->has_error = true;
      return p;
    }

  Lexer lexer (line);
  if (!lexer.Pattern ())
    {
      parser->Unlexed (lexer, "pattern");
      goto failed;
    }

  while (Token *tok = lexer.GetToken ())
    switch (tok->GetKind ())
      {
      case Token::WHITESPACE:
      case Token::IDENTIFIER:
      case Token::STRING:
	p->Append (std::move (*tok));
	break;

      case Token::OPEN:
	if (!p->ParseEscape (parser, lexer))
	  goto failed;
	break;
	    
      default:
	parser->Unparsed ("pattern", tok);
	goto failed;
      }

  return p;
}

// Canonicalize the pattern:
// Remove leading & trailing whitespace
// Regexify whitespace if !whiteSpace
// Regexify strings
// Promote fixed regexes to strings
// Concatenate adjacent strings & whitespace
// Prepend ^ and postpend $, if needed
bool Pattern::Canonicalize ()
{
  std::vector<Token> canon;
  canon.reserve (atoms.size ());
  int eof = 0;

  std::string acc; // Current accumulation

  if (matchSol)
    acc.push_back ('^');

  bool eol = matchEol;
  for (auto token = atoms.begin (); token != atoms.end (); ++token)
    {
      switch (token->GetKind ())
	{
	case Token::WHITESPACE:
	  if (token == atoms.begin () || token + 1 == atoms.end ())
	    // drop leading and trailing whitespace
	    continue;

	  if (matchSpace)
	    // Spacing mush match exactly
	    acc.append (std::move (token->GetString ()));
	  else
	    // Any horizontal whitespace
	    acc.append ("[ \t]+");
	  break;

	case Token::STRING:
	  {
	    std::string_view string = token->GetString ();
	    // SOL can also be matched with explicit ^
	    if (canon.empty () && acc.size () == matchSol
		&& string.front () == '^')
	      {
		string.remove_prefix (1);
		if (acc.empty ())
		  acc.push_back ('^');
	      }
	    // EOL can also match be matched with explicit $
	    if (token + 1 == atoms.end () && string.back () == '$')
	      {
		eol = true;
		string.remove_suffix (1);
	      }
	    std::string escaped = Regex::Protect (string);
	    acc.append (std::move (escaped));
	    eof = -1;
	  }
	  break;

	case Token::IDENTIFIER:
	  if (acc.size ())
	    {
	      canon.push_back (Token (Token::STRING, std::move (acc)));
	      acc.clear ();
	    }
	  if (!eof && token->GetString () == "EOF")
	    eof = 1;
	  else
	    eof = -1;
	  canon.push_back (std::move (*token));
	  break;

	case Token::REGEX_CAPTURE:
	  {
	    std::vector<Token> &capture = token->GetCapture ();
	    
	    if (capture.size () == 1)
	      {
		Error () << "capture captures nothing";
		return false;
	      }

	    if (capture.size () == 2
		&& capture[0].GetKind () == Token::EMPTY
		&& capture[1].GetKind () == Token::STRING
		&& !token->GetUser ())
	      // A null capture of a single non-capturing string can
	      // just be a string
	      acc.append (std::move (capture[1].GetString ()));
	    else
	      {
		if (acc.size ())
		  {
		    canon.push_back (Token (Token::STRING, std::move (acc)));
		    acc.clear ();
		  }
		canon.push_back (std::move (*token));
	      }
	    eof = -1;
	  }
	  break;

	case Token::VALUE_CAPTURE:
	  {
	    if (acc.size ())
	      {
		canon.push_back (Token (Token::STRING, std::move (acc)));
		acc.clear ();
	      }
	    canon.push_back (std::move (*token));
	    eof = -1;
	  }
	  break;

	default:
	  Unreachable ();
	}
    }

  if (eol)
    acc.push_back ('$');

  if (acc.size ())
    canon.push_back (Token (Token::STRING, std::move (acc)));

  if (!canon.size () && kind < CAPTURE_HWM)
    {
      Error () << kinds[kind] << " pattern is empty";
      return false;
    }

  if (eof == 1)
    {
      canon.clear ();
      is_eof = true;
    }

  atoms = std::move (canon);

  return true;
}

Regex::Result Pattern::DoExpand (Symtab const &symtab)
{
  for (auto token = atoms.begin (); token != atoms.end (); ++token)
    switch (token->GetKind ())
      {
      case Token::STRING:
	expansion.append (token->GetString ());
	break;

      case Token::IDENTIFIER:
	if (auto *val = symtab.text.Get (token->GetString ()))
	  Regex::Protect (expansion, *val);
	else
	  return Regex::NOTFOUND;
	break;

      case Token::REGEX_CAPTURE:
	{
	  std::vector<Token> const &capture = token->GetCapture ();
	  bool is_capture = capture[0].GetKind () == Token::IDENTIFIER;

	  if (is_capture)
	    expansion.push_back ('(');
	  for (auto cap = capture.begin () + 1; cap != capture.end (); ++cap)
	    switch (cap->GetKind ())
	      {
	      case Token::STRING:
		expansion.append (cap->GetString ());
		break;

	      case Token::IDENTIFIER:
		if (auto *val = symtab.text.Get (cap->GetString ()))
		  Regex::Protect (expansion, *val);
		else
		  return Regex::NOTFOUND;
		break;

	      default:
		Unreachable ();
	      }
	  if (is_capture)
	    expansion.push_back (')');
	}
	break;
	  
      case Token::VALUE_CAPTURE:
	{
	  std::vector<Token> const &capture = token->GetCapture ();
	  if (capture[0].GetKind () == Token::IDENTIFIER)
	    {
	      unsigned long fmt = capture[1].GetInteger ();
	      char const *format = nullptr;
	      switch (fmt & 255)
		{
		case 'u':
		  format = "[0-9]+";
		  break;

		default:
		  Unreachable ();
		}
	      expansion.push_back ('(');
	      expansion.append (std::string (format));
	      expansion.push_back (')');
	    }
	  else
	    {
	      unsigned long val = 0, op;
	      bool sub = false;
	      for (auto cap = capture.begin () + 1;
		   cap != capture.end (); ++cap)
		switch (token->GetKind ())
		  {
		  case Token::MINUS:
		    sub = true;
		    break;

		  case Token::IDENTIFIER:
		    if (auto *v = symtab.ints.Get (cap->GetString ()))
		      op = *v;
		    else
		      return Regex::NOTFOUND;
		    goto acc;

		  case Token::INTEGER:
		    op = cap->GetInteger ();
		  acc:
		    if (sub)
		      val -= op;
		    else
		      val += op;
		    sub = false;
		    break;

		  default:
		    Unreachable ();
		  }

	      unsigned long fmt = capture[0].GetInteger ();
	      switch (fmt & 255)
		{
		case 'u':
		  expansion.append (std::to_string (val));
		  break;

		default:
		  Unreachable ();
		}
	    }
	}
	break;

      default:
	Unreachable ();
      }

  return Regex::FOUND;
}

// Expand tokens to expansion
Regex::Result Pattern::Expand (Symtab const &symtab)
{
  if (Errored ())
    return Regex::FAILED;

  if (!expansion.empty () || atoms.empty ())
    // We have a pattern already
    return Regex::FOUND;

  auto exit = Regex::FOUND;
  if (atoms.size () == 1
      && atoms[0].GetKind () == Token::STRING)
    {
      // It's a single string, just use it.  We'll never be called
      // again, so it's ok to move it.
      expansion = std::move (atoms[0].GetString ());
      atoms.clear ();
    }
  else
    exit = DoExpand (symtab);
  
  if (exit == Regex::FOUND)
    {
      int fail;

      exit = Regex::Create (regex, expansion, fail);
      if (exit == Regex::FAILED)
	Error () << "regex '" << expansion << "' contains "
		 << Regex::Error (fail);
    }

  return exit;
}

Regex::Result Pattern::Compare
  (Symtab &symtab, std::string_view const &text, bool eof)
{
  if (is_eof != eof)
    return Regex::NOTFOUND;
  else if (eof)
    {
      has_matched = true;
      return Regex::FOUND;
    }

  std::cmatch match;
  int error;
  auto matched = Regex::Search (regex, text, match, error);
  if (matched == Regex::NOTFOUND)
    ;
  else if (matched == Regex::FAILED)
    Error () << "regex '" << expansion
	     << "' encountered " << Regex::Error (error);
  else
    {
      has_matched = true;

      auto match_iter = ++match.begin ();
      for (auto atom_iter = atoms.begin ();
	   atom_iter != atoms.end (); ++atom_iter)
	if (atom_iter->GetKind () == Token::REGEX_CAPTURE
	    || atom_iter->GetKind () == Token::VALUE_CAPTURE)
	  {
	    int captures = atom_iter->GetUser ();
	    auto const &capture = atom_iter->GetCapture ();
	    if (capture[0].GetKind () == Token::IDENTIFIER)
	      {
		Assert (match_iter->matched);
		captures++;
		auto const &var = capture[0].GetString ();
		if (atom_iter->GetKind () == Token::REGEX_CAPTURE)
		  {
		    if (!symtab.text.Set (var, match_iter->str ()))
		      Error () << "multiple definitions of '" << var << '\'';
		  }
		else
		  {
		    // FIXME: Implement
		    Unreachable ();
		  }
	      }
	    Assert (match.end () - match_iter >= captures);
	    match_iter += captures;
	  }

      // For some reason there can be trailing unmatched matches
      for (; match_iter != match.end (); ++match_iter)
	Assert (!match_iter->matched);
    }

  return matched;
}

// Write to stdout
void Pattern::Result (Logger &logger, Logger::Status s) const
{
  logger.Result (s, file, line) << kinds[GetKind ()] << ' ' << original;
}

/* Emit result for a skipped-over pattern.  */
void Pattern::Result (Logger &log) const
{
  if (Errored ())
    Result (log, Logger::ERROR);
  else if (!Matched ())
    Result (log, Logger::PassFail (GetKind () >= CAPTURE_HWM, is_xfail));
}

void Pattern::Matched (Logger &log) const
{
  Assert (!Errored () && Matched ());

  Result (log, Logger::PassFail (GetKind () < CAPTURE_HWM, is_xfail));
}
