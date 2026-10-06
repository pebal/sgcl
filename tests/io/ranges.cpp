//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The ranges of io (tests/ranges.h): the lines of a buffered reader, a
// generator that reads the stream as it is looked at
#include "tests/types.h"
#include "tests/ranges.h"

SGCL_CHECK_RANGE(std::ranges::input_range, decltype(std::declval<io::buffered_reader&>().lines()));
