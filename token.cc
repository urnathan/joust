// Joust			-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// Not For Distribution

// Joust
#include "token.hh"

namespace Joust {

Token::Token (Token &&from)
  : kind (from.kind)
{
  if (kind >= INTEGER_LWM && kind < INTEGER_HWM)
    new (&value.integer) decltype (value.integer)
      (std::move (from.value.integer));
  else if (kind >= STRING_LWM && kind < STRING_HWM)
    new (&value.string) decltype (value.string)
      (std::move (from.value.string));
  else if (kind >= CAPTURE_LWM && kind < CAPTURE_HWM)
    new (&value.capture) decltype (value.capture)
      (std::move (from.value.capture));
}

Token::~Token ()
{
  if (kind >= INTEGER_LWM && kind < INTEGER_HWM)
    {
      using type = decltype (value.integer);
      value.integer.type::~type ();
    }
  else if (kind >= STRING_LWM && kind < STRING_HWM)
    {
      using type = decltype (value.string);
      value.string.type::~type ();
    }
  else if (kind >= CAPTURE_LWM && kind < CAPTURE_HWM)
    {
      using type = decltype (value.capture);
      value.capture.type::~type ();
    }
}

std::ostream &operator<< (std::ostream &s, std::vector<Token> const &tokens)
{
  s << '{';
  for (auto token = tokens.begin (); token != tokens.end (); ++token)
    {
      if (token != tokens.begin ())
	s << ", ";
      s << *token;
    }
  s << '}';

  return s;
}

std::ostream &operator<< (std::ostream &s, Token const &token)
{
  Token::Kind kind = token.GetKind ();

  s << Token::kinds[kind];

  if (kind >= Token::INTEGER_LWM && kind < Token::INTEGER_HWM)
    s << ':' << token.GetInteger ();
  else if (kind >= Token::STRING_LWM && kind < Token::STRING_HWM)
    s << ":'" << token.GetString () << '\'';
  else if (kind >= Token::CAPTURE_LWM && kind < Token::CAPTURE_HWM)
    s << ' ' << token.value.capture;

  return s;
}

std::ostream &operator<< (std::ostream &s, Token const*token)
{
  if (token)
    s << *token;
  else
    s << "end of line";
  return s;
}

}
