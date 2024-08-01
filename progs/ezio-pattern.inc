// Joust/EZIO: Expect Zero Irregularities Observed	-*- mode:c++ -*-
// Copyright (C) 2020-2024 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

class Pattern {
  class Lexer : public gaige::Lexer {
  public:
    using Parent = gaige::Lexer;

  public:
    using Parent::Parent;

  public:
    bool option ();
    bool pattern ();

  protected:
    bool patternRegexp ();
  };

public:
#define PATTERN_KINDS MATCH, NEXT, DAG, LABEL, NOT, NEVER, NONE, UNKNOWN
  enum Kinds : unsigned char {
    NMS_LIST(NMS_IDENT, PATTERN_KINDS),
    PATTERN_HWM,
    CAPTURE_HWM = NOT,
  };

public:
  static char const *const KindNames[PATTERN_HWM];

private:
  SrcLoc Loc;

private:
  Kinds Kind;

private:
  bool IsMatchSpace : 1 = false;
  bool IsMatchSol   : 1 = false;
  bool IsMatchEol   : 1 = false;
  bool IsEOF        : 1 = false;
  bool IsXFail      : 1 = false;
  bool HasError     : 1 = false;
  bool HasMatch     : 1 = false;
  bool HasCapture   : 1 = false;

private:
  std::string Original;
  std::string Expansion;
  std::regex Regex;

private:
  std::vector<Token> Atoms;

protected:
  Pattern (Kinds kind, Parser const *parser, std::string_view const &text)
    : Loc(parser->loc()), Kind(kind), Original(text) {
    if (parser->isMatchSol())
      IsMatchSol = true;
    if (parser->isMatchEol())
      IsMatchEol = true;
    if (parser->isMatchSpace())
      IsMatchSpace = true;
    if (parser->isXFail())
      IsXFail = true;
  }

public:
  ~Pattern () = default;

public:
  Kinds kind () const { return Kind; }

public:
  bool hasError () const { return HasError; }
  bool hasMatch () const { return HasMatch; }
  bool hasCapture () const { return HasCapture; }

private:
  void Append (Token &&a) { Atoms.push_back(std::move(a)); }
  unsigned Size () const { return Atoms.size(); }

public:
  Error error ();

public:
  bool Canonicalize ();

public:
  regex::Results Expand (Symbols const &);
  regex::Results Compare (Symbols &, std::string_view const &text, bool eof);

private:
  regex::Results DoExpand (Symbols const &);

public:
  static Pattern *Parse (Parser *, Kinds kind, std::string_view const &);

private:
  bool ParseEscape (Parser *, Lexer &);

private:
  void result (Tester &, Tester::Statuses) const;

public:
  void result (Tester &) const;
  void match (Tester &) const;

  friend std::ostream &operator<< (std::ostream &s, Pattern const &p);
};

constinit char const *const Pattern::KindNames[PATTERN_HWM]
    = {NMS_LIST(NMS_STRING, PATTERN_KINDS)};

