// Joust Test Suite			-*- mode:c++ -*-
// Copyright (C) 2020-2024 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#ifndef GAIGE_SYMBOLS_HH

// C++
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>

namespace gaige {

class Symbols {
private:
  std::unordered_map<std::string, std::string> Table;

public:
  Symbols () = default;
  ~Symbols () = default;

private:
  Symbols (Symbols const &) = delete;
  Symbols &operator= (Symbols const &) = delete;

public:
  std::string const *value (std::string const &var) const;

public:
  bool value (std::string_view const &var, std::string_view const &val);
  bool define (std::string_view const &define);
  std::string setOriginValues (char const *src);
  void readFile (char const *file);
};

} // namespace gaige

#define GAIGE_SYMBOLS_HH
#endif
