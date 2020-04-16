// Joust			-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// Not For Distribution

#pragma once

// NMS
#include "fatal.hh"
// C++
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace Joust {

class Token
{
#define TOKEN_KINDS							\
  TOKEN_KIND_FROB (EMPTY, "empty"),					\
    TOKEN_KIND_FROB (OPEN, "{"),					\
    TOKEN_KIND_FROB (CLOSE, "}"),					\
    TOKEN_KIND_FROB (COLON, ":"),					\
    TOKEN_KIND_FROB (MINUS, "-"),					\
    TOKEN_KIND_FROB (INTEGER, "integer"),				\
    TOKEN_KIND_FROB (FORMAT, "format"),					\
    TOKEN_KIND_FROB (STRING, "string"),					\
    TOKEN_KIND_FROB (IDENTIFIER, "identifier"),				\
    TOKEN_KIND_FROB (WHITESPACE, "whitespace"),				\
    TOKEN_KIND_FROB (REGEX_CAPTURE, "regex-capture"),			\
    TOKEN_KIND_FROB (VALUE_CAPTURE, "value-capture")
#define TOKEN_KIND_FROB(KIND, STRING) KIND
public:
  enum Kind : unsigned char
  {
    TOKEN_KINDS,
    TOKEN_HWM,
    KEYWORD_HWM = INTEGER,
    INTEGER_LWM = INTEGER,
    INTEGER_HWM = STRING,
    STRING_LWM = STRING,
    STRING_HWM = REGEX_CAPTURE,
    CAPTURE_LWM = REGEX_CAPTURE,
    CAPTURE_HWM = TOKEN_HWM
  };
#undef TOKEN_KIND_FROB

public:
  static constexpr char const *kinds[]
#define TOKEN_KIND_FROB(KIND,STRING) STRING
    = { TOKEN_KINDS };
#undef TOKEN_KIND_FROB
#undef TOKEN_KINDS

private:
  union Value
  {
    unsigned long integer;
    std::string string;
    std::vector<Token> capture;

    Value () {};
    ~Value () {};    
  };
  Value value;
  Kind kind;

public:
  Token (Kind kind_ = EMPTY)
    : kind (kind_)
  {
    if (kind >= INTEGER_LWM && kind < INTEGER_HWM)
      new (&value.integer) decltype (value.integer) ();
    else if (kind >= STRING_LWM && kind < STRING_HWM)
      new (&value.string) decltype (value.string) ();
    else if (kind >= CAPTURE_LWM && kind < CAPTURE_HWM)
      new (&value.capture) decltype (value.capture) ();
  }
  Token (Kind kind_, unsigned long integer)
    : Token (kind_)
  {
    using namespace NMS;
    Assert (kind >= INTEGER_LWM && kind < INTEGER_HWM);
    new (&value.integer) decltype (value.integer) (std::move (integer));
  }
  Token (Kind kind_, std::string &&string)
    : Token (kind_)
  {
    using namespace NMS;
    Assert (kind_ >= STRING_LWM && kind_ < STRING_HWM);
    new (&value.string) decltype (value.string) (std::move (string));
  }
  Token (Kind kind_, std::string_view const &string)
    : Token (kind_, std::string (string))
  {
  }
  Token (Kind kind_, std::vector<Token> &&capture)
    : Token (kind_)
  {
    using namespace NMS;
    Assert (kind_ >= CAPTURE_LWM && kind_ < CAPTURE_HWM);
    new (&value.capture) decltype (value.capture) (std::move (capture));
  }

public:
  Token (Token &&from);
  ~Token ();
  
  Token &operator= (Token &&from)
  {
    if (this != &from)
      {
	Token::~Token ();
	new (this) Token (std::move (from));
      }
    return *this;
  }

public:
  Kind GetKind () const
  {
    return kind;
  }

public:
  unsigned long GetInteger () const
  {
    using namespace NMS;
    Assert (kind >= INTEGER_LWM && kind < INTEGER_HWM);
    return value.integer;
  }
  std::string const &GetString () const
  {
    using namespace NMS;
    Assert (kind >= STRING_LWM && kind < STRING_HWM);
    return value.string;
  }
  std::string &GetString ()
  {
    using namespace NMS;
    Assert (kind >= STRING_LWM && kind < STRING_HWM);
    return value.string;
  }
  std::vector<Token> const &GetCapture () const
  {
    using namespace NMS;
    Assert (kind >= CAPTURE_LWM && kind < CAPTURE_HWM);
    return value.capture;
  }
  std::vector<Token> &GetCapture ()
  {
    using namespace NMS;
    Assert (kind >= CAPTURE_LWM && kind < CAPTURE_HWM);
    return value.capture;
  }

public:
  friend std::ostream &operator<< (std::ostream &s,
				   std::vector<Token> const &tokens);
  friend std::ostream &operator<< (std::ostream &s, Token const &token);
  friend std::ostream &operator<< (std::ostream &s, Token const*token);
};


}
