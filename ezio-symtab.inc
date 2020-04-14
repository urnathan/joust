// EZIO: Expect Zero Irregulities Observed	-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// Not For Distribution

struct Symtab
{
public:
  class Int
  {
  private:
    std::unordered_map<std::string, unsigned long> table;

  public:
    Int () = default;
    ~Int () = default;

  private:
    Int (Int const &) = delete;
    Int &operator= (Int const &) = delete;

  public:
    unsigned long const *Get (std::string const &var) const;

  public:
    bool Set (std::string_view const &var, unsigned long val);
  };

public:
  Symbols text;
  Int ints;
};

unsigned long const *Symtab::Int::Get (std::string const &var) const
{
  auto iter = table.find (var);

  return iter == table.end () ? nullptr : &iter->second;
}

bool Symtab::Int::Set (std::string_view const &var, unsigned long v)
{
  auto [iter, inserted] = table.emplace (var, v);
  return inserted;
}
