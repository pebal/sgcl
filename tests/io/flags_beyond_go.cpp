//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// io::flags beyond Go's flag package (tests/io/flags.cpp holds it to Go):
// a second name, combined one-letter bools, lists, the environment's
// defaults, required flags, subcommands, the one-line forms, and the usage
// of each. Go has none of these in its standard library: the cases are
// written out here, and that a command line Go takes keeps its meaning is
// tests/io/flags.cpp's Go oracle, run on the same parser.
#include "tests/types.h"

#include <cstdlib>
#include <string>
#include <string_view>

namespace {
    namespace io = sgcl::io;
    using sgcl::string;
    using sgcl::vector;

    vector<string> line(std::initializer_list<const char*> args) {
        vector<string> out;
        for (auto a : args) {
            out.push_back(string(a));
        }
        return out;
    }

    std::string refused(const sgcl::expected<void, io::error>& r) {
        if (r) {
            return "ok";
        }
        if (r.error().code() == io::errc::help_requested) {
            return "help";
        }
        return std::string(r.error().path().view());
    }

    std::string after_header(std::string_view usage) {
        return std::string(usage.substr(usage.find('\n') + 1));
    }

    struct Env {
        const char* name;
        Env(const char* n, const char* value)
        : name(n) {
            ::setenv(n, value, 1);
        }
        ~Env() {
            ::unsetenv(name);
        }
    };
}

TEST(IoFlagsBeyondGo_Tests, ASecondName) {
    int port = 8080;
    bool verbose = false;
    io::flags f;
    f.add("port", port, "the port", {.short_name = "p"});
    f.add("verbose", verbose, "verbose", {.short_name = "v"});
    EXPECT_EQ(refused(f.parse(line({"-p", "1", "-v"}))), "ok");
    EXPECT_EQ(port, 1);
    EXPECT_TRUE(verbose);
    EXPECT_EQ(refused(f.parse(line({"--p=2", "--verbose=false"}))), "ok");
    EXPECT_EQ(port, 2);
    EXPECT_FALSE(verbose);
    EXPECT_EQ(refused(f.parse(line({"-port", "3"}))), "ok");
    EXPECT_EQ(port, 3);
    EXPECT_EQ(refused(f.parse(line({"-p"}))), "flag needs an argument: -p");   // the name as written
    EXPECT_EQ(refused(f.parse(line({"-p", "x"}))), "invalid value \"x\" for flag -p: parse error");
    EXPECT_EQ(after_header(f.usage().view()),
              "  -p, -port int\n"
              "    \tthe port (default 8080)\n"
              "  -v, -verbose\n"
              "    \tverbose\n");
    // a second name taken already, or the flag's own, or a bad one
    int other = 0;
    EXPECT_THROW(f.add("x", other, "", {.short_name = "p"}), std::invalid_argument);
    EXPECT_THROW(f.add("p", other, ""), std::invalid_argument);
    EXPECT_THROW(f.add("y", other, "", {.short_name = "y"}), std::invalid_argument);
    EXPECT_THROW(f.add("z", other, "", {.short_name = "-z"}), std::invalid_argument);
    EXPECT_THROW(f.add("w", other, "", {.short_name = "a=b"}), std::invalid_argument);
}

TEST(IoFlagsBeyondGo_Tests, CombinedOneLetterBools) {
    bool a = false, b = false, c = false;
    int n = 0;
    io::flags f;
    f.add("a", a, "");
    f.add("b", b, "");
    f.add("all", c, "", {.short_name = "c"});
    f.add("n", n, "");
    EXPECT_EQ(refused(f.parse(line({"-abc"}))), "ok");
    EXPECT_TRUE(a && b && c);
    a = b = c = false;
    EXPECT_EQ(refused(f.parse(line({"--ba"}))), "ok");
    EXPECT_TRUE(a && b && !c);
    EXPECT_EQ(refused(f.parse(line({"-an"}))), "flag provided but not defined: -an");     // n is no bool
    EXPECT_EQ(refused(f.parse(line({"-ab=1"}))), "flag provided but not defined: -ab");   // a value: no combination
    EXPECT_EQ(refused(f.parse(line({"-ax"}))), "flag provided but not defined: -ax");
    // a flag of the whole name wins over the combination
    bool ab = false;
    a = b = false;
    f.add("ab", ab, "");
    EXPECT_EQ(refused(f.parse(line({"-ab"}))), "ok");
    EXPECT_TRUE(ab && !a && !b);
    // -h stays help
    bool h = false, e = false, l = false, p = false;
    io::flags g;
    g.add("e", e, "");
    g.add("l", l, "");
    g.add("p", p, "");
    g.add("h", h, "");   // defined: -h is the flag
    EXPECT_EQ(refused(g.parse(line({"-help"}))), "help");   // not h, e, l, p
}

