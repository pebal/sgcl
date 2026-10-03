//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// rational at the boundaries the core sweep covers (DESIGN 408): a value
// moved from, which keeps its value (the move is the copy), and the value
// as its own argument in every operation and compound assignment, at zero,
// at one, at a negative and at a value of several limbs.
#include "tests/types.h"
#include "sgcl/math/math.h"

using math::big_integer;
using math::rational;

namespace {
    rational several_limbs() {
        return rational(big_integer(1) << 200, (big_integer(3) << 130) + 1);
    }
}

TEST(RationalSelf_Tests, MovedFromKeepsItsValue) {
    for (rational r : {rational(), rational(1), rational(-7, 3), several_limbs()}) {
        rational copy = r;
        rational moved(std::move(r));
        EXPECT_EQ(r, copy);   // the move is the copy (rational.h)
        EXPECT_EQ(moved, copy);
        rational target;
        target = std::move(r);
        EXPECT_EQ(r, copy);
        EXPECT_EQ(target, copy);
        EXPECT_EQ(r + r, copy + copy);   // every member works on it
        EXPECT_EQ(r.abs(), copy.abs());
        EXPECT_EQ(r.denominator().sign(), 1);
    }
}

TEST(RationalSelf_Tests, ItselfAsTheArgument) {
    for (rational r : {rational(), rational(1), rational(-7, 3), several_limbs()}) {
        const rational v = r;
        rational a = r;
        a = a;
        EXPECT_EQ(a, v);
        a = std::move(a);
        EXPECT_EQ(a, v);
        a += a;
        EXPECT_EQ(a, v * 2);
        a -= a;
        EXPECT_EQ(a, rational());
        a = v;
        a *= a;
        EXPECT_EQ(a, v * v);
        a = v;
        if (v != rational()) {
            a /= a;
            EXPECT_EQ(a, rational(1));
        } else {
            EXPECT_THROW(a /= a, domain_error);
            EXPECT_EQ(a, v);   // the value as it was
        }
        a = v;
        EXPECT_EQ(a - a, rational());
        EXPECT_TRUE(a == a);
        EXPECT_TRUE((a <=> a) == 0);
        EXPECT_EQ(rational(a.numerator(), a.denominator()), v);   // made of its own parts
    }
}
