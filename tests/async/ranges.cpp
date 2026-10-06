//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The channels as ranges (tests/ranges.h); the views over them, and what
// take leaves, are in channel.cpp
#include "tests/types.h"
#include "tests/ranges.h"

#include <memory>

SGCL_CHECK_RANGE(std::ranges::input_range, async::channel<int>);
SGCL_CHECK_RANGE(std::ranges::input_range, const async::channel<int>);
SGCL_CHECK_RANGE(std::ranges::input_range, async::receive_channel<int>);
SGCL_CHECK_RANGE(std::ranges::input_range, async::channel<std::unique_ptr<int>>);
