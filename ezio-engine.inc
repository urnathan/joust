// EZIO: Expect Zero Irregulities Observed	-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// Not For Distribution

class Engine 
{
private:
  struct Frame 
  {
    std::vector<Pattern *> nones;
    std::vector<Pattern *> body;
    Pattern *head = nullptr;

  public:
    Frame (Pattern *h = nullptr)
      : head (h)
    {
    }
    Frame (Frame &&from)
      : nones (std::move (from.nones)),
	body (std::move (from.body)),
	head (from.head)
    {
      from.head = nullptr;
    }
    ~Frame ();

  private:
    Frame &operator= (Frame &&) = delete;
    
  };

private:
  Symtab &syms;
  std::vector<Pattern *> nevers;
  std::vector<Frame> frames;
  unsigned frameIndex = 0;
  unsigned bodyIndex = 0;

public:
  Engine (Symtab &s)
    :syms (s)
  {
  }
  ~Engine ();

private:
  Engine (Engine &&) = delete;
  Engine &operator= (Engine &&) = delete;

public:
  void FrameAdvance (unsigned frameIndex);
  void BodyAdvance (unsigned frameIndex);
  void Process (char const *file);
  void Process (std::string_view const &line, bool eof);

public:
  void Add (Pattern *);
  void Initialize (Scanner &);
  void Finalize ();

  friend std::ostream &operator<< (std::ostream &, Engine const &);
  friend std::ostream &operator<< (std::ostream &s, Engine::Frame const &e);
  friend std::ostream &operator<< (std::ostream &s,
				   std::vector<Engine::Frame> const &vec);
};

Engine::Frame::~Frame ()
{
  for (auto *n : nones)
    delete n;
  for (auto *b : body)
    delete b;
  delete head;
}

Engine::~Engine ()
{
  for (auto *n : nevers)
    delete n;
}

std::ostream &operator<< (std::ostream &s, std::vector<Pattern *> const &vec)
{
  s << '{';
  for (unsigned ix = 0, len = vec.size (); ix != len; ix++)
    {
      if (ix)
	s << ",\n";
      s << '[' << ix << ']' << *vec[ix];
    }
  s << '}';
  return s;
}

std::ostream &operator<< (std::ostream &s, std::vector<Engine::Frame> const &vec)
{
  s << "{\n";
  for (unsigned ix = 0, len = vec.size (); ix != len; ix++)
    {
      if (ix)
	s << ",\n";
      s << '[' << ix << ']' << vec[ix];
    }
  s << '}';
  return s;
}

std::ostream &operator<< (std::ostream &s, Engine::Frame const &f)
{
  s << '{';
  char const *eol = "";
  if (!f.nones.empty ())
    {
      s << "nones=" << f.nones;
      eol = "\n";
    }

  if (auto h = f.head)
    {
      s << eol << "head=" << *h;
      eol = "\n";
    }

  if (!f.body.empty ())
    s << eol << "body=" << f.body;

  s << '}';

  return s;
}

std::ostream &operator<< (std::ostream &s, Engine const &e)
{
  s << '{';
  char const *eol = "";
  if (!e.nevers.empty ())
    {
      s << "nevers=" << e.nevers;
      eol = "\n";
    }
  
  if (!e.frames.empty ())
    s << eol << "frames=" << e.frames;

  s << '}';

  return s;
}

void Engine::Add (Pattern *p)
{
  if (!p->Errored ())
    p->Canonicalize ();

  switch (p->GetKind ())
    {
    case Pattern::NEVER:
      nevers.push_back (p);
      break;

    case Pattern::BLOCK:
      if (frames.empty ())
	frames.push_back (Frame ());
      frames.push_back (Frame (p));
      break;

    default:
      {
	if (frames.empty ())
	  frames.push_back (Frame ());

	auto &frame = frames.back ();
	if (p->GetKind () == Pattern::NONE)
	  frame.nones.push_back (p);
	else
	  {
	    if (p->GetKind () == Pattern::NOT)
	      if ((frame.body.empty () && !frame.head)
		  || (frame.body.back ()->GetKind () != Pattern::MATCH
		      && frame.body.back ()->GetKind () != Pattern::NEXT))
		p->Error () << "pattern requires positive match predecessor";
	    frame.body.push_back (p);
	  }
      }
      break;
    }
}

void Engine::BodyAdvance (unsigned bIndex)
{
  Frame const &frame = frames[frameIndex];

  for (unsigned ix = bodyIndex; ix != bIndex; ix++)
    {
      auto *p = frame.body[ix];

      if (p->Errored ())
	p->Result (Tester::ERROR);
      else if (p->GetKind () != Pattern::NOT)
	p->Result (false);
      else if (!p->Matched ())
	p->Result (true);
    }
  bodyIndex = bIndex;
}

void Engine::FrameAdvance (unsigned fIndex)
{
  bool head = false;
  for (; frameIndex != fIndex; frameIndex++,  head = true, bodyIndex = 0)
    {
      Frame const &frame = frames[frameIndex];

      if (head && frame.head)
	{
	  if (frame.head->Errored ())
	    frame.head->Result (Tester::ERROR);
	  else
	    frame.head->Result (false);
	}

      BodyAdvance (frame.body.size ());
      for (auto none = frame.nones.begin (); none != frame.nones.end (); ++none)
	{
	  auto *p = *none;
	  if (p->Errored ())
	    p->Result (Tester::ERROR);
	  else if (!p->Matched ())
	    p->Result (true);
	}
    }

  bodyIndex = 0;

  if (frameIndex == frames.size ())
    for (auto never = nevers.begin (); never != nevers.end (); ++never)
      {
	auto *p = *never;
	if (p->Errored ())
	  p->Result (Tester::ERROR);
	else if (!p->Matched ())
	  p->Result (true);
      }
}

