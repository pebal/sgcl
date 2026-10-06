//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// io::watch: the changes of a directory, its tree, a file, with real files of
// this machine (FSEvents on macOS): created, modified, removed, renamed, the
// attributes, the coalescing of a path's events, the paths given back under
// the path named (macOS's temporary directory is a link, /var into
// /private/var), the stop, the channel closed by its receiver, the errors.
#include "tests/types.h"

#include <chrono>
#include <string>
#include <thread>

namespace {
    namespace io = sgcl::io;
    using namespace std::chrono_literals;
    using sgcl::string;

    sgcl::async::task<sgcl::optional<io::watch_event>> next_of(sgcl::async::channel<io::watch_event> changes) {
        co_return co_await changes.receive();
    }

    // The events until one for `path` with `op` (or 10 s), kept in `seen`
    bool wait_for(sgcl::async::channel<io::watch_event>& changes, const std::string& path, io::watch_op op, std::string& seen) {
        auto deadline = std::chrono::steady_clock::now() + 10s;
        while (std::chrono::steady_clock::now() < deadline) {
            auto task = sgcl::async::spawn(sgcl::async::with_timeout(next_of(changes), 1 * sgcl::second));
            auto& got = task.wait();
            if (!got) {
                continue;   // a second without an event
            }
            if (!*got) {
                return false;   // closed
            }
            const io::watch_event& e = **got;
            seen += std::string(e.path.view()) + " " + std::to_string(int(e.ops)) + "\n";
            if (e.path.view() == path && (e.ops & op)) {
                return true;
            }
        }
        return false;
    }

    // Whatever comes within `d`
    std::vector<std::pair<std::string, int>> drain(sgcl::async::channel<io::watch_event>& changes, std::chrono::milliseconds d) {
        std::vector<std::pair<std::string, int>> out;
        auto deadline = std::chrono::steady_clock::now() + d;
        while (std::chrono::steady_clock::now() < deadline) {
            if (auto e = changes.try_receive()) {
                out.push_back({std::string(e->path.view()), int(e->ops)});
            } else {
                std::this_thread::sleep_for(5ms);
            }
        }
        return out;
    }

    struct IoWatch_Tests : testing::Test {
        std::string _dir;   // as io names it: $TMPDIR, a link on macOS

        void SetUp() override {
            _dir = io::make_temp_dir({}, "sgcl-watch-*").value().str();
        }

        void TearDown() override {
            io::remove_all(string(_dir));
        }

        std::string at(const std::string& name) const {
            return _dir + "/" + name;
        }
    };
}

TEST_F(IoWatch_Tests, ADirectorysEntries) {
    sgcl::async::stop_source stop;
    auto changes = io::watch(string(_dir), {.coalesce = 10 * sgcl::millisecond}, stop.token());
    ASSERT_TRUE(changes) << changes.error().message();
    std::string seen;
    ASSERT_TRUE(io::write_file(string(at("a.txt")), "one"));
    EXPECT_TRUE(wait_for(*changes, at("a.txt"), io::watch_op::created, seen)) << seen;
    std::this_thread::sleep_for(50ms);
    ASSERT_TRUE(io::append_file(string(at("a.txt")), "two"));
    EXPECT_TRUE(wait_for(*changes, at("a.txt"), io::watch_op::modified, seen)) << seen;
    ASSERT_TRUE(io::chmod(string(at("a.txt")), io::permissions(0600)));
    EXPECT_TRUE(wait_for(*changes, at("a.txt"), io::watch_op::attribute, seen)) << seen;
    ASSERT_TRUE(io::rename(string(at("a.txt")), string(at("b.txt"))));
    EXPECT_TRUE(wait_for(*changes, at("b.txt"), io::watch_op::renamed, seen)) << seen;
    ASSERT_TRUE(io::remove(string(at("b.txt"))));
    EXPECT_TRUE(wait_for(*changes, at("b.txt"), io::watch_op::removed, seen)) << seen;
    ASSERT_TRUE(io::mkdir(string(at("sub"))));
    seen.clear();
    EXPECT_TRUE(wait_for(*changes, at("sub"), io::watch_op::created, seen)) << seen;
    // a directory's entry says it is one
    EXPECT_NE(seen.find(at("sub")), std::string::npos);
    // not recursive: an entry of sub is not this watch's
    ASSERT_TRUE(io::write_file(string(at("sub/deep.txt")), "x"));
    for (auto& [path, ops] : drain(*changes, 300ms)) {
        EXPECT_NE(path, at("sub/deep.txt"));
    }
    stop.request_stop();
    auto end = sgcl::async::spawn(sgcl::async::with_timeout(next_of(*changes), 5 * sgcl::second));
    auto& last = end.wait();
    ASSERT_TRUE(last);       // not timed out
    EXPECT_FALSE(*last);     // closed
}

TEST_F(IoWatch_Tests, ATree) {
    ASSERT_TRUE(io::mkdir_all(string(at("x/y"))));
    sgcl::async::stop_source stop;
    auto changes = io::watch(string(_dir), {.recursive = true, .coalesce = 10 * sgcl::millisecond}, stop.token());
    ASSERT_TRUE(changes);
    std::string seen;
    ASSERT_TRUE(io::write_file(string(at("x/y/z.txt")), "deep"));
    EXPECT_TRUE(wait_for(*changes, at("x/y/z.txt"), io::watch_op::created, seen)) << seen;
    ASSERT_TRUE(io::mkdir_all(string(at("new/dir"))));
    ASSERT_TRUE(io::write_file(string(at("new/dir/f")), ""));
    EXPECT_TRUE(wait_for(*changes, at("new/dir/f"), io::watch_op::created, seen)) << seen;   // a directory made after the start
    stop.request_stop();
}

