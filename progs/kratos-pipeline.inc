// Joust/KRATOS: Kapture Run And Test Output Safely	-*- mode:c++ -*-
// Copyright (C) 2020-2024 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

class Pipeline {
public:
#define PIPELINE_KINDS RUN, SIGNAL, REQUIRE, END
  enum Kinds : unsigned char {
    NMS_LIST(NMS_IDENT, PIPELINE_KINDS),
    PIPELINE_HWM,
    PIPE_HWM = REQUIRE
  };

public:
  static char const *const KindNames[PIPELINE_HWM];

private:
  std::vector<Command> Commands;
  std::string Src;
  Kinds Kind = RUN;
  unsigned ExitCode   : 8 = 0;
  bool IsExitInverted : 1 = false;
  bool IsXFailed      : 1 = false;
  bool IsStopped      : 1 = false;
  bool IsHereDoc      : 1 = false;

public:
  Pipeline (Kinds kind, bool inverted, unsigned exit = 0)
    : Kind(kind), ExitCode(exit), IsExitInverted(inverted) {
    Commands.emplace_back();
  }

public:
  Kinds kind () const { return Kind; }

public:
  void src (std::string &&stdin, bool here) {
    Src = stdin;
    IsHereDoc = here;
  }

public:
  int execute (Tester &, unsigned const *);

public:
  void result (Tester &, Tester::Statuses);

private:
  friend std::ostream &operator<< (std::ostream &s, Pipeline const &pipe);
  friend class Parser;
};

constinit char const *const Pipeline::KindNames[PIPELINE_HWM]
    = {NMS_LIST(NMS_STRING, PIPELINE_KINDS)};

static constinit unsigned char const sigs[]
    = {SIGHUP, SIGQUIT, SIGPIPE, SIGCHLD, SIGALRM, SIGTERM};

std::ostream &operator<< (std::ostream &s, Pipeline const &pipe) {
  {
    auto &cmd = pipe.Commands.front();

    if (pipe.IsHereDoc) {
      for (size_t base = 0, eol; base != pipe.Src.size(); base = eol + 1) {
        eol = pipe.Src.find_first_of('\n', base);
        assert(eol != pipe.Src.npos);
        s << cmd.loc().file() << ':' << cmd.loc().line() << " in<<"
          << std::string_view(&pipe.Src[base], eol - base) << '\n';
      }
    } else if (!pipe.Src.empty())
      s << cmd.loc().file() << ':' << cmd.loc().line() << " in< " << pipe.Src
        << '\n';

    s << cmd.loc().file() << ':' << cmd.loc().line() << " "
      << Pipeline::KindNames[pipe.Kind] << ':';
    if (pipe.ExitCode)
      s << pipe.ExitCode;
    if (pipe.IsExitInverted)
      s << '!';
    s << cmd << '\n';
  }

  static char const *const outs[] = {" out", " err"};
  for (unsigned ix = 1; ix != pipe.Commands.size(); ix++) {
    auto &filter = pipe.Commands[ix];
    if (!filter.empty()) {
      s << filter.loc().file() << ':' << filter.loc().line() << outs[ix - 1];
      s << (filter.redirect() == Command::R_FILE ? '>' : '|');
      if (ix == 2)
        s << '&';
      s << filter << '\n';
    }
  }

  return s;
}

#ifndef USE_EPOLL
// Signals we saw
volatile unsigned sigs_seen;

void NoteSignal (int signal) {
  for (unsigned ix = sizeof(sigs); ix--;)
    if (signal == sigs[ix]) {
      sigs_seen = sigs_seen | (1 << ix);
      break;
    }
}
#endif

// This mucks about with signals, so expects to be single threaded

