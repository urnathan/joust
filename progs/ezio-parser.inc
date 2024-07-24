// Joust/EZIO: Expect Zero Irregularities Observed	-*- mode:c++ -*-
// Copyright (C) 2020-2024 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#if !defined(EZIO_PARSER)
#define EZIO_PARSER 1

class Parser : public Scanner {
  using Parent = Scanner;

private:
  bool MatchSpace = false;
  bool MatchSol = false;
  bool MatchEol = false;
  bool XFail = false;
  Engine &E;

public:
  Parser(char const *file, Engine &e)
    : Parent(file), E(e) {}

public:
  bool isMatchSpace () const { return MatchSpace; }
  bool isMatchSol () const { return MatchSol; }
  bool isMatchEol () const { return MatchEol; }
  bool isXFail () const { return XFail; }

public:
  void parseError (char const *ctx, Token const *);
  void lexError (Lexer const &lexer, char const *ctx);

protected:
  virtual bool processLine (std::string_view const &variant,
                            std::string_view const &pattern) override;

private:
  void options (std::string_view const &);
};

#else

void Parser::lexError(Lexer const &lexer, char const *ctx) {
  error() << "failed to lex " << ctx << " after '" << lexer.before()
          << "' at '" << lexer.after() << "'";
}

void Parser::parseError(char const *ctx, Token const *token) {
  error() << "unexpected '" << token << "' parsing " << ctx;
}

void Parser::options(std::string_view const &text) {
  Lexer lexer(text);

  while (char c = lexer.peekChar())
    switch (c) {
    case ' ':
    case '\t':
    case ',':
      lexer.advanceChar();
      break;

    case '!':
      lexer.advanceChar();
      lexer.append(Token(Token::STRING, std::string("!")));
      break;

    default:
      if (!lexer.isIdentifier()) {
        lexError(lexer, "options");
        return;
      }
      break;
    }

  bool on = true;

  while (Token const *token = lexer.getToken())
    switch (token->kind()) {
    case Token::STRING:
      assert(token->string() == "!");
      if (!on)
        goto unexpected;
      on = false;
      break;

    case Token::IDENTIFIER:
      if (token->string() == "matchSpace")
        MatchSpace = on;
      else if (token->string() == "matchLine")
        MatchSol = MatchEol = on;
      else if (token->string() == "matchSol")
        MatchSol = on;
      else if (token->string() == "matchEol")
        MatchEol = on;
      else if (token->string() == "xfail")
        XFail = on;
      else
        goto unexpected;
      on = true;
      break;

    default: {
    unexpected:
      parseError("options", token);
    }
    }
}

bool Parser::processLine(std::string_view const &variant,
                         std::string_view const &pattern) {
  if (variant == "OPTION")
    options(std::move(pattern));
  else if (variant == "END")
    return true;
  else {
    unsigned code = 0;
    if (variant.size()) {
      for (; code != Pattern::UNKNOWN; ++code)
        if (variant == Pattern::KindNames[code])
          goto found;
      return Parent::processLine(variant, pattern);
    found:;
    }

    auto pat = pattern;
    // Strip leading and trailing whitespace
    while (!pat.empty()) {
      if (pat.front() == ' ' || pat.front() == '\t')
        pat.remove_prefix(1);
      else if (pat.back() == ' ' || pat.back() == '\t')
        pat.remove_suffix(1);
      else
        break;
    }
    if (auto *p = Pattern::Parse(this, Pattern::Kinds(code), pat))
      E.add(p);

    XFail = false;
  }

  return false;
}

#endif
