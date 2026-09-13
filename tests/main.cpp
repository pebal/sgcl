//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "types.h"

// The stack is scanned conservatively: a pointer left in a frame by the
// previous test, however deep, would keep whatever now occupies that heap
// slot alive. The frames of every test body lie below this listener's frame,
// so zeroing the whole unused stack here starts each test clean.
struct ClearStackListener : ::testing::EmptyTestEventListener {
    void OnTestStart(const ::testing::TestInfo&) override {
        collector::clear_stack(SIZE_MAX);
    }
};

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::UnitTest::GetInstance()->listeners().Append(new ClearStackListener);
    return RUN_ALL_TESTS();
}