TEST_F(IoWatch_Tests, AFile) {
    ASSERT_TRUE(io::write_file(string(at("watched.txt")), "v1"));
    ASSERT_TRUE(io::write_file(string(at("other.txt")), "v1"));
    sgcl::async::stop_source stop;
    auto changes = io::watch(string(at("watched.txt")), {.coalesce = 10 * sgcl::millisecond}, stop.token());
    ASSERT_TRUE(changes) << changes.error().message();
    std::this_thread::sleep_for(50ms);
    ASSERT_TRUE(io::append_file(string(at("other.txt")), "v2"));
    ASSERT_TRUE(io::append_file(string(at("watched.txt")), "v2"));
    std::string seen;
    EXPECT_TRUE(wait_for(*changes, at("watched.txt"), io::watch_op::modified, seen)) << seen;
    EXPECT_EQ(seen.find("other.txt"), std::string::npos) << seen;
    for (auto& [path, ops] : drain(*changes, 200ms)) {
        EXPECT_EQ(path, at("watched.txt"));
    }
    ASSERT_TRUE(io::remove(string(at("watched.txt"))));
    EXPECT_TRUE(wait_for(*changes, at("watched.txt"), io::watch_op::removed, seen)) << seen;
    stop.request_stop();
}

TEST_F(IoWatch_Tests, TheEventsOfAPathCoalesced) {
    sgcl::async::stop_source stop;
    auto changes = io::watch(string(_dir), {.coalesce = 300 * sgcl::millisecond}, stop.token());
    ASSERT_TRUE(changes);
    for (int i : sgcl::range(20)) {
        ASSERT_TRUE(io::append_file(string(at("burst.txt")), string(std::to_string(i))));
    }
    std::string seen;
    ASSERT_TRUE(wait_for(*changes, at("burst.txt"), io::watch_op::modified, seen)) << seen;
    auto rest = drain(*changes, 700ms);
    size_t events = 1 + rest.size();
    EXPECT_LE(events, 4u) << seen;   // twenty writes, a few events at most
    stop.request_stop();
}

TEST_F(IoWatch_Tests, TheChannelClosedByItsReceiver) {
    auto changes = io::watch(string(_dir), {.coalesce = 10 * sgcl::millisecond});   // no stop
    ASSERT_TRUE(changes);
    changes->close();
    ASSERT_TRUE(io::write_file(string(at("after.txt")), "x"));   // the next change ends the watch
    std::this_thread::sleep_for(300ms);
    EXPECT_TRUE(changes->closed());
}

TEST_F(IoWatch_Tests, ThePathsAsNamed) {
    // a path with a trailing separator and a link in it ($TMPDIR on macOS)
    sgcl::async::stop_source stop;
    auto changes = io::watch(string(_dir + "/"), {.coalesce = 10 * sgcl::millisecond}, stop.token());
    ASSERT_TRUE(changes);
    ASSERT_TRUE(io::write_file(string(at("named.txt")), "x"));
    std::string seen;
    EXPECT_TRUE(wait_for(*changes, at("named.txt"), io::watch_op::created, seen)) << seen;
    stop.request_stop();
}

TEST_F(IoWatch_Tests, TheDirectoryRemoved) {
    ASSERT_TRUE(io::mkdir(string(at("gone"))));
    sgcl::async::stop_source stop;
    auto changes = io::watch(string(at("gone")), {.coalesce = 10 * sgcl::millisecond}, stop.token());
    ASSERT_TRUE(changes);
    std::this_thread::sleep_for(50ms);
    ASSERT_TRUE(io::remove(string(at("gone"))));
    std::string seen;
    EXPECT_TRUE(wait_for(*changes, at("gone"), io::watch_op::removed, seen)) << seen;
    stop.request_stop();
}

TEST_F(IoWatch_Tests, Errors) {
    auto missing = io::watch(string(at("missing")));
    ASSERT_FALSE(missing);
    EXPECT_TRUE(missing.error().is_not_found());
    EXPECT_EQ(missing.error().op(), "watch");
    // a stop requested before the first event ends it at once
    sgcl::async::stop_source stop;
    auto changes = io::watch(string(_dir), {}, stop.token());
    ASSERT_TRUE(changes);
    stop.request_stop();
    auto end = sgcl::async::spawn(sgcl::async::with_timeout(next_of(*changes), 5 * sgcl::second));
    auto& last = end.wait();
    ASSERT_TRUE(last);
    EXPECT_FALSE(*last);
}

namespace {
    sgcl::async::task<std::string> watch_in_a_task(string dir) {
        sgcl::async::stop_source stop;
        auto changes = io::watch(dir, {.coalesce = 10 * sgcl::millisecond}, stop.token());
        if (!changes) {
            co_return "watch failed";
        }
        (void)co_await io::async_write_file(dir + "/task.txt", string("x"));
        std::string out;
        while (auto e = co_await changes->receive()) {
            if (e->path.view().ends_with("/task.txt")) {
                out = "created " + std::to_string((e->ops & io::watch_op::created) ? 1 : 0);
                break;
            }
        }
        stop.request_stop();
        co_return out;
    }
}

TEST_F(IoWatch_Tests, FromATask) {
    auto t = sgcl::async::spawn(sgcl::async::with_timeout(watch_in_a_task(string(_dir)), 10 * sgcl::second));
    auto& r = t.wait();
    ASSERT_TRUE(r);
    EXPECT_EQ(*r, "created 1");
}
