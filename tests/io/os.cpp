//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

#include <string>
#include <string_view>

namespace {
    using namespace sgcl::io;
    namespace io = sgcl::io;
}

TEST(IoOs_Tests, Environment) {
    ASSERT_TRUE(io::setenv("SGCL_IO_TEST", "value"));
    auto v = io::getenv("SGCL_IO_TEST");
    ASSERT_TRUE(v);
    EXPECT_EQ(std::string_view(*v), "value");
    ASSERT_TRUE(io::setenv("SGCL_IO_EMPTY", ""));
    ASSERT_TRUE(io::getenv("SGCL_IO_EMPTY"));   // set, empty
    EXPECT_EQ(std::string_view(expand_env("a $SGCL_IO_TEST b ${SGCL_IO_TEST}c $SGCL_IO_UNSET d $ e ${x")), "a value b valuec  d $ e ${x");
    bool found = false;
    for (auto& [k, val] : io::environ()) {
        if (std::string_view(k) == "SGCL_IO_TEST") {
            found = std::string_view(val) == "value";
        }
    }
    EXPECT_TRUE(found);
    ASSERT_TRUE(io::unsetenv("SGCL_IO_TEST"));
    EXPECT_FALSE(io::getenv("SGCL_IO_TEST"));
    io::unsetenv("SGCL_IO_EMPTY");
}

TEST(IoOs_Tests, ProcessAndDirectories) {
    auto a = args();
    ASSERT_GE(a.size(), 1u);
    EXPECT_TRUE(a[0].contains("tests_io"));
    auto exe = executable();
    ASSERT_TRUE(exe);
    EXPECT_TRUE(path::is_abs(*exe));
    EXPECT_TRUE(is_regular(*exe));
    EXPECT_TRUE(exe->contains("tests_io"));
    EXPECT_GT(pid(), 0);
    auto host = hostname();
    ASSERT_TRUE(host);
    EXPECT_FALSE(host->empty());
    auto home = home_dir();
    ASSERT_TRUE(home);
    EXPECT_TRUE(is_directory(*home));
    ASSERT_TRUE(cache_dir());
    ASSERT_TRUE(config_dir());
    EXPECT_TRUE(is_directory(temp_dir()));
    auto wd = working_dir();
    ASSERT_TRUE(wd);
    auto tmp = make_temp_dir();
    ASSERT_TRUE(tmp);
    ASSERT_TRUE(io::chdir(*tmp));
    auto now = working_dir();
    ASSERT_TRUE(now);
    // the temporary directory may be reached through a symlink (/tmp on macOS)
    EXPECT_EQ(io::stat(*now)->modified, io::stat(*tmp)->modified);
    ASSERT_TRUE(io::chdir(*wd));
    io::remove_all(*tmp);
}

TEST(IoOs_Tests, StandardStreams) {
    EXPECT_EQ(io::stdout.fd(), 1);
    EXPECT_FALSE(io::stdout.file()->is_closed());
    EXPECT_EQ(io::stdout.file(), io::stdout.file());   // one file, made once
    EXPECT_EQ(io::stderr.fd(), 2);
    EXPECT_EQ(io::stderr.file()->path(), "stderr");
    EXPECT_EQ(io::stdin.fd(), 0);
    // the C streams under their names in the same unit
    EXPECT_EQ(fileno(::stderr), 2);
    EXPECT_EQ(fileno(::stdin), 0);
    EXPECT_GE(fprintf(::stderr, "%s", ""), 0);
    EXPECT_EQ(is_terminal(1), ::isatty(1) == 1);
}
