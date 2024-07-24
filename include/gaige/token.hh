// Joust Test Suite			-*- mode:c++ -*-
// Copyright (C) 2020-2024 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#ifndef GAIGE_TOKEN_HH

// NMS
#include "nms/fatal.hh"
#include "nms/macros.hh"
// C++
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace gaige {

class Token {
#define TOKEN_KINDS                                                           \
  (EMPTY, "empty"), (OPEN, "{"), (CLOSE, "}"), (COLON, ":"),                  \
      (INTEGER, "integer"), (FORMAT, "format"), (STRING, "string"),           \
      (IDENTIFIER, "identifier"), (WHITESPACE, "whitespace"),                 \
      (REGEX_CAPTURE, "regex-capture")
public:
  enum Kinds : unsigned char {
    NMS_LIST(NMS_1ST, TOKEN_KINDS),
    TOKEN_HWM,
    KEYWORD_HWM = INTEGER,
    INTEGER_LWM = INTEGER,
    INTEGER_HWM = STRING,
    STRING_LWM = STRING,
    STRING_HWM = REGEX_CAPTURE,
    CAPTURE_LWM = REGEX_CAPTURE,
    CAPTURE_HWM = TOKEN_HWM
  };

public:
  static char const *const KindNames[TOKEN_HWM];

private:
  union {
    unsigned long Integer;
    std::string String;
    std::vector<Token> Capture;
  };
  Kinds Kind;
  unsigned char User = 0;

public:
  Token(Kinds kind = EMPTY)
    : Kind(kind) {
    if (Kind >= INTEGER_LWM && Kind < INTEGER_HWM)
      new (&Integer) decltype(Integer)();
    else if (Kind >= STRING_LWM && Kind < STRING_HWM)
      new (&String) decltype(String)();
    else if (Kind >= CAPTURE_LWM && Kind < CAPTURE_HWM)
      new (&Capture) decltype(Capture)();
  }
  Token(Kinds kind, unsigned long integer)
    : Kind(kind) {
    assert(Kind >= INTEGER_LWM && Kind < INTEGER_HWM);
    new (&Integer) decltype(Integer)(std::move(integer));
  }
  Token(Kinds kind, std::string &&string)
    : Kind(kind) {
    assert(Kind >= STRING_LWM && Kind < STRING_HWM);
    new (&String) decltype(String)(std::move(string));
  }
  Token(Kinds kind, std::string_view const &string)
    : Token(kind, std::string(string)) {}
  Token(Kinds kind, std::vector<Token> &&capture)
    : Kind(kind) {
    assert(Kind >= CAPTURE_LWM && Kind < CAPTURE_HWM);
    new (&Capture) decltype(Capture)(std::move(capture));
  }

public:
  Token(Token &&from);
  ~Token();

  Token &operator= (Token &&from) {
    if (this != &from) {
      Token::~Token();
      new (this) Token(std::move(from));
    }
    return *this;
  }

public:
  Kinds kind () const { return Kind; }

public:
  unsigned user () const { return User; }
  void user (unsigned u) { User = u; }

public:
  unsigned long integer () const {
    assert(Kind >= INTEGER_LWM && Kind < INTEGER_HWM);
    return Integer;
  }
  std::string const &string () const {
    assert(Kind >= STRING_LWM && Kind < STRING_HWM);
    return String;
  }
  std::string &string () {
    assert(Kind >= STRING_LWM && Kind < STRING_HWM);
    return String;
  }
  std::vector<Token> const &capture () const {
    assert(Kind >= CAPTURE_LWM && Kind < CAPTURE_HWM);
    return Capture;
  }
  std::vector<Token> &capture () {
    assert(Kind >= CAPTURE_LWM && Kind < CAPTURE_HWM);
    return Capture;
  }

public:
  friend std::ostream &operator<< (std::ostream &s,
                                   std::vector<Token> const &tokens);
  friend std::ostream &operator<< (std::ostream &s, Token const &token);
  friend std::ostream &operator<< (std::ostream &s, Token const *token);
};
std::ostream &operator<< (std::ostream &s, std::vector<Token> const &tokens);
std::ostream &operator<< (std::ostream &s, Token const &token);
std::ostream &operator<< (std::ostream &s, Token const *token);

} // namespace gaige

#define GAIGE_TOKEN_HH
#endif
