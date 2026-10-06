//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// io::lock_file and io::file_lock: a whole file (flock) and byte ranges
// (open-file-description locks), shared and exclusive, a wait, a try, a
// timeout, the async form, the guard; two opens of one file in this process
// exclude each other as two processes do, and Python's fcntl in another
// process is the other side of the cross-process cases.
#include "tests/types.h"

#include <chrono>
#include <cstdlib>
#include <fcntl.h>
#include <string>
#include <sys/file.h>
#include <thread>

namespace {
    namespace io = sgcl::io;
    using namespace std::chrono_literals;
    using sgcl::string;

    struct IoLock_Tests : testing::Test {
        std::string _dir;

        void SetUp() override {
            _dir = io::make_temp_dir({}, "sgcl-lock-*").value().str();
        }

        void TearDown() override {
            io::remove_all(string(_dir));
        }

        string at(const char* name) const {
            return string(_dir + "/" + name);
        }

        io::file opened(const char* name) const {
            return io::open(at(name), io::open_flags::read | io::open_flags::write | io::open_flags::create).value();
        }
    };

    const io::lock_options Try = {.timeout = sgcl::duration::zero()};

    io::lock_options try_as(io::lock_mode m, uint64_t offset = 0, uint64_t length = 0) {
        return {.mode = m, .offset = offset, .length = length, .timeout = sgcl::duration::zero()};
    }

    bool busy(const sgcl::expected<io::file_lock, io::error>& r) {
        return !r && r.error().code() == std::errc::operation_would_block && r.error().is_timeout();
    }
}

TEST_F(IoLock_Tests, TheWholeFile) {
    io::file a = opened("f.lock");
    io::file b = opened("f.lock");
    {
        auto held = io::lock_file(a);
        ASSERT_TRUE(held) << held.error().message();
        EXPECT_TRUE(*held);
        EXPECT_EQ(held->mode(), io::lock_mode::exclusive);
        EXPECT_EQ(held->offset(), 0u);
        EXPECT_EQ(held->length(), 0u);
        EXPECT_TRUE(held->file() == a);
        EXPECT_TRUE(busy(io::lock_file(b, Try)));
        EXPECT_TRUE(busy(io::lock_file(b, try_as(io::lock_mode::shared))));
        auto refused = io::lock_file(b, Try);
        EXPECT_EQ(refused.error().op(), "lock");
        EXPECT_EQ(refused.error().path(), at("f.lock"));
    }   // the guard's end unlocks
    auto mine = io::lock_file(b, Try);
    ASSERT_TRUE(mine);
    ASSERT_TRUE(mine->unlock());
    // shared beside shared, exclusive refused while either is held
    auto r1 = io::lock_file(a, try_as(io::lock_mode::shared));
    auto r2 = io::lock_file(b, try_as(io::lock_mode::shared));
    ASSERT_TRUE(r1 && r2);
    io::file c = opened("f.lock");
    EXPECT_TRUE(busy(io::lock_file(c, Try)));
    ASSERT_TRUE(r1->unlock());
    EXPECT_TRUE(busy(io::lock_file(c, Try)));
    ASSERT_TRUE(r2->unlock());
    EXPECT_TRUE(io::lock_file(c, Try));
}

TEST_F(IoLock_Tests, ByteRanges) {
    io::file a = opened("r.db");
    io::file b = opened("r.db");
    auto first = io::lock_file(a, try_as(io::lock_mode::exclusive, 0, 100));
    ASSERT_TRUE(first) << first.error().message();
    EXPECT_EQ(first->offset(), 0u);
    EXPECT_EQ(first->length(), 100u);
    auto beside = io::lock_file(b, try_as(io::lock_mode::exclusive, 100, 100));
    EXPECT_TRUE(beside) << beside.error().message();
    EXPECT_TRUE(busy(io::lock_file(b, try_as(io::lock_mode::exclusive, 50, 100))));
    EXPECT_TRUE(busy(io::lock_file(b, try_as(io::lock_mode::shared, 99, 1))));
    // shared ranges overlap
    auto s1 = io::lock_file(a, try_as(io::lock_mode::shared, 1000, 10));
    auto s2 = io::lock_file(b, try_as(io::lock_mode::shared, 1005, 10));
    EXPECT_TRUE(s1 && s2);
    // a length of 0: to the end of the file and past it
    auto tail = io::lock_file(a, try_as(io::lock_mode::exclusive, 5000, 0));
    ASSERT_TRUE(tail);
    EXPECT_TRUE(busy(io::lock_file(b, try_as(io::lock_mode::exclusive, 1ull << 40, 1))));
    ASSERT_TRUE(tail->unlock());
    EXPECT_TRUE(io::lock_file(b, try_as(io::lock_mode::exclusive, 1ull << 40, 1)));
    // unlocked, the range is free
    ASSERT_TRUE(first->unlock());
    EXPECT_TRUE(io::lock_file(b, try_as(io::lock_mode::exclusive, 50, 50)));
    // past INT64_MAX: refused
    auto huge = io::lock_file(a, try_as(io::lock_mode::exclusive, ~0ull, 1));
    ASSERT_FALSE(huge);
    EXPECT_EQ(huge.error().code(), std::errc::invalid_argument);
}

