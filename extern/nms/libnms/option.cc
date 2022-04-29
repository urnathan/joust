// NMS Test Suite			-*- mode:c++ -*-
// Copyright (C) 2020-2022 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#include "nms/cfg.h"
// NMS
#include "nms/fatal.hh"
#include "nms/option.hh"
// C
#include <cerrno>
#include <cstring>

namespace NMS
{

void Option::Help (
    FILE *stream, char const *args) const
{
  fprintf (stream, "Usage: %s [options] %s\n",
	   program_invocation_short_name, args);
  Help (stream);
}

void Option::Help (
    FILE *stream) const
{
  static char const pfx[]
    = "          " "          " "       ";
  if (IsEnd ())
    return;
  else if (IsSubOptions ())
      subOptions->Help (stream);
  else if (helpText)
    {
      unsigned len = fprintf (stream, "  ");

      if (shortName)
	len += fprintf (stream, "-%c", shortName);

      if (longName)
	{
	  if (shortName)
	    len += fprintf (stream, " ");
	  len += fprintf (stream, "--%s", longName);
	}

      auto *help = helpText;
      if (HasParameter ())
	{
	  auto *colon = strchr (help, ':');
	  int l;
	  if (colon)
	    {
	      l = colon - help;
	      colon = help;
	      help += l + 1;
	    }
	  else
	    {
	      colon = "ARG";
	      l = 3;
	    }

	  len += fprintf (stream, "%c%.*s",
			  " ="[IsConcatenated ()], l, colon);
	}

      if (len > strlen (pfx))
	{
	  fprintf (stream, "\n");
	  len = 0;
	}

      len += fprintf (stream, "%s", pfx + len);
      fprintf (stream, "%s\n", help);
      if (helpFn)
	  helpFn (stream, pfx);
    }

  return this[1].Help (stream);
}

int Option::Parse (
    int argc, char **argv, void *flags) const
{
  int argno = 1;

  while (argno != argc && argv[argno][0] == '-' && argv[argno][1])
    {
      auto const *arg = argv[argno];

      argno++;
      if (arg[1] == '-')
	{
	  // long option
	  arg += 2;
	  auto *next = argno < argc ? argv[argno] : nullptr;
	  auto used = ParseLong (arg, next, flags);
	  if (used >= 0)
	    argno += used;
	  else
	    Fatal ("unknown option '--%s'", arg);
	}
      else
	{
	  // short option
	  arg += 1;
	  while (*arg)
	    {
	      auto *next = argno < argc ? argv[argno] : nullptr;
	      auto used = ParseShort (arg, next, flags);
	      if (used >= 0)
		argno += used;
	      else
		Fatal ("unknown option '-%c'", *arg);
	    }
	}
    }

  return argno;
}

int Option::ParseShort (
    char const *&arg, char const *next, void *vars) const noexcept
{
  if (IsEnd ())
    return -1;
  
  if (IsSubOptions ())
    {
      auto *subVars = subObject ?: Var (vars);
      auto used = subOptions->ParseShort (arg, next, subVars);
      if (used >= 0)
	return used;
    }
  else if (shortName == *arg)
    {
      arg++;
      char const *param = nullptr;

      auto used = 0;
      if (parseFn (flags, nullptr, nullptr))
	{
	  if (*arg)
	    {
	      if (IsConcatenated ())
		{
		  param = arg;
		  arg += strlen (arg);
		}
	    }
	  else if (next)
	    {
	      param = next;
	      used = 1;
	    }

	  if (!param)
	    Fatal ("option '-%c' requires %s argument", shortName,
		   IsConcatenated () ? "an" : "a separate");
	}
      if (!parseFn (flags, Var (vars), param))
	Fatal ("option '-%c %s' is ill-formed", shortName, param);
      return used;
    }

  return this[1].ParseShort (arg, next, vars);
}

int Option::ParseLong (
    char const *arg, char const *next, void *vars) const noexcept
{
  if (IsEnd ())
    return -1;
  
  if (IsSubOptions ())
    {
      auto *subVars = subObject ?: Var (vars);
      auto used = subOptions->ParseLong (arg, next, subVars);
      if (used >= 0)
	return used;
    }
  else if (longName)
    {
      size_t len = strlen (longName);

      if (0 == strncmp (longName, arg, len))
	{
	  auto used = 0;
	  char const *param = nullptr;
	  bool hasParm = parseFn (flags, nullptr, nullptr);

	  if (!arg[len])
	    {
	      if (hasParm)
		{
		  param = next;
		  if (!param)
		    Fatal ("option '--%s' requires %s argument", longName,
			   IsConcatenated () ? "an" : "a separate");
		  used = 1;
		}
	    }
	  else if (arg[len] == '=' && IsConcatenated ())
	    {
	      if (hasParm)
		param = &arg[len + 1];
	      else
		used = -1;
	    }
	  else
	    used = -1;

	  if (used >= 0)
	    {
	      if (!parseFn (flags, Var (vars), param))
		Fatal ("option '--%s %s' is ill-formed", longName, param);
	  
	      return used;
	    }
	}
    }

  return this[1].ParseLong (arg, next, vars);
}

bool Option::Parser<bool>::Fn (
    Flags, bool *var, char const *) noexcept
{
  if (!var)
    return false;
  *var = true;
  return true;
}

bool Option::ParseFalse (
    Flags, bool *var, char const *) noexcept
{
  if (!var)
    return false;
  *var = false;
  return true;
}

bool Option::Parser<signed>::Fn (
    Flags, signed *var, char const *param) noexcept
{
  if (var)
    {
      char *eptr;
      *var = strtol (param, &eptr, 0);
      if (*eptr || eptr == param)
	return false;
    }

  return true;
}

bool Option::Parser<unsigned>::Fn (
    Flags, unsigned *var, char const *param) noexcept
{
  if (var)
    {
      char *eptr;
      *var = strtoul (param, &eptr, 0);
      if (*eptr || eptr == param)
	return false;
    }

  return true;
}

bool Option::Parser<char const *>::Fn (
    Flags, char const **var, char const *param) noexcept
{
  if (var)
    *var = param;
  return true;
}

bool Option::Parser<std::string>::Fn (
    Flags, std::string *var, char const *param) noexcept
{
  if (var)
    *var = param;
  return true;
}

bool Option::Parser<std::string_view>::Fn (
    Flags, std::string_view *var, char const *param) noexcept
{
  if (var)
    *var = param;
  return true;
}

}
