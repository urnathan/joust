// Joust/EZIO: Expect Zero Irregularities Observed	-*- mode:c++ -*-
// Copyright (C) 2020-2024 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

class Engine : public Tester {
public:
  using Parent = Tester;

private:
  struct Frame {
    std::vector<Pattern *> None;
    std::vector<Pattern *> Body;

  public:
    Frame (Pattern *head = nullptr) { Body.emplace_back(head); }
    Frame (Frame &&from)
      : None(std::move(from.None)), Body(std::move(from.Body)) {}
    ~Frame ();

  private:
    Frame &operator= (Frame &&) = delete;
  };
  using BodyIter = std::vector<Pattern *>::iterator;
  using FrameIter = std::vector<Frame>::iterator;

private:
  Symbols &Syms;
  std::vector<Pattern *> Nevers;
  std::vector<Frame> Frames;
  FrameIter CurrentFrame; // Current frame
  BodyIter CurrentBody;   // Current element

public:
  Engine (Symbols &syms, std::ostream &sum, std::ostream &log)
    : Parent(sum, log), Syms(syms) {}
  ~Engine ();

private:
  Engine (Engine &&) = delete;
  Engine &operator= (Engine &&) = delete;

public:
  void advanceBody(BodyIter);

public:
  // Advance to frame
  void advanceFrame(FrameIter);

public:
  void process (char const *file);
  void process (std::string_view const &line, bool eof);

public:
  // Add pattern to frames or nevers, might start a new frame
  void add (Pattern *);

public:
  void initialize ();
  void finalize ();

  friend std::ostream &operator<< (std::ostream &, Engine const &);
  friend std::ostream &operator<< (std::ostream &s, Frame const &e);
  friend std::ostream &operator<< (std::ostream &s,
                                   std::vector<Frame> const &);
};

Engine::Frame::~Frame () {
  for (auto *n : None)
    delete n;
  for (auto *b : Body)
    delete b;
}

Engine::~Engine () {
  for (auto *n : Nevers)
    delete n;
}

std::ostream &operator<< (std::ostream &s, std::vector<Pattern *> const &vec) {
  for (auto const *p : vec)
    if (p)
      s << *p;

  return s;
}

std::ostream &operator<< (std::ostream &s,
                          std::vector<Engine::Frame> const &vec) {
  for (auto const &f : vec)
    s << f;

  return s;
}

std::ostream &operator<< (std::ostream &s, Engine::Frame const &f) {
  s << f.Body << f.None;

  return s;
}

std::ostream &operator<< (std::ostream &s, Engine const &e) {
  s << e.Frames << e.Nevers;

  return s;
}

void Engine::add (Pattern *p) {
  if (!p->hasError())
    p->Canonicalize();

  switch (p->kind()) {
  case Pattern::NEVER:
    // Add to the nevers
    Nevers.push_back(p);
    break;

  case Pattern::LABEL:
    // Start a new frame
    Frames.emplace_back(p);
    break;

  case Pattern::NONE:
    // Append to the current frame's nones
    if (Frames.empty())
      Frames.emplace_back();

    Frames.back().None.emplace_back(p);
    break;

  default:
    // Append to the current frame's body
    if (Frames.empty())
      Frames.emplace_back();

    Frames.back().Body.emplace_back(p);
    break;
  }
}

void Engine::advanceBody (BodyIter bodyIx) {
  for (; CurrentBody != bodyIx; ++CurrentBody)
    (*CurrentBody)->result(*this);
}

void Engine::advanceFrame (FrameIter frameIx) {
  while (CurrentFrame != frameIx) {
    advanceBody(CurrentFrame->Body.end());
    for (auto &None : CurrentFrame->None)
      None->result(*this);
    ++CurrentFrame;
    if (CurrentFrame != Frames.end())
      CurrentBody = CurrentFrame->Body.begin();
  }
}

void Engine::initialize () {
  if (Frames.empty()) {
    if (Nevers.empty())
      fatalExit("nothing to check");
    Frames.emplace_back();
  }
  CurrentFrame = Frames.begin();
  CurrentBody = CurrentFrame->Body.begin();
  if (!*CurrentBody)
    ++CurrentBody;
}

