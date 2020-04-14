// EZIO: Expect Zero Irregulities Observed	-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// Not For Distribution

// EZIO is a pattern matcher.  It looks for CHECK lines in the
// provided source and then applies those to the text provided in
// stdin.

// Joust
#include "error.hh"
#include "lexer.hh"
#include "regex.hh"
#include "scanner.hh"
#include "symbols.hh"
#include "tester.hh"
#include "token.hh"
// Utils
#include "fatal.hh"
#include "option.hh"
// C++
#include <algorithm>
#include <iostream>
#include <regex>
#include <string>
#include <string_view>
#include <unordered_map>
// OS
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

using namespace Utils;
using namespace Joust;

namespace {

class Engine;
class Parser : public Scanner
{
  using Parent = Scanner;

private:
  bool matchSpace = false;
  bool matchLine = false;
  bool xfail = false;
  Engine &e;

public:
  Parser (char const *file_, Engine &e_)
    : Parent (file_), e (e_)
  {
  }

public:
  bool GetMatchSpace () const
  {
    return matchSpace;
  }
  bool GetMatchLine () const
  {
    return matchLine;
  }
  bool GetXfail () const
  {
    return xfail;
  }

public:
  void Unparsed (char const *ctx, Token const *);
  void Unlexed (Lexer const &lexer, char const *ctx);
  void completed ();

protected:
  virtual bool ProcessLine (std::string_view const &variant,
			    std::string_view const &pattern) override;

private:
  void Options (std::string_view const &);
};

#include "ezio-symtab.inc"
#include "ezio-pattern.inc"
#include "ezio-engine.inc"

void Parser::Unlexed (Lexer const &lexer, char const *ctx)
{
  Error ()
    << "failed to lex " << ctx
    << " after '" << lexer.Before ()
    << "' at '" << lexer.After () << "'";
}

void Parser::Unparsed (char const *ctx, Token const *token)
{
  Error () << "unexpected '" << token << "' parsing " << ctx;
}

void Parser::Options (std::string_view const &text)
{
  Lexer lexer (text);

  while (char c = lexer.Peek ())
    switch (c)
      {
      case ' ':
      case '\t':
      case ',':
	lexer.Advance ();
	break;

      case '!':
	lexer.Advance ();
	lexer.Append (Token (Token::STRING, std::string ("!")));
	break;

      default:
	if (!lexer.Identifier ())
	  {
	    Unlexed (lexer, "options");
	    return;
	  }
	break;
      }
  
  bool on = true;

  while (Token const *token = lexer.GetToken ())
    switch (token->GetKind ())
      {
      case Token::STRING:
	Assert (token->GetString () == "!");
	if (!on)
	  goto unexpected;
	on = false;
	break;

      case Token::IDENTIFIER:
	if (token->GetString () == "matchspace")
	  matchSpace = on;
	else if (token->GetString () == "matchline")
	  matchLine = on;
	else if (token->GetString () == "xfail")
	  xfail = on;
	else
	  goto unexpected;
	on = true;
	break;

      default:
	{
	unexpected:
	  Unparsed ("options", token);
	}
      }
}

bool Parser::ProcessLine (std::string_view const &variant,
			  std::string_view const &pattern) 
{
  if (variant == "OPTION")
    Options (std::move (pattern));
  else
    {
      unsigned code = 0;
      if (variant.size ())
	{
	  for (; code != Pattern::UNKNOWN; ++code)
	    if (variant == Pattern::kinds[code])
	      goto found;
	  return Parent::ProcessLine (variant, pattern);
	found:;
	}
      
      if (auto *p
	  = Pattern::Parse (this, Pattern::Kind (code), pattern))
	e.Add (p);

      xfail = false;
    }

  return false;
}

}

static void Title (FILE *stream)
{
  fprintf (stream, "Expect Zero Irregularities Observed\n");
  fprintf (stream, "Copyright 2020 Nathan Sidwell, nathan@acm.org\n");
}

int main (int argc, char *argv[])
{
  struct Flags 
  {
    std::vector<char const *> prefixes; // Pattern prefixes
    std::vector<char const *> defines;  // Var defines
    char const *include = nullptr;  // file of var defines
    bool help = false;
    bool version = false;
    bool verbose = false;
  } flags;
  auto append = []
    (Option const *option, char const *opt, char const *arg, void *f)
		{
		  if (!arg[0])
		    Fatal ("option '%s' is empty", opt);
		  for (char const *p = arg; *p; p++)
		    {
		      if (*p == '='
			  && option->offset == offsetof (Flags, defines))
			break;
		      if (!std::isalnum (*p))
			Fatal ("option '%s%s%s' is ill-formed with '%c'", opt,
			       option->argform[0] == '+' ? "" : " ",
			       option->argform[0] == '+' ? "" : arg, *p);
		    }
		  option->Flag<std::vector<char const *>> (f).push_back (arg);
		};
  static constexpr Option const options[] =
    {
      {"help", 'h', offsetof (Flags, help), nullptr, nullptr, "Help"},
      {"version", 0, offsetof (Flags, version), nullptr, nullptr, "Version"},
      {"verbose", 'v', offsetof (Flags, verbose), nullptr, nullptr, "Verbose"},
      {nullptr, 'D', offsetof (Flags, defines), append, "+var=val", "Define"},
      {"include", 'i', offsetof (Flags, include), nullptr,
       "file", "File of defines"},
      {"prefix", 'p', offsetof (Flags, prefixes), append,
       "prefix", "Pattern prefix (repeatable)"},
      {nullptr, 0, 0, nullptr, nullptr, nullptr}
    };
  int argno = options->Process (argc, argv, &flags);
  if (flags.help)
    {
      Title (stdout);
      options->Help (stdout, "pattern-file [test-file]");
      return 0;
    }
  if (flags.version)
    {
      Title (stdout);
      BuildNote (stdout);
      return 0;
    }

  if (argno == argc)
    Fatal ("pattern filename missing");
  char const *patternFile = argv[argno++];
  char const *outputFile = "-";
  if (argno != argc)
    outputFile = argv[argno++];
  if (argno != argc)
    Fatal ("unexpected argument after '%s'", outputFile);

  // FIXME: determine if FD-3 is a writable pipe.  If so, it's where
  // we should write status to.  Otherwise dup FD-1 and write to there

  if (!flags.prefixes.size ())
    flags.prefixes.push_back ("CHECK");

  Symtab syms;

  // Register defines
  syms.text.Set ("EOF", "");
  for (auto d : flags.defines)
    syms.text.Define (std::string_view (d));
  if (flags.include)
    syms.text.Read (flags.include);
  std::string pathname = syms.text.SetPaths (patternFile);
  Engine engine (syms);
  {
    Parser parser (patternFile, engine);

    // Scan the pattern file
    parser.ScanFile (pathname.c_str (), flags.prefixes);
    engine.Initialize (parser);
  }

  if (Error::Errors ())
    Fatal ("failed to construct patterns '%s'", patternFile);

  if (flags.verbose)
    {
      // FIXME: dump the patterns
    }

  engine.Process (outputFile);

  engine.Finalize ();

  return Error::Errors ();
}
