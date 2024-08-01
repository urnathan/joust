// Joust Test Suite			-*- mode:c++ -*-
// Copyright (C) 2020-2024 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#ifndef GAIGE_LEXER_HH

// NMS
#include "nms/fatal.hh"
// Gaige
#include "gaige/token.hh"
// C++
#include <string>
#include <string_view>
#include <vector>

namespace gaige {

class Lexer {
protected:
  std::vector<Token> Tokens;     // Lexed tokens
  std::string_view const String; // Source of characters
  unsigned LexChar = 0;          // Next char to lex
  unsigned LexToken = 0;         // Next token to return

public:
  Lexer (std::string_view const &s)
    : String(s) {}

  Lexer () = delete;
  ~Lexer () = default;

private:
  Lexer &operator= (Lexer const &) = delete;
  Lexer (Lexer const &) = delete;

public:
  Token *getToken () {
    if (Token *peeked = peekToken()) {
      LexToken++;
      return peeked;
    }
    return nullptr;
  }
  Token *peekToken (unsigned advance = 0) {
    if (LexToken + advance >= Tokens.size())
      return nullptr;
    return &Tokens[LexToken + advance];
  }

public:
  std::string_view before () const;
  std::string_view after () const;

public:
  char peekChar (unsigned a = 0) const {
    if (LexChar + a >= String.size())
      return 0;
    return String[LexChar + a];
  }
  void advanceChar (unsigned a = 1) {
    assert(LexChar + a <= String.size());
    LexChar += a;
  }
  char skipWS ();

public:
  // Advance and peek
  char advancePeekChar () {
    advanceChar();
    return peekChar();
  }
  // Peek and advance
  char peekAdvanceChar () {
    char c = peekChar();
    if (c)
      advanceChar();
    return c;
  }

public:
  void append (Token &&tok);
  // Append char C to final token KIND
  void append (Token::Kinds, char c);

public:
  bool isIdentifier ();
  bool isInteger ();
};

} // namespace gaige

#define GAIGE_LEXER_HH
#endif
