//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// io::file and the whole-file functions of file.h at their boundaries
// (DESIGN 408): an empty and a moved-from handle, a closed file at every
// operation, zero bytes read and written, the end at every position, a pipe
// closed at either end, offsets at their limits, paths that are empty, end
// in a separator, name a directory or a FIFO, or change between calls.
#include "tests/types.h"

using namespace sgcl::async;

#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <thread>

namespace {
    using namespace sgcl::io;
    namespace io = sgcl::io;

    struct IoFileBounds_Tests : testing::Test {
        std::string _dir;   // a std::string: the fixture lies in memory gtest allocates (The rules, 1)

        void SetUp() override {
            auto d = make_temp_dir({}, "sgcl-io-bounds-*");
            ASSERT_TRUE(d) << d.error().message();
            _dir = d->str();
        }

        void TearDown() override {
            io::remove_all(string(_dir));
        }

        string at(const char* name) const {
            return path::join(string(_dir), string(name));
        }
    };
}

// An empty file handle holds no file; one moved from is still the file, a
// move of a tracked word being a copy
TEST_F(IoFileBounds_Tests, EmptyAndMovedFromHandles) {
    file none;
    EXPECT_FALSE(none);
    EXPECT_TRUE(none == file());
    auto f = create(at("a"));
    ASSERT_TRUE(f);
    file g = std::move(*f);
    EXPECT_TRUE(*f);
    EXPECT_TRUE(*f == g);
    file h;
    h = std::move(g);
    EXPECT_TRUE(g);
    EXPECT_TRUE(g == h);
    EXPECT_TRUE(h.close());
    EXPECT_TRUE(f->is_closed());
}

// A closed file answers every operation with errc::closed and keeps its
// path; a second close succeeds; its descriptor is -1
TEST_F(IoFileBounds_Tests, AClosedFileAtEveryOperation) {
    auto opened = open(at("c"), open_flags::read | open_flags::write | open_flags::create);
    ASSERT_TRUE(opened);
    file f = *opened;
    ASSERT_TRUE(f.write("abc"));
    ASSERT_TRUE(f.close());
    EXPECT_TRUE(f.is_closed());
    EXPECT_TRUE(f.close());
    EXPECT_EQ(f.fd(), -1);
    EXPECT_EQ(f.path(), at("c"));
    byte out[4];
    EXPECT_EQ(error_of(f.read(out)).code(), errc::closed);
    EXPECT_EQ(error_of(f.read(slice<byte>())).code(), errc::closed);
    EXPECT_EQ(error_of(f.write("x")).code(), errc::closed);
    EXPECT_EQ(error_of(f.write(slice<const byte>())).code(), errc::closed);
    EXPECT_EQ(error_of(f.seek(0)).code(), errc::closed);
    EXPECT_EQ(error_of(f.read_at(out, 0)).code(), errc::closed);
    EXPECT_EQ(error_of(f.write_at(slice<const byte>(out, 1), 0)).code(), errc::closed);
    EXPECT_EQ(error_of(f.sync()).code(), errc::closed);
    EXPECT_EQ(error_of(f.truncate(0)).code(), errc::closed);
    EXPECT_EQ(error_of(f.stat()).code(), errc::closed);
    EXPECT_EQ(error_of(f.chmod(permissions(0600))).code(), errc::closed);
    EXPECT_EQ(error_of(f.read_all()).code(), errc::closed);
    EXPECT_EQ(error_of(f.read_all_text()).code(), errc::closed);
    EXPECT_EQ(error_of(f.read_full(out)).code(), errc::closed);
    EXPECT_EQ(error_of(f.tell()).code(), errc::closed);
    EXPECT_EQ(error_of(f.size()).code(), errc::closed);
    EXPECT_EQ(error_of(f.rewind()).code(), errc::closed);
    EXPECT_EQ(error_of(spawn(f.async_read(out)).wait()).code(), errc::closed);
    EXPECT_EQ(error_of(spawn(f.async_write(slice<const byte>(out, 1))).wait()).code(), errc::closed);
    EXPECT_EQ(error_of(spawn(f.async_read_at(out, 0)).wait()).code(), errc::closed);
    EXPECT_EQ(error_of(spawn(f.async_write_at(slice<const byte>(out, 1), 0)).wait()).code(), errc::closed);
    EXPECT_EQ(error_of(spawn(f.async_sync()).wait()).code(), errc::closed);
    EXPECT_EQ(error_of(spawn(f.async_truncate(0)).wait()).code(), errc::closed);
    EXPECT_EQ(error_of(spawn(f.async_chmod(permissions(0600))).wait()).code(), errc::closed);
    buffer sink;
    EXPECT_EQ(error_of(io::copy(sink, f)).code(), errc::closed);
    EXPECT_EQ(error_of(io::copy(f, buffer("x"))).code(), errc::closed);
    EXPECT_EQ(error_of(io::map(f)).code(), errc::closed);
    EXPECT_EQ(value_of(read_text(at("c"))), "abc");   // the file itself is as it was
}

