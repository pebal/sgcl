//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

#include <string>
#include <string_view>

namespace {
    using namespace sgcl::io;
    namespace io = sgcl::io;

    // A type of the program's with a text form: env reads it by its parse
    struct Level {
        int n = 0;

        static sgcl::expected<Level, sgcl::number_error> parse(const sgcl::string& text) {
            auto v = sgcl::parse<int>(text.view());
            if (!v) {
                return sgcl::unexpected(v.error());
            }
            return Level{*v};
        }

        friend bool operator==(const Level&, const Level&) = default;
    };
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

// env(name, fallback): the value as the fallback's type; the fallback for
// an unset or empty variable; a value that is no such type throws with
// the name, the value and the type
TEST(IoOs_Tests, EnvAsTheFallbacksType) {
    using namespace std::chrono_literals;
    ASSERT_TRUE(io::unsetenv("SGCL_ENV_X"));
    EXPECT_EQ(io::env("SGCL_ENV_X", 8080), 8080);
    EXPECT_EQ(std::string_view(io::env("SGCL_ENV_X", "localhost")), "localhost");
    EXPECT_EQ(io::env("SGCL_ENV_X", 5s), 5 * sgcl::second);
    ASSERT_TRUE(io::setenv("SGCL_ENV_X", ""));
    EXPECT_EQ(io::env("SGCL_ENV_X", 7), 7);   // empty: unset
    EXPECT_EQ(std::string_view(io::env("SGCL_ENV_X", sgcl::string("d"))), "d");

    ASSERT_TRUE(io::setenv("SGCL_ENV_X", "9090"));
    EXPECT_EQ(io::env("SGCL_ENV_X", 8080), 9090);
    EXPECT_EQ(io::env("SGCL_ENV_X", uint16_t(1)), 9090);
    EXPECT_EQ(io::env("SGCL_ENV_X", 0.5), 9090.0);
    EXPECT_EQ(std::string_view(io::env("SGCL_ENV_X", "localhost")), "9090");
    ASSERT_TRUE(io::setenv("SGCL_ENV_X", "true"));
    EXPECT_TRUE(io::env("SGCL_ENV_X", false));
    ASSERT_TRUE(io::setenv("SGCL_ENV_X", "1m30s"));
    EXPECT_EQ(io::env("SGCL_ENV_X", 5s), 90 * sgcl::second);
    EXPECT_EQ(io::env("SGCL_ENV_X", sgcl::duration(5s)), 90 * sgcl::second);
    ASSERT_TRUE(io::setenv("SGCL_ENV_X", "3"));
    EXPECT_EQ(io::env("SGCL_ENV_X", Level{1}), Level{3});   // T::parse

    ASSERT_TRUE(io::setenv("SGCL_ENV_X", "abc"));
    try {
        (void)io::env("SGCL_ENV_X", 8080);
        ADD_FAILURE() << "no exception";
    } catch (const std::invalid_argument& e) {
        EXPECT_EQ(std::string_view(e.what()), "sgcl::io::env: SGCL_ENV_X=\"abc\" is not an integer: not a number");
    }
    ASSERT_TRUE(io::setenv("SGCL_ENV_X", "70000"));
    EXPECT_THROW((void)io::env("SGCL_ENV_X", uint16_t(1)), std::invalid_argument);   // out of the type's range
    ASSERT_TRUE(io::setenv("SGCL_ENV_X", "yes"));
    EXPECT_THROW((void)io::env("SGCL_ENV_X", false), std::invalid_argument);
    ASSERT_TRUE(io::setenv("SGCL_ENV_X", "5 s"));
    try {
        (void)io::env("SGCL_ENV_X", 5s);
        ADD_FAILURE() << "no exception";
    } catch (const std::invalid_argument& e) {
        EXPECT_TRUE(std::string_view(e.what()).starts_with("sgcl::io::env: SGCL_ENV_X=\"5 s\" is not a duration: "));
    }
    ASSERT_TRUE(io::setenv("SGCL_ENV_X", "high"));
    try {
        (void)io::env("SGCL_ENV_X", Level{1});
        ADD_FAILURE() << "no exception";
    } catch (const std::invalid_argument& e) {
        EXPECT_EQ(std::string_view(e.what()), "sgcl::io::env: SGCL_ENV_X=\"high\" is not a value of its type: not a number");
    }
    ASSERT_TRUE(io::unsetenv("SGCL_ENV_X"));
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
    EXPECT_EQ(value_of(io::stat(*now)).modified, value_of(io::stat(*tmp)).modified);
    ASSERT_TRUE(io::chdir(*wd));
    io::remove_all(*tmp);
}

TEST(IoOs_Tests, StandardStreams) {
    EXPECT_EQ(io::stdout.fd(), 1);
    EXPECT_FALSE(io::stdout.file().is_closed());
    EXPECT_EQ(io::stdout.file(), io::stdout.file());   // one file, made once
    EXPECT_EQ(io::stderr.fd(), 2);
    EXPECT_EQ(io::stderr.file().path(), "stderr");
    EXPECT_EQ(io::stdin.fd(), 0);
    // the C streams under their names in the same unit
    EXPECT_EQ(fileno(::stderr), 2);
    EXPECT_EQ(fileno(::stdin), 0);
    EXPECT_GE(fprintf(::stderr, "%s", ""), 0);
    EXPECT_EQ(is_terminal(1), ::isatty(1) == 1);
}