void Engine::process (std::string_view const &text, bool eof) {
  bool maybe_captures = false;

  if (CurrentBody != CurrentFrame->Body.begin()) {
    // We're inside a frame, process it
    for (auto peek = CurrentBody; peek != CurrentFrame->Body.end(); ++peek) {
      auto *p = *peek;

      if (p->hasError())
        continue;

      if (p->kind() < Pattern::CAPTURE_HWM && p->hasMatch())
        continue;

      auto expn = p->Expand(Syms);
      if (expn != regex::FOUND) {
        // We couldn't compile the pattern
        if (!maybe_captures && expn == regex::NOTFOUND)
          p->error() << Pattern::KindNames[p->kind()]
                     << " contains unexpandable escape";
        continue;
      }

      auto cmp = p->Compare(Syms, text, eof);
      if (cmp == regex::FAILED)
        continue;

      switch (p->kind()) {
      case Pattern::DAG:
        if (cmp == regex::FOUND) {
          p->match(*this);
          return;
        }
        if (p->hasCapture())
          maybe_captures = true;
        continue;

      case Pattern::NEXT:
      case Pattern::MATCH:
        if (cmp == regex::FOUND) {
          advanceBody(peek);
          p->match(*this);
          CurrentBody = ++peek;
          return;
        }

        if (p->kind() == Pattern::NEXT)
          advanceBody(++peek);
        else if (p->hasCapture())
          maybe_captures = true;
        goto no_match;

      case Pattern::NOT:
        if (cmp == regex::FOUND)
          p->match(*this);
        continue;

      default:
        unreachable();
      }
    }
  no_match:;
  }

  // Look if any HEAD matches, and if so advance to just after it
  bool first = true;
  for (auto framePeek
       = CurrentFrame + bool(CurrentBody != CurrentFrame->Body.begin());
       framePeek != Frames.end(); ++framePeek, first = false) {
    Pattern *p = framePeek->Body.front();

    assert(p);

    auto expn = p->Expand(Syms);
    if (expn != regex::FOUND) {
      // Failed to expand it, ignore this but check if definitely
      // an error
      if (expn == regex::NOTFOUND && !maybe_captures && first)
        p->error() << "LABEL contains unexpandable escape";
      continue;
    }

    if (p->Compare(Syms, text, eof) != regex::FOUND)
      continue;

    // We've found a matching LABEL, advance and be done
    advanceFrame(framePeek);
    p->match(*this);
    ++CurrentBody;

    return;
  }

  // Failed a positive match, check the negative ones

  // Look for NONEs
  if (CurrentFrame != Frames.end())
    for (auto const &none : CurrentFrame->None)
      if (none->Expand(Syms) == regex::FOUND
          && none->Compare(Syms, text, eof) == regex::FOUND)
        none->match(*this);

  // Look for NEVERs
  for (auto const &never : Nevers)
    if (never->Expand(Syms) == regex::FOUND
        && never->Compare(Syms, text, eof) == regex::FOUND)
      never->match(*this);
}

void Engine::process (char const *file) {
  int fd = 0;
  if (file[file[0] == '-']) {
    fd = open(file, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
    fatal:
      fatalExit("cannot read test file '%s': %m", file);
    }
  }

  size_t len = [] (int fd_) {
    struct stat stat;
    return fstat(fd_, &stat)                    ? ~size_t(0)
           : (stat.st_mode & S_IFMT) == S_IFREG ? stat.st_size
                                                : 0;
  }(fd);
  if (len == ~size_t(0))
    goto fatal;

  size_t alloc = 0;
  void *buffer = nullptr;

  if (!len) {
    // Not a pageable file.  Read until EOF
    alloc = 0x10000;
    buffer = mmap(nullptr, alloc, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (buffer == MAP_FAILED)
      goto fatal;

    // We need to make sure there's at least 1 byte allocated and
    // not used, to deal with the \n appending below.
    ssize_t l;
    do {
      l = read(fd, reinterpret_cast<char *>(buffer) + len, alloc - len);

      if (l < 0)
        goto fatal;

      len += l;
      if (len == alloc) {
        void *ext;
#ifdef HAVE_MREMAP
        // mremap is linux-specific.
        ext = mremap(buffer, alloc, alloc * 2, MREMAP_MAYMOVE);
#else
        // Sadly we can't just try mapping exactly at the right
        // place, because that'll zap any existing mapping that
        // we don't know about (MAP_FIXED_NOREPLACE is also
        // linux-specific)
        ext = mmap(nullptr, alloc * 2, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
        if (ext == MAP_FAILED)
          goto fatal;
#ifndef HAVE_MREMAP
        memcpy(ext, buffer, alloc);
        munmap(buffer, alloc);
#endif
        buffer = ext;
        alloc *= 2;
      }
    } while (l);
  } else {
    size_t page_size = sysconf(_SC_PAGE_SIZE);
    alloc = (len + page_size) & ~(page_size - 1);
    buffer = mmap(nullptr, alloc, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (buffer == MAP_FAILED)
      goto fatal;

    // Don't really care about error code
    madvise(buffer, alloc, MADV_SEQUENTIAL);
  }
  close(fd);

  auto *begin = reinterpret_cast<char const *>(buffer);
  auto *end = begin + len;

  // Ensure the file ends in a newline
  if (len && end[-1] != '\n')
    *const_cast<char *>(end++) = '\n';

  for (unsigned line = 1; begin != end; line++) {
    auto *eol = std::find(begin, end, '\n');
    auto text = std::string_view(begin, eol);
    log() << line << ':' << text << '\n';
    process(text, false);

    begin = eol + 1;
  }

  munmap(buffer, alloc);
}

void Engine::finalize () {
  process("", true);
  advanceFrame(Frames.end());
  for (auto const &never : Nevers)
    never->result(*this);
}
