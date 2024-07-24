// Joust Test Suite			-*- mode:c++ -*-
// Copyright (C) 2020-2024 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#include "joust/cfg.h"
// NMS
#include "nms/fatal.hh"
// Gaige
#include "gaige/readBuffer.hh"
// C
#include <cerrno>
// OS
#include <unistd.h>

using namespace gaige;

int ReadBuffer::read() {
  assert(FD >= 0);

  // Read
  size_t lwm = size();
  size_t hwm = capacity();
  if (hwm - lwm < BlockSize / 2)
    hwm += BlockSize;
  resize(hwm);

  ssize_t count = ::read(FD, data() + lwm, hwm - lwm);
  resize(lwm + (count >= 0 ? count : 0));

  int res = count ? 0 : -1;
  if (count < 0 && errno != EINTR)
    res = errno;

  return res;
}
