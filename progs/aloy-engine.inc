// Joust/ALOY: Apply List, Observe Yield		-*- mode:c++ -*-
// Copyright (C) 2020-2024 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#if !defined(ALOY_ENGINE)
#define ALOY_ENGINE
class Engine : public Tester {
public:
  using Parent = Tester;

private:
  enum class FDs {
    MAKE_IN,  // make_in
    MAKE_OUT, // make_out
    SIGNAL,   // sig_fd
    HWM
  };

private:
  std::deque<Job> Jobs;
  std::vector<std::string> Command;
  std::string UsedTokens;  // tokens to send back to make
  std::string ReadyTokens; // tokens we've got from make
  Job Generator;

private:
  unsigned JobLimit = 1;  // static number of jobs we can spawn
  unsigned FixedJobs = 0; // non-jobserver jobs running
  unsigned MakeWant = 0;
  int MakeIn = -1, MakeOut = -1;

private:
#ifdef USE_EPOLL
  int SigFD = -1;
  int PollFD = -1;
#else
  static int const PollFD = -1;
  sigset_t SigSelect;
  struct sigaction *OldActions = nullptr;
#endif

private:
  bool Stopping = false;

private:
  unsigned Retired = 0;   // Jobs written out
  unsigned Completed = 0; // Jobs completed, but waiting to write
  unsigned Running = 0;   // Jobs running
  unsigned Pending = 0;   // Jobs waiting to run

private:
  unsigned Counts[STATUS_HWM];

public:
  Engine(unsigned limit, std::ostream &sum, std::ostream &log);
  ~Engine();

public:
  bool isLive () const {
    return !Generator.isReady() || !Jobs.empty() || !UsedTokens.empty()
           || !ReadyTokens.empty();
  }
  void init (char const *tester, std::vector<std::string> *genner, int argc,
             char const *const argv[]);
  void fini (std::ostream * = nullptr);
  void stop (int sig);
  void process ();
  void retire (std::ostream * = nullptr);
  void fini (Job const &, std::ostream * = nullptr, bool is_generator = false);
  void spawn ();
  std::string getProgress ();

public:
  auto result (Statuses status) {
    Counts[status]++;
    return Parent::result(status, SrcLoc());
  }

private:
  void readGenerator ();
  void handleSignal (int sig);

private:
  void stopMake (int);
  void wantMake ();
  void readMake ();
  void writeMake ();
  void queueMake (int token);

  friend std::ostream &operator<< (std::ostream &, Engine const &);
};

#else

static constexpr unsigned char sigs[]
    = {SIGHUP, SIGQUIT, SIGPIPE, SIGCHLD, SIGTERM};

Engine::Engine(unsigned limit, std::ostream &sum, std::ostream &log)
  : Parent(sum, log), JobLimit(limit) {
  for (unsigned ix = STATUS_HWM; ix--;)
    Counts[ix] = 0;

  // Get jobserver FDs
  if (JobLimit)
    ;
  else if (char const *makeflags = getenv("MAKEFLAGS")) {
    // --jobserver-auth=RN,WN
    std::string_view mflags(makeflags);
    constexpr std::string_view jsa = "--jobserver-auth=";
    bool first = true;

    while (!mflags.empty()) {
      auto not_space = mflags.find_first_not_of(' ');
      if (not_space == mflags.npos)
        not_space = mflags.size();
      auto space = mflags.find_first_of(' ', not_space);
      if (space == mflags.npos)
        space = mflags.size();

      auto option = mflags.substr(not_space, space - not_space);
      mflags.remove_prefix(space);

      if (option == "--")
        break;
      else if (option.starts_with(jsa)) {
        option.remove_prefix(jsa.size());
        Lexer lexer(option);
        struct stat stat_buf;

        if (!lexer.isInteger() || lexer.peekAdvanceChar() != ','
            || !lexer.isInteger() || lexer.peekChar()
            || (MakeIn = lexer.getToken()->integer(),
                MakeOut = lexer.getToken()->integer(),
                fstat(MakeIn, &stat_buf) < 0
                    || (stat_buf.st_mode & S_IFMT) != S_IFIFO
                    || fstat(MakeOut, &stat_buf) < 0
                    || (stat_buf.st_mode & S_IFMT) != S_IFIFO)) {
          /* Not available after all.  */
          std::cerr << "MAKEFLAGS" << '=' << makeflags
                    << (" specifies unusable jobserver"
                        " (prefix command with '+'?)\n");
          MakeIn = MakeOut = -1;
        }
        break;
      } else if (first) {
        if (option.find_first_of('n') != option.npos)
          // Non-execution
          Command.clear();
        first = false;
      }
    }

    // We can always spawn one job without asking the jobserver
    JobLimit = 1;
  } else
    JobLimit = 1;
}