// Zero bytes read and written: nothing done, 0, on a file and on a pipe
TEST_F(IoFileBounds_Tests, ZeroBytes) {
    auto f = open(at("z"), open_flags::read | open_flags::write | open_flags::create);
    ASSERT_TRUE(f);
    EXPECT_EQ(value_of(f->write(slice<const byte>())), 0u);
    EXPECT_EQ(value_of(f->write("")), 0u);
    EXPECT_EQ(value_of(f->read(slice<byte>())), 0u);
    EXPECT_EQ(value_of(f->read_at(slice<byte>(), 0)), 0u);
    EXPECT_EQ(value_of(f->write_at(slice<const byte>(), 5)), 0u);
    EXPECT_EQ(value_of(f->stat()).size, 0u);
    EXPECT_EQ(value_of(spawn(f->async_read(slice<byte>())).wait()), 0u);
    EXPECT_EQ(value_of(spawn(f->async_write(slice<const byte>())).wait()), 0u);
    ASSERT_TRUE(f->close());
    auto p = pipe();
    ASSERT_TRUE(p);
    EXPECT_EQ(value_of(p->write.write(slice<const byte>())), 0u);
    EXPECT_EQ(value_of(p->read.read(slice<byte>())), 0u);   // returns at once: nothing to wait for
    EXPECT_EQ(value_of(spawn(p->read.async_read(slice<byte>())).wait()), 0u);
    ASSERT_TRUE(p->read.close());
    EXPECT_EQ(value_of(p->write.write(slice<const byte>())), 0u);   // nothing written, no EPIPE
    EXPECT_EQ(error_of(p->write.write("x")).code(), std::errc::broken_pipe);
}

// The end of a file at every position: a read of n bytes from each offset
// gives what is left, then 0; read_at past the end gives 0
TEST_F(IoFileBounds_Tests, TheEndAtEveryPosition) {
    ASSERT_TRUE(write_file(at("e"), "abcdef"));
    auto f = open(at("e"));
    ASSERT_TRUE(f);
    for (int64_t off = 0; off <= 8; ++off) {
        ASSERT_EQ(value_of(f->seek(off)), uint64_t(off));
        byte out[16];
        size_t want = off < 6 ? size_t(6 - off) : 0;
        EXPECT_EQ(value_of(f->read(out)), want) << off;
        EXPECT_EQ(value_of(f->read(out)), 0u) << off;
        EXPECT_EQ(value_of(f->read_at(out, uint64_t(off))), want) << off;
        ASSERT_TRUE(f->seek(off));
        auto full = f->read_full(slice<byte>(out, 3));
        if (want == 0) {
            EXPECT_EQ(value_of(full), 0u);
        } else if (want < 3) {
            ASSERT_FALSE(full);
            EXPECT_EQ(full.error().count(), want);
        } else {
            EXPECT_EQ(value_of(full), 3u);
        }
    }
    byte out[4];
    EXPECT_FALSE(f->read_at(out, UINT64_MAX));                  // past the largest offset: the system's EINVAL
    EXPECT_FALSE(f->seek(-1));
    EXPECT_EQ(value_of(f->tell()), 8u);                         // a refused seek moves nothing
    ASSERT_TRUE(f->close());
}

