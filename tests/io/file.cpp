//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

using namespace sgcl::async;

#include <string>
#include <string_view>
#include <thread>

namespace {
    using namespace sgcl::io;
    namespace io = sgcl::io;

    // A directory of its own per test, removed afterwards
    struct IoFile_Tests : testing::Test {
        // A std::string: the fixture is allocated by gtest with new, where a
        // tracked_ptr (a sgcl::string) may not live (The rules, 1)
        std::string _dir;

        void SetUp() override {
            auto d = make_temp_dir({}, "sgcl-io-*");
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

    std::string_view as_text(std::span<const byte> b) {
        return std::string_view(reinterpret_cast<const char*>(b.data()), b.size());
    }
}

TEST_F(IoFile_Tests, CreateWriteReadBack) {
    auto f = create(at("a.txt"));
    ASSERT_TRUE(f) << f.error().message();
    EXPECT_FALSE((*f)->is_nonblocking());
    EXPECT_EQ((*f)->path(), at("a.txt"));
    ASSERT_TRUE((*f)->write("hello\nworld\n"));
    EXPECT_EQ(*(*f)->tell(), 12u);
    ASSERT_TRUE((*f)->close());
    EXPECT_TRUE((*f)->is_closed());
    EXPECT_TRUE((*f)->close());   // again: nothing, no error
    auto again = (*f)->write("x");
    ASSERT_FALSE(again);
    EXPECT_TRUE(again.error().is_closed());

    auto r = read_text(at("a.txt"));
    ASSERT_TRUE(r);
    EXPECT_EQ(std::string_view(*r), "hello\nworld\n");
    auto bytes = read_file(at("a.txt"));
    ASSERT_TRUE(bytes);
    EXPECT_EQ(bytes->size(), 12u);

    auto missing = io::open(at("nope"));
    ASSERT_FALSE(missing);
    EXPECT_TRUE(missing.error().is_not_found());
    EXPECT_EQ(missing.error().op(), "open");
    EXPECT_EQ(missing.error().path(), at("nope"));
}

TEST_F(IoFile_Tests, WriteFileAppendFileAndFlags) {
    ASSERT_TRUE(write_file(at("w"), "first"));
    ASSERT_TRUE(write_file(at("w"), "second"));   // truncated
    EXPECT_EQ(std::string_view(*read_text(at("w"))), "second");
    ASSERT_TRUE(append_file(at("w"), " third"));
    EXPECT_EQ(std::string_view(*read_text(at("w"))), "second third");
    ASSERT_TRUE(append_file(at("new"), "made"));   // created by append
    EXPECT_EQ(std::string_view(*read_text(at("new"))), "made");
    auto excl = io::open(at("w"), open_flags::write | open_flags::create | open_flags::exclusive);
    ASSERT_FALSE(excl);
    EXPECT_TRUE(excl.error().is_exists());
    auto ro = io::open(at("w"));
    ASSERT_TRUE(ro);
    auto w = (*ro)->write("x");
    ASSERT_FALSE(w);
    EXPECT_EQ(w.error().code(), std::errc::bad_file_descriptor);
    auto wo = io::open(at("absent"), open_flags::write);   // write without create
    ASSERT_FALSE(wo);
    EXPECT_TRUE(wo.error().is_not_found());
}

TEST_F(IoFile_Tests, SeekReadAtWriteAtTruncateStat) {
    auto f = io::open(at("s"), open_flags::read | open_flags::write | open_flags::create);
    ASSERT_TRUE(f);
    ASSERT_TRUE((*f)->write("0123456789"));
    EXPECT_EQ(*(*f)->size(), 10u);
    EXPECT_EQ(*(*f)->tell(), 10u);   // size() keeps the position
    ASSERT_TRUE((*f)->rewind());
    byte b[4];
    EXPECT_EQ(*(*f)->read_full(b), 4u);
    EXPECT_EQ(as_text(b), "0123");
    EXPECT_EQ(*(*f)->seek(-2, seek_from::end), 8u);
    EXPECT_EQ(*(*f)->read(b), 2u);
    EXPECT_EQ(as_text(std::span<const byte>(b, 2)), "89");
    EXPECT_EQ(*(*f)->read_at(b, 3), 4u);   // the position untouched
    EXPECT_EQ(as_text(b), "3456");
    EXPECT_EQ(*(*f)->tell(), 10u);
    std::string_view rep = "AB";
    EXPECT_EQ(*(*f)->write_at(std::as_bytes(std::span(rep)), 1), 2u);
    ASSERT_TRUE((*f)->truncate(6));
    ASSERT_TRUE((*f)->sync());
    auto info = (*f)->stat();
    ASSERT_TRUE(info);
    EXPECT_EQ(info->size, 6u);
    EXPECT_TRUE(info->is_regular());
    EXPECT_EQ(std::string_view(info->name), "s");
    EXPECT_EQ(std::string_view(*read_text(at("s"))), "0AB345");
    ASSERT_TRUE((*f)->chmod(permissions(0600)));
    EXPECT_EQ(static_cast<unsigned>(io::stat(at("s"))->mode), 0600u);
}

TEST_F(IoFile_Tests, BufferedOverAFile) {
    {
        auto f = create(at("lines"));
        ASSERT_TRUE(f);
        sgcl::tracked_ptr w = make_tracked<buffered_writer>(*f);
        for (int i = 0; i < 5000; ++i) {
            ASSERT_TRUE(w->write(string("line " + std::to_string(i) + "\n")));
        }
        ASSERT_TRUE(w->close());   // flushes and closes the file
        EXPECT_TRUE((*f)->is_closed());
    }
    auto f = io::open(at("lines"));
    ASSERT_TRUE(f);
    sgcl::tracked_ptr r = make_tracked<buffered_reader>(*f);
    int n = 0;
    for (auto line : r->lines()) {
        ASSERT_EQ(line, "line " + std::to_string(n));
        ++n;
    }
    EXPECT_EQ(n, 5000);
    EXPECT_FALSE(r->last_error());
}

// A file held as an io::reader is closed through the handle, as a writer
// is (Go's ReadCloser); a buffered reader closes its source, as a buffered
// writer closes its writer; a stream with no close closes nothing
TEST_F(IoFile_Tests, AReaderHandleClosesItsStream) {
    ASSERT_TRUE(write_file(at("r"), "abc"));
    auto f = io::open(at("r"));
    ASSERT_TRUE(f);
    io::reader h(*f);
    EXPECT_TRUE(h.has_close());
    ASSERT_TRUE(h.close());
    EXPECT_TRUE((*f)->is_closed());
    byte b[1];
    auto after = h.read(b);
    ASSERT_FALSE(after);
    EXPECT_TRUE(after.error().is_closed());
    auto g = io::open(at("r"));
    ASSERT_TRUE(g);
    sgcl::tracked_ptr br = make_tracked<buffered_reader>(*g);
    ASSERT_EQ(*br->read(b), 1u);
    ASSERT_TRUE(br->close());
    EXPECT_TRUE((*g)->is_closed());
    io::reader none(make_tracked<buffer>(std::string_view("x")));
    EXPECT_FALSE(none.has_close());
    EXPECT_TRUE(none.close());
    EXPECT_TRUE(io::reader().close());
    auto k = io::open(at("r"));
    ASSERT_TRUE(k);
    io::reader hk(*k);
    EXPECT_TRUE(sgcl::async::spawn(hk.async_close()).wait());
    EXPECT_TRUE((*k)->is_closed());
    EXPECT_TRUE(sgcl::async::spawn(none.async_close()).wait());
    EXPECT_TRUE(sgcl::async::spawn(io::reader().async_close()).wait());
    io::writer sink([](slice<const byte> d) { return d.size(); });
    EXPECT_FALSE(sink.has_close());
    EXPECT_TRUE(sgcl::async::spawn(sink.async_close()).wait());   // nothing to close: no pool
    auto tf = sgcl::async::spawn(async_temp_file(dir(), "t-*")).wait();
    ASSERT_TRUE(tf);
    EXPECT_TRUE(std::string_view((*tf)->path()).starts_with(_dir));
    (void)(*tf)->close();
    auto td = sgcl::async::spawn(async_make_temp_dir(dir(), "d-*")).wait();
    ASSERT_TRUE(td);
    EXPECT_TRUE(is_directory(*td));
    sgcl::async::scheduler::stop();
}

TEST_F(IoFile_Tests, TempFileAndPipe) {
    auto t = temp_file(dir(), "up-*.tmp");
    ASSERT_TRUE(t) << t.error().message();
    auto name = path::base((*t)->path());
    EXPECT_TRUE(name.starts_with("up-"));
    EXPECT_TRUE(name.ends_with(".tmp"));
    EXPECT_EQ(name.size(), 3 + 10 + 4);
    EXPECT_TRUE(exists((*t)->path()));
    EXPECT_EQ(static_cast<unsigned>((*t)->stat()->mode), 0600u);
    ASSERT_TRUE((*t)->write("tmp"));
    ASSERT_TRUE((*t)->rewind());
    EXPECT_EQ(std::string_view(*(*t)->read_all_text()), "tmp");

    auto p = io::pipe();
    ASSERT_TRUE(p);
    auto& [rd, wr] = *p;
    EXPECT_EQ(p->read, rd);                                // the ends by name too
    EXPECT_EQ(p->write, wr);
    EXPECT_TRUE(rd->is_nonblocking());
    std::thread producer([w = sgcl::root_ptr<file>(wr)] {   // a thread's closure is unmanaged memory: a root_ptr
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        w->write("through the pipe");
        w->close();
    });
    auto got = rd->read_all_text();   // the synchronous form polls on EAGAIN
    producer.join();
    ASSERT_TRUE(got);
    EXPECT_EQ(std::string_view(*got), "through the pipe");
}

namespace {
    std::string_view textof(std::span<const byte> b) {
        return std::string_view(reinterpret_cast<const char*>(b.data()), b.size());
    }