Engine::~Engine() {}

std::ostream &operator<< (std::ostream &s, Engine const &self) {
  for (unsigned ix = 0; ix != Tester::STATUS_HWM; ix++)
    if (ix == Tester::PASS || self.Counts[ix])
      s << self.StatusNames[ix] << ' ' << self.Counts[ix] << '\n';

  return s;
}

#ifndef USE_EPOLL
// Signals we saw
volatile unsigned sigs_seen;

static void noteSignal (int signal) {
  for (unsigned ix = sizeof(sigs); ix--;)
    if (signal == sigs[ix]) {
      sigs_seen = sigs_seen | (1 << ix);
      break;
    }
}
#endif

void Engine::init(char const *tester, std::vector<std::string> *genner,
                  int argc, char const *const argv[]) {
  auto now = time(nullptr);
  *this << "Test run: " << ctime(&now) << "\n";

  // Initialize command vector and gen vector
  Command.emplace_back(tester);
  bool dashed = false;
  for (int ix = 0; ix != argc; ix++) {
    std::string_view arg(argv[ix]);
    if (!dashed && arg == "--") {
      Command.reserve(ix + 1);
      for (auto &word : Generator.command())
        Command.emplace_back(std::move(word));
      Generator.command().clear();
      dashed = true;
    } else
      Generator.command().emplace_back(arg);
  }

  // Signal fd and block sigchild
  sigset_t sigmask;
  sigemptyset(&sigmask);
  for (unsigned ix = sizeof(sigs); ix--;)
    sigaddset(&sigmask, sigs[ix]);
#ifdef USE_EPOLL
  while (sigprocmask(SIG_BLOCK, &sigmask, nullptr) < 0)
    assert(errno == EINTR);

  SigFD = signalfd(-1, &sigmask, SFD_NONBLOCK | SFD_CLOEXEC);
  assert(SigFD >= 0);

  // Epoll fd
  PollFD = epoll_create1(EPOLL_CLOEXEC);
  assert(PollFD >= 0);

  epoll_event ev;
  ev.events = EPOLLIN;
  ev.data.u64 = unsigned(FDs::SIGNAL);
  while (epoll_ctl(PollFD, EPOLL_CTL_ADD, SigFD, &ev) < 0)
    assert(errno == EINTR || errno == EAGAIN);
#else
  while (sigprocmask(SIG_UNBLOCK, &sigmask, nullptr) < 0)
    assert(errno == EINTR);
  while (sigprocmask(SIG_BLOCK, &sigmask, &sigselect) < 0)
    assert(errno == EINTR);
  old_actions = new struct sigaction[sizeof(sigs)];
  // Install signal handlers
  for (unsigned ix = sizeof(sigs); ix--;) {
    struct sigaction action;
    action.sa_handler = NoteSignal;
    sigfillset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(sigs[ix], &action, &old_actions[ix]);
  }
#endif

  if (genner) {
    if (Generator.spawn(*this, *genner, PollFD))
      FixedJobs++;
  } else {
    // Create pending job queue
    for (auto &word : Generator.command()) {
      Jobs.emplace_back(std::move(word));
      Pending++;
    }
    Generator.command().clear();
  }
}