TEST(IoFlagsBeyondGo_Tests, Lists) {
    vector<string> include = {"/usr/include"};
    vector<int> ports;
    io::flags f;
    f.add("I", include, "an include directory");
    f.add("port", ports, "a port", {.short_name = "p"});
    EXPECT_EQ(refused(f.parse(line({}))), "ok");
    EXPECT_EQ(include.size(), 1u);   // the default, untouched
    EXPECT_EQ(refused(f.parse(line({"-I", "a", "-p", "1", "-I=b", "--port=2", "-I", ""}))), "ok");
    ASSERT_EQ(include.size(), 3u);   // the first occurrence replaced the default
    EXPECT_EQ(include[0], "a");
    EXPECT_EQ(include[1], "b");
    EXPECT_EQ(include[2], "");
    ASSERT_EQ(ports.size(), 2u);
    EXPECT_EQ(ports[1], 2);
    // a refused value leaves the list as it was at that point, the message Go's
    EXPECT_EQ(refused(f.parse(line({"-p", "7", "-p", "x"}))), "invalid value \"x\" for flag -p: parse error");
    ASSERT_EQ(ports.size(), 1u);
    EXPECT_EQ(ports[0], 7);
    EXPECT_EQ(refused(f.parse(line({"-p"}))), "flag needs an argument: -p");
    EXPECT_EQ(after_header(f.usage().view()),
              "  -I string...\n"
              "    \tan include directory (default [/usr/include])\n"   // the default is the variable's at add, as Go's
              "  -p, -port int...\n"
              "    \ta port\n");
    // a list of bools takes -flag as true each time
    vector<bool> seen;
    io::flags g;
    g.add("x", seen, "");
    EXPECT_EQ(refused(g.parse(line({"-x", "-x=false", "-x"}))), "ok");
    ASSERT_EQ(seen.size(), 3u);
    EXPECT_TRUE(seen[0] && !seen[1] && seen[2]);
}

TEST(IoFlagsBeyondGo_Tests, TheEnvironmentsDefaults) {
    int port = 8080;
    vector<string> tags;
    sgcl::duration timeout = 5 * sgcl::second;
    io::flags f;
    f.add("port", port, "the port", {.env = "SGCL_TEST_PORT"});
    f.add("tag", tags, "a tag", {.env = "SGCL_TEST_TAGS"});
    f.add("timeout", timeout, "the timeout", {.env = "SGCL_TEST_TIMEOUT"});
    {
        Env a("SGCL_TEST_PORT", "9090");
        Env b("SGCL_TEST_TAGS", "x,y,,z");
        Env c("SGCL_TEST_TIMEOUT", "1m");
        EXPECT_EQ(refused(f.parse(line({}))), "ok");
        EXPECT_EQ(port, 9090);
        ASSERT_EQ(tags.size(), 4u);
        EXPECT_EQ(tags[2], "");
        EXPECT_EQ(timeout, sgcl::minute);
        EXPECT_EQ(refused(f.parse(line({"-port", "1", "-tag", "w"}))), "ok");   // the command line wins
        EXPECT_EQ(port, 1);
        ASSERT_EQ(tags.size(), 5u);   // the environment's list, then the command line's value after it
        EXPECT_EQ(tags[4], "w");
    }
    {
        Env a("SGCL_TEST_PORT", "eighty");
        EXPECT_EQ(refused(f.parse(line({}))), "invalid value \"eighty\" for $SGCL_TEST_PORT (flag -port): parse error");
        EXPECT_EQ(refused(f.parse(line({"-port", "80"}))), "invalid value \"eighty\" for $SGCL_TEST_PORT (flag -port): parse error");
    }
    {
        Env a("SGCL_TEST_PORT", "");   // empty: unset
        port = 3;
        EXPECT_EQ(refused(f.parse(line({}))), "ok");
        EXPECT_EQ(port, 3);
    }
    EXPECT_NE(f.usage().view().find("  -port int\n    \tthe port (default 8080) [$SGCL_TEST_PORT]\n"), std::string_view::npos) << f.usage().view();
}