    task<void> delayed_ping(tracked_ptr<file> wr);

    task<std::string> async_case(string file, std::string big) {
        // a regular file: through the blocking pool
        auto w = co_await async_write_file(file, string(big));
        if (!w) {
            co_return "write: " + w.error().message().str();
        }
        auto r = co_await async_read_file(file);
        if (!r || r->size() != big.size()) {
            co_return "read_file";
        }
        auto f = io::open(file);
        if (!f) {
            co_return "open";
        }
        byte head[6];
        auto n = co_await (*f)->async_read_full(head);
        if (!n || *n != 6 || textof(head) != "qqqqqq") {
            co_return "read";
        }
        auto at5 = co_await (*f)->async_read_at(head, big.size() - 3);
        if (!at5 || *at5 != 3) {
            co_return "read_at";
        }
        // a pipe: through the reactor, the reader waiting for the writer
        auto p = io::pipe();
        if (!p) {
            co_return "pipe";
        }
        auto [rd, wr] = *p;
        sgcl::async::go(delayed_ping(wr));
        auto got = co_await rd->async_read_all_text();
        if (!got) {
            co_return "pipe read: " + got.error().message().str();
        }
        co_return got->str();
    }

    task<void> delayed_ping(tracked_ptr<file> wr) {
        co_await sgcl::async::sleep(std::chrono::milliseconds(5));
        co_await wr->async_write("ping");
        wr->close();
    }

