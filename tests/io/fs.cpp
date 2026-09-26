//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

using namespace sgcl::async;

#include <string>
#include <string_view>

namespace {
    using namespace sgcl::io;
    namespace io = sgcl::io;

    struct IoFs_Tests : testing::Test {
        // A std::string: the fixture is allocated by gtest with new, where a
        // tracked_ptr (a sgcl::string) may not live (The rules, 1)
        std::string _dir;

        void SetUp() override {
            auto d = make_temp_dir({}, "sgcl-fs-*");
            ASSERT_TRUE(d) << d.error().message();
            _dir = d->str();
        }

        void TearDown() override {
            io::remove_all(dir());
        }

        string dir() const {
            return string(_dir);
        }

        string at(const string& name) const {
            return path::join(dir(), name);
        }
    };
}

TEST_F(IoFs_Tests, MkdirRemoveRename) {
    ASSERT_TRUE(io::mkdir(at("d")));
    EXPECT_TRUE(is_directory(at("d")));
    auto again = io::mkdir(at("d"));
    ASSERT_FALSE(again);
    EXPECT_TRUE(again.error().is_exists());
    auto deep = io::mkdir(at("x/y/z"));
    ASSERT_FALSE(deep);
    EXPECT_TRUE(deep.error().is_not_found());
    ASSERT_TRUE(io::mkdir_all(at("x/y/z")));
    ASSERT_TRUE(io::mkdir_all(at("x/y/z")));   // existing: fine
    EXPECT_TRUE(is_directory(at("x/y/z")));
    ASSERT_TRUE(write_file(at("x/f"), "f"));
    auto notdir = io::mkdir_all(at("x/f/g"));
    ASSERT_FALSE(notdir);

    ASSERT_TRUE(io::rename(at("x/f"), at("x/g")));
    EXPECT_FALSE(exists(at("x/f")));
    EXPECT_TRUE(is_regular(at("x/g")));
    ASSERT_TRUE(copy_file(at("x/g"), at("x/h")));
    EXPECT_EQ(std::string_view(*read_text(at("x/h"))), "f");

    auto full = io::remove(at("x"));   // not empty
    ASSERT_FALSE(full);
    ASSERT_TRUE(io::remove(at("x/g")));
    auto gone = io::remove(at("x/g"));
    ASSERT_FALSE(gone);
    EXPECT_TRUE(gone.error().is_not_found());
    ASSERT_TRUE(io::remove_all(at("x")));
    EXPECT_FALSE(exists(at("x")));
    ASSERT_TRUE(io::remove_all(at("x")));   // nothing there: no error
}

TEST_F(IoFs_Tests, StatLstatSymlinkChmod) {
    ASSERT_TRUE(write_file(at("t"), "12345"));
    auto s = io::stat(at("t"));
    ASSERT_TRUE(s);
    EXPECT_EQ(s->size, 5u);
    EXPECT_TRUE(s->is_regular());
    EXPECT_EQ(std::string_view(s->name), "t");
    auto now = std::chrono::system_clock::now();
    EXPECT_LT(now - s->modified, std::chrono::seconds(60));
    ASSERT_TRUE(io::symlink(at("t"), at("l")));
    EXPECT_TRUE(io::stat(at("l"))->is_regular());     // followed
    EXPECT_TRUE(io::lstat(at("l"))->is_symlink());    // not followed
    EXPECT_EQ(std::string_view(*read_link(at("l"))), at("t"));
    ASSERT_TRUE(io::chmod(at("t"), permissions::owner_read | permissions::owner_write));
    EXPECT_EQ(static_cast<unsigned>(io::stat(at("t"))->mode), 0600u);
    file_time then(std::chrono::seconds(1000000000));
    ASSERT_TRUE(set_modified(at("t"), then));
    EXPECT_EQ(io::stat(at("t"))->modified, then);
    auto missing = io::stat(at("none"));
    ASSERT_FALSE(missing);
    EXPECT_TRUE(missing.error().is_not_found());
    EXPECT_FALSE(exists(at("none")));
}

TEST_F(IoFs_Tests, ReadDirSortedWithTypes) {
    ASSERT_TRUE(io::mkdir(at("sub")));
    ASSERT_TRUE(write_file(at("b"), ""));
    ASSERT_TRUE(write_file(at("a"), ""));
    ASSERT_TRUE(io::symlink(at("a"), at("c")));
    auto entries = read_dir(dir());
    ASSERT_TRUE(entries);
    ASSERT_EQ(entries->size(), 4u);
    EXPECT_EQ(std::string_view((*entries)[0].name), "a");
    EXPECT_EQ((*entries)[0].type, file_type::regular);
    EXPECT_EQ(std::string_view((*entries)[1].name), "b");
    EXPECT_EQ(std::string_view((*entries)[2].name), "c");
    EXPECT_EQ((*entries)[2].type, file_type::symlink);
    EXPECT_EQ(std::string_view((*entries)[3].name), "sub");
    EXPECT_TRUE((*entries)[3].is_directory());
    EXPECT_EQ(std::string_view((*entries)[3].path), at("sub"));
    EXPECT_TRUE((*entries)[3].info()->is_directory());
    auto none = read_dir(at("none"));
    ASSERT_FALSE(none);
    EXPECT_TRUE(none.error().is_not_found());
}

