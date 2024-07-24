// Joust/ALOY: Apply List, Observe Yield		-*- mode:c++ -*-
// Copyright (C) 2020-2024 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#if !defined(ALOY_JOB)
#define ALOY_JOB
class Job {
  std::vector<std::string> Command;
  ReadBuffer Buffers[2]; // Job's stdout, stderr
  pid_t Pid = pid_t(-1); // Job's PID
  int ExitStatus = 0;    // Exit status of job
  short MakeToken = -1;  // Make job-server token
  int State = 0;

public:
  Job(std::string_view const &cmd) { Command.emplace_back(cmd); }
  Job(std::string &&cmd) { Command.emplace_back(std::move(cmd)); }
  Job() {}

private:
  // We rely on object reference invariance, so ban copies
  Job(Job const &) = delete;
  Job &operator= (Job const &) = delete;

public:
  std::vector<std::string> &command () { return Command; }

  auto &buffer (unsigned ix) { return Buffers[ix]; }

  auto const &buffer (unsigned ix) const { return Buffers[ix]; }

public:
  void read (Engine &, unsigned subcode, int poll_fd);

  bool spawn (Engine &, std::vector<std::string> const &preamble, int poll_fd,
              int token = -1);
  int reap (int status);
  bool isPid (pid_t p) const { return Pid == p; }
  void stop (int signal) {
    if (Pid > 0)
      kill(Pid, signal);
  }
  bool isReady () const { return !State; }
  void reportExit (Engine &) const;

  friend std::ostream &operator<< (std::ostream &, Job const &);
};

#else

void Job::read(Engine &log, unsigned subcode, int poll_fd [[maybe_unused]]) {
  assert(subcode < 2 && State > 0);

  if (int done = Buffers[subcode].read()) {
    if (done >= 0) {
      static char const *const io[] = {" stdout:", " stderr:"};
      log.result(Tester::ERROR)
          << "failed reading " << *this << io[subcode] << strerror(done);
    }

    int fd = Buffers[subcode].close();
#ifdef USE_EPOLL
    epoll_ctl(poll_fd, EPOLL_CTL_DEL, fd, nullptr);
#endif
    close(fd);
    State--;
  }
}

// Spawn a job, return true if we managed to spawn it.

bool Job::spawn(Engine &log, std::vector<std::string> const &preamble,
                int poll_fd [[maybe_unused]], int token) {
  assert(!State && Buffers[0].empty() && Buffers[1].empty());

  if (!preamble.size())
    // No execute mode, we're done
    return false;

  int job_fds[2]{-1, -1};
  int null_fd = -1;

  int err;
  for (unsigned ix = 0; ix != 2; ix++) {
    int pipe[2];
    if (makePipe(pipe) < 0) {
      err = errno;
      goto fail;
    }
    job_fds[ix] = pipe[1];
    Buffers[ix].open(pipe[0]);

#ifdef USE_EPOLL
    epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.u64 = reinterpret_cast<uint64_t>(this) | ix;
    while (epoll_ctl(poll_fd, EPOLL_CTL_ADD, pipe[0], &ev) < 0)
      assert(errno == EINTR);
#endif
  }

  null_fd = open("/dev/null", O_RDONLY | O_CLOEXEC);
  if (null_fd < 0)
    err = errno;
  else {
    auto [p, e]
        = gaige::spawn(null_fd, job_fds[0], job_fds[1], Command, &preamble);
    Pid = p;
    err = e;
  }

fail:
  if (err)
    log.result(Tester::ERROR)
        << "failed spawning " << preamble[0] << ":" << strerror(err);

  if (Pid) {
    State = 3;

    // Keep the make token
    MakeToken = token;
  } else {
    if (null_fd > 0)
      close(null_fd);
    for (unsigned ix = 0; ix != 2; ix++) {
      close(job_fds[ix]);
      int fd = Buffers[ix].close();
#ifdef EPOLL_CTL
      epoll_ctl(poll_fd, EPOLL_CTL_DEL, fd, nullptr);
#endif
      close(fd);
    }
  }

  return Pid != 0;
}

// Reap a completed job, returns the make token (or -1)
int Job::reap(int status) {
  assert(Pid >= 0 && State);
  ExitStatus = status;

  Pid = pid_t(-1);
  State--;

  int r = MakeToken;
  MakeToken = -1;
  return r;
}

void Job::reportExit(Engine &log) const {
  if (WIFSIGNALED(ExitStatus)) {
    int sig = WTERMSIG(ExitStatus);
    log.result(Tester::ERROR) << *this << " terminated with signal " << sig;
  } else if (WIFEXITED(ExitStatus)) {
    if (int code = WEXITSTATUS(ExitStatus))
      log.result(Tester::ERROR) << *this << " exited with status " << code;
  } else
    unreachable();
}

std::ostream &operator<< (std::ostream &s, Job const &job) {
  if (!job.Command.empty())
    s << job.Command[0];

  return s;
}

#endif
