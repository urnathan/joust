// NMS Test Suite			-*- mode:c++ -*-
// Copyright (C) 2022 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

// RUN-REQUIRE: test 0 -ne ${NMS_BACKTRACE}
// RUN-SIGNAL:!0 ${subdir}${stem} |& ezio ${test}
// RUN-SIGNAL:!0 ${subdir}${stem} 1 |& ezio ${test}

// CHECK: stack overflow at {:.*}/04-recursion.cc:3{:[0-9]}
// CHECK: 00-0x{:.*} {:.*}fatal.cc:{:[0-9]+} NMS::HCF
// CHECK: 0{:.}-0x{:.*} {:.*}04-recursion.cc:{:[0-9]+} Recurse
// CHECK: 0{:.}-0x{:.*} {:.*}04-recursion.cc:{:[0-9]+} Recurse
// CHECK-NEXT: 0{:.}-0x{:.*} repeat frame {:[0-9]+}...
// CHECK: Version 
// CHECK: Build is

#include "nms/cfg.h"
#include "nms/fatal.hh"
#include <signal.h>

using namespace NMS;








[[gnu::noinline]] unsigned
Recurse (unsigned i)
{
  if (i)
    return 1 + Recurse (i + 1);
  return 0;
}




[[gnu::noinline]] void
Driver () 
{
  Recurse (1);
}

void
SegFault (int sig, siginfo_t *info, void *ucontext)
{
  NMS::SignalHandler (sig, info, ucontext);
}

int
main (int argc, char **)
{
  SignalHandlers ();
  if (argc > 1)
    {
      struct sigaction action;
      action.sa_sigaction = &SegFault;
      sigemptyset (&action.sa_mask);
      action.sa_flags = SA_NODEFER | SA_SIGINFO | SA_ONSTACK;
      sigaction (SIGSEGV, &action, nullptr);
    }

  Driver ();
}