    task<void> write_big(tracked_ptr<file> wr, std::string big) {
        co_await wr->async_write(string(big));
        wr->close();
    }

    task<size_t> pipe_case(std::string big) {
        auto p = io::pipe();
        auto [rd, wr] = *p;
        auto writer = sgcl::async::spawn(write_big(wr, big));
        auto got = co_await rd->async_read_all();
        co_await writer;
        co_return got ? got->size() : 0;
    }
}

TEST_F(IoFile_Tests, AsyncOnThePoolAndTheReactor) {
    auto t = sgcl::async::spawn(async_case(at("async"), std::string(200000, 'q')));
    EXPECT_EQ(t.wait(), "ping");
    sgcl::async::scheduler::stop();
}

TEST_F(IoFile_Tests, PipeWriterBlocksUntilRead) {
    // more than the pipe's capacity: the writer waits for the reader
    auto t = sgcl::async::spawn(pipe_case(std::string(1 << 20, 'p')));
    EXPECT_EQ(t.wait(), size_t(1 << 20));
    sgcl::async::scheduler::stop();
}

// The async forms of the whole-file functions and read_dir, made from
// temporaries and started only after the full expression that made them:
// a task is lazy, so it must hold what it was given (a copy of the path
// and of the text), not a reference into the caller's frame. Under the
// address sanitizer a reference kept would be a use after its scope
TEST_F(IoFile_Tests, AsyncFormsHoldTheirArguments) {
    auto write = io::async_write_file(at(string("a")), string("written"));
    auto append = io::async_append_file(at(string("a")), string(" and more"));
    auto write_bytes = io::async_write_file(at(string("b")), as_bytes(string("bytes").as_slice()));
    auto append_bytes = io::async_append_file(at(string("b")), as_bytes(string("!").as_slice()));
    auto read_text = io::async_read_text(at(string("a")));
    auto read_bytes = io::async_read_file(at(string("b")));
    auto list = io::async_read_dir(string(dir()));
    EXPECT_TRUE(spawn(std::move(write)).wait());
    EXPECT_TRUE(spawn(std::move(append)).wait());
    EXPECT_TRUE(spawn(std::move(write_bytes)).wait());
    EXPECT_TRUE(spawn(std::move(append_bytes)).wait());
    auto text = spawn(std::move(read_text)).wait();
    ASSERT_TRUE(text);
    EXPECT_EQ(text->view(), "written and more");
    auto bytes = spawn(std::move(read_bytes)).wait();
    ASSERT_TRUE(bytes);
    EXPECT_EQ(as_text(std::span<const byte>(bytes->data(), bytes->size())), "bytes!");
    auto entries = spawn(std::move(list)).wait();
    ASSERT_TRUE(entries);
    EXPECT_EQ(entries->size(), 2u);
    sgcl::async::scheduler::stop();
}
