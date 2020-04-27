// EZIO: Expect Zero Irregularities Observed	-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// Not For Distribution

// Joust
#include "symbols.hh"
// NMS
#include "fatal.hh"
// OS
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

using namespace NMS;

namespace Joust {

std::string const *Symbols::Get (std::string const &var) const
{
  auto iter = table.find (var);

  return iter == table.end () ? nullptr : &iter->second;
}

bool Symbols::Set (std::string_view const &var, std::string_view const &v)
{
  auto [iter, inserted] = table.emplace (var, v);
  return inserted;
}

bool Symbols::Define (std::string_view const &define)
{
  auto eq = std::find (define.begin (), define.end (), '=');
  auto val = eq + (eq != define.end ());

  return Set (std::string_view (define.begin (), eq),
	      std::string_view (val, define.end ()));
}

// src=$srcdir$srcFile
// stem=$(basename -s .* $srcFile)
// subdir=$(dir $srcFile)
// tmp=${src:/=-}.TMPNAM
std::string Symbols::Origin (char const *s)
{
  std::string_view srcFile (s);

  Set ("src", srcFile);

  auto slash = srcFile.find_last_of ('/');
  if (slash == srcFile.npos)
    slash = 0;
  else
    slash++;
  Set ("subdir", srcFile.substr (0, slash));

  auto dot = srcFile.find_last_of ('.');
  if (dot == srcFile.npos || dot < slash)
    dot = srcFile.size ();
  Set ("stem", srcFile.substr (slash, dot - slash));

  std::string tmp (srcFile);
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
  std::string srcdir ("srcdir");
  if (auto *sdir = Get (srcdir))
    path.append (*sdir);
  else
    Set (srcdir, "");
  path.append (srcFile);

  return path;
}

void Symbols::Read (char const *file)
{
  int fd = open (file, O_RDONLY | O_CLOEXEC);
  if (fd < 0)
    {
    fatal:
      Fatal ("cannot read defines '%s': %m", file);
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