// Offsets and sizes at their limits on a file opened for writing: refused
// by the system, the file as it was
TEST_F(IoFileBounds_Tests, OffsetsAtTheirLimits) {
    auto f = open(at("l"), open_flags::read | open_flags::write | open_flags::create);
    ASSERT_TRUE(f);
    ASSERT_TRUE(f->write("abc"));
    EXPECT_FALSE(f->write_at(slice<const byte>(reinterpret_cast<const byte*>("x"), 1), UINT64_MAX));
    EXPECT_FALSE(f->truncate(UINT64_MAX));
    EXPECT_EQ(value_of(f->stat()).size, 3u);
    ASSERT_TRUE(f->truncate(0));
    EXPECT_EQ(value_of(f->stat()).size, 0u);
    EXPECT_EQ(value_of(f->tell()), 3u);   // the position stays past the end
    ASSERT_TRUE(f->write("d"));            // a hole of zeros before it
    EXPECT_EQ(value_of(read_text(at("l"))), std::string("\0\0\0d", 4));
    ASSERT_TRUE(f->close());
}

// A pipe closed at either end: the reader sees the end after what was
// written, at every point of it; the writer sees EPIPE; a pipe has no
// position, no offsets and no size
TEST_F(IoFileBounds_Tests, APipeClosedAtEitherEnd) {
    for (size_t k = 0; k <= 3; ++k) {
        auto p = pipe();
        ASSERT_TRUE(p);
        ASSERT_TRUE(p->write.write(std::string_view("abc").substr(0, k)));
        ASSERT_TRUE(p->write.close());
        EXPECT_EQ(value_of(p->read.read_all_text()), std::string("abc").substr(0, k));
        byte out[2];
        EXPECT_EQ(value_of(p->read.read(out)), 0u);
        ASSERT_TRUE(p->read.close());
    }
    auto p = pipe();
    ASSERT_TRUE(p);
    byte out[2];
    EXPECT_EQ(error_of(p->read.seek(0)).code(), std::errc::invalid_seek);
    EXPECT_EQ(error_of(p->read.read_at(out, 0)).code(), std::errc::invalid_seek);
    EXPECT_EQ(error_of(p->write.write_at(slice<const byte>(out, 1), 0)).code(), std::errc::invalid_seek);
    EXPECT_FALSE(p->write.truncate(0));
    ASSERT_TRUE(p->read.close());
    EXPECT_EQ(error_of(p->write.write("x")).code(), std::errc::broken_pipe);
    EXPECT_EQ(error_of(spawn(p->write.async_write(slice<const byte>(out, 1))).wait()).code(), std::errc::broken_pipe);
    ASSERT_TRUE(p->write.close());
}

// A pipe's writer closed while a task waits on the read: the wait ends at
// the end of the stream; the reader closed under a waiting read: closed
TEST_F(IoFileBounds_Tests, AWaitingReadOfAPipe) {
    auto p = pipe();
    ASSERT_TRUE(p);
    file r = p->read;
    file w = p->write;
    auto reading = spawn([](file f) -> task<expected<size_t, io::error>> {
        byte out[4];
        co_return co_await f.async_read(slice<byte>(out, 4));
    }(r));
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    ASSERT_TRUE(w.close());
    EXPECT_EQ(value_of(reading.wait()), 0u);
    auto q = pipe();
    ASSERT_TRUE(q);
    file r2 = q->read;
    auto waiting = spawn([](file f) -> task<expected<size_t, io::error>> {
        byte out[4];
        co_return co_await f.async_read(slice<byte>(out, 4));
    }(r2));
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    ASSERT_TRUE(r2.close());
    EXPECT_EQ(error_of(waiting.wait()).code(), errc::closed);
    ASSERT_TRUE(q->write.close());
    ASSERT_TRUE(r.close());
}

// A descriptor that is not open: from_fd checks nothing, and every
// operation is the system's EBADF (is_closed())
TEST_F(IoFileBounds_Tests, AFileOverNoDescriptor) {
    file f = from_fd(-1, "nothing");
    EXPECT_TRUE(f);
    byte out[2];
    auto r = f.read(out);
    ASSERT_FALSE(r);
    EXPECT_TRUE(r.error().is_closed());
    EXPECT_EQ(r.error().path(), "nothing");
    auto w = f.write("x");
    ASSERT_FALSE(w);
    EXPECT_TRUE(w.error().is_closed());
    EXPECT_FALSE(f.stat());
}

