//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The ranges of encoding (tests/ranges.h)
#include "tests/types.h"
#include "tests/ranges.h"

#include <vector>

SGCL_CHECK_RANGE(std::ranges::forward_range, encoding::csv::row);
SGCL_CHECK_RANGE(std::ranges::forward_range, encoding::asn1);
SGCL_CHECK_RANGE(std::ranges::input_range, decltype(std::declval<encoding::csv::reader&>().rows()));

// rows() reads a record at the look at it: take(n) reads n records, and the
// reader goes on from the next one
TEST(EncodingRanges_Tests, TakeOfRowsLeavesTheRestToTheReader) {
    encoding::csv::reader r(string("a,1\nb,2\nc,3\n"));
    std::vector<std::string> names;
    for (auto row : r.rows() | std::views::take(1)) {
        names.emplace_back(row[0].data(), row[0].size());
    }
    auto next = r.next();
    ASSERT_TRUE(next);
    EXPECT_EQ(names, (std::vector<std::string>{"a"}));
    EXPECT_EQ((*next)[0], "b");
}
