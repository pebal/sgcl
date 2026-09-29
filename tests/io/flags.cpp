//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// io::flags: the command line as Go's flag package takes it. Go itself is
// the oracle (tests/io/go_flags/main.go): the same flags, the same command
// lines, and the usage, the error of every case, the value of every flag
// after it and the arguments left compared byte for byte. Then what Go
// has no counterpart of: the positional list's absence, the description
// as a value, the program's own types, the rules at add, and parse(argc,
// argv) ending the process with 0 and 2.
#include "tests/types.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {
    namespace io = sgcl::io;
    using sgcl::string;
    using sgcl::vector;

    // The variables of the flags Go's program defines, with its defaults
    struct Vars {
        int port = 8080;
        string host = "localhost";
        bool v = false;
        bool tls = true;
        sgcl::duration timeout = 5 * sgcl::second;
        double ratio = 0.5;
        int64_t n = 0;
        uint64_t size = 0;
        string name;
        string note = "a \"quoted\"\ttab";
        double big = 1e6;
        vector<string> rest;

        io::flags described(bool positional) {
            io::flags f;
            f.add("port", port, "the `port` to listen on");
            f.add("host", host, "the host");
            f.add("v", v, "verbose");
            f.add("tls", tls, "serve over TLS");
            f.add("timeout", timeout, "how long to wait");
            f.add("ratio", ratio, "a ratio");
            f.add("n", n, "a count");
            f.add("size", size, "a size");
            f.add("name", name, "a name");
            f.add("note", note, "a note\non two lines");
            f.add("big", big, "a large number");
            if (positional) {
                f.positional("args", rest, "the rest");
            }
            return f;
        }

        // Go's output of a case: the flags by name, Value.String() quoted
        std::string values() const {
            std::vector<std::pair<std::string, std::string>> all = {
                {"big", io::detail::flag_text(big)},
                {"host", io::detail::flag_text(host)},
                {"n", io::detail::flag_text(n)},
                {"name", io::detail::flag_text(name)},
                {"note", io::detail::flag_text(note)},
                {"port", io::detail::flag_text(port)},
                {"ratio", io::detail::flag_text(ratio)},
                {"size", io::detail::flag_text(size)},
                {"timeout", io::detail::flag_text(timeout)},
                {"tls", io::detail::flag_text(tls)},
                {"v", io::detail::flag_text(v)},
            };
            std::string out;
            for (auto& [k, val] : all) {
                out += k + "=";
                io::detail::go_quote(out, val);
                out += '\n';
            }
            for (auto& a : rest) {
                out += "arg ";
                io::detail::go_quote(out, a.view());
                out += '\n';
            }
            return out;
        }
    };

    std::vector<std::vector<std::string>> cases() {
        return {
            {},
            {"-port", "9090"},
            {"--port=0x1F"},
            {"-port=1_000"},
            {"-port", "0b101"},
            {"-port", "017"},
            {"-port", "0o17"},
            {"-port", "0"},
            {"-port", "+5"},
            {"-port", "-5"},
            {"-port", "0x"},
            {"-port", "_1"},
            {"-port", "1__0"},
            {"-port", "1_"},
            {"-port", "0x_1F"},
            {"-port", "0_17"},
            {"-port="},
            {"-port", "a\xff"},
            {"-port", "\"q\"\\"},
            {"-port", "1", "-port", "2"},
            {"-n", "9223372036854775807"},
            {"-n", "9223372036854775808"},
            {"-n", "-9223372036854775808"},
            {"-n", "-9223372036854775809"},
            {"-n", "99999999999999999999x"},
            {"-size", "-1"},
            {"-size", "18446744073709551615"},
            {"-size", "18446744073709551616"},
            {"-size", "0xFFFF_FFFF"},
            {"-v"},
            {"--v"},
            {"-v=false"},
            {"-v=yes"},
            {"-v=TRUE"},
            {"-v="},
            {"-v", "false"},
            {"-tls=0"},
            {"-tls=F"},
            {"-timeout", "1m30s"},
            {"-timeout", "1.5h"},
            {"-timeout", "5"},
            {"-timeout", "-1.5ms"},
            {"-timeout", "1h2m3.5s4ms"},
            {"-ratio", "1e6"},
            {"-ratio", "123456"},
            {"-ratio", "1234567"},
            {"-ratio", "0.0001"},
            {"-ratio", "0.00001"},
            {"-ratio", "0x1p-2"},
            {"-ratio", "0x1.8p1"},
            {"-ratio", "inf"},
            {"-ratio", "+Inf"},
            {"-ratio", "-Infinity"},
            {"-ratio", "infinit"},
            {"-ratio", "nan"},
            {"-ratio", "NaN"},
            {"-ratio", "+nan"},
            {"-ratio", "1e400"},
            {"-ratio", "-1e400"},
            {"-ratio", "1e-400"},
            {"-ratio", "1_000.5"},
            {"-ratio", "1__0.5"},
            {"-ratio", "1e1_0"},
            {"-ratio", ".5"},
            {"-ratio", "5."},
            {"-ratio", "."},
            {"-ratio", "0x1.8"},
            {"-ratio", "0x"},
            {"-ratio", "1e"},
            {"-ratio", "-0"},
            {"-ratio", "abc"},
            {"-name", "\xc5\xbc\xc3\xb3\xc5\x82\xc4\x87\xc2\xa0\x01\x7f\a\v"},
            {"-name", "\xe2\x80\xa8\xef\xbf\xbd\xf0\x9f\x98\x80\xf4\x8f\xbf\xbf"},
            {"-name", "\xed\xa0\x80\xc0\xaf"},
            {"-name", ""},
            {"-name"},
            {"-x"},
            {"-x=1"},
            {"---x"},
            {"-=x"},
            {"--=x"},
            {"-h"},
            {"--help"},
            {"-help=1"},
            {"file", "-v"},
            {"-v", "--", "-port", "1"},
            {"-", "x"},
            {"--"},
            {"-v", "-", "-port", "1"},
        };
    }

    // The input of Go's program: a case a line, the arguments separated by
    // 0x1f
    std::string cases_text() {
        std::string out;
        for (auto& c : cases()) {
            for (size_t i = 0; i < c.size(); ++i) {
                if (i) {
                    out += '\x1f';
                }
                out += c[i];
            }
            out += '\n';
        }
        return out;
    }

    std::string go_output() {
        const auto dir = std::filesystem::path(__FILE__).parent_path() / "go_flags";
        const auto tmp = std::filesystem::temp_directory_path() / ("sgcl_io_go_flags_" + std::to_string(::getpid()));
        std::filesystem::create_directories(tmp);
        const auto bin = tmp / "go_flags";
        {
            std::ofstream mod(tmp / "go.mod");
            mod << "module goflags\n\ngo 1.22\n";
        }
        std::filesystem::copy_file(dir / "main.go", tmp / "main.go", std::filesystem::copy_options::overwrite_existing);
        std::string build = "cd '" + tmp.string() + "' && go build -o '" + bin.string() + "' . 2>&1";
        if (std::system(build.c_str()) != 0) {
            return {};
        }
        {
            std::ofstream in(tmp / "cases.txt", std::ios::binary);
            in << cases_text();
        }
        std::string run = "'" + bin.string() + "' < '" + (tmp / "cases.txt").string() + "' > '" + (tmp / "out.txt").string() + "'";
        if (std::system(run.c_str()) != 0) {
            return {};
        }
        std::ifstream out(tmp / "out.txt", std::ios::binary);
        std::stringstream ss;
        ss << out.rdbuf();
        std::filesystem::remove_all(tmp);
        return ss.str();
    }

    std::string after_header(std::string_view usage) {
        return std::string(usage.substr(usage.find('\n') + 1));
    }
}