// Paths: empty, ending in a separator, a directory, a FIFO
TEST_F(IoFileBounds_Tests, PathsThatAreNoFile) {
    EXPECT_TRUE(error_of(open("")).is_not_found());
    EXPECT_TRUE(error_of(read_file("")).is_not_found());
    EXPECT_TRUE(error_of(read_text("")).is_not_found());
    EXPECT_FALSE(write_file("", "x"));
    EXPECT_FALSE(append_file("", "x"));
    ASSERT_TRUE(write_file(at("f"), "abc"));
    string trailing = at("f") + "/";
    EXPECT_EQ(error_of(open(trailing)).code(), std::errc::not_a_directory);
    EXPECT_EQ(error_of(read_text(trailing)).code(), std::errc::not_a_directory);
    EXPECT_FALSE(write_file(trailing, "x"));
    EXPECT_EQ(value_of(read_text(at("f"))), "abc");
    ASSERT_TRUE(mkdir(at("d")));
    string dir_slash = at("d") + "/";
    EXPECT_EQ(error_of(read_file(at("d"))).code(), std::errc::is_a_directory);
    EXPECT_EQ(error_of(read_text(at("d"))).code(), std::errc::is_a_directory);
    EXPECT_EQ(error_of(read_text(dir_slash)).code(), std::errc::is_a_directory);
    EXPECT_EQ(error_of(write_file(at("d"), "x")).code(), std::errc::is_a_directory);
    EXPECT_EQ(error_of(append_file(at("d"), "x")).code(), std::errc::is_a_directory);
    EXPECT_EQ(error_of(create(dir_slash)).code(), std::errc::is_a_directory);
    EXPECT_EQ(error_of(spawn(async_read_text(at("d"))).wait()).code(), std::errc::is_a_directory);
}

// The whole-file functions on a FIFO: its size is 0, so read_file and
// read_text read on to the end its writer makes
TEST_F(IoFileBounds_Tests, AFifoInPlaceOfAFile) {
    string fifo = at("fifo");
    ASSERT_EQ(::mkfifo(fifo.c_str(), 0600), 0);
    std::string fifo_path = fifo.str();
    for (int round = 0; round < 2; ++round) {
        std::thread writer([fifo_path] {
            auto w = io::open(string(fifo_path), open_flags::write);
            if (w) {
                (void)w->write("through a fifo");
                (void)w->close();
            }
        });
        if (round == 0) {
            EXPECT_EQ(value_of(read_text(fifo)), "through a fifo");
        } else {
            auto bytes = value_of(read_file(fifo));
            EXPECT_EQ(std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size()), "through a fifo");
        }
        writer.join();
    }
}

// A path that changes between calls: a file, then gone, then a directory,
// then a symlink to itself; an open file outlives its name
TEST_F(IoFileBounds_Tests, APathThatChangesBetweenCalls) {
    string p = at("changing");
    ASSERT_TRUE(write_file(p, "first"));
    auto f = open(p);
    ASSERT_TRUE(f);
    ASSERT_TRUE(io::remove(p));
    EXPECT_TRUE(error_of(read_text(p)).is_not_found());
    EXPECT_EQ(value_of(f->read_all_text()), "first");   // the file outlives its name
    ASSERT_TRUE(mkdir(p));
    EXPECT_EQ(error_of(read_text(p)).code(), std::errc::is_a_directory);
    ASSERT_TRUE(io::remove(p));
    ASSERT_TRUE(symlink(p, p));
    EXPECT_EQ(error_of(read_text(p)).code(), std::errc::too_many_symbolic_link_levels);
    EXPECT_EQ(error_of(open(p)).code(), std::errc::too_many_symbolic_link_levels);
    ASSERT_TRUE(io::remove(p));
    ASSERT_TRUE(write_file(p, ""));
    EXPECT_EQ(value_of(read_text(p)), "");
    EXPECT_TRUE(value_of(read_file(p)).empty());
    ASSERT_TRUE(append_file(p, ""));
    ASSERT_TRUE(append_file(p, "x"));
    EXPECT_EQ(value_of(read_text(p)), "x");
    ASSERT_TRUE(f->close());
}

