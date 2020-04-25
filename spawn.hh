// Joust			-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// Not For Distribution

#pragma once

// C++
#include <string>
#include <vector>

namespace Joust
{
// Return pid_t or negated errno
pid_t Spawn (int fd_in, int fd_out, int fd_err,
	     std::vector<std::string> const &words, int &err);
}
