// Joust			-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// Not For Distribution

#pragma once

// C++
#include <vector>

namespace Joust {

template<typename T> class NullIterator
{
  T *ptr = nullptr;

public:
  template<typename Iter>
  NullIterator (Iter iter)
    : ptr (&*iter) 
  {
  }
  NullIterator (std::nullptr_t ptr_ = nullptr)
    : ptr (ptr_)
  {
  }
  NullIterator (T *ptr_)
    : ptr (ptr_)
  {
  }

public:
  operator bool () const 
  {
    return ptr != nullptr;
  }
  bool operator! () const
  {
    return !bool (*this);
  }

public:
  T *operator-> () const
  {
    return ptr;
  }
  T &operator* () const
  {
    return *ptr;
  }

public:
  bool operator== (NullIterator o) const
  {
    return ptr == o.ptr;
  }
  bool operator!= (NullIterator o) const
  {
    return !(*this == o);
  }

public:
  NullIterator &operator++ ()
  {
    ++ptr;
    return *this;
  }
  NullIterator operator+ (size_t addend)
  {
    return NullIterator (ptr + addend);
  }
  NullIterator operator+ (bool addend)
  {
    return NullIterator (ptr + addend);
  }
};

template<typename Iter> NullIterator (Iter b)
  -> NullIterator<typename std::iterator_traits<Iter>::value_type>;

}