void Engine::fini(std::ostream *summary) {
  fini(Generator, summary, true);

  if (summary)
    *summary << "# Summary of " << Retired << " test programs \n" << *this;
  if (Retired)
    sum() << '\n';
  *this << "# Summary of " << Retired << " test programs \n" << *this;

  flush();

#ifdef USE_EPOLL
  close(PollFD);
  PollFD = -1;
  close(SigFD);
  SigFD = -1;
#else
  // Restore signal handlers
  for (unsigned ix = sizeof(sigs); ix--;)
    sigaction(sigs[ix], &OldActions[ix], nullptr);
  delete[] OldActions;
  OldActions = nullptr;
#endif
}

void Engine::readGenerator() {
  // Lex
  auto &buffer = Generator.buffer(0);
  auto first = buffer.begin();
  auto last = buffer.end();
  while (first != last) {
    auto eol = std::find(first, last, '\n');
    if (eol == last && buffer.isOpen())
      break;

    std::string_view line(first, eol);
    first = eol + buffer.isOpen();

    bool sol = true;
    for (size_t pos = 0;; sol = false) {
      pos = line.find_first_not_of(' ', pos);
      if (pos == line.npos)
        break;
      if (sol && line[pos] == '#')
        break;

      size_t end = line.find_first_of(' ', pos);
      if (end == line.npos)
        end = line.size();

      Jobs.emplace_back(line.substr(pos, end - pos));
      Pending++;
      pos = end;
    }
  }

  // Adjust
  if (first != buffer.begin())
    buffer.erase(buffer.begin(), first);

  wantMake();
}

void Engine::handleSignal(int sig) {
  switch (sig) {
  case SIGCHLD:
    // Get child status.  There could be multiple children to deal with.
    {
      int status;
      while (pid_t child = waitpid(-1, &status, WNOHANG)) {
        if (child == pid_t(-1)) {
          assert(!Running && Generator.isReady());
          break;
        }

        if (WIFEXITED(status) || WIFSIGNALED(status)) {
          if (Generator.isPid(child)) {
            Generator.reap(status);
            FixedJobs--;
          } else {
            auto iter = Jobs.begin();
            for (unsigned limit = Completed + Running; limit--; ++iter)
              if (iter->isPid(child)) {
                int token = iter->reap(status);
                if (token != -1)
                  queueMake(token);
                else
                  FixedJobs--;

                Completed++;
                Running--;
                break;
              }
          }
        }
        if (!Running && !Generator.isReady())
          break;
      }
    }
    break;

  case SIGHUP:
  case SIGINT:
  case SIGQUIT:
  case SIGTERM:
    stop(sig);
    break;

  case SIGPIPE:
  default:
    unreachable();
  }
}

void Engine::stop(int sig) {
  Stopping = true;
  Generator.stop(sig);
  if (Pending)
    do
      Jobs.pop_back();
    while (--Pending);
  for (auto &job : Jobs)
    job.stop(sig);
}

void Engine::wantMake() {
  if (MakeIn >= 0) {
    unsigned want = Pending;
    unsigned ready = ReadyTokens.size() + (JobLimit - FixedJobs);
    if (want < ready)
      want = 0;
    else
      want -= ready;

#ifdef USE_EPOLL
    if (!want) {
      if (MakeWant)
        while (epoll_ctl(PollFD, EPOLL_CTL_DEL, MakeIn, nullptr) < 0)
          assert(errno == EINTR || errno == EAGAIN);
    } else {
      if (!MakeWant) {
        epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.u64 = unsigned(FDs::MAKE_IN);
        while (epoll_ctl(PollFD, EPOLL_CTL_ADD, MakeIn, &ev) < 0)
          assert(errno == EINTR || errno == EAGAIN);
      }
    }
#endif
    MakeWant = want;
  }
}

void Engine::stopMake(int err) {
  assert(MakeOut >= 0 && MakeIn >= 0);
#ifdef USE_EPOLL
  epoll_ctl(PollFD, EPOLL_CTL_DEL, MakeOut, nullptr);
  epoll_ctl(PollFD, EPOLL_CTL_DEL, MakeIn, nullptr);
#endif
  MakeOut = MakeIn = -1;
  UsedTokens.clear();
  result(Tester::ERROR) << " terminating jobserver:" << strerror(err);
}

