// Joust Test Suite			-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#pragma once

// C++
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>

namespace Joust
{

class Symbols
{
private:
  std::unordered_map<std::string, std::string> table;

public:
  Symbols
    ()
    = default;
  ~Symbols
    ()
    = default;

private:
  Symbols
    (Symbols const &)
    = delete;
  Symbols &operator=
    (Symbols const &)
    = delete;

public:
  std::string const *Get
    (std::string const &var)
    const;

public:
  std::string Origin
    (char const *src);
  bool Set
    (std::string_view const &var, std::string_view const &val);
  bool Define
    (std::string_view const &define);
  void Read
    (char const *file);
};

}
