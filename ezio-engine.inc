// Joust/EZIO: Expect Zero Irregularities Observed	-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

class Engine : public Logger
{
public:
  using Parent = Logger;

private:
  struct Frame 
  {
    std::vector<Pattern *> none;
    std::vector<Pattern *> body;

  public:
    Frame (Pattern *head = nullptr)
    {
      body.emplace_back (head);
    }
    Frame (Frame &&from)
      : none (std::move (from.none)),
	body (std::move (from.body))
    {
    }
    ~Frame ();

  private:
    Frame &operator= (Frame &&) = delete;
    
  };
  using BodyIter = std::vector<Pattern *>::iterator;
  using FrameIter = std::vector<Frame>::iterator;

private:
  Symbols &syms;
  std::vector<Pattern *> nevers;
  std::vector<Frame> frames;
  FrameIter frameIter; // Current frame
  BodyIter bodyIter; // Current element

public:
  Engine (Symbols &syms_, std::ostream &sum_, std::ostream &log_)
    : Parent (sum_, log_), syms (syms_)
  {
  }
  ~Engine ();

private:
  Engine (Engine &&) = delete;
  Engine &operator= (Engine &&) = delete;

public:
  void BodyAdvance (BodyIter);

public:
  // Advance to frame
  void FrameAdvance (FrameIter);

public:
  void Process (char const *file);
  void Process (std::string_view const &line, bool eof);

public:
  // Add pattern to frames or nevers, might start a new frame
  void Add (Pattern *);

public:
  void Initialize ();
  void Finalize ();

  friend std::ostream &operator<< (std::ostream &, Engine const &);
  friend std::ostream &operator<< (std::ostream &s, Frame const &e);
  friend std::ostream &operator<< (std::ostream &s, std::vector<Frame> const &);
};

Engine::Frame::~Frame ()
{
  for (auto *n : none)
    delete n;
  for (auto *b : body)
    delete b;
}

Engine::~Engine ()
{
  for (auto *n : nevers)
    delete n;
}

std::ostream &operator<< (std::ostream &s, std::vector<Pattern *> const &vec)
{
  for (auto iter = vec.begin (); iter != vec.end (); ++iter)
    if (auto p = *iter)
      s << *p;

  return s;
}

std::ostream &operator<< (std::ostream &s, std::vector<Engine::Frame> const &vec)
{
  for (auto iter = vec.begin (); iter != vec.end (); ++iter)
    s << *iter;

  return s;
}

std::ostream &operator<< (std::ostream &s, Engine::Frame const &f)
{
  s << f.body << f.none;
 
  return s;
}

std::ostream &operator<< (std::ostream &s, Engine const &e)
{
  s << e.frames << e.nevers;

  return s;
}

void Engine::Add (Pattern *p)
{
  if (!p->Errored ())
    p->Canonicalize ();

  switch (p->GetKind ())
    {
    case Pattern::NEVER:
      // Add to the nevers
      nevers.push_back (p);
      break;

    case Pattern::LABEL:
      // Start a new frame
      frames.emplace_back (p);
      break;

    case Pattern::NONE:
      // Append to the current frame's nones
      if (frames.empty ())
	frames.emplace_back ();

      frames.back ().none.emplace_back (p);
      break;

    default:
      // Append to the current frame's body
      if (frames.empty ())
	frames.emplace_back ();

      frames.back ().body.emplace_back (p);
      break;
    }
}

void Engine::BodyAdvance (BodyIter bodyIx)
{
  for (Frame const &frame = *frameIter;
       bodyIter != bodyIx; ++bodyIter)
    (*bodyIter)->Result (*this);
}

void Engine::FrameAdvance (FrameIter frameIx)
{
  while (frameIter != frameIx)
    {
      BodyAdvance (frameIter->body.end ());
      for (auto iter = frameIter->none.begin ();
	   iter != frameIter->none.end (); ++iter)
	(*iter)->Result (*this);
      ++frameIter;
      if (frameIter != frames.end ())
	bodyIter = frameIter->body.begin ();
    }
}

void Engine::Initialize ()
{
  if (frames.empty ())
    {
      if (nevers.empty ())
	Fatal ("nothing to check");
      frames.emplace_back ();
    }
  frameIter = frames.begin ();
  bodyIter = frameIter->body.begin ();
  if (!*bodyIter)
    ++bodyIter;
}