TEST(IoFlags_Tests, AsGoTakesThem) {
    if (std::system("command -v go > /dev/null 2>&1") != 0) {
        GTEST_SKIP() << "no go on the PATH";
    }
    std::string go = go_output();
    ASSERT_FALSE(go.empty()) << "the Go program did not build or run";
    // the usage: PrintDefaults byte for byte
    auto u0 = go.find("=== usage\n") + 10;
    auto u1 = go.find("=== end\n");
    Vars defaults;
    EXPECT_EQ(after_header(defaults.described(false).usage().view()), go.substr(u0, u1 - u0));
    // every case
    std::string_view rest(go);
    rest.remove_prefix(u1 + 8);
    auto all = cases();
    for (size_t k = 0; k < all.size(); ++k) {
        ASSERT_TRUE(rest.starts_with("=== case\n")) << k;
        rest.remove_prefix(9);
        size_t next = rest.find("=== case\n");
        std::string_view want = rest.substr(0, next);
        rest = next == std::string_view::npos ? std::string_view() : rest.substr(next);

        Vars vars;
        vector<string> args;
        for (auto& a : all[k]) {
            args.push_back(string(std::string_view(a)));
        }
        auto r = vars.described(true).parse(args);
        std::string got = "err ";
        if (r) {
            got += "nil";
        } else if (r.error().code() == io::errc::help_requested) {
            got += std::string(r.error().message().view());   // "flag: help requested", Go's ErrHelp
        } else {
            EXPECT_EQ(r.error().code(), io::make_error_code(io::errc::invalid_argument));
            got += std::string(r.error().path().view());
        }
        got += '\n';
        if (r) {
            got += vars.values();
        }
        std::string text;
        for (auto& a : all[k]) {
            io::detail::go_quote(text, a);
            text += ' ';
        }
        EXPECT_EQ(got, want) << "case " << k << ": " << text;
    }
    EXPECT_TRUE(rest.empty());
}