TEST_F(IoLock_Tests, TheGuard) {
    io::file a = opened("g.lock");
    io::file b = opened("g.lock");
    io::file_lock none;
    EXPECT_FALSE(none);
    EXPECT_TRUE(none.unlock());
    io::file_lock held = io::lock_file(a).value();
    io::file_lock moved = std::move(held);
    EXPECT_FALSE(held);
    EXPECT_TRUE(moved);
    EXPECT_TRUE(held.unlock());   // the moved-from unlocks nothing
    EXPECT_TRUE(busy(io::lock_file(b, Try)));
    ASSERT_TRUE(moved.unlock());
    EXPECT_FALSE(moved);
    EXPECT_TRUE(moved.unlock());  // a second does nothing
    // an assignment unlocks what the target held
    io::file_lock x = io::lock_file(a).value();
    x = io::file_lock();
    EXPECT_TRUE(io::lock_file(b, Try));
    // a file closed under the guard has given the lock back with its descriptor
    io::file c = opened("h.lock");
    io::file_lock y = io::lock_file(c).value();
    ASSERT_TRUE(c.close());
    auto u = y.unlock();
    ASSERT_FALSE(u);
    EXPECT_EQ(u.error().code(), io::errc::closed);
    EXPECT_TRUE(io::lock_file(at("h.lock"), Try));
    // a closed file is not locked
    auto closed = io::lock_file(c);
    ASSERT_FALSE(closed);
    EXPECT_EQ(closed.error().code(), io::errc::closed);
}

TEST_F(IoLock_Tests, ATimeout) {
    io::file a = opened("t.lock");
    io::file b = opened("t.lock");
    for (bool range : {false, true}) {
        io::lock_options hold;
        if (range) {
            hold.offset = 10;
            hold.length = 10;
        }
        io::file_lock held = io::lock_file(a, hold).value();
        io::lock_options wait = hold;
        wait.timeout = 100 * sgcl::millisecond;
        auto started = std::chrono::steady_clock::now();
        auto r = io::lock_file(b, wait);
        auto took = std::chrono::steady_clock::now() - started;
        ASSERT_FALSE(r) << range;
        EXPECT_EQ(r.error().code(), std::errc::timed_out);
        EXPECT_TRUE(r.error().is_timeout());
        EXPECT_GE(took, 90ms);
        EXPECT_LT(took, 5s);
    }
}

TEST_F(IoLock_Tests, AWaitEndsWhenTheHolderLetsGo) {
    io::file a = opened("w.lock");
    io::file b = opened("w.lock");
    for (bool range : {false, true}) {
        io::lock_options o;
        if (range) {
            o.offset = 0;
            o.length = 8;
        }
        io::file_lock held = io::lock_file(a, o).value();
        const int fd = a.fd();
        std::thread letting_go([fd, range] {   // a descriptor and a flag: nothing of the library's
            std::this_thread::sleep_for(100ms);
            if (range) {
                struct flock fl = {};
                fl.l_type = F_UNLCK;
                fl.l_whence = SEEK_SET;
                fl.l_start = 0;
                fl.l_len = 8;
                ::fcntl(fd, F_OFD_SETLK, &fl);
            } else {
                ::flock(fd, LOCK_UN);
            }
        });
        auto started = std::chrono::steady_clock::now();
        auto mine = io::lock_file(b, o);
        letting_go.join();
        ASSERT_TRUE(mine) << mine.error().message();
        EXPECT_GE(std::chrono::steady_clock::now() - started, 90ms);
        ASSERT_TRUE(mine->unlock());
    }
}