void Engine::readMake() {
  unsigned const block_size = 16;
  char buffer[block_size];

  unsigned want = MakeWant < block_size ? MakeWant : block_size;
  ssize_t got = read(MakeIn, buffer, want);
  if (got < 0) {
    if (errno != EINTR && errno != EAGAIN)
      stopMake(errno);
  } else
    ReadyTokens.append(buffer, got);

  wantMake();
}

// Write the token back to jobcontrol to
// allow other processes a chance.
void Engine::queueMake(int token) {
#ifdef USE_EPOLL
  if (UsedTokens.empty()) {
    epoll_event ev;
    ev.events = EPOLLOUT;
    ev.data.u64 = unsigned(FDs::MAKE_OUT);
    while (epoll_ctl(PollFD, EPOLL_CTL_ADD, MakeOut, &ev) < 0)
      assert(errno == EINTR || errno == EAGAIN);
  }
#endif
  UsedTokens.push_back(char(token));
}

// Able to write something to the make pipe.  Do it.

void Engine::writeMake() {
  ssize_t wrote = write(MakeOut, UsedTokens.data(), UsedTokens.size());
  if (wrote < 0) {
    if (errno != EINTR && errno != EAGAIN)
      stopMake(errno);
    wrote = 0;
  }

  if (size_t(wrote) == UsedTokens.size()) {
    UsedTokens.clear();
#ifdef USE_EPOLL
    while (epoll_ctl(PollFD, EPOLL_CTL_DEL, MakeOut, nullptr) < 0)
      assert(errno == EINTR || errno == EAGAIN);
#endif
  } else
    UsedTokens.erase(0, wrote);
}

void Engine::process() {
#ifdef USE_EPOLL
  constexpr int max_events = 20;
  epoll_event events[max_events];

  int count = epoll_wait(PollFD, events, max_events, -1);
  if (count < 0) {
    count = 0;
    assert(errno == EINTR || errno == EAGAIN);
  }

  for (int ix = 0; ix != count; ix++)
    switch (uint64_t cookie = events[ix].data.u64) {
    case unsigned(FDs::SIGNAL): {
      signalfd_siginfo sig;

      ssize_t bytes = read(SigFD, &sig, sizeof(sig));
      assert(bytes == sizeof(sig));
      handleSignal(sig.ssi_signo);
    } break;

    case unsigned(FDs::MAKE_IN):
      readMake();
      break;

    case unsigned(FDs::MAKE_OUT):
      writeMake();
      break;

    default: {
      Job *job = reinterpret_cast<Job *>(cookie ^ (cookie & 7));

      job->read(*this, cookie & 7, PollFD);

      if (!Stopping && !(cookie & 7) && job == &Generator)
        readGenerator();
    } break;
    }
#else
  fd_set rd_set, wr_set;
  FD_ZERO(&rd_set);
  FD_ZERO(&wr_set);
  int hwm = 0;

  if (make_want) {
    FD_SET(make_in, &rd_set);
    if (hwm <= make_in)
      hwm = make_in + 1;
  }
  if (!used_tokens.empty()) {
    FD_SET(make_out, &wr_set);
    if (hwm <= make_out)
      hwm = make_out + 1;
  }
  for (unsigned ix = 0; ix != 2; ix++)
    if (generator.Buffer(ix).IsOpen()) {
      int fd = generator.Buffer(ix).GetFD();
      FD_SET(fd, &rd_set);
      if (hwm <= fd)
        hwm = fd + 1;
    }
  for (auto iter = jobs.begin(); iter != jobs.end(); ++iter)
    for (unsigned ix = 0; ix != 2; ix++)
      if (iter->Buffer(ix).IsOpen()) {
        int fd = iter->Buffer(ix).GetFD();
        FD_SET(fd, &rd_set);
        if (hwm <= fd)
          hwm = fd + 1;
      }

  int count = pselect(hwm, &rd_set, &wr_set, nullptr, nullptr, &sigselect);
  if (count < 0)
    assert(errno == EINTR);
  else {
    if (make_want && FD_ISSET(make_in, &rd_set))
      ReadMake();
    if (!used_tokens.empty() && FD_ISSET(make_out, &wr_set))
      WriteMake();
    for (unsigned ix = 0; ix != 2; ix++)
      if (generator.Buffer(ix).IsOpen()) {
        int fd = generator.Buffer(ix).GetFD();
        if (FD_ISSET(fd, &rd_set)) {
          generator.Read(*this, ix, poll_fd);

          if (!stopping && !ix)
            ReadGenerator();
        }
      }
    for (auto iter = jobs.begin(); iter != jobs.end(); ++iter)
      for (unsigned ix = 0; ix != 2; ix++)
        if (iter->Buffer(ix).IsOpen()) {
          int fd = iter->Buffer(ix).GetFD();
          if (FD_ISSET(fd, &rd_set))
            iter->Read(*this, ix, poll_fd);
        }
  }

  if (int seen_sig = sigs_seen) {
    sigs_seen = 0;
    for (unsigned ix = sizeof(sigs); ix--;)
      if (seen_sig & (1 << ix))
        HandleSignal(sigs[ix]);
  }
#endif
}

