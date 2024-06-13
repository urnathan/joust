// Joust Test Suite			-*- mode:c++ -*-
// Copyright (C) 2020-2024 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#include "joust/cfg.h"
// Gaige
#include "gaige/token.hh"

using namespace gaige;

constinit char const *const Token::KindNames[TOKEN_HWM]
= {NMS_LIST (NMS_2ND, TOKEN_KINDS)};

Token::Token (Token &&from)
  : Kind (from.Kind), User (from.User)
{
  if (Kind >= INTEGER_LWM && Kind < INTEGER_HWM)
    new (&Integer) decltype (Integer) (std::move (from.Integer));
  else if (Kind >= STRING_LWM && Kind < STRING_HWM)
    new (&String) decltype (String) (std::move (from.String));
  else if (Kind >= CAPTURE_LWM && Kind < CAPTURE_HWM)
    new (&Capture) decltype (Capture) (std::move (from.Capture));
}

Token::~Token ()
{
  if (Kind >= INTEGER_LWM && Kind < INTEGER_HWM)
    {
      using type = decltype (Integer);
      Integer.type::~type ();
    }
  else if (Kind >= STRING_LWM && Kind < STRING_HWM)
    {
      using type = decltype (String);
      String.type::~type ();
    }
  else if (Kind >= CAPTURE_LWM && Kind < CAPTURE_HWM)
    {
      using type = decltype (Capture);
      Capture.type::~type ();
    }
}

std::ostream &gaige::operator<< (std::ostream &s,
				 std::vector<Token> const &tokens)
{
  s << '{';
  for (const auto &token : tokens)
    {
      if (&token != &*tokens.begin ())
	s << ", ";
      s << token;
    }
  s << '}';

  return s;
}

std::ostream &gaige::operator<< (std::ostream &s, Token const &token)
{
  Token::Kinds kind = token.kind ();

  s << Token::KindNames[kind];

  if (kind >= Token::INTEGER_LWM && kind < Token::INTEGER_HWM)
    s << ':' << token.Integer;
  else if (kind >= Token::STRING_LWM && kind < Token::STRING_HWM)
    s << ":'" << token.String << '\'';
  else if (kind >= Token::CAPTURE_LWM && kind < Token::CAPTURE_HWM)
    s << ' ' << token.Capture;

  return s;
}

std::ostream &gaige::operator<< (std::ostream &s, Token const *token)
{
  if (token)
    s << *token;
  else
    s << "end of line";
  return s;
}
