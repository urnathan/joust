// NMS Test Suite			-*- mode:c++ -*-
// Copyright (C) 2022 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

// RUN: ${subdir}${stem}

#include "nms/cfg.h"
#include "nms/fatal.hh"

using namespace NMS;

int
main ()
{
  SignalHandlers ();

  Assert (true);
  Assert (true, "ok");
}