// temp_file and make_temp_dir: the last '*' replaced, the random part
// appended without one, an empty pattern, a directory that is not there
TEST_F(IoFileBounds_Tests, TempNamesAtTheirBounds) {
    auto f = temp_file(string(_dir), "a*b*c");
    ASSERT_TRUE(f);
    string name = path::base(f->path());
    EXPECT_TRUE(name.view().starts_with("a*b"));
    EXPECT_TRUE(name.view().ends_with("c"));
    EXPECT_EQ(name.size(), 3u + 10u + 1u);
    auto g = temp_file(string(_dir), "");
    ASSERT_TRUE(g);
    EXPECT_EQ(path::base(g->path()).size(), 10u);
    auto h = temp_file(string(_dir), "plain");
    ASSERT_TRUE(h);
    EXPECT_TRUE(path::base(h->path()).view().starts_with("plain"));
    EXPECT_TRUE(error_of(temp_file(at("missing"))).is_not_found());
    EXPECT_TRUE(error_of(make_temp_dir(at("missing"))).is_not_found());
    auto d = make_temp_dir(string(_dir), "");
    ASSERT_TRUE(d);
    EXPECT_TRUE(is_directory(*d));
    ASSERT_TRUE(f->close());
    ASSERT_TRUE(g->close());
    ASSERT_TRUE(h->close());
}

// read_lines: the lines buffered_reader::lines() gives of the same bytes
// (the oracle), for no line, empty lines, ends with and without "\r", a
// line of the block's size, one byte short and one past it, NUL bytes and
// bytes that are not UTF-8; then a path that is empty, missing, a
// directory, a FIFO; the task's form, with the caller's path gone before
// it runs
TEST_F(IoFileBounds_Tests, ReadLines) {
    auto oracle = [](const std::string& text) {
        std::vector<std::string> lines;
        buffered_reader in{buffer(string(text))};
        for (auto line : in.lines()) {
            lines.emplace_back(line.data(), line.size());
        }
        return lines;
    };
    auto of = [](const vector<string>& v) {
        std::vector<std::string> lines;
        for (auto& line : v) {
            lines.emplace_back(line.data(), line.size());
        }
        return lines;
    };
    const size_t block = sgcl::config::io_buffer_size;
    const std::string texts[] = {
        "", "\n", "\n\n", "a", "a\n", "a\nb", "a\r\nb\r\n", "\r\n", "\r", "a\r", "a\rb\n", "\r\r\n",
        std::string("x\0y\n\0", 5), "\xff\xfe\n\xc3",
        std::string(block - 1, 'a') + "\n" + std::string(block, 'b') + "\n" + std::string(block + 1, 'c'),
        std::string(3 * block + 7, 'd') + "\r\n",
    };
    for (const auto& text : texts) {
        ASSERT_TRUE(write_file(at("t"), slice<const char>(text.data(), text.size())));
        auto expected = oracle(text);
        EXPECT_EQ(of(value_of(read_lines(at("t")))), expected) << text.size();
        EXPECT_EQ(of(value_of(spawn(async_read_lines(at("t"))).wait())), expected) << text.size();
    }
    EXPECT_EQ(of(value_of(read_lines(at("t")))).back().size(), 3 * block + 7);
    EXPECT_TRUE(error_of(read_lines("")).is_not_found());
    EXPECT_TRUE(error_of(read_lines(at("missing"))).is_not_found());
    EXPECT_TRUE(error_of(spawn(async_read_lines(at("missing"))).wait()).is_not_found());
    ASSERT_TRUE(mkdir(at("d")));
    EXPECT_EQ(error_of(read_lines(at("d"))).code(), std::errc::is_a_directory);
    EXPECT_EQ(error_of(spawn(async_read_lines(at("d"))).wait()).code(), std::errc::is_a_directory);
    string fifo = at("fifo");
    ASSERT_EQ(::mkfifo(fifo.c_str(), 0600), 0);
    std::string fifo_path = fifo.str();
    std::thread writer([fifo_path] {
        auto w = io::open(string(fifo_path), open_flags::write);
        if (w) {
            (void)w->write("one\ntwo");
            (void)w->close();
        }
    });
    EXPECT_EQ(of(value_of(read_lines(fifo))), (std::vector<std::string>{"one", "two"}));
    writer.join();
    ASSERT_TRUE(write_file(at("kept"), "x\ny\n"));
    auto task = [&] {
        string gone = at("kept");
        return async_read_lines(gone);
    }();
    EXPECT_EQ(of(value_of(spawn(std::move(task)).wait())), (std::vector<std::string>{"x", "y"}));
}
