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

    std::string_view v(const sgcl::string& s) {
        return std::string_view(s);
    }
}

TEST(IoPath_Tests, Clean) {
    EXPECT_EQ(v(path::clean("")), ".");
    EXPECT_EQ(v(path::clean("abc")), "abc");
    EXPECT_EQ(v(path::clean("abc/def")), "abc/def");
    EXPECT_EQ(v(path::clean("a/b/c")), "a/b/c");
    EXPECT_EQ(v(path::clean(".")), ".");
    EXPECT_EQ(v(path::clean("..")), "..");
    EXPECT_EQ(v(path::clean("../..")), "../..");
    EXPECT_EQ(v(path::clean("../../abc")), "../../abc");
    EXPECT_EQ(v(path::clean("/abc")), "/abc");
    EXPECT_EQ(v(path::clean("/")), "/");
    EXPECT_EQ(v(path::clean("abc/")), "abc");
    EXPECT_EQ(v(path::clean("abc/def/")), "abc/def");
    EXPECT_EQ(v(path::clean("a/b/c/")), "a/b/c");
    EXPECT_EQ(v(path::clean("./")), ".");
    EXPECT_EQ(v(path::clean("../")), "..");
    EXPECT_EQ(v(path::clean("../../")), "../..");
    EXPECT_EQ(v(path::clean("/abc/")), "/abc");
    EXPECT_EQ(v(path::clean("abc//def//ghi")), "abc/def/ghi");
    EXPECT_EQ(v(path::clean("//abc")), "/abc");
    EXPECT_EQ(v(path::clean("///abc")), "/abc");
    EXPECT_EQ(v(path::clean("//abc//")), "/abc");
    EXPECT_EQ(v(path::clean("abc//")), "abc");
    EXPECT_EQ(v(path::clean("abc/./def")), "abc/def");
    EXPECT_EQ(v(path::clean("/./abc/def")), "/abc/def");
    EXPECT_EQ(v(path::clean("abc/.")), "abc");
    EXPECT_EQ(v(path::clean("abc/def/ghi/../jkl")), "abc/def/jkl");
    EXPECT_EQ(v(path::clean("abc/def/../ghi/../jkl")), "abc/jkl");
    EXPECT_EQ(v(path::clean("abc/def/..")), "abc");
    EXPECT_EQ(v(path::clean("abc/def/../..")), ".");
    EXPECT_EQ(v(path::clean("/abc/def/../..")), "/");
    EXPECT_EQ(v(path::clean("abc/def/../../..")), "..");
    EXPECT_EQ(v(path::clean("/abc/def/../../..")), "/");
    EXPECT_EQ(v(path::clean("abc/def/../../../ghi/jkl/../../../mno")), "../../mno");
    EXPECT_EQ(v(path::clean("/../abc")), "/abc");
    EXPECT_EQ(v(path::clean("abc/./../def")), "def");
    EXPECT_EQ(v(path::clean("abc//./../def")), "def");
    EXPECT_EQ(v(path::clean("abc/../../././../def")), "../../def");
}

TEST(IoPath_Tests, JoinBaseDirExtStemSplit) {
    EXPECT_EQ(v(path::join("a", "b", "c")), "a/b/c");
    EXPECT_EQ(v(path::join("a", "")), "a");
    EXPECT_EQ(v(path::join("", "")), "");
    EXPECT_EQ(v(path::join("/", "a")), "/a");
    EXPECT_EQ(v(path::join("a/", "/b")), "a/b");
    EXPECT_EQ(v(path::join("a/../", "b")), "b");
    EXPECT_EQ(v(path::join({"x", "y"})), "x/y");
    EXPECT_EQ(v(path::base("")), "");
    EXPECT_EQ(v(path::base("/")), "/");
    EXPECT_EQ(v(path::base("a/b/c.txt")), "c.txt");
    EXPECT_EQ(v(path::base("a/b/")), "b");
    EXPECT_EQ(v(path::base("c")), "c");
    EXPECT_EQ(v(path::dir("")), ".");
    EXPECT_EQ(v(path::dir("a")), ".");
    EXPECT_EQ(v(path::dir("a/b/c")), "a/b");
    EXPECT_EQ(v(path::dir("/a")), "/");
    EXPECT_EQ(v(path::dir("/")), "/");
    EXPECT_EQ(v(path::dir("a/b/")), "a/b");
    EXPECT_EQ(v(path::ext("a/b.tar.gz")), ".gz");
    EXPECT_EQ(v(path::ext("a.b/c")), "");
    EXPECT_EQ(v(path::ext(".bashrc")), ".bashrc");
    EXPECT_EQ(v(path::stem("a/b.tar.gz")), "b.tar");
    EXPECT_EQ(v(path::stem("a/b")), "b");
    auto [d, f] = path::split("a/b/c.txt");
    EXPECT_EQ(v(d), "a/b/");
    EXPECT_EQ(v(f), "c.txt");
    auto [d2, f2] = path::split("c.txt");
    EXPECT_EQ(v(d2), "");
    EXPECT_EQ(v(f2), "c.txt");
    auto list = path::split_list("/usr/bin::/bin:");
    ASSERT_EQ(list.size(), 2u);
    EXPECT_EQ(v(list[0]), "/usr/bin");
    EXPECT_EQ(v(list[1]), "/bin");
    EXPECT_TRUE(path::is_abs("/x"));
    EXPECT_FALSE(path::is_abs("x"));
    EXPECT_FALSE(path::is_abs(""));
}

