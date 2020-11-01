// Joust Test Suite			-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

// Joust
#include "scanner.hh"
#include "fatal.hh"
// C++
#include <iostream>
// C
#include <cstring>
// OS
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

namespace Joust
{

// Read the pattern file.  These are lines containing one of PREFIXES,
// everything after the prefix, up to \n is the pattern.
// Lines must match the regexp [^:alnum:]prefix(-opt)?: to be recognized

bool Scanner::ScanFile
  (std::string const &fname, std::vector<char const *> const &prefixes)
{
  int fd = open (fname.c_str (), O_RDONLY | O_CLOEXEC);
  if (fd < 0)
    {
    fatal:
      int err = errno;
      Error () << "cannot read file '" << fname << "': " << strerror (err);
      return false;
    }
  size_t len = [] (int fd_)
	       {
		 struct stat stat;
		 return fstat (fd_, &stat) ? ~size_t (0) : stat.st_size;
	       } (fd);
  if (len == ~size_t (0))
    goto fatal;

  size_t page_size = sysconf (_SC_PAGE_SIZE);
  size_t alloc = (len + page_size) & ~(page_size - 1);
  void *buffer = mmap (nullptr, alloc, PROT_READ | PROT_WRITE,
		       MAP_PRIVATE, fd, 0);
  if (buffer == MAP_FAILED)
    goto fatal;
  // It is safe to close a mapped file
  close (fd);

  // Don't really care about error code
  madvise (buffer, alloc, MADV_SEQUENTIAL);

  auto *begin = reinterpret_cast <char const *> (buffer);
  auto *end = begin + len;

  // Ensure the template ends in a newline
  if (end[-1] != '\n')
    *const_cast <char *> (end++) = '\n';

  Assert (prefixes.size ());

  std::vector<unsigned> lengths;
  lengths.reserve (prefixes.size ());
  
  std::string initial_chars;
  std::vector<char const *> initial_points;

  // Build up the set of unique initial characters
  for (auto p : prefixes)
    {
      lengths.push_back (std::strlen (p));
      auto c = p[0];
      if (initial_chars.find (c) == initial_chars.npos)
	{
	  initial_chars.push_back (c);
	  initial_points.push_back (begin);
	}
    }

  line = 1;
  bool ended = false;
  for (char const *base = begin; ;)
    {
      // Advance any initial points that are before BEGIN, and
      // determine the earliest potential start
      char const *pattern = nullptr;
      for (unsigned ix = initial_points.size (); ix--;)
	{
	  if (initial_points[ix] <= base)
	    initial_points[ix] = std::find (base, end, initial_chars[ix]);
	  if (!pattern || pattern > initial_points[ix])
	    pattern = initial_points[ix];
	}

      if (pattern == end)
	break;

      char const *colon = nullptr;
      char const *variant = nullptr;

      // Potential start must be preceded by non-alpha
      if (pattern == base || !(isalnum (pattern[-1]) || pattern[-1] == '_'))
	{
	  // Test each prefix, yes even the ones that can't possibly match
	  for (unsigned ix = prefixes.size (); ix--;)
	    if (size_t (end - pattern) > lengths[ix]
		&& std::equal (prefixes[ix], prefixes[ix] + lengths[ix],
			       pattern))
	      {
		variant = colon = pattern + lengths[ix];
		if (*colon == ':')
		  goto found;
		if (*colon == '-')
		  {
		    variant = colon + 1;
		    colon = std::find_if_not (variant, end,
					      [] (char c) 
					      { return isalpha (c); });
		    if (*colon == ':')
		      goto found;
		  }
	      }
	}

      // This doesn't meet the prefix requirements.
      base = pattern + 1;
      continue;

    found:
      // Count lines from begin to colon, and find the eol of colon
      char const *eol;

      for (;;)
	{
	  eol = std::find (begin, end, '\n');
	  Assert (eol != end);

	  if (eol > colon)
	    break;
	  begin = eol + 1;
	  line++;
	}

      // Process the pattern
      std::string_view pattern_text (colon + 1, eol);
      std::string_view variant_text (variant, colon);

      if (ProcessLine (std::move (variant_text), std::move (pattern_text)))
	{
	  ended = true;
	  break;
	}

      base = begin = eol + 1;
      line++;
    }

  munmap (buffer, alloc);

  return ended;
}

bool Scanner::ProcessLine
  (std::string_view const &variant, std::string_view const &)
{
  Error () << "unrecognized command variant '" << variant << '\'';
  return true;
}

}
