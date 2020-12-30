// Joust Test Suite			-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#include "config.h"
// Gaige
#include "gaige/lexer.hh"

namespace Joust
{

std::string_view Lexer::Before
  ()
  const
{
  return string.substr (0, c_ix);
}

std::string_view Lexer::After
  ()
  const
{
  return string.substr (c_ix, string.size () - c_ix);
}

void Lexer::Append
  (Token &&tok)
{
  tokens.push_back (std::move (tok));
}

void Lexer::Append
  (Token::Kind kind, char c)
{
  if (!(tokens.size () && tokens.back ().GetKind () == kind))
    Append (Token (kind));
  tokens.back ().GetString ().push_back (c);
}

char Lexer::SkipWS
  ()
{
  char c;
  unsigned a = 0;
  while ((c = Peek (a)))
    {
      if (c != ' ' && c != '\t')
	break;
      a++;
    }
  Advance (a);
  return c;
}

bool Lexer::Identifier
  ()
{
  char c = Peek ();
  if (!std::isalpha (c))
    return false;

  unsigned begin = c_ix;
  while (std::isalnum (AdvancePeek ()))
    continue;

  Append (Token (Token::IDENTIFIER, string.substr (begin, c_ix - begin)));

  return true;
}

// Base 10 integer
bool Lexer::Integer
  ()
{
  unsigned long val = 0;
  char c = Peek ();
  if (!std::isdigit (c))
    return false;

  do
    {
      val = val * 10 + (c - '0');
      c = AdvancePeek ();
    }
  while (std::isdigit (c));

  Append (Token (Token::INTEGER, long (val)));

  return !std::isalpha (c);
}

}
