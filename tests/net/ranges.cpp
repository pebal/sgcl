//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The ranges of net (tests/ranges.h)
#include "tests/types.h"
#include "tests/ranges.h"

SGCL_CHECK_RANGE(std::ranges::input_range, net::http::headers);
SGCL_CHECK_RANGE(std::ranges::contiguous_range, net::query_params);
