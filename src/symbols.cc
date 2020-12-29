// Joust Test Suite			-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#include "config.h"
// NMS
#include "nms/fatal.hh"
// Joust
#include "symbols.hh"
// OS
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

namespace Joust
{

std::string const *Symbols::Get
  (std::string const &var)
  const
{
  auto iter = table.find (var);

  return iter == table.end () ? nullptr : &iter->second;
}

bool Symbols::Set
  (std::string_view const &var, std::string_view const &v)
{
  auto [iter, inserted] = table.emplace (var, v);
  return inserted;
}

bool Symbols::Define
  (std::string_view const &define)
{
  auto eq = std::find (define.begin (), define.end (), '=');
  auto val = eq + (eq != define.end ());

  return Set (std::string_view (define.begin (), eq),
	      std::string_view (val, define.end ()));
}

// test=$testFile
// stem=$(basename -s .* $testFile)
// subdir=$(dir $testFile)
// tmp=${test:/=-}.TMPNAM
std::string Symbols::Origin
  (char const *s)
{
  std::string_view testFile (s);

  Set ("test", testFile);

  auto slash = testFile.find_last_of ('/');
  if (slash == testFile.npos)
    slash = 0;
  else
    slash++;
  Set ("subdir", testFile.substr (0, slash));

  auto dot = testFile.find_last_of ('.');
  if (dot == testFile.npos || dot < slash)
    dot = testFile.size ();
  Set ("stem", testFile.substr (slash, dot - slash));

  std::string tmp (testFile);
  for (size_t pos = 0;;)
    {
      pos = tmp.find_first_of ('/', pos);
      if (pos == tmp.npos)
	break;
      tmp[pos] = '-';
    }
  tmp.append (".tmp");
  Set ("tmp", tmp);

  std::string path;
  std::string testdir ("testdir");
  if (auto *tdir = Get (testdir))
    path.append (*tdir);
  else
    {
      Set (testdir, ".");
      path.append (".");
    }
  path.append ("/");
  path.append (testFile);

  return path;
}

void Symbols::Read
  (char const *file)
{
  int fd = open (file, O_RDONLY | O_CLOEXEC);
  if (fd < 0)
    {
    fatal:
      NMS::Fatal ("cannot read defines '%s': %m", file);
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

  while (begin != end)
    {
      auto eol = std::find (begin, end, '\n');

      auto space = std::find (begin, eol, '=');
      auto val = space + (space != eol);

      Set (std::string_view (begin, space), std::string_view (val, eol));

      begin = eol + 1;
    }

  munmap (buffer, alloc);
}

}
