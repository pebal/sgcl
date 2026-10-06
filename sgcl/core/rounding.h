//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The rounding of a number to fewer digits, one enum for every module that
// rounds: math::decimal and big_float, txt's locale formatting of numbers.
#pragma once

#include <cstdint>

namespace sgcl {
    enum class rounding : uint8_t {
        half_even,     // to the nearest, a tie to the even digit (the default: IEEE 754, CLDR)
        half_up,       // to the nearest, a tie away from zero
        half_down,     // to the nearest, a tie towards zero
        up,            // away from zero
        down,          // towards zero (truncation)
        ceiling,       // towards +infinity
        floor,         // towards -infinity
        unnecessary    // the result must be exact: the operation reports it when it is not
    };
}
