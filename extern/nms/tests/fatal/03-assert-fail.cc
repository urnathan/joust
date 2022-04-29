// NMS Test Suite			-*- mode:c++ -*-
// Copyright (C) 2022 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

// RUN-REQUIRE: test 0 -ne ${NMS_CHECKING}
// RUN-SIGNAL:!0 ${subdir}${stem} 0 |& ezio -p 0 ${test}
// RUN-SIGNAL:!0 ${subdir}${stem} 1 |& ezio -p 1 ${test}
// RUN-SIGNAL:!0 ${subdir}${stem} 2 |& ezio -p 2 ${test}
// RUN-SIGNAL:!0 ${subdir}${stem} 3 |& ezio -p 3 ${test}
// RUN-SIGNAL:!0 ${subdir}${stem} 4 |& ezio -p 4 ${test}
// RUN-SIGNAL:!0 ${subdir}${stem} 5 |& ezio -p 5 ${test}

// CHECK-ABORT: signal:

// CHECK-HCF: boom at

#include "nms/cfg.h"
#include "nms/fatal.hh"
#include <stdlib.h>

using namespace NMS;

int main (int, char **argv)
{
  SignalHandlers ();

  switch (argv[1][0])
    {
    case '0':
      Assert (false);
      // CHECK-0: assertion failed at {:.*}03-assert-fail.cc:30
    case '1':
      Assert (false, "one");
      // CHECK-1: assertion failed (one) at {:.*}03-assert-fail.cc:33
    case '2':
      Unimplemented ();
      // CHECK-2: unimplemented functionality at
    case '3':
      Unimplemented ("three");
      // CHECK-3: unimplemented functionality (three) at
    case '4':
      Unreachable ();
      // CHECK-4: unreachable reached at
    case '5':
      Unreachable ("five");
      // CHECK-5: unreachable reached (five) at
    }
  return 1;
}
