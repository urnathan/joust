// Joust Test Suite			-*- mode:c++ -*-
// Copyright (C) 2020-2024 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#include "joust/cfg.h"
// NMS
#include "nms/fatal.hh"
// Gaige
#include "gaige/symbols.hh"
// C++
#include <algorithm>
// OS
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace nms;
using namespace gaige;

std::string const *Symbols::value (std::string const &var) const
{
  auto iter = Table.find (var);

  return iter == Table.end () ? nullptr : &iter->second;
}

bool Symbols::value (std::string_view const &var,
		     std::string_view const &v)
{
  auto [iter, inserted] = Table.emplace (var, v);
  return inserted;
}

bool Symbols::define (std::string_view const &define)
{
  auto eq = std::find (define.begin (), define.end (), '=');
  auto val = eq + (eq != define.end ());

  return value (std::string_view (define.begin (), eq),
		std::string_view (val, define.end ()));
}

// test=$testFile
// stem=$(basename -s .* $testFile)
// subdir=$(dir $testFile)
// tmp=${test:/=-}.TMPNAM
std::string Symbols::setOriginValues (char const *s)
{
  std::string_view testFile (s);

  value ("test", testFile);

  auto slash = testFile.find_last_of ('/');
  if (slash == testFile.npos)
    slash = 0;
  else
    slash++;
  value ("subdir", testFile.substr (0, slash));

  auto dot = testFile.find_last_of ('.');
  if (dot == testFile.npos || dot < slash)
    dot = testFile.size ();
  value ("stem", testFile.substr (slash, dot - slash));

  std::string tmp (testFile);
  for (size_t pos = 0;;)
    {
      pos = tmp.find_first_of ('/', pos);
      if (pos == tmp.npos)
	break;
      tmp[pos] = '-';
    }
  tmp.append (".tmp");
  value ("tmp", tmp);

  std::string path;
  std::string testdir ("testdir");
  if (auto *tdir = value (testdir))
    path.append (*tdir);
  else
    {
      value (testdir, ".");
      path.append (".");
    }
  path.append ("/");
  path.append (testFile);

  return path;
}

void Symbols::readFile (char const *file)
{
  int fd = open (file, O_RDONLY | O_CLOEXEC);
  if (fd < 0)
    {
    fatal:
      fatalExit ("?cannot read defines '%s': %m", file);
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
  void *buffer
    = mmap (nullptr, alloc, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
  if (buffer == MAP_FAILED)
    goto fatal;
  // It is safe to close a mapped file
  close (fd);

  // Don't really care about error code
  madvise (buffer, alloc, MADV_SEQUENTIAL);

  auto *begin = reinterpret_cast<char *> (buffer);
  auto *end = begin + len;

  // Ensure the template ends in a newline
  if (end[-1] != '\n')
    *end++ = '\n';

  while (begin != end)
    {
      auto eol = std::find (begin, end, '\n');
      if (eol != begin)
	{
	  auto eql = std::find (begin, eol, '=');

	  if (eql != eol)
	    // Value
	    value (std::string_view (begin, eql),
		   std::string_view (eql + 1, eol));
	  else
	    {
	      // Nested file
	      *eol = 0;
	      readFile (begin);
	    }
	}
      begin = eol + 1;
    }

  munmap (buffer, alloc);
}