void Engine::fini(Job const &job, std::ostream *out, bool is_generator) {
  if (is_generator)
    log() << "# Test generator: " << job << '\n';
  else {
    log() << "# Test:" << Retired << " " << job << '\n';
    log() << "ALOY:";
    for (const auto &cmd : Command)
      log() << cmd << ' ';
    log() << job << '\n';
  }

  auto &log_text = job.buffer(1);
  log() << std::string_view(log_text.data(), log_text.size());

  if (!is_generator) {
    unsigned bad_count = 0;
    std::string_view bad_line;
    auto &sum_text = job.buffer(0);
    for (auto sol = sum_text.data(), end = sol + sum_text.size();
         sol != end;) {
      auto eol = std::find(sol, end, '\n');
      auto line = std::string_view(sol, eol);

      Statuses st = decodeStatus(line);
      if (st == STATUS_HWM) {
        bad_count++;
        if (!bad_line.size())
          bad_line = line;
      } else if (st < Tester::STATUS_REPORT) {
        Counts[st]++;
        if (out && (st == Tester::FAIL || st == Tester::ERROR))
          *out << line << '\n';
      }
      sum() << line << '\n';
      sol = eol + (eol != end);
    }
    if (bad_count) {
      result(Tester::ERROR)
          << job << ": unexpected summary line '" << bad_line << '\'';
      Counts[Tester::ERROR]++;
    }
  }

  job.reportExit(*this);

  log() << '\n';
}

void Engine::retire(std::ostream *out) {
  while (Completed && Jobs.front().isReady()) {
    fini(Jobs.front(), out, false);
    Jobs.pop_front();

    Completed--;
    Retired++;
  }
}

void Engine::spawn() {
  for (auto iter = Jobs.begin() + (Completed + Running); Pending; ++iter) {
    int token = -1;
    if (JobLimit > FixedJobs)
      ;
    else if (ReadyTokens.size()) {
      token = ReadyTokens.back();
      ReadyTokens.pop_back();
    } else
      break;

    if (iter->spawn(*this, Command, PollFD, token)) {
      Running++;
      if (token < 0)
        FixedJobs++;
    } else {
      Completed++;
      if (token >= 0)
        queueMake(token);
    }
    Pending--;
  }

  while (!ReadyTokens.empty()) {
    queueMake(ReadyTokens.back());
    ReadyTokens.pop_back();
  }

  wantMake();
}

std::string Engine::getProgress() {
  unsigned done = Retired + Completed;
  unsigned total = done + Running + Pending;
  std::ostringstream progress;

  progress << "[" << Retired;
  if (Completed)
    progress << "(&" << Completed << ')';
  if (Running)
    progress << '+' << Running;
  progress << '/' << total << "] " << done * 100 / (total + !total) << '%';
  if (!Jobs.empty())
    progress << ' ' << Jobs.front();
  else if (!Generator.isReady())
    progress << " ...";
  return progress.str();
}
#endif
