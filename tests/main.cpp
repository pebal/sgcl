//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

// The stack is scanned conservatively: a pointer left in a frame by the
// previous test, however deep, would keep whatever now occupies that heap
// slot alive. The frames of every test body lie below this listener's frame,
// so zeroing the whole unused stack here starts each test clean. The
// thread's block of cells (root_ptr.h: a task's handle is a root_ptr) is
// let go of when a test ends, so that a block whose cells the test gave
// back is freed by the next cycle and the counts of the tests after
// start from what is alive.
struct ClearStackListener : ::testing::EmptyTestEventListener {
    void OnTestStart(const ::testing::TestInfo&) override {
        collector::clear_stack(SIZE_MAX);
    }

    void OnTestEnd(const ::testing::TestInfo&) override {
        sgcl::detail::cell_allocator.release();
    }
};

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::UnitTest::GetInstance()->listeners().Append(new ClearStackListener);
    return RUN_ALL_TESTS();
}