TEST(IoFlagsBeyondGo_Tests, RequiredFlags) {
    string user;
    int port = 0;
    io::flags f;
    f.add("user", user, "the user", {.required = true});
    f.add("port", port, "the port", {.env = "SGCL_TEST_REQ_PORT", .required = true});
    EXPECT_EQ(refused(f.parse(line({}))), "flag is required: -port");
    EXPECT_EQ(refused(f.parse(line({"-port", "1"}))), "flag is required: -user");
    EXPECT_EQ(refused(f.parse(line({"-port", "1", "-user", ""}))), "ok");   // given, though empty
    {
        Env a("SGCL_TEST_REQ_PORT", "2");
        EXPECT_EQ(refused(f.parse(line({"-user", "ann"}))), "ok");   // the environment gives it
        EXPECT_EQ(port, 2);
    }
    EXPECT_EQ(refused(f.parse(line({"-h"}))), "help");   // help before the check
    EXPECT_NE(f.usage().view().find("  -user string\n    \tthe user (required)\n"), std::string_view::npos) << f.usage().view();
}

TEST(IoFlagsBeyondGo_Tests, Subcommands) {
    bool verbose = false;
    int port = 8080;
    bool release = false;
    vector<string> targets;
    io::flags root("a tool");
    root.add("v", verbose, "verbose");
    io::flags serve("runs the server");
    serve.add("port", port, "the port");
    io::flags build("builds the targets\non two lines");
    build.add("release", release, "an optimized build");
    build.positional("targets", targets, "what to build");
    root.add_command("serve", serve);
    root.add_command("build", build);
    EXPECT_EQ(refused(root.parse(line({"-v", "serve", "-port", "1"}))), "ok");
    EXPECT_TRUE(verbose);
    EXPECT_EQ(port, 1);
    EXPECT_TRUE(serve.chosen());
    EXPECT_FALSE(build.chosen());
    EXPECT_FALSE(root.chosen());
    EXPECT_EQ(refused(root.parse(line({"build", "-release", "a", "b"}))), "ok");
    EXPECT_FALSE(serve.chosen());
    EXPECT_TRUE(build.chosen());
    EXPECT_TRUE(release);
    ASSERT_EQ(targets.size(), 2u);
    EXPECT_EQ(refused(root.parse(line({}))), "ok");   // no command: none chosen
    EXPECT_FALSE(serve.chosen() || build.chosen());
    EXPECT_EQ(refused(root.parse(line({"deploy"}))), "unknown command: deploy");
    EXPECT_EQ(refused(root.parse(line({"serve", "-v"}))), "flag provided but not defined: -v");   // the root's flags before the command
    EXPECT_EQ(refused(root.parse(line({"serve", "extra"}))), "unexpected argument: extra");
    EXPECT_EQ(refused(root.parse(line({"serve", "-h"}))), "help");
    EXPECT_TRUE(serve.chosen());   // the help asked for is the command's
    EXPECT_EQ(refused(root.parse(line({"--", "serve"}))), "ok");   // after --: still the command's name
    EXPECT_TRUE(serve.chosen());
    // the usage of the root lists the commands, a command's has its name in the header
    EXPECT_EQ(after_header(root.usage().view()),
              "a tool\n"
              "  -v\tverbose\n"
              "Commands:\n"
              "  build\n"
              "    \tbuilds the targets\n"
              "    \ton two lines\n"
              "  serve\n"
              "    \truns the server\n");
    EXPECT_TRUE(serve.usage().view().find(" serve:\nruns the server\n  -port int\n") != std::string_view::npos) << serve.usage().view();
    // a command's own commands
    io::flags nested("nested");
    bool deep = false;
    nested.add("deep", deep, "");
    serve.add_command("tls", nested);
    EXPECT_EQ(refused(root.parse(line({"serve", "-port", "2", "tls", "-deep"}))), "ok");
    EXPECT_TRUE(serve.chosen() && nested.chosen() && deep);
    EXPECT_TRUE(nested.usage().view().find(" serve tls:\n") != std::string_view::npos) << nested.usage().view();
    // the rules at add_command
    io::flags x;
    EXPECT_THROW(root.add_command("serve", x), std::invalid_argument);
    EXPECT_THROW(root.add_command("", x), std::invalid_argument);
    EXPECT_THROW(root.add_command("-x", x), std::invalid_argument);
}

