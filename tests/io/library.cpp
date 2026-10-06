//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// io::open_library and io::library (a library of this test's own, compiled
// by the system's compiler at the start, and zlib of the system), hard links
// (io::link, file_info::links) and the owners (chown, lchown, file::chown,
// file_info's uid and gid, held against stat(1) of the system).
#include "tests/types.h"

#include <cstdlib>
#include <fstream>
#include <string>
#include <unistd.h>

namespace {
    namespace io = sgcl::io;
    using sgcl::string;

    struct IoLibrary_Tests : testing::Test {
        std::string _dir;

        void SetUp() override {
            _dir = io::make_temp_dir({}, "sgcl-library-*").value().str();
        }

        void TearDown() override {
            io::remove_all(string(_dir));
        }

        string at(const char* name) const {
            return string(_dir + "/" + name);
        }

        // A library with a function, a variable and a function that calls
        // back, built by the system's compiler
        string built() const {
            std::string src = _dir + "/plugin.c";
            std::ofstream(src) << "int sgcl_answer(int x) { return x * 2 + 1; }\n"
                                  "int sgcl_counter = 41;\n"
                                  "int sgcl_apply(int (*f)(int), int v) { return f(v); }\n";
            std::string out = _dir + "/" + std::string(io::library_file_name("plugin").view());
            std::string cmd = "cc -shared -fPIC -o '" + out + "' '" + src + "' 2>&1";
            EXPECT_EQ(std::system(cmd.c_str()), 0);
            return string(out);
        }
    };

    int twice(int v) {
        return v * 2;
    }
}

TEST_F(IoLibrary_Tests, ALibraryOfItsOwn) {
    string path = built();
    auto lib = io::open_library(path);
    ASSERT_TRUE(lib) << lib.error().message();
    EXPECT_TRUE(*lib);
    EXPECT_EQ(lib->path(), path);
    EXPECT_FALSE(lib->is_closed());
    auto answer = lib->symbol<int(int)>("sgcl_answer");
    ASSERT_TRUE(answer) << answer.error().message();
    EXPECT_EQ((*answer)(20), 41);
    auto counter = lib->address("sgcl_counter");
    ASSERT_TRUE(counter);
    EXPECT_EQ(*static_cast<int*>(*counter), 41);
    *static_cast<int*>(*counter) = 7;   // the library's own variable
    EXPECT_EQ(*static_cast<int*>(*lib->address("sgcl_counter")), 7);
    auto apply = lib->symbol<int(int (*)(int), int)>("sgcl_apply");
    ASSERT_TRUE(apply);
    EXPECT_EQ((*apply)(&twice, 21), 42);
    // a name it does not have: the loader's text
    auto missing = lib->symbol<void()>("sgcl_no_such_function");
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error().code(), io::errc::library);
    EXPECT_NE(missing.error().path().view().find("sgcl_no_such_function"), std::string_view::npos) << missing.error().message();
    EXPECT_EQ(missing.error().op(), "symbol");
    // copies are the library; close unloads, once
    io::library copy = *lib;
    EXPECT_TRUE(copy == *lib);
    ASSERT_TRUE(lib->close());
    EXPECT_TRUE(copy.is_closed());
    EXPECT_TRUE(copy.close());   // a second does nothing
    auto after = copy.symbol<int(int)>("sgcl_answer");
    ASSERT_FALSE(after);
    EXPECT_EQ(after.error().code(), io::errc::closed);
    // opened again, loaded again
    auto again = io::open_library(path, {.global = false, .lazy = true});
    ASSERT_TRUE(again);
    EXPECT_EQ((*again->symbol<int(int)>("sgcl_answer"))(1), 3);
}

TEST_F(IoLibrary_Tests, TheSystemsZlib) {
    // zlib by its name, found by the loader in the system's paths
    auto z = io::open_library(io::library_file_name("z"));
    ASSERT_TRUE(z) << z.error().message();
    auto bound = z->symbol<unsigned long(unsigned long)>("compressBound");
    ASSERT_TRUE(bound);
    EXPECT_EQ((*bound)(100), 100u + (100 >> 12) + (100 >> 14) + (100 >> 25) + 13);   // zlib.h's own formula
    auto version = z->symbol<const char*()>("zlibVersion");
    ASSERT_TRUE(version);
    EXPECT_EQ(std::string((*version)()).substr(0, 2), "1.");
}