int Pipeline::execute (Tester &logger, unsigned const *limits) {
  assert(Commands.size() == 1 || Commands.size() == 3);

  std::cerr << *this;

  int here_fd = -1;
  {
    auto &cmd = Commands.front();

    if (cmd.Words.empty()) {
      cmd.error() << "no command to execute";
      return EINVAL;
    }

    if (IsHereDoc) {
      int pipe[2];

      if (makePipe(pipe) < 0) {
        int err = errno;
        cmd.error() << "cannot create pipe: " << strerror(err);
      } else {
        here_fd = pipe[1];
        cmd.Stdin = pipe[0];
      }
    } else {
      char const *in = Src.c_str();
      if (!*in)
        in = "/dev/null";
      int fd = open(in, O_RDONLY | O_CLOEXEC);
      if (fd < 0) {
        int err = errno;
        cmd.error() << "cannot read '" << in << "': " << strerror(err);
      } else
        cmd.Stdin = fd;
    }
  }

  // Determine sinks of cmd[0]
  int fds[2]{1, 2};
  for (unsigned ix = 1; ix != Commands.size(); ix++) {
    auto &filt = Commands[ix];

    if (filt.redirect() == Command::R_FILE) {
      // To a file
      assert(filt.Words.size() == 1);

      char const *out = filt.Words.front().c_str();
      int fd = open(out, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
                    S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
      if (fd < 0) {
        int err = errno;
        filt.error() << "cannot write '" << out << "': " << strerror(err);
      } else
        fds[ix - 1] = fd;
    } else {
      // pipe ends: 0-read from, 1-write to
      int pipe[2];
      if (makePipe(pipe) < 0) {
        int err = errno;
        filt.error() << "cannot create pipe: " << strerror(err);
      } else {
        fds[ix - 1] = pipe[1];
        filt.Stdin = pipe[0];
      }
    }
  }

  // Signals to block
  sigset_t sigmask, sigorig;
  sigemptyset(&sigmask);
  for (unsigned ix = sizeof(sigs); ix--;)
    sigaddset(&sigmask, sigs[ix]);
#ifdef USE_EPOLL
  while (sigprocmask(SIG_BLOCK, &sigmask, &sigorig) < 0)
    assert(errno == EINTR);

  int poll_fd = epoll_create1(EPOLL_CLOEXEC);
  int sig_fd = signalfd(-1, &sigmask, SFD_NONBLOCK | SFD_CLOEXEC);
  assert(poll_fd >= 0 && sig_fd >= 0);

  {
    epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.u64 = 0;
    while (epoll_ctl(poll_fd, EPOLL_CTL_ADD, sig_fd, &ev) < 0)
      assert(errno == EINTR);
    if (here_fd >= 0) {
      ev.events = EPOLLOUT;
      ev.data.u64 = 1;
      while (epoll_ctl(poll_fd, EPOLL_CTL_ADD, here_fd, &ev) < 0)
        assert(errno == EINTR);
    }
  }
#else
  while (sigprocmask(SIG_UNBLOCK, &sigmask, &sigorig) < 0)
    assert(errno == EINTR);
  sigset_t sigselect;
  while (sigprocmask(SIG_BLOCK, &sigmask, &sigselect) < 0)
    assert(errno == EINTR);

  // Install signal handlers
  struct sigaction old_actions[sizeof(sigs)];
  for (unsigned ix = sizeof(sigs); ix--;) {
    struct sigaction action;
    action.sa_handler = NoteSignal;
    sigfillset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(sigs[ix], &action, &old_actions[ix]);
  }
#endif

  // We must buffer the (up to) 4 streams from the 2 filters,
  // otherwise the output can be randomly intermixed, and that'll be
  // terribly confusing at best.
  ReadBuffer streams[4];
  unsigned num_streams = 0;
  unsigned subtasks = 0;
  {
    if (Commands.front().execute(fds[0], fds[1], limits))
      subtasks++;

    for (unsigned ix = 1; ix != Commands.size(); ix++) {
      auto &filt = Commands[ix];

      // pipe ends: 0-read from, 1-write to
      int pipe[2][2] = {{-1, -1}, {-1, -1}};

      if (filt.Stdin < 0)
        continue;

      if (filt.empty())
        // Expect no output
        pipe[0][0] = filt.Stdin;
      else {
        if (makePipe(pipe[0]) < 0 || makePipe(pipe[1]) < 0) {
          int err = errno;
          filt.error() << "cannot create pipe: " << strerror(err);
          if (pipe[0][0] >= 0) {
            close(pipe[0][0]);
            close(pipe[0][1]);
          }
          pipe[0][0] = pipe[0][1] = -1;
        }
        if (filt.execute(pipe[0][1], pipe[1][1]))
          subtasks++;
      }

      for (unsigned io = 0; io != 2; io++)
        if (pipe[io][0] >= 0) {
          unsigned strix = (ix - 1) * 2 + io;
          int pipe_fd = pipe[io][0];

          streams[strix].open(pipe_fd);
          num_streams++;
#ifdef USE_EPOLL
          epoll_event ev;
          ev.events = EPOLLIN;
          ev.data.u64 = strix | 4;
          if (epoll_ctl(poll_fd, EPOLL_CTL_ADD, pipe_fd, &ev) < 0)
            unreachable();
#endif
        }
    }
  }

  // Wait for completion
  itimerval timeout = {{0, 0}, {0, 0}};
  if (limits && limits[PL_HWM]) {
    timeout.it_value.tv_sec = limits[PL_HWM];
    setitimer(ITIMER_REAL, &timeout, nullptr);
    timeout.it_value.tv_sec = 0;
  }

  static char const *const io_streams[] = {" stdout:", " stderr:"};
  size_t here_pos = 0;
  bool signalled = false;
  int exit_code = -1;
  while (subtasks || num_streams || here_fd >= 0) {
    unsigned seen_sig = 0;
    int count;
#ifdef USE_EPOLL
    constexpr int max_events = 20;
    epoll_event events[max_events];

    count = epoll_wait(poll_fd, events, max_events, -1);
#else
    fd_set rd_set;
    fd_set wr_set;
    int hwm = 0;

    FD_ZERO(&rd_set);
    FD_ZERO(&wr_set);

    if (here_fd >= 0) {
      FD_SET(here_fd, &wr_set);
      hwm = here_fd + 1;
    }

    for (unsigned ix = 4; ix--;)
      if (streams[ix].IsOpen()) {
        int fd = streams[ix].GetFD();
        FD_SET(fd, &rd_set);
        if (fd >= hwm)
          hwm = fd + 1;
      }
    count = pselect(hwm, &rd_set, &wr_set, nullptr, nullptr, &sigselect);
    seen_sig = sigs_seen;
    sigs_seen = 0;
#endif
    if (count < 0) {
      count = 0;
      assert(errno == EINTR);
    }

    for (int ix = 0; ix != count; ix++) {
      unsigned cookie;

#ifdef USE_EPOLL
      cookie = unsigned(events[ix].data.u32);
#else
      while (hwm--) {
        if (FD_ISSET(hwm, &wr_set)) {
          cookie = 1; // here_doc
          break;
        } else if (FD_ISSET(hwm, &rd_set)) {
          for (unsigned stream = 4; stream--;)
            if (streams[stream].IsOpen() && streams[stream].GetFD() == hwm) {
              cookie = stream | 4;
              goto found_fd;
            }
          unreachable();
        }
      }
    found_fd:;
      assert(hwm >= 0);
#endif
      switch (cookie) {
      default: {
        unsigned stream = cookie & 3;
        assert(stream < 4 && num_streams);
        if (int done = streams[stream].read()) {
          if (done >= 0) {
            auto *cmd = &Commands[1 + stream / 2];
            unsigned iostr = stream & 1;
            if (cmd->Words.empty()) {
              cmd = &Commands[0];
              iostr = stream >> 1;
            }

            cmd->error() << "failed reading " << cmd->Words.front()
                         << io_streams[iostr] << strerror(done);
            result(logger, Tester::ERROR);
          }
          int fd = streams[stream].close();
#ifdef USE_EPOLL
          epoll_ctl(poll_fd, EPOLL_CTL_DEL, fd, nullptr);
#endif
          close(fd);
          num_streams--;
        }
      } break;

      case 1: {
        // Here doc
        size_t limit = Src.size() - here_pos;
        ssize_t wrote = write(here_fd, Src.data() + here_pos, limit);
        if (wrote < 0) {
          int err = errno;
          if (err != EINTR && err != EAGAIN) {
            Commands[0].error()
                << "failed writing here doc at byte " << here_pos << " ("
                << limit << " remain): " << strerror(err);
            result(logger, Tester::ERROR);
            here_pos = Src.size();
          }
        } else
          here_pos += wrote;
        if (here_pos == Src.size()) {
#ifdef USE_EPOLL
          epoll_ctl(poll_fd, EPOLL_CTL_DEL, here_fd, nullptr);
#endif
          close(here_fd);
          here_fd = -1;
        }
      } break;

      case 0: {
        // Signal
#ifdef USE_EPOLL
        signalfd_siginfo sig;
        ssize_t bytes = read(sig_fd, &sig, sizeof(sig));
        assert(bytes == sizeof(sig));
        seen_sig = sig.ssi_signo;
#endif
        break;
      } break;
      }
    }

    if (seen_sig) {
      int signo;
#ifdef USE_EPOLL
      signo = seen_sig;
#else
      unsigned ix = sizeof(sigs);

    another_signal:
      while (!(seen_sig & (1 << --ix)))
        continue;
      seen_sig ^= 1 << ix;
      signo = sigs[ix];
#endif

      switch (signo) {
      case SIGCHLD:
        // Get child status.  There could be multiple children
        // to deal with.
        {
          int status;
          while (pid_t child = waitpid(-1, &status, WNOHANG)) {
            if (child == pid_t(-1)) {
              assert(!subtasks);
              break;
            }

            for (auto &cmd : Commands)
              if (child == cmd.Pid) {
                bool is_sig = false;
                int ex = 0;
                if (WIFEXITED(status))
                  ex = WEXITSTATUS(status);
                else if (WIFSIGNALED(status)) {
                  ex = WTERMSIG(status);
                  is_sig = true;
                } else
                  goto found_pid;

                cmd.Pid = -1;
                subtasks--;
                if (&cmd == &Commands[0]) {
                  if (limits)
                    setitimer(ITIMER_REAL, &timeout, nullptr);
                  signalled = is_sig;
                  exit_code = ex;
                } else if (is_sig || ex) {
                  cmd.error() << '\'' << cmd.Words.front() << "' exited with "
                              << (is_sig ? "signal " : "code ") << ex;
                  result(logger, Tester::ERROR);
                }
                goto found_pid;
              }
            unreachable();
          found_pid:;
          }
        }
        break;

      case SIGHUP:
      case SIGINT:
      case SIGQUIT:
      case SIGTERM:
        IsStopped = true;
        Commands.front().stop(signo);
        break;

      case SIGALRM:
        Commands.front().stop(SIGTERM);
        Commands.front().error()
            << "TIMEOUT after " << limits[PL_HWM] << " seconds";
        result(logger, Tester::ERROR);
        break;

      case SIGPIPE:
        break;

      default:
        unreachable();
      }
#ifndef USE_EPOLL
      if (seen_sig)
        goto another_signal;
#endif
    }
  }

#ifdef USE_EPOLL
  close(poll_fd);
  close(sig_fd);
#else
  // Restore signal handlers
  for (unsigned ix = sizeof(sigs); ix--;)
    sigaction(sigs[ix], &old_actions[ix], nullptr);
#endif
  while (sigprocmask(SIG_SETMASK, &sigorig, nullptr) < 0)
    assert(errno == EINTR);

  for (unsigned ix = 1; ix != Commands.size(); ix++) {
    auto &filt = Commands[ix];
    if (filt.Stdin < 0)
      continue;

    for (unsigned io = 0; io != 2; io++) {
      auto &stream = streams[ix * 2 - 2 + io];
      if (size_t size = stream.size()) {
        std::ostream *s = &std::cout;
        if (filt.empty()) {
          s = &std::cerr;
          logger.result(Tester::ERROR, Commands[0].loc())
              << "# Unexpected" << io_streams[ix - 1] << Commands[0];
        } else if (io) {
          s = &std::cerr;
          *s << "# Checker " << filt.loc().file() << ':' << filt.loc().line()
             << filt << '\n';
        }
        *s << std::string_view(stream.data(), size);
      }
    }
  }

  assert(exit_code >= 0);

  bool pass = (signalled == (Kind == SIGNAL)
               && (exit_code == ExitCode) == !IsExitInverted);
  if (!pass && (Kind != REQUIRE || signalled))
    logger.log() << Commands.front().Words[0] << " exited with "
                 << (signalled ? "signal " : "code ") << exit_code << '\n';
  if (Kind == REQUIRE)
    result(logger, pass ? Tester::PASS : Tester::UNSUPPORTED);
  else
    result(logger, Tester::passFail(pass, IsXFailed));

  if (IsStopped) {
    Commands.front().error() << " terminated via signal";
    return EINTR;
  }

  return pass ? 0 : EINVAL;
}

void Pipeline::result (Tester &logger, Tester::Statuses status) {
  auto const &cmd = Commands.front();
  auto l = logger.result(status, cmd.loc());

  l << KindNames[Kind];
  if (IsExitInverted || ExitCode) {
    l << ' ';
    if (IsExitInverted)
      l << '!';
    if (ExitCode)
      l << int(ExitCode);
  }
  l << ' ' << cmd.Words.front();
}