// PATTERN_REGEXP is
//   white-space: [ \t]+
//   close-braces: }} -> return
//   variable: $ [:alpha:|] [:alnum:|]*
//   variable: $ { [:alpha:|] [:alnum:|]* }
//   string: $$
//   string: .+
// Final TOKEN_CLOSE has capture count
bool Pattern::Lexer::patternRegexp () {
  bool escape = false;
  unsigned square = 0;
  bool brace = false;
  int captures = 0;

  while (char c = peekChar())
    switch (c) {
    case '[':
      if (!escape && !square && !brace)
        square = LexChar;
      goto append;

    case ']':
      if (LexChar != square + 1)
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
      else if (!escape) {
        advanceChar();
        append(Token(Token::CLOSE));
        Tokens.back().user(captures);
        return true;
      }
      goto append;

    case '\\':
      if (!escape && !square) {
        escape = true;
        goto escape;
      }
      goto append;

    case '-':
      if (LexChar == square + 1)
        square++;
      goto append;

    case '$':
      if (escape || square)
        goto append;
      {
        char c = peekChar(1);
        bool braced = c == '{';
        if (!(braced || std::isalpha(c) || c == '_'))
          // not ${ or $[:alpha:|]
          goto append;

        advanceChar(1 + braced);
        bool ident = isIdentifier();
        if (braced) {
          if (peekChar() != '}')
            return false;
          if (!ident)
            // ${} emit the $ eat the {}
            goto append;
          advanceChar();
        } else
          assert(ident);
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
      advanceChar();
      append(Token::STRING, c);
      break;
    }

  return false;
}

bool Pattern::Lexer::pattern () {
  while (char c = peekChar())
    switch (c) {
    case ' ':
    case '\t':
      advanceChar();
      append(Token::WHITESPACE, c);
      break;

    case '{': {
      char c = peekChar(1);

      bool ident = std::isalpha(c) || c == '_';
      if (!(ident || c == ':'))
        goto append;

      advanceChar();
      append(Token(Token::OPEN));

      if (ident) {
        if (!isIdentifier())
          return false;
        c = peekChar();
      }

      if (c == ':') {
        advanceChar();
        append(Token(Token::COLON));
      }

      if (!patternRegexp())
        return false;
    } break;

    case '$': {
      char c = peekChar(1);

      bool braced = c == '{';
      if (!(braced || std::isalpha(c) || c == '_'))
        // not ${ or $[:alpha:|]
        goto append;

      advanceChar(1 + braced);
      bool ident = isIdentifier();
      if (braced) {
        if (peekChar() != '}')
          return false;
        if (!ident)
          // ${} emit the $ eat the {}
          goto append;
        advanceChar();
      } else
        assert(ident);
    } break;

    default:
    append:
      advanceChar();
      append(Token::STRING, c);
      break;
    }

  return true;
}

Error Pattern::error () {
  HasError = true;
  return Error(Loc);
}

std::ostream &operator<< (std::ostream &s, Pattern const &p) {
  return s << p.Loc.file() << ':' << p.Loc.line() << ":"
           << p.KindNames[p.kind()] << ", " << p.Atoms << '\n';
}

// match-escape: IDENTIFIER-opt COLON regexp-expand CLOSE
// regexp-expand: regexp-expand-opt STRING
//              | regexp-expand-opt IDENTIFIER

bool Pattern::ParseEscape (Parser *parser, Lexer &lexer) {
  Token escape(Token::REGEX_CAPTURE);
  Token *next = lexer.getToken();

  Token ident;
  if (next && next->kind() == Token::IDENTIFIER) {
    if (kind() >= CAPTURE_HWM)
      error() << "capture not permitted in " << KindNames[kind()];
    ident = std::move(*next);
    next = lexer.getToken();
  }

  if (!next) {
  failed:
    parser->parseError("pattern {{escape}}", next);
    return false;
  }

  if (next->kind() != Token::COLON)
    goto failed;

  escape.capture().emplace_back(std::move(ident));

  while ((next = lexer.getToken()))
    if (next->kind() == Token::IDENTIFIER || next->kind() == Token::STRING
        || next->kind() == Token::WHITESPACE)
      escape.capture().emplace_back(std::move(*next));
    else
      break;

  if (!next)
    goto failed;
  if (next->kind() != Token::CLOSE)
    goto failed;

  escape.user(next->user());
  Append(std::move(escape));

  return true;
}

// Parse the match. Its grammar isL
// match: match-opt match-word
// match-word: STRING | IDENTIFIER | OPEN match-escape

Pattern *Pattern::Parse (Parser *parser, Kinds kind,
                         std::string_view const &line) {
  auto p = new Pattern(kind, parser, line);
  if (kind == UNKNOWN) {
  failed:
    p->HasError = true;
    return p;
  }

  Lexer lexer(line);
  if (!lexer.pattern()) {
    parser->lexError(lexer, "pattern");
    goto failed;
  }

  while (Token *tok = lexer.getToken())
    switch (tok->kind()) {
    case Token::WHITESPACE:
    case Token::IDENTIFIER:
    case Token::STRING:
      p->Append(std::move(*tok));
      break;

    case Token::OPEN:
      if (!p->ParseEscape(parser, lexer))
        goto failed;
      break;

    default:
      parser->parseError("pattern", tok);
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
bool Pattern::Canonicalize () {
  std::vector<Token> canon;
  canon.reserve(Atoms.size());
  int eof = 0;

  std::string acc; // Current accumulation

  if (IsMatchSol)
    acc.push_back('^');

  bool eol = IsMatchEol;
  for (auto token = Atoms.begin(); token != Atoms.end(); ++token) {
    switch (token->kind()) {
    case Token::WHITESPACE:
      if (IsMatchSpace)
        // Spacing mush match exactly
        acc.append(std::move(token->string()));
      else
        // Any horizontal whitespace
        acc.append("[ \t]+");
      break;

    case Token::STRING: {
      std::string_view string = token->string();
      // SOL can also be matched with explicit ^
      if (canon.empty() && acc.size() == IsMatchSol && string.front() == '^') {
        string.remove_prefix(1);
        if (acc.empty())
          acc.push_back('^');
      }
      // EOL can also match be matched with explicit $
      if (std::next(token) == Atoms.end() && string.back() == '$') {
        eol = true;
        string.remove_suffix(1);
      }
      std::string escaped = regex::protect(string);
      acc.append(std::move(escaped));
      eof = -1;
    } break;

    case Token::IDENTIFIER:
      if (acc.size()) {
        canon.push_back(Token(Token::STRING, std::move(acc)));
        acc.clear();
      }
      if (!eof && token->string() == "EOF")
        eof = 1;
      else
        eof = -1;
      canon.push_back(std::move(*token));
      break;

    case Token::REGEX_CAPTURE: {
      std::vector<Token> &capture = token->capture();

      if (capture.size() == 1) {
        error() << "capture captures nothing";
        return false;
      }

      if (capture[0].kind() == Token::IDENTIFIER)
        HasCapture = true;

      if (capture.size() == 2 && capture[0].kind() == Token::EMPTY
          && capture[1].kind() == Token::STRING && !token->user())
        // A null capture of a single non-capturing string can
        // just be a string
        acc.append(std::move(capture[1].string()));
      else {
        if (acc.size()) {
          canon.push_back(Token(Token::STRING, std::move(acc)));
          acc.clear();
        }
        canon.push_back(std::move(*token));
      }
      eof = -1;
    } break;

    default:
      unreachable();
    }
  }

  if (eol)
    acc.push_back('$');

  if (acc.size())
    canon.push_back(Token(Token::STRING, std::move(acc)));

  if (!canon.size() && kind() < CAPTURE_HWM) {
    error() << KindNames[kind()] << " pattern is empty";
    return false;
  }

  if (eof == 1) {
    canon.clear();
    IsEOF = true;
  }

  Atoms = std::move(canon);

  return true;
}

regex::Results Pattern::DoExpand (Symbols const &symbols) {
  for (auto &token : Atoms)
    switch (token.kind()) {
    case Token::STRING:
      Expansion.append(token.string());
      break;

    case Token::IDENTIFIER:
      if (auto *val = symbols.value(token.string()))
        regex::protect(Expansion, *val);
      else
        return regex::NOTFOUND;
      break;

    case Token::REGEX_CAPTURE: {
      std::vector<Token> const &capture = token.capture();
      bool is_capture = capture[0].kind() == Token::IDENTIFIER;

      if (is_capture)
        Expansion.push_back('(');
      for (auto cap = capture.begin() + 1; cap != capture.end(); ++cap)
        switch (cap->kind()) {
        case Token::STRING:
          Expansion.append(cap->string());
          break;

        case Token::IDENTIFIER:
          if (auto *val = symbols.value(cap->string()))
            regex::protect(Expansion, *val);
          else
            return regex::NOTFOUND;
          break;

        default:
          unreachable();
        }
      if (is_capture)
        Expansion.push_back(')');
    } break;

    default:
      unreachable();
    }

  return regex::FOUND;
}

// Expand tokens to expansion
regex::Results Pattern::Expand (Symbols const &symbols) {
  if (hasError())
    return regex::FAILED;

  if (!Expansion.empty() || Atoms.empty())
    // We have a pattern already
    return regex::FOUND;

  auto exit = regex::FOUND;
  if (Atoms.size() == 1 && Atoms[0].kind() == Token::STRING) {
    // It's a single string, just use it.  We'll never be called
    // again, so it's ok to move it.
    Expansion = std::move(Atoms[0].string());
    Atoms.clear();
  } else
    exit = DoExpand(symbols);

  if (exit == regex::FOUND) {
    int fail;

    exit = regex::create(Regex, Expansion, fail);
    if (exit == regex::FAILED)
      error() << "regex '" << Expansion << "' contains " << regex::error(fail);
  } else
    Expansion.clear();

  return exit;
}

regex::Results Pattern::Compare (Symbols &symbols,
                                 std::string_view const &text, bool eof) {
  if (IsEOF != eof)
    return regex::NOTFOUND;
  else if (eof) {
    HasMatch = true;
    return regex::FOUND;
  }

  std::cmatch match;
  int err;
  auto matched = regex::search(Regex, text, match, err);
  if (matched == regex::NOTFOUND)
    ;
  else if (matched == regex::FAILED)
    error() << "regex '" << Expansion << "' encountered " << regex::error(err);
  else {
    HasMatch = true;

    auto match_iter = ++match.begin();
    for (auto const &atom : Atoms)
      if (atom.kind() == Token::REGEX_CAPTURE) {
        int captures = atom.user();
        auto const &capture = atom.capture();
        if (capture[0].kind() == Token::IDENTIFIER) {
          assert(match_iter->matched);
          captures++;
          auto const &var = capture[0].string();
          if (!symbols.value(var, match_iter->str()))
            error() << "multiple definitions of '" << var << '\'';
        }
        assert(match.end() - match_iter >= captures);
        match_iter += captures;
      }

    // For some reason there can be trailing unmatched matches
    for (; match_iter != match.end(); ++match_iter)
      assert(!match_iter->matched);
  }

  return matched;
}

// Write to stdout
void Pattern::result (Tester &logger, Tester::Statuses s) const {
  logger.result(s, Loc) << KindNames[kind()] << ' ' << Original;
}

/* Emit result for a skipped-over pattern.  */
void Pattern::result (Tester &log) const {
  if (hasError())
    result(log, Tester::ERROR);
  else if (!hasMatch())
    result(log, Tester::passFail(kind() >= CAPTURE_HWM, IsXFail));
}

void Pattern::match (Tester &log) const {
  assert(!hasError() && hasMatch());

  result(log, Tester::passFail(kind() < CAPTURE_HWM, IsXFail));
}