namespace {
    sgcl::async::task<std::string> lock_in_a_task(io::file f, io::lock_options o) {
        auto r = co_await io::async_lock_file(f, o);
        if (!r) {
            co_return std::string(r.error().message().view());
        }
        co_return r->unlock() ? "locked" : "unlock failed";
    }
}

TEST_F(IoLock_Tests, FromATask) {
    io::file a = opened("a.lock");
    io::file b = opened("a.lock");
    io::file_lock held = io::lock_file(a).value();
    std::string path = at("a.lock").str();
    EXPECT_EQ(sgcl::async::spawn(lock_in_a_task(b, Try)).wait(), "lock " + path + ": Resource temporarily unavailable");
    io::lock_options within;
    within.timeout = 60 * sgcl::millisecond;
    EXPECT_EQ(sgcl::async::spawn(lock_in_a_task(b, within)).wait(), "lock " + path + ": Operation timed out");
    const int fd = a.fd();
    std::thread letting_go([fd] {
        std::this_thread::sleep_for(100ms);
        ::flock(fd, LOCK_UN);
    });
    EXPECT_EQ(sgcl::async::spawn(lock_in_a_task(b, {})).wait(), "locked");   // waited on the timers
    letting_go.join();
    // the path form
    auto locking = sgcl::async::spawn(io::async_lock_file(at("new.lock")));
    auto t = std::move(locking.wait());   // a guard: moved out of the task's result
    ASSERT_TRUE(t);
    EXPECT_TRUE(io::exists(at("new.lock")));
    EXPECT_TRUE(busy(io::lock_file(at("new.lock"), Try)));
    io::file_lock taken = std::move(*t);
    ASSERT_TRUE(taken.unlock());
    auto failing = sgcl::async::spawn(io::async_lock_file(string(_dir + "/no/such/dir/x.lock")));
    auto& bad = failing.wait();
    ASSERT_FALSE(bad);
    EXPECT_TRUE(bad.error().is_not_found());
}

TEST_F(IoLock_Tests, ThePathForm) {
    auto held = io::lock_file(at("p.lock"));
    ASSERT_TRUE(held);
    EXPECT_TRUE(io::exists(at("p.lock")));
    EXPECT_TRUE(busy(io::lock_file(at("p.lock"), Try)));
    EXPECT_FALSE(io::lock_file(string(_dir + "/missing/p.lock")));
}

// Another process, Python's fcntl: its flock against the whole file, its
// lockf (a classic fcntl lock, bytes 10 to 110) against a range
TEST_F(IoLock_Tests, AcrossProcesses) {
    if (std::system("python3 -c 'import fcntl' > /dev/null 2>&1") != 0) {
        GTEST_SKIP() << "no python3";
    }
    for (bool range : {false, true}) {
        string file = at(range ? "x.range" : "x.whole");
        string ready = string(file.str() + ".ready");
        std::string script = "import fcntl, os, time\n"
                             "f = open('" + file.str() + "', 'a+')\n" +
                             (range ? "fcntl.lockf(f, fcntl.LOCK_EX, 100, 10)\n" : "fcntl.flock(f, fcntl.LOCK_EX)\n") +
                             "open('" + ready.str() + "', 'w').close()\n"
                             "time.sleep(0.4)\n";
        io::write_file(at("hold.py"), string(script)).value();
        io::command holder("python3", at("hold.py"));
        ASSERT_TRUE(holder.start());
        for (int i = 0; i < 500 && !io::exists(ready); ++i) {
            std::this_thread::sleep_for(10ms);
        }
        ASSERT_TRUE(io::exists(ready));
        io::lock_options o = range ? try_as(io::lock_mode::exclusive, 50, 10) : Try;
        EXPECT_TRUE(busy(io::lock_file(file, o))) << range;
        io::lock_options later = o;
        later.timeout = 5 * sgcl::second;
        auto mine = io::lock_file(file, later);   // the holder ends, its lock with it
        EXPECT_TRUE(mine) << range << ": " << (mine ? "" : mine.error().message().view());
        (void)holder.wait();
    }
}