TEST(IoPath_Tests, AbsAndRel) {
    auto a = path::abs("/x/../y");
    ASSERT_TRUE(a);
    EXPECT_EQ(v(*a), "/y");
    auto r = path::abs("z");
    ASSERT_TRUE(r);
    EXPECT_TRUE(path::is_abs(*r));
    EXPECT_TRUE(r->ends_with("/z"));
    EXPECT_EQ(v(*path::rel("a/b", "a/b/c/d")), "c/d");
    EXPECT_EQ(v(*path::rel("a/b/c", "a/b")), "..");
    EXPECT_EQ(v(*path::rel("a/b", "a/b")), ".");
    EXPECT_EQ(v(*path::rel("a", "b")), "../b");
    EXPECT_EQ(v(*path::rel("/a/b", "/a/c/d")), "../c/d");
    EXPECT_EQ(v(*path::rel("/", "/a")), "a");
    EXPECT_EQ(v(*path::rel(".", "a/b")), "a/b");
    EXPECT_EQ(v(*path::rel("a/b", ".")), "../..");
    EXPECT_EQ(v(*path::rel("a/../b", "c")), "../c");
    EXPECT_EQ(v(*path::rel("ab", "abc")), "../abc");   // not a prefix of elements
    auto bad = path::rel("/a", "b");
    ASSERT_FALSE(bad);
    EXPECT_EQ(bad.error().code(), make_error_code(errc::invalid_path));
    auto up = path::rel("..", "a");
    ASSERT_FALSE(up);
}

TEST(IoPath_Tests, Match) {
    EXPECT_TRUE(*path::match("abc", "abc"));
    EXPECT_TRUE(*path::match("*", "abc"));
    EXPECT_TRUE(*path::match("*c", "abc"));
    EXPECT_TRUE(*path::match("a*", "a"));
    EXPECT_TRUE(*path::match("a*", "abc"));
    EXPECT_FALSE(*path::match("a*", "ab/c"));   // '*' stops at the separator
    EXPECT_TRUE(*path::match("a*/b", "abc/b"));
    EXPECT_FALSE(*path::match("a*/b", "a/c/b"));
    EXPECT_TRUE(*path::match("a*b*c*d*e*/f", "axbxcxdxe/f"));
    EXPECT_TRUE(*path::match("a*b*c*d*e*/f", "axbxcxdxexxx/f"));
    EXPECT_FALSE(*path::match("a*b*c*d*e*/f", "axbxcxdxe/xxx/f"));
    EXPECT_TRUE(*path::match("a*b?c*x", "abxbbxdbxebxczzx"));
    EXPECT_FALSE(*path::match("a*b?c*x", "abxbbxdbxebxczzy"));
    EXPECT_TRUE(*path::match("ab[c]", "abc"));
    EXPECT_TRUE(*path::match("ab[b-d]", "abc"));
    EXPECT_FALSE(*path::match("ab[e-g]", "abc"));
    EXPECT_FALSE(*path::match("ab[^c]", "abc"));
    EXPECT_TRUE(*path::match("ab[^b-d]", "abz"));
    EXPECT_TRUE(*path::match("a\\*b", "a*b"));
    EXPECT_FALSE(*path::match("a\\*b", "ab"));
    EXPECT_TRUE(*path::match("a?b", "a☺b"));   // a character is a code point
    EXPECT_FALSE(*path::match("a?b", "a☺☺b"));
    EXPECT_TRUE(*path::match("[a-ζ]*", "α"));
    EXPECT_FALSE(*path::match("[a-ζ]", "ω"));
    EXPECT_TRUE(*path::match("*ω", "αβω"));
    EXPECT_FALSE(*path::match("*x", "xxx/"));
    EXPECT_FALSE(*path::match("a", ""));
    EXPECT_TRUE(*path::match("", ""));
    auto bad = path::match("[", "a");
    ASSERT_FALSE(bad);
    EXPECT_EQ(bad.error().code(), make_error_code(errc::invalid_pattern));
    EXPECT_FALSE(path::match("a[", "a"));
    EXPECT_FALSE(path::match("a\\", "a"));
    EXPECT_FALSE(path::match("[z-a]", "a"));
}

TEST(IoPath_Tests, Glob) {
    auto d = make_temp_dir({}, "sgcl-glob-*");
    ASSERT_TRUE(d);
    string dir = *d;
    ASSERT_TRUE(io::mkdir_all(path::join(dir, "a/x")));
    ASSERT_TRUE(io::mkdir_all(path::join(dir, "b/x")));
    ASSERT_TRUE(write_file(path::join(dir, "a/x/1.txt"), ""));
    ASSERT_TRUE(write_file(path::join(dir, "a/x/2.log"), ""));
    ASSERT_TRUE(write_file(path::join(dir, "b/x/3.txt"), ""));
    ASSERT_TRUE(write_file(path::join(dir, "top.txt"), ""));
    auto g = path::glob(dir + "/*/x/*.txt");
    ASSERT_TRUE(g);
    ASSERT_EQ(g->size(), 2u);
    EXPECT_EQ((*g)[0], dir + "/a/x/1.txt");
    EXPECT_EQ((*g)[1], dir + "/b/x/3.txt");
    g = path::glob(dir + "/*.txt");
    ASSERT_EQ(g->size(), 1u);
    g = path::glob(dir + "/top.txt");   // no meta: the file if it exists
    ASSERT_EQ(g->size(), 1u);
    g = path::glob(dir + "/none.txt");
    ASSERT_EQ(g->size(), 0u);
    g = path::glob(dir + "/nodir/*");
    ASSERT_EQ(g->size(), 0u);
    EXPECT_FALSE(path::glob("["));
    io::remove_all(dir);
}
