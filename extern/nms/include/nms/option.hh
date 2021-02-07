// NMS Test Suite			-*- mode:c++ -*-
// Copyright (C) 2020-2021 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#ifndef NMS_OPTION_HH

// C++
#include <string>
#include <string_view>
#include <vector>
// C
#include <cstdio>
#include <cstddef>

namespace NMS
{

class Option 
{
public:
  enum Flags : unsigned short
  {
    F_None,
    F_IsConcatenated = 1 << 0,
    F_HWMShift = 1,
  };

public:
  template<typename T = void>
  using ParseFn
    = bool
    (Flags flags, T *var, char const *param)
    noexcept;
  using HelpFn
    = void
    (FILE *, char const *pfx)
    noexcept;

private:
  char const *longName = nullptr;	// --string name
  char shortName = 0;		// -charname
  Flags flags = F_None;
  unsigned offset = 0;		// offset in struct
  union
  {
    struct
    {
      ParseFn<> *parseFn = nullptr;
      char const *helpText = nullptr;
      HelpFn *helpFn = nullptr;
    };
    struct
    {
      Option const *subOptions;
      void *subObject;
    };
  };

private:
  constexpr bool HasParameter
    ()
    const
    noexcept
  {
    return parseFn (flags, nullptr, nullptr);
  }
  constexpr bool IsConcatenated
    ()
    const
    noexcept
  {
    return flags & F_IsConcatenated;
  }
  constexpr bool IsSubOptions
    ()
    const
    noexcept
  {
    return shortName == 1;
  }
  constexpr bool IsEnd
    ()
    const
    noexcept
  {
    return !(shortName || longName);
  }

public:
  constexpr Option
    ()
    noexcept
  {
  }
  constexpr Option
    (char const *longName_, char shortName_, Flags flags_,
     unsigned offset_, ParseFn<> parseFn_,
     char const *helpText_, HelpFn *helpFn_ = nullptr)
    noexcept
    : longName (longName_), shortName (shortName_),
      flags (flags_), offset (offset_), parseFn (parseFn_),
      helpText (helpText_), helpFn (helpFn_)
  {
  }
  constexpr Option
    (char const *longName_, char shortName_,
     unsigned offset_, ParseFn<> parseFn_,
     char const *helpText_, HelpFn *helpFn_ = nullptr)
      noexcept
      : Option (longName_, shortName_, F_None,
		offset_, parseFn_, helpText_, helpFn_)
  {
  }
#define OPTION_FLD(T, M) offsetof (T, M)
#define OPTION_FLDFN(T, M) OPTION_FLD (T, M), &T::M
#define OPTION_FN(FN) (::NMS::Option::Adaptor<FN>::Fn)
  template <typename C, typename M>
  constexpr Option
    (char const *longName_, char shortName_, Flags flags_,
     unsigned offset_, M C::*,
     char const *helpText_, HelpFn *helpFn_ = nullptr)
    noexcept
    : Option (longName_, shortName_, flags_,
	      offset_, Adaptor<Parser<M>::Fn>::Fn,
	      helpText_, helpFn_)
  {
  }
  template <typename C, typename M>
  constexpr Option
    (char const *longName_, char shortName_,
     unsigned offset_, M C::*mptr_,
     char const *helpText_, HelpFn *helpFn_ = nullptr)
    noexcept
    : Option (longName_, shortName_, F_None,
	      offset_, mptr_, helpText_, helpFn_)
  {
  }
  constexpr Option
    (Option const *subOptions_, void *subObject_)
    noexcept
    : shortName (1),
      subOptions (subOptions_), subObject (subObject_)
  {
  }
  constexpr Option
    (Option const *subOptions_, unsigned offset_)
    noexcept
    : shortName (1), offset (offset_),
      subOptions (subOptions_)
  {
  }

private:
  void *Var
    (void *f)
    const
    noexcept
  {
    return reinterpret_cast<char *> (f) + offset;
  }

public:
  void Help
    (FILE *stream, char const *)
    const;
  int Parse
    (int argc, char **argv, void *vars)
    const;

private:
  int ParseShort
    (char const *&opt, char const *next, void *vars)
    const
    noexcept;
  int ParseLong
    (char const *opt, char const *next, void *vars)
    const
    noexcept;
  void Help
    (FILE *stream)
    const;

public:
  template<typename T>
  struct Parser;

private:
  template<typename FT>
  struct ParseArg;
  template<typename T>
  struct ParseArg<ParseFn<T> &>
  {
    using ArgType = T;
  };

public:
  template<auto &FN,
	   typename ArgType
	   = typename ParseArg<decltype (FN) &>::ArgType>
  struct Adaptor
  {
    static bool Fn
      (Flags flags, void *var, char const *param)
      noexcept
    {
      return FN (flags, reinterpret_cast<ArgType *> (var), param);
    }
  };

public:
  static ParseFn<bool> ParseFalse;
};

template<>
struct Option::Parser<bool>
{
  static ParseFn<bool> Fn;
};

template<>
struct Option::Parser<signed>
{
  static ParseFn<signed> Fn;
};
template<>
struct Option::Parser<unsigned>
{
  static ParseFn<unsigned> Fn;
};
template<>
struct Option::Parser<char const *>
{
  static ParseFn<char const *> Fn;
};
template<>
struct Option::Parser<std::string>
{
  static ParseFn<std::string> Fn;
};
template<>
struct Option::Parser<std::string_view>
{
  static ParseFn<std::string_view> Fn;
};

template<typename T>
struct Option::Parser<std::vector<T>>
{
  static bool Fn
    (Flags flags, std::vector<T> *var, char const *param)
    noexcept
  {
    if (!var)
      return Parser<T>::Fn (flags, nullptr, param);
    else
      {
	T v;
	bool ok = Parser<T>::Fn (flags, &v, param);
	if (ok)
	  var->emplace_back (v);
	return ok;
      }
  }
};

}

#define NMS_OPTION_HH
#endif