void Engine::Initialize (Scanner &scanner)
{
  if (frames.empty () && nevers.empty ())
    scanner.Error () << "nothing to check";
}

void Engine::Process (std::string_view const &text, bool eof)
{
  // If any block-head matches, advance to that block
  // Otherwise if body matches, advance to next body
  // Othterwise if body is NEXT, fail and repeat
  // Otherwise if any none or never matches, fail

  if (frameIndex == frames.size ())
    return;

  bool maybe_captures = false;

  // Look in the current frame
  Frame &frame = frames[frameIndex];
  for (unsigned ix = bodyIndex; ix != frame.body.size (); ix++)
    {
      auto *p = frame.body[ix];
      auto expn = p->Expand (syms);
      if (expn != Regex::FOUND)
	{
	  if (expn == Regex::NOTFOUND)
	    p->Error () << "match contains unexpandable escape";
	  continue;
	}

      switch (p->GetKind ())
	{
	case Pattern::NEXT:
	  Assert (ix == bodyIndex);
	  [[fallthrough]];
	case Pattern::MATCH:
	  {
	    auto cmp = p->Compare (syms, text, eof);
	    if (cmp == Regex::FAILED)
	      continue;

	    if (cmp == Regex::FOUND)
	      {
		BodyAdvance (ix);
		p->Result (true);
		bodyIndex = ix + 1;
		return;
	      }

	    if (p->GetKind () == Pattern::NEXT)
	      {
		p->Result (false);
		bodyIndex = ix + 1;
	      }
	    else
	      maybe_captures = true;
	  }
	  goto no_match;

	case Pattern::NOT:
	  if (p->Compare (syms, text, eof) == Regex::FOUND)
	    p->Result (false);
	  continue;

	default:
	  Unreachable ();
	}
    }
 no_match:;

  // Look in future frames
  for (unsigned ix = frameIndex + 1; ix < frames.size (); ix++)
    {
      auto *p = frames[ix].head;

      if (!p)
	continue;

      auto expn = p->Expand (syms);
      if (expn != Regex::FOUND)
	{
	  if (expn == Regex::NOTFOUND && !maybe_captures && ix == frameIndex + 1)
	    p->Error () << "block contains unexpandable escape";
	  continue;
	}

      if (p->Compare (syms, text, eof) != Regex::FOUND)
	continue;

      FrameAdvance (ix);
      p->Result (true);
      return;
    }

  // Look for NONEs
  for (auto none = frame.nones.begin (); none != frame.nones.end (); ++none)
    {
      auto *p = *none;
      if (p->Expand (syms) == Regex::FOUND
	  && p->Compare (syms, text, eof) == Regex::FOUND)
	p->Result (false);
    }

  // Look for NEVERs
  for (auto never = nevers.begin (); never != nevers.end (); ++never)
    {
      auto *p = *never;
      if (p->Expand (syms) == Regex::FOUND
	  && p->Compare (syms, text, eof) == Regex::FOUND)
	p->Result (false);
    }
}

void Engine::Process (char const *file)
{
  int fd = 0;
  if (file[0] != '-' || file[1])
    {
      fd = open (file, O_RDONLY | O_CLOEXEC);
      if (fd < 0)
	{
	fatal:
	  Fatal ("cannot read test file '%s': %m", file);
	}
    }

  size_t len = [] (int fd_)
	       {
		 struct stat stat;
		 return fstat (fd_, &stat) ? ~size_t (0) :
		   (stat.st_mode & S_IFMT) == S_IFREG ? stat.st_size : 0;} (fd);
  if (len == ~size_t (0))
    goto fatal;

  size_t alloc = 0;
  void *buffer = nullptr;

  if (!len)
    {
      // Not a pageable file.  Read until EOF
      alloc = 0x10000;
      buffer = mmap (nullptr, alloc, PROT_READ | PROT_WRITE,
		     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
      if (buffer == MAP_FAILED)
	goto fatal;

      // We need to make sure there's at least 1 byte allocated and
      // not used, to deal with the \n appending below.
      ssize_t l;
      do
	{
	  l = read (fd, reinterpret_cast<char *>(buffer) + len, alloc - len);

	  if (l < 0)
	    goto fatal;

	  len += l;
	  if (len == alloc)
	    {
	      // mremap is linux-specific.  Sue me.
	      buffer = mremap (buffer, alloc, alloc + 0x10000, MREMAP_MAYMOVE);
	      if (buffer == MAP_FAILED)
		goto fatal;
	      alloc += 0x10000;
 	    }
	}
      while (l);
    }
  else
    {
      size_t page_size = sysconf (_SC_PAGE_SIZE);
      alloc = (len + page_size) & ~(page_size - 1);
      buffer = mmap (nullptr, alloc, PROT_READ | PROT_WRITE,
		     MAP_PRIVATE, fd, 0);
      if (buffer == MAP_FAILED)
	goto fatal;
      // Don't really care about error code
      madvise (buffer, alloc, MADV_SEQUENTIAL);
    }
  close (fd);

  auto *begin = reinterpret_cast <char const *> (buffer);
  auto *end = begin + len;

  // Ensure the template ends in a newline
  if (len && end[-1] != '\n')
    *const_cast <char *> (end++) = '\n';

  for (unsigned line = 1; begin != end; line++)
    {
      auto *eol = std::find (begin, end, '\n');

      Process (std::string_view (begin, eol), false);

      begin = eol + 1;
    }

  munmap (buffer, alloc);
}

void Engine::Finalize ()
{
  Process ("", true);
  FrameAdvance (frames.size ());
}
