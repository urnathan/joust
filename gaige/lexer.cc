// Joust Test Suite			-*- mode:c++ -*-
// Copyright (C) 2020-2024 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#include "joust/cfg.h"
// Gaige
#include "gaige/lexer.hh"

using namespace gaige;

std::string_view Lexer::before () const { return String.substr(0, LexChar); }

std::string_view Lexer::after () const {
  return String.substr(LexChar, String.size() - LexChar);
}

void Lexer::append (Token &&tok) { Tokens.push_back(std::move(tok)); }

void Lexer::append (Token::Kinds kind, char c) {
  if (!(Tokens.size() && Tokens.back().kind() == kind))
    append(Token(kind));
  Tokens.back().string().push_back(c);
}

char Lexer::skipWS () {
  char c;
  unsigned a = 0;
  while ((c = peekChar(a))) {
    if (c != ' ' && c != '\t')
      break;
    a++;
  }
  advanceChar(a);
  return c;
}

bool Lexer::isIdentifier () {
  char c = peekChar();
  if (!(std::isalpha(c) || c == '_'))
    return false;

  unsigned begin = LexChar;
  for (;;) {
    auto peek = advancePeekChar();
    if (!(peek == '_' || std::isalnum(peek)))
      break;
  }

  append(Token(Token::IDENTIFIER, String.substr(begin, LexChar - begin)));

  return true;
}

// Base 10 integer
bool Lexer::isInteger () {
  unsigned long val = 0;
  char c = peekChar();
  if (!std::isdigit(c))
    return false;

  do {
    val = val * 10 + (c - '0');
    c = advancePeekChar();
  } while (std::isdigit(c));

  append(Token(Token::INTEGER, long(val)));

  return !(std::isalpha(c) || c == '_');
}
