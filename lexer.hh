// Joust			-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// Not For Distribution

#pragma once

// Joust
#include "token.hh"
#include "fatal.hh"
// C++
#include <string>
#include <string_view>
#include <vector>

namespace Joust {

class Lexer
{
protected:
  std::vector<Token> tokens; // Lexed tokens
  std::string_view const string; // Source of characters
  unsigned c_ix = 0; // Next char to lex
  unsigned t_ix = 0; // Next token to return

public:
  Lexer (std::string_view const &s)
    : string (s)
  {
  }

  Lexer () = delete;
  ~Lexer () = default;

private:
  Lexer &operator= (Lexer const &) = delete;
  Lexer (Lexer const &) = delete;

public:
  Token *GetToken ()
  {
    if (Token *peeked = PeekToken ())
      {
	t_ix++;
	return peeked;
      }
    return nullptr;
  }
  Token *PeekToken (unsigned advance = 0)
  {
    if (t_ix + advance >= tokens.size ())
      return nullptr;
    return &tokens[t_ix + advance];
  }

public:
  std::string_view Before () const;
  std::string_view After () const;

public:
  char Peek (unsigned a = 0) const
  {
    if (c_ix + a >= string.size ())
      return 0;
    return string[c_ix + a];
  }
  void Advance (unsigned a = 1)
  {
    Assert (c_ix + a <= string.size ());
    c_ix += a;
  }
  char SkipWS ();

public:
  // Advance and peek
  char AdvancePeek ()
  {
    Advance ();
    return Peek ();
  }
  // Peek and advance
  char PeekAdvance ()
  {
    char c = Peek ();
    if (c)
      Advance ();
    return c;
  }

public:
  void Append (Token &&tok);
  // Append char C to final token KIND
  void Append (Token::Kind, char c);


public:
  bool Identifier ();
  bool Integer ();
};

}