void Engine::Process (std::string_view const &text, bool eof)
{
  bool maybe_captures = false;

  if (bodyIter != frameIter->body.begin ())
    {
      // We're inside a frame, process it
      for (auto peek = bodyIter; peek != frameIter->body.end (); ++peek)
	{
	  auto *p = *peek;

	  if (p->Errored ())
	    continue;

	  if (p->GetKind () < Pattern::CAPTURE_HWM && p->Matched ())
	    continue;

	  auto expn = p->Expand (syms);
	  if (expn != Regex::FOUND)
	    {
	      // We couldn't compile the pattern
	      if (!maybe_captures && expn == Regex::NOTFOUND)
		p->Error () << Pattern::kinds[p->GetKind ()]
			    << " contains unexpandable escape";
	      continue;
	    }

	    auto cmp = p->Compare (syms, text, eof);
	    if (cmp == Regex::FAILED)
	      continue;

	    switch (p->GetKind ())
	      {
	      case Pattern::DAG:
		if (cmp == Regex::FOUND)
		  {
		    p->Matched (*this);
		    return;
		  }
		if (p->Captures ())
		  maybe_captures = true;
		continue;

	      case Pattern::NEXT:
	      case Pattern::MATCH:
		if (cmp == Regex::FOUND)
		  {
		    BodyAdvance (peek);
		    p->Matched (*this);
		    bodyIter = ++peek;
		    return;
		  }

		if (p->GetKind () == Pattern::NEXT)
		  BodyAdvance (++peek);
		else if (p->Captures ())
		  maybe_captures = true;
		goto no_match;

	      case Pattern::NOT:
		if (cmp == Regex::FOUND)
		  p->Matched (*this);
		continue;

	      default:
		Unreachable ();
	      }
	  }
    no_match:;
    }

  // Look if any HEAD matches, and if so advance to just after it
  bool first = true;
  for (auto framePeek = frameIter + bool (bodyIter != frameIter->body.begin ());
       framePeek != frames.end (); ++framePeek, first = false)
    {
      Pattern *p = framePeek->body.front ();

      Assert (p);

      auto expn = p->Expand (syms);
      if (expn != Regex::FOUND)
	{
	  // Failed to expand it, ignore this but check if definitely
	  // an error
	  if (expn == Regex::NOTFOUND && !maybe_captures && first)
	    p->Error () << "LABEL contains unexpandable escape";
	  continue;
	}

      if (p->Compare (syms, text, eof) != Regex::FOUND)
	continue;

      // We've found a matching LABEL, advance and be done
      FrameAdvance (framePeek);
      p->Matched (*this);
      ++bodyIter;

      return;     
    }

  // Failed a positive match, check the negative ones

  // Look for NONEs
  if (frameIter != frames.end ())
    for (auto iter = frameIter->none.begin ();
	 iter != frameIter->none.end (); ++iter)
      {
	auto *p = *iter;
	if (p->Expand (syms) == Regex::FOUND
	    && p->Compare (syms, text, eof) == Regex::FOUND)
	  p->Matched (*this);
      }

  // Look for NEVERs
  for (auto iter = nevers.begin (); iter != nevers.end (); ++iter)
    {
      auto *p = *iter;
      if (p->Expand (syms) == Regex::FOUND
	  && p->Compare (syms, text, eof) == Regex::FOUND)
	p->Matched (*this);
    }
}

void Engine::Process (char const *file)
{
  int fd = 0;
  if (file[file[0] == '-'])
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
		   (stat.st_mode & S_IFMT) == S_IFREG ? stat.st_size : 0;
	       } (fd);
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
	      void *ext;
#ifdef __linux__
	      // mremap is linux-specific.
	      ext = mremap (buffer, alloc, alloc * 2, MREMAP_MAYMOVE);
#else
	      // Sadly we can't just try mapping exactly at the right
	      // place, because that'll zap any existing mapping that
	      // we don't know about (MAP_FIXED_NOREPLACE is also
	      // linux-specific)
	      ext = mmap (nullptr, alloc * 2, PROT_READ | PROT_WRITE,
			  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
	      if (ext == MAP_FAILED)
		goto fatal;
#ifndef __linux__
	      memcpy (ext, buffer, alloc);
	      munmap (buffer, alloc);
#endif
	      buffer = ext;
	      alloc *= 2;
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

  // Ensure the file ends in a newline
  if (len && end[-1] != '\n')
    *const_cast <char *> (end++) = '\n';

  for (unsigned line = 1; begin != end; line++)
    {
      auto *eol = std::find (begin, end, '\n');
      auto text = std::string_view (begin, eol);
      Log () << line << ':' << text << '\n';
      Process (text, false);

      begin = eol + 1;
    }

  munmap (buffer, alloc);
}

void Engine::Finalize ()
{
  Process ("", true);
  FrameAdvance (frames.end ());
  for (auto iter = nevers.begin (); iter != nevers.end (); ++iter)
    (*iter)->Result (*this);
}