TEST(IoFlagsBeyondGo_Tests, CommandsBesideAPositionalList) {
    vector<string> files;
    bool run = false;
    io::flags root;
    root.positional("files", files, "the files");
    io::flags exec("runs");
    exec.add("now", run, "");
    root.add_command("exec", exec);
    EXPECT_EQ(refused(root.parse(line({"a.txt", "exec"}))), "ok");   // a name that is no command: the list's
    EXPECT_FALSE(exec.chosen());
    ASSERT_EQ(files.size(), 2u);
    EXPECT_EQ(refused(root.parse(line({"exec", "-now"}))), "ok");
    EXPECT_TRUE(exec.chosen() && run);
}

TEST(IoFlagsBeyondGo_Tests, TheOneLineForms) {
    int port = 8080;
    bool verbose = false;
    vector<string> tags;
    io::flags f("a server", {{"port", port, "the port", {.short_name = "p"}},
                             {"verbose", verbose, "verbose", {.short_name = "v"}},
                             {"tag", tags, "a tag"}});
    EXPECT_EQ(refused(f.parse(line({"-p", "1", "-v", "-tag", "a", "-tag", "b"}))), "ok");
    EXPECT_EQ(port, 1);
    EXPECT_TRUE(verbose);
    EXPECT_EQ(tags.size(), 2u);
    io::flags g({{"port", port, "the port"}});
    EXPECT_EQ(refused(g.parse(line({"-port=9"}))), "ok");
    EXPECT_EQ(port, 9);
    EXPECT_THROW((io::flags{{"x", port, ""}, {"x", verbose, ""}}), std::invalid_argument);
}

// parse_flags(argc, argv, ...): the process ended as parse(argc, argv) ends it
TEST(IoFlagsBeyondGo_Tests, ParseFlagsEndsTheProcess) {
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    auto run = [](std::vector<const char*> args) {
        int port = 8080;
        bool verbose = false;
        std::vector<char*> argv;
        for (auto a : args) {
            argv.push_back(const_cast<char*>(a));
        }
        io::parse_flags(int(argv.size()), argv.data(), {{"port", port, "the port", {.short_name = "p"}}, {"v", verbose, "verbose"}});
        std::exit(port == 9090 && verbose ? 3 : 4);
    };
    EXPECT_EXIT(run({"prog", "-p", "9090", "-v"}), testing::ExitedWithCode(3), "");
    EXPECT_EXIT(run({"prog", "-h"}), testing::ExitedWithCode(0), "^Usage of prog:\n  -p, -port int\n    \tthe port \\(default 8080\\)\n  -v\tverbose\n$");
    EXPECT_EXIT(run({"prog", "-q"}), testing::ExitedWithCode(2), "^flag provided but not defined: -q\nUsage of prog:\n");
    // a command's error prints the command's usage
    auto sub = [](std::vector<const char*> args) {
        int port = 0;
        std::vector<char*> argv;
        for (auto a : args) {
            argv.push_back(const_cast<char*>(a));
        }
        io::flags root;
        io::flags serve("serves", {{"port", port, "the port"}});
        root.add_command("serve", serve);
        root.parse(int(argv.size()), argv.data());
        std::exit(serve.chosen() ? 3 : 4);
    };
    EXPECT_EXIT(sub({"prog", "serve", "-port", "x"}), testing::ExitedWithCode(2), "^invalid value \"x\" for flag -port: parse error\nUsage of prog serve:\nserves\n");
    EXPECT_EXIT(sub({"prog", "serve", "-h"}), testing::ExitedWithCode(0), "^Usage of prog serve:\n");
    EXPECT_EXIT(sub({"prog", "serve"}), testing::ExitedWithCode(3), "");
}
