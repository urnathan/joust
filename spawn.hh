// Joust			-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// Not For Distribution

#pragma once

// C++
#include <string>
#include <vector>
#include <tuple>

namespace Joust
{


// Return pid_t 7 errno
std::tuple<pid_t,int> Spawn (int fd_in, int fd_out, int fd_err,
			     std::vector<std::string> const &words);
}
