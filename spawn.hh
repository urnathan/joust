// Joust Test Suite			-*- mode:c++ -*-
// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#pragma once

// C++
#include <string>
#include <vector>
#include <tuple>

namespace Joust
{

enum ProcLimits
{
  PL_CPU,
  PL_MEM,
  PL_FILE,
  PL_HWM
};

// Return pid_t & errno
std::tuple<pid_t,int> Spawn (int fd_in, int fd_out, int fd_err,
			     std::vector<std::string> const &words,
			     std::vector<std::string> const *wrapper = nullptr,
			     unsigned const *limits = nullptr);
}