TEST(IoFlags_Tests, NoPositionalListRefusesTheRest) {
    Vars vars;
    auto r = vars.described(false).parse(vector<string>{"-v", "file"});
    ASSERT_FALSE(r);
    EXPECT_EQ(std::string_view(r.error().path()), "unexpected argument: file");
    EXPECT_TRUE(vars.v);   // what came before it is set, as Go sets it
    EXPECT_EQ(std::string_view(r.error().message()), "flag unexpected argument: file: invalid command line");
    EXPECT_TRUE(vars.described(false).parse(vector<string>{"-v", "--"}));
}

// A value refused leaves its variable as it was (Go's Set stores what
// strconv returned, 0 or the type's limit, before it fails)
TEST(IoFlags_Tests, ARefusedValueLeavesTheVariable) {
    Vars vars;
    EXPECT_FALSE(vars.described(true).parse(vector<string>{"-port", "x"}));
    EXPECT_EQ(vars.port, 8080);
    EXPECT_FALSE(vars.described(true).parse(vector<string>{"-n", "9223372036854775808"}));
    EXPECT_EQ(vars.n, 0);
    EXPECT_FALSE(vars.described(true).parse(vector<string>{"-ratio", "1e400"}));
    EXPECT_EQ(vars.ratio, 0.5);
    EXPECT_FALSE(vars.described(true).parse(vector<string>{"-timeout", "5"}));
    EXPECT_EQ(vars.timeout, 5 * sgcl::second);
    EXPECT_FALSE(vars.described(true).parse(vector<string>{"-tls=maybe"}));
    EXPECT_TRUE(vars.tls);
}

TEST(IoFlags_Tests, TheUsage) {
    int port = 8080;
    bool v = false;
    vector<string> files;
    io::flags f("serves the files of a directory");
    f.add("port", port, "the port to listen on");
    f.add("v", v, "log every request");
    f.positional("files", files, "the files to serve");
    string usage = f.usage();
    EXPECT_EQ(after_header(usage.view()),
              "serves the files of a directory\n"
              "  -port int\n"
              "    \tthe port to listen on (default 8080)\n"
              "  -v\tlog every request\n"
              "  files...\n"
              "    \tthe files to serve\n");
    EXPECT_TRUE(std::string_view(usage).starts_with("Usage of "));
}

