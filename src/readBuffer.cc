// Joust Test Suite			-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#include "config.h"
// NMS
#include "nms/fatal.hh"
// Joust
#include "readBuffer.hh"
// C
#include <cerrno>
// OS
#include <unistd.h>

namespace Joust
{

int ReadBuffer::Read
  ()
{
  Assert (fd >= 0);

  // Read
  size_t lwm = size ();
  size_t hwm = capacity ();
  if (hwm - lwm < blockSize / 2)
    hwm += blockSize;
  resize (hwm);

  ssize_t count = read (fd, data () + lwm, hwm - lwm);
  resize (lwm + (count >= 0 ? count : 0));

  int res = count ? 0 : -1;
  if (count < 0 && errno != EINTR)
    res = errno;

  return res;
}

}
