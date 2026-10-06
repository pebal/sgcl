//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The ranges of compress (tests/ranges.h): a 7z archive's walk, a generator
// that decodes the next entry as it is looked at
#include "tests/types.h"
#include "tests/ranges.h"

SGCL_CHECK_RANGE(std::ranges::input_range, decltype(std::declval<compress::sevenzip::archive&>().walk()));
