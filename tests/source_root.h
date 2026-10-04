//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include <filesystem>

#ifndef SGCL_TEST_SOURCE_ROOT
#error "SGCL_TEST_SOURCE_ROOT is not defined: a test program is made by sgcl_test_program (tests/CMakeLists.txt)"
#endif

// The repository's root, absolute, where the tests' data files are found:
// __FILE__ may be relative (a build directory inside the tree).
inline std::filesystem::path source_root() {
    return std::filesystem::path(SGCL_TEST_SOURCE_ROOT);
}