TEST_F(IoFs_Tests, WalkDirOrderSkipStop) {
    ASSERT_TRUE(io::mkdir_all(at("a/b")));
    ASSERT_TRUE(io::mkdir_all(at("a/skip/deep")));
    ASSERT_TRUE(write_file(at("a/b/f"), ""));
    ASSERT_TRUE(write_file(at("a/g"), ""));
    ASSERT_TRUE(write_file(at("z"), ""));
    std::vector<std::string> seen;
    auto r = walk_dir(dir(), [&](const directory_entry& e, const optional<error>& err) {
        EXPECT_FALSE(err);
        seen.push_back(e.path.str().substr(_dir.size() + 1));
        return std::string_view(e.name) == "skip" ? walk_action::skip_dir : walk_action::next;
    });
    ASSERT_TRUE(r);
    std::vector<std::string> expected = {"a", "a/b", "a/b/f", "a/g", "a/skip", "z"};
    EXPECT_EQ(seen, expected);
    int count = 0;
    walk_dir(dir(), [&](const directory_entry&, const optional<error>&) {
        return ++count == 2 ? walk_action::stop : walk_action::next;
    });
    EXPECT_EQ(count, 2);
    auto notdir = walk_dir(at("z"), [](const directory_entry&, const optional<error>&) { return walk_action::next; });
    ASSERT_FALSE(notdir);
}

namespace {
    task<size_t> count_entries(string dir) {
        auto e = co_await async_read_dir(dir);
        co_return e ? e->size() : 0;
    }
}

TEST_F(IoFs_Tests, AsyncReadDir) {
    ASSERT_TRUE(write_file(at("one"), ""));
    auto t = sgcl::async::spawn(count_entries(dir()));
    EXPECT_EQ(t.wait(), 1u);
    sgcl::async::scheduler::stop();
}

namespace {
    // Every async_ form of the file system in one task, the walk's order
    // that of walk_dir
    task<std::string> fs_in_a_task(string d) {
        string sub = path::join(d, "x/y");
        if (!co_await async_mkdir_all(sub)) {
            co_return "mkdir_all";
        }
        auto f = co_await async_create(path::join(sub, "f"));
        if (!f || !(*f)->write(string("hello"))) {
            co_return "create";
        }
        if (!co_await (*f)->async_sync() || !co_await (*f)->async_truncate(4)) {
            co_return "sync, truncate";
        }
        (void)(*f)->close();
        auto st = co_await async_stat(path::join(sub, "f"));
        if (!st || st->size != 4) {
            co_return "stat";
        }
        if (!co_await async_copy_file(path::join(sub, "f"), path::join(d, "g"))) {
            co_return "copy_file";
        }
        auto g = co_await async_open(path::join(d, "g"));
        if (!g) {
            co_return "open";
        }
        (void)(*g)->close();
        auto copied = read_text(path::join(d, "g"));
        if (!copied || *copied != "hell") {
            co_return "the copy";
        }
        std::string order;
        auto w = co_await async_walk_dir(d, [&](const directory_entry& e, const optional<error>&) {
            order += e.path.str().substr(d.size() + 1) + " ";
            return walk_action::next;
        });
        if (!w) {
            co_return "walk_dir";
        }
        auto l = co_await async_lstat(d);
        if (!l || !l->is_directory()) {
            co_return "lstat";
        }
        if (!co_await async_remove_all(path::join(d, "x"))) {
            co_return "remove_all";
        }
        auto missing = co_await async_open(path::join(d, "x/y/f"));
        if (missing || !missing.error().is_not_found()) {
            co_return "open of a missing file";
        }
        co_return order;
    }
}

TEST_F(IoFs_Tests, AsyncFormsOnThePool) {
    auto t = sgcl::async::spawn(fs_in_a_task(dir()));
    EXPECT_EQ(t.wait(), "g x x/y x/y/f ");
    sgcl::async::scheduler::stop();
}

// The task's walk skips and stops as walk_dir does
TEST_F(IoFs_Tests, AsyncWalkDirSkipsAndStops) {
    ASSERT_TRUE(io::mkdir_all(at("a/b")));
    ASSERT_TRUE(io::mkdir_all(at("a/skip/deep")));
    ASSERT_TRUE(write_file(at("a/b/f"), ""));
    ASSERT_TRUE(write_file(at("a/g"), ""));
    ASSERT_TRUE(write_file(at("z"), ""));
    std::vector<std::string> seen;
    auto r = sgcl::async::spawn(async_walk_dir(dir(), [&](const directory_entry& e, const optional<error>&) {
        seen.push_back(e.path.str().substr(_dir.size() + 1));
        return std::string_view(e.name) == "skip" ? walk_action::skip_dir : walk_action::next;
    })).wait();
    ASSERT_TRUE(r);
    std::vector<std::string> expected = {"a", "a/b", "a/b/f", "a/g", "a/skip", "z"};
    EXPECT_EQ(seen, expected);
    int count = 0;
    sgcl::async::spawn(async_walk_dir(dir(), [&](const directory_entry&, const optional<error>&) {
        return ++count == 2 ? walk_action::stop : walk_action::next;
    })).wait();
    EXPECT_EQ(count, 2);
    auto notdir = sgcl::async::spawn(async_walk_dir(at("z"), [](const directory_entry&, const optional<error>&) { return walk_action::next; })).wait();
    ASSERT_FALSE(notdir);
    sgcl::async::scheduler::stop();
}