// A copy has the flags added so far: a common set copied and extended
// per command, the common one left as it was
TEST(IoFlags_Tests, ACommonSetCopiedPerCommand) {
    bool v = false;
    int port = 1;
    int workers = 1;
    io::flags common;
    common.add("v", v, "verbose");
    io::flags serve = common;
    serve.add("port", port, "the port");
    io::flags work = common;
    work.add("workers", workers, "the workers");
    EXPECT_TRUE(serve.parse(vector<string>{"-v", "-port", "80"}));
    EXPECT_EQ(port, 80);
    EXPECT_FALSE(common.parse(vector<string>{"-port", "80"}));   // not in the common set
    EXPECT_FALSE(work.parse(vector<string>{"-port", "80"}));
    EXPECT_TRUE(work.parse(vector<string>{"-workers=4"}));
    EXPECT_EQ(workers, 4);
}

namespace {
    // A type of the program's: read by its parse, shown by its to_string
    struct Level {
        int n = 0;

        static sgcl::expected<Level, sgcl::number_error> parse(const string& text) {
            auto v = sgcl::parse<int>(text.view());
            if (!v) {
                return sgcl::unexpected(v.error());
            }
            return Level{*v};
        }

        string to_string() const {
            return sgcl::to_string(n);
        }
    };
}

TEST(IoFlags_Tests, AProgramsOwnType) {
    Level level{3};
    io::flags f;
    f.add("level", level, "the level");
    EXPECT_TRUE(f.parse(vector<string>{"-level", "7"}));
    EXPECT_EQ(level.n, 7);
    auto r = f.parse(vector<string>{"-level=high"});
    ASSERT_FALSE(r);
    EXPECT_EQ(std::string_view(r.error().path()), "invalid value \"high\" for flag -level: not a number");
    EXPECT_NE(std::string_view(f.usage()).find("  -level value\n    \tthe level (default 3)\n"), std::string_view::npos);
}

TEST(IoFlags_Tests, TheRulesAtAdd) {
    int x = 0;
    io::flags f;
    EXPECT_THROW(f.add("-x", x, ""), std::invalid_argument);
    EXPECT_THROW(f.add("a=b", x, ""), std::invalid_argument);
    EXPECT_THROW(f.add("", x, ""), std::invalid_argument);
    f.add("x", x, "");
    EXPECT_THROW(f.add("x", x, ""), std::invalid_argument);   // Go: flag redefined
}

// parse(argc, argv): -h prints the usage and ends with 0, a command line
// refused prints Go's message and the usage and ends with 2
TEST(IoFlags_Tests, TheProgramsCommandLineEndsTheProcess) {
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    auto run = [](std::vector<const char*> args) {
        int port = 8080;
        std::vector<char*> argv;
        for (auto a : args) {
            argv.push_back(const_cast<char*>(a));
        }
        io::flags f;
        f.add("port", port, "the port");
        f.parse(int(argv.size()), argv.data());
        std::exit(port == 9090 ? 3 : 4);   // parsed: the value tells
    };
    EXPECT_EXIT(run({"prog", "-port", "9090"}), testing::ExitedWithCode(3), "");
    EXPECT_EXIT(run({"prog", "-h"}), testing::ExitedWithCode(0), "^Usage of prog:\n  -port int\n    \tthe port \\(default 8080\\)\n$");
    EXPECT_EXIT(run({"prog", "-port", "x"}), testing::ExitedWithCode(2), "^invalid value \"x\" for flag -port: parse error\nUsage of prog:\n");
    EXPECT_EXIT(run({"prog", "-y"}), testing::ExitedWithCode(2), "^flag provided but not defined: -y\n");
}