TEST_F(IoLibrary_Tests, WhatCannotBeLoaded) {
    auto none = io::open_library(at("no-such-library.dylib"));
    ASSERT_FALSE(none);
    EXPECT_EQ(none.error().code(), io::errc::library);
    EXPECT_EQ(none.error().op(), "open_library");
    EXPECT_NE(none.error().path().view().find("no-such-library"), std::string_view::npos) << none.error().message();
    EXPECT_TRUE(none.error().message().view().ends_with(": dynamic library error"));
    std::ofstream(at("not-a-library").str()) << "text";
    EXPECT_FALSE(io::open_library(at("not-a-library")));
    io::library empty;
    EXPECT_FALSE(empty);
    EXPECT_EQ(io::library_file_name("z"), "libz.dylib");
}

TEST_F(IoLibrary_Tests, HardLinks) {
    ASSERT_TRUE(io::write_file(at("data.bin"), "bytes"));
    EXPECT_EQ(io::stat(at("data.bin"))->links, 1u);
    ASSERT_TRUE(io::link(at("data.bin"), at("data.link")));
    EXPECT_EQ(io::stat(at("data.bin"))->links, 2u);
    EXPECT_EQ(io::stat(at("data.link"))->links, 2u);
    ASSERT_TRUE(io::append_file(at("data.link"), "+"));   // one file under two names
    EXPECT_EQ(io::read_text(at("data.bin")).value(), "bytes+");
    auto taken = io::link(at("data.bin"), at("data.link"));
    ASSERT_FALSE(taken);
    EXPECT_TRUE(taken.error().is_exists());
    EXPECT_EQ(taken.error().op(), "link");
    EXPECT_FALSE(io::link(at("missing"), at("x")));
    ASSERT_TRUE(io::mkdir(at("dir")));
    EXPECT_FALSE(io::link(at("dir"), at("dir.link")));   // no hard link of a directory
    ASSERT_TRUE(io::remove(at("data.bin")));
    EXPECT_EQ(io::stat(at("data.link"))->links, 1u);
    EXPECT_EQ(sgcl::async::spawn(io::async_link(at("data.link"), at("third"))).wait().has_value(), true);
    EXPECT_EQ(io::stat(at("third"))->links, 2u);
}

TEST_F(IoLibrary_Tests, TheOwners) {
    ASSERT_TRUE(io::write_file(at("owned"), "x"));
    auto info = io::stat(at("owned"));
    ASSERT_TRUE(info);
    EXPECT_EQ(info->uid, uint32_t(::getuid()));
    // the group: on macOS the directory's (BSD), on Linux the process's; stat(1) is the oracle below
    // stat(1) agrees
    std::string cmd = "stat -f '%u %g' '" + at("owned").str() + "' > '" + at("stat.out").str() + "'";
    ASSERT_EQ(std::system(cmd.c_str()), 0);
    std::string text = std::string(io::read_text(at("stat.out")).value().view());
    EXPECT_EQ(text, std::to_string(info->uid) + " " + std::to_string(info->gid) + "\n");
    // to itself, -1 keeping: allowed to anyone
    ASSERT_TRUE(io::chown(at("owned"), -1, -1));
    ASSERT_TRUE(io::chown(at("owned"), int(info->uid), int(info->gid)));
    ASSERT_TRUE(sgcl::async::spawn(io::async_chown(at("owned"), -1, int(info->gid))).wait());
    io::file f = io::open(at("owned")).value();
    ASSERT_TRUE(f.chown(int(info->uid), -1));
    ASSERT_TRUE(sgcl::async::spawn(f.async_chown(-1, -1)).wait());
    // a link itself
    ASSERT_TRUE(io::symlink(at("owned"), at("owned.lnk")));
    ASSERT_TRUE(io::lchown(at("owned.lnk"), -1, -1));
    // to root: refused to a process that is not
    if (::geteuid() != 0) {
        auto refused = io::chown(at("owned"), 0, -1);
        ASSERT_FALSE(refused);
        EXPECT_TRUE(refused.error().is_permission());
        EXPECT_EQ(refused.error().op(), "chown");
        EXPECT_FALSE(f.chown(0, -1));
    }
    EXPECT_FALSE(io::chown(at("missing"), -1, -1));
    ASSERT_TRUE(f.close());
    auto closed = f.chown(-1, -1);
    ASSERT_FALSE(closed);
    EXPECT_EQ(closed.error().code(), io::errc::closed);
}
