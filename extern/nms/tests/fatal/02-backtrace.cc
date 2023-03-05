// NMS Utilities			-*- mode:c++ -*-
// Copyright (C) 2022-2023 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

// RUN-REQUIRE: test 0 -ne ${NMS_BACKTRACE}
// RUN-SIGNAL:!0 ${subdir}${stem} |& ezio -p ABORT -p TAIL ${test}
// RUN-SIGNAL:!0 ${subdir}${stem} boom |& ezio -p HCF -p TAIL ${test}

// CHECK-ABORT: signal ({:.*})

// CHECK-HCF: boom at

// CHECK-TAIL: 00-0x{:.*} {:fatal/02-backtrace()|fatal.cc:.* NMS::HCF}
// CHECK-TAIL: 0{:.}-0x{:.*} {:fatal/02-backtrace()|backtrace.cc:.* main}
// CHECK-TAIL: version 
// CHECK-TAIL: Build is

#include "nms/cfg.h"
#include "nms/fatal.hh"
#include <stdlib.h>

using namespace NMS;

int
main (int argc, char **argv)
{
  SetBuild (argv[0]);
  SignalHandlers ();

  if (argc == 1)
    abort ();
  else
    HCF (argv[1]);
}
